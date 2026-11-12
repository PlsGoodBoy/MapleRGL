#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace maple {
// Pure scheduling data: no Game, Vec2, EnemyKind or rendering dependency.
enum class EncounterKind { None, Swarm, Encirclement, Rush, EliteHunt, Crisis, Breather, Boss, Count };
enum class EncounterPhase { Idle, Telegraph, Active, Cooldown };
enum class DirectorEnemyRole { Common, Swarm, Fast, Armored, Ranged, Support, Elite, Boss, Ground, Charger };
enum class SpawnGeometry { Scatter, Arc, OpenRing, Lane };

struct EncounterRule {
 EncounterKind kind=EncounterKind::Swarm;
 float weight=1,unlock_progress=0,telegraph=3,duration=16,cooldown=9,spawn_interval=2;
 int base_budget=20,batch_size=4;
 SpawnGeometry geometry=SpawnGeometry::Arc;
 bool important=true;
};
std::array<EncounterRule,6> default_encounter_rules();
struct DirectorConfig {
 std::array<EncounterRule,6> rules=default_encounter_rules();
 float first_encounter=12,encounter_gap_min=8,encounter_gap_max=14;
 float min_safe_distance=320,escape_half_angle=.52f;
 float low_health=.32f,high_pressure=.78f,pressure_decay_seconds=7;
 float breather_duration=12,breather_cooldown=18,boss_telegraph=4,boss_recovery=10;
 float ambient_interval=.8f;
 std::size_t max_spawns_per_update=16;
};
struct DirectorInput {
 float elapsed=0,chapter_duration=600;
 int chapter=0,difficulty=0; // difficulty: Normal=0, Hard=1, Nightmare=2.
 float hp_ratio=1,recent_pressure=0;
 std::size_t enemy_count=0,max_enemies=500;
 float spawn_radius=520;
 // Scale requested ordinary counts once; cadence and Boss counts are unchanged.
 float spawn_budget_multiplier=1;
 float reserved_escape_angle=0,reserved_escape_half_angle=0;
 float move_x=0,move_y=0; // Optional current escape preference, in world coordinates.
 bool boss_due=false,boss_alive=false,boss_defeated=false;
 bool external_important_active=false;
};
struct DirectorSpawn {
 DirectorEnemyRole role=DirectorEnemyRole::Common;
 float offset_x=0,offset_y=0; // Relative to the CURRENT player position, not a stale origin.
 float speed_scale=1,health_scale=1;
 std::uint32_t encounter_id=0;
};
struct DirectorOutput {
 EncounterKind kind=EncounterKind::None,ended_kind=EncounterKind::None;
 EncounterPhase phase=EncounterPhase::Idle;
 SpawnGeometry geometry=SpawnGeometry::Scatter;
 std::uint32_t encounter_id=0;
 float remaining=0,cooldown_remaining=0,pressure=0;
 float escape_angle=0,escape_half_angle=0,min_spawn_distance=0;
 int budget_remaining=0;
 bool announced=false,started=false,ended=false,aborted=false;
 bool important_active=false,allow_special_events=true,suppress_environment=false;
 std::uint64_t normal_spawn_budget=0,event_spawn_budget=0,requested_spawns=0,capacity_limited=0;
 std::vector<DirectorSpawn> spawns;
};
class EncounterDirector {
public:
 explicit EncounterDirector(DirectorConfig config={});
 void reset(std::uint32_t seed=1,int chapter=0);
 DirectorOutput update(float dt,const DirectorInput& input);
 const DirectorConfig& config() const {return config_;}
 EncounterKind kind() const {return kind_;}
 EncounterPhase phase() const {return phase_;}
 float pressure() const {return pressure_;}
 static const char* encounter_name(EncounterKind kind);
 static const char* encounter_hint(EncounterKind kind);
private:
 DirectorConfig config_;
 std::uint32_t seed_=1,random_state_=1,serial_=0;
 int chapter_=0,budget_=0,spawn_index_=0,elite_hunts_=0;
 EncounterKind kind_=EncounterKind::None,last_kind_=EncounterKind::None;
 EncounterPhase phase_=EncounterPhase::Idle;
 EncounterRule rule_{};
 float remaining_=0,next_encounter_=0,spawn_clock_=0,ambient_clock_=0;
 float pressure_=0,escape_angle_=0;
 bool boss_sent_=false,boss_seen_=false,boss_request_started_=false;
 std::uint32_t random_word();
 float random_unit();
 EncounterRule select_rule(const DirectorInput& input);
 void begin(const EncounterRule& rule,const DirectorInput& input,DirectorOutput& output);
 void finish(bool aborted,DirectorOutput& output);
 void spawn_batch(const DirectorInput& input,DirectorOutput& output,int count,bool ambient);
 void snapshot(const DirectorInput& input,DirectorOutput& output) const;
};
} // namespace maple

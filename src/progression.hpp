#pragma once
#include "game.hpp"
#include "../third_party/json.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace maple {
constexpr int profile_schema_version = 8;
enum class TalentId { Damage, Crit, FirstEliteBonus, Health, MoveSpeed, DeathProtection, XP, Reroll, Cooldown, Count };
constexpr int talent_count = static_cast<int>(TalentId::Count);
struct TalentDefinition { TalentId id; const char* key; const char* name; const char* description; int route; int max_rank; int base_cost; int cost_step; };
const std::array<TalentDefinition,talent_count>& talent_definitions();
const char* talent_route_name(int route);
enum class AchievementId { FirstHundred,SeasonedHunter,ThousandFoes,EliteHunter,BossSlayer,EventHelper,TreasureFinder,FirstChapter,CampaignVictor,ArcherVictory,MageVictory,WarriorVictory,LevelTwenty,FirstEvolution,TripleEvolution,Dashing,SixSkills,EventVeteran,RunSlayer,UnscathedVictory,Count };
constexpr int achievement_count=static_cast<int>(AchievementId::Count);
enum class AchievementMetric { Kills,Elites,Bosses,Events,Items,Chapters,CampaignWins,ArcherWins,MageWins,WarriorWins,Level,Evolutions,Dashes,Skills,RunKills,LowDamageWins };
struct AchievementDefinition { AchievementId id; const char* key; const char* name; const char* description; AchievementMetric metric; uint64_t target; EquipmentId equipment_reward=EquipmentId::None; uint32_t relic_reward_mask=0; bool challenge_reward=false; int crystal_reward=0; };
struct AchievementStatus { AchievementId id; uint64_t progress=0,target=0; bool completed=false; };
struct AchievementStats { uint64_t kills=0,elites=0,bosses=0,events=0,items=0,dashes=0,chapters=0,campaign_wins=0,low_damage_wins=0; std::array<uint64_t,3> class_wins{}; std::array<uint64_t,enemy_kind_count> enemy_kills{}; int best_level=1,best_evolutions=0,best_skills=0,best_run_kills=0; };
const std::array<AchievementDefinition,achievement_count>& achievement_definitions();
std::string achievement_reward_text(AchievementId id);
std::string equipment_unlock_requirement(EquipmentId id);
std::string relic_unlock_requirement(RelicId id);
enum class RunOutcome { Victory, Defeat, Abandoned, Interrupted };
const char* run_outcome_name(RunOutcome outcome);
struct RunReport {
 std::vector<GearStack> run_gear;
 std::array<int,resonance_count> resonance_directions{};
 ResonanceStatistics resonance;
 ScrollState scrolls;RunTiming timing;ContentStatistics content;
 bool adventurer=false; int upgrade_choices=0,bonus_choices=0;
 RunOutcome outcome=RunOutcome::Defeat; CharacterClass character=CharacterClass::Archer;
 uint32_t seed=0; float elapsed=0; int kills=0,elite_kills=0,events_succeeded=0,level=1;
 bool boss_defeated=false; double damage_dealt=0,damage_taken=0,xp_acquired=0;
 RunMode mode=RunMode::Classic; int chapters_cleared=0,bosses_defeated=0;
 std::array<int,skill_count> skills{}; std::array<int,passive_count> passives{}; std::array<double,skill_count> skill_damage{}; std::array<int,enemy_kind_count> enemy_kills{};
 uint64_t consumed_skills=0; int fusions_created=0; float first_fusion_time=-1;
 std::array<int,synergy_count> synergy_triggers{};
 int dash_uses=0,items_collected=0,rerolls_used=0,death_protections_used=0,evolved_count=0; std::array<EquipmentId,equipment_slot_count> equipment{}; uint32_t owned_relic_mask=0; bool detailed_metrics=false; Difficulty difficulty=Difficulty::Normal; uint32_t daily_key=0,enemy_seen_mask=0; int daily_variant=0,endless_cycle=0;
};
RunReport make_run_report(const Game& game,RunOutcome outcome,uint32_t seed=0);
int settlement_crystals(const RunReport& report);
struct ProfileSettings { bool music=true,sound=true,ui_sound=true; float master_volume=1.f,sound_volume=.35f,music_volume=.35f,ui_volume=.5f,zoom=1.6f; bool shake=true,show_stats=false; float friendly_effect_alpha=.70f,shake_strength=1.f,damage_number_density=.70f; bool low_effects=false;int game_speed=1; float flash_strength=1.f,ui_scale=1.f;bool guidance=true;uint32_t guide_seen=0,boss_hints_seen=0; };
struct ActiveRun { uint64_t id=0; CharacterClass character=CharacterClass::Archer; uint32_t seed=0; int64_t started_at=0; RunMode mode=RunMode::Classic; int claimed_chapters=0,claimed_crystals=0; RunReport claimed_report; RunBonuses bonuses; RunSetup setup; RunReport accounted_report; int achievement_crystals=0; std::string daily_date; };
struct SettlementReceipt { uint64_t run_id=0; int crystals=0; RunReport report; int64_t settled_at=0; };
struct ProfileRecords { uint64_t runs=0,victories=0,total_kills=0,total_crystals=0; float longest_run=0; int best_level=1; double total_elapsed=0; std::array<uint64_t,3> class_runs{}; };
struct RunBest { bool recorded=false; float longest=0; int kills=0,level=1,bosses=0; double damage=0; uint64_t runs=0,victories=0,best_score=0; };
struct DailyRecord { std::string date; uint64_t attempts=0,wins=0; bool reward_claimed=false; RunBest best; };
struct DailySpec { bool valid=false; std::string date,rules,error; uint32_t seed=0; CharacterClass character=CharacterClass::Archer; RunSetup setup; RunBonuses bonuses; };
struct BestiaryEntry { EnemyKind kind=EnemyKind::Slime; bool seen=false; uint64_t kills=0; };
constexpr int run_best_count=3*3*4;
DailySpec make_daily_spec(const std::string& date);
struct Profile {
 int schema_version=profile_schema_version; uint64_t revision=0,next_run_serial=1,closed_through_serial=0;
 int64_t crystals=0; std::array<int,talent_count> talent_ranks{};
 std::optional<ActiveRun> active_run;
 std::vector<SettlementReceipt> history;
 ProfileSettings settings; ProfileRecords records; ContentStatistics lifetime_content;
 std::array<bool,equipment_count> owned_equipment{}; std::array<EquipmentId,equipment_slot_count> equipped{}; uint32_t unlocked_relic_mask=0; bool challenge_eligible=false;
 std::array<bool,achievement_count> achievement_completed{}; AchievementStats achievement_stats;
 bool nightmare_eligible=false; uint32_t enemy_seen_mask=0; std::array<RunBest,run_best_count> bests{}; std::vector<DailyRecord> daily_records;
 // Reserved structured extension points are round-tripped, not used as locks on Combat content.
 nlohmann::json unlocks=nlohmann::json::object();
 nlohmann::json achievements=nlohmann::json::array();
 nlohmann::json chapter_progress=nlohmann::json::object();
 // Only an explicit chapter-boundary snapshot is accepted, never a mid-battle save.
 nlohmann::json chapter_checkpoint=nullptr;
};
struct ProgressionResult {
 bool ok=false; bool already_claimed=false; uint64_t run_id=0; int crystals_awarded=0; int progress_crystals_awarded=0,achievement_crystals_awarded=0,daily_crystals_awarded=0; std::vector<AchievementId> new_achievements; std::string message;
 explicit operator bool()const{return ok;}
};
enum class ProfileLoadState { NewProfile, Loaded, Migrated, RecoveredBackup, FutureVersion, Corrupt, Unavailable };
std::filesystem::path default_profile_directory();
class ProfileWriteLease;
class ProgressionStore {
public:
 // An explicit directory isolates tests/diagnostics. Default resolves LocalAppData with Unicode APIs.
 explicit ProgressionStore(std::filesystem::path directory={});
 ~ProgressionStore();
 ProgressionStore(const ProgressionStore&)=delete;
 ProgressionStore& operator=(const ProgressionStore&)=delete;
 ProgressionResult load();
 const Profile& profile()const{return profile_;}
 const std::filesystem::path& directory()const{return directory_;}
 std::filesystem::path profile_path()const{return directory_/L"profile.json";}
 bool read_only()const{return read_only_;}
 ProfileLoadState load_state()const{return load_state_;}
 const std::string& last_error()const{return last_error_;}
 int talent_rank(TalentId talent)const;
 int talent_cost(TalentId talent)const; // -1 at cap or for an invalid ID.
 RunBonuses run_bonuses()const;
 bool owns_equipment(EquipmentId id)const;
 bool relic_unlocked(RelicId id)const;
 EquipmentId equipped(EquipmentSlot slot)const;
 ProgressionResult equip(EquipmentSlot slot,EquipmentId id,CharacterClass for_class,EquipmentId expected_current);
 RunSetup run_setup(CharacterClass character,RunMode mode=RunMode::Classic,Difficulty difficulty=Difficulty::Normal)const;
 bool mode_unlocked(RunMode mode)const;
 std::string mode_unlock_requirement(RunMode mode)const;
 bool difficulty_unlocked(Difficulty difficulty)const;
 std::string difficulty_unlock_requirement(Difficulty difficulty)const;
 RunBest run_best(CharacterClass character,Difficulty difficulty,RunMode mode)const;
 std::optional<DailyRecord> daily_record(const std::string& date)const;
 std::array<BestiaryEntry,enemy_kind_count> bestiary()const;
 std::array<AchievementStatus,achievement_count> achievement_status()const;
 bool challenge_unlocked()const{return profile_.challenge_eligible;}
 // Persists the serial BEFORE combat. Any abandoned previous active run closes with zero award.
 ProgressionResult begin_run(CharacterClass character,uint32_t seed,RunMode mode=RunMode::Classic,Difficulty difficulty=Difficulty::Normal);
 ProgressionResult begin_daily(const std::string& date);
 ProgressionResult claim_chapter(uint64_t run_id,int chapter_index,const RunReport& cumulative_report,const RunBuild& build);
 ProgressionResult choose_chapter_supply(uint64_t run_id,int choice);
 ProgressionResult update_checkpoint_timing(uint64_t run_id,const RunTiming& timing);
 bool checkpoint_build(RunBuild& build)const;
 // Atomic currency + receipt + records write. Retrying any closed serial never credits it again.
 ProgressionResult settle(uint64_t run_id,const RunReport& report);
 ProgressionResult purchase(TalentId talent,int expected_rank);
 ProgressionResult save_settings(const ProfileSettings& settings);
 ProgressionResult save_chapter_checkpoint(uint64_t run_id,const nlohmann::json& checkpoint);
private:
 std::filesystem::path directory_,oversized_baseline_; std::unique_ptr<ProfileWriteLease> writer_lease_; Profile profile_; nlohmann::json document_=nlohmann::json::object();
 std::string baseline_bytes_,last_error_; bool baseline_exists_=false,read_only_=false,recovered_=false;
 ProfileLoadState load_state_=ProfileLoadState::NewProfile;
 ProgressionResult commit(Profile candidate);
 ProgressionResult begin_snapshot(CharacterClass character,uint32_t seed,RunSetup setup,RunBonuses bonuses,const std::string& daily_date);
};
}

#pragma once
#include "encounter_director.hpp"
#include "run_state.hpp"
#include "run_equipment.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace maple {
struct Vec2 { float x=0, y=0; Vec2 operator+(Vec2 v) const {return {x+v.x,y+v.y};} Vec2 operator-(Vec2 v) const {return {x-v.x,y-v.y};} Vec2 operator*(float s) const {return {x*s,y*s};} Vec2& operator+=(Vec2 v){x+=v.x;y+=v.y;return *this;} };
float length(Vec2 v); Vec2 normalized(Vec2 v);
enum class State { Menu, Playing, Paused, Upgrade, Victory, Defeat, ChapterComplete, ScrollChoice };
enum class RunMode { Classic, Campaign, Endless, Daily };
enum class Difficulty { Normal, Hard, Nightmare };

enum class CharacterClass { Archer, Mage, Warrior };
enum class RelicId { LifeSeed, DashWard, ThunderToken, SplitArrow, BarbedQuiver, WindFeather, FrostBell, EmberCore, EchoRune, ThornBadge, WarCry, IronWill, Count };
constexpr int relic_count=static_cast<int>(RelicId::Count);
enum class EquipmentSlot { Weapon, Armor, Accessory };
constexpr int equipment_slot_count=3;
enum class EquipmentId { None, TravelerKnife, LeatherVest, AmberPendant, ReedBow, ApprenticeWand, IronBlade, ScoutCloak, ScholarRobe, MagnetPendant, Count };
constexpr int equipment_count=static_cast<int>(EquipmentId::Count);
struct RelicDefinition { RelicId id;const char* key;const char* name;const char* description;int allowed_class=-1,rarity=1;bool initially_unlocked=false; };
struct EquipmentDefinition { EquipmentId id;const char* key;const char* name;const char* description;EquipmentSlot slot;int allowed_class=-1,rarity=1;bool initially_owned=false; };
const std::array<RelicDefinition,relic_count>& relic_definitions();
const std::array<EquipmentDefinition,equipment_count>& equipment_definitions();
uint32_t initial_relic_mask();
bool relic_allowed(RelicId id,CharacterClass character);
bool equipment_allowed(EquipmentId id,EquipmentSlot slot,CharacterClass character);
struct RunSetup { RunMode mode=RunMode::Classic;bool enable_loot=false;uint32_t unlocked_relic_mask=0;std::array<EquipmentId,equipment_slot_count> equipment{};Difficulty difficulty=Difficulty::Normal;uint32_t daily_key=0;int daily_variant=0;CharacterClass daily_character=CharacterClass::Archer;uint32_t starting_relic_mask=0; };

enum class EnemyKind { Slime, Mushroom, BlueMushroom, Pig, Elite, Boss, Ranger, Splitter, Support, Minion, GroundController=10, WindupCharger=11 };
constexpr int enemy_kind_count=12;
constexpr int xp_bucket_count=60;
// Saved numeric IDs are append-only; JSON uses skill_key rather than display order.
enum class Skill { Homing=0, Arrow=1, Nova=2, Pierce=3, Lightning=4, Zone=5, Summon=6, Cleave=7, Orbit=8, Spread=9, Rain=10, Ricochet=11, Frost=12, Meteor=13, Rune=14, Charge=15, BladeWave=16, Retaliate=17, StarReturn=18, FrostStorm=19, DragonArmor=20, HunterMark=21, ArrowTrap=22, OrbitalOrb=23, GravityWell=24, HeavyStrike=25, Bulwark=26, PhoenixRain=27, HunterVolley=28, MoltenStar=29, ArcaneRing=30, SwordCyclone=31, DashBreaker=32, FlameDance=33, FeatherStorm=34, StarBow=35, BattleFocus=36, SpearCombo=37, HolyShield=38, Judgement=39, DragonThrust=40, EarthShatter=41, PoisonMist=42, IceLance=43, HolyLight=44, HolyDragon=45, FireBeast=46, MagicVortex=47, StormShot=48, Sniper=49, IceArrow=50, BlastArrow=51, SilverEagle=52, Decoy=53, BattleBlades=54, DragonFormation=55, EchoShield=56, MountainBreaker=57, SpiritJudgement=58, AzureRift=59, Sanctuary=60, PoisonFlare=61, ThunderLance=62, HeavenDragon=63, InfernoArray=64, VenomVortex=65, FrostFire=66, PurityWell=67, ThousandArrows=68, EagleExecution=69, FrozenBarrage=70, SiegeBolt=71, TrickDecoy=72, TwinHunt=73, PrismShot=74, ThunderPrison=75, FrostArmor=76, MiasmaDragon=77, DivineVerdict=78, FrozenDragonSpear=79, PhoenixAssault=80, MirrorShield=81, DragonBreaker=82, FissureArrows=83, SpearBolt=84, StormArrowStream=85, PoisonHuntingGround=86, DragonPhoenix=87, GlacialSniper=88, FlowingFire=89,
#define X(id,s,key,name,desc,interval,roles,style,icon) s=id,
#include "adventure_catalogue.inc"
#undef X
 Count=179 };
constexpr int skill_count=static_cast<int>(Skill::Count);
constexpr int synergy_count=6;
enum class Resonance {FrostRelay,PoisonIgnition,ArmorBreak,HuntAssist,DashFlame,ShieldEcho,ColdHeat,BattleFocus,FrostHunt,ArcaneVolley,EarthResonance,HolyShelter,ShadowHunt,ToxicCoin,BladeFistBeat,ThunderEnergy,TurretCalibration,MachineLink,DragonBeat,RosePursuit,Count};
constexpr int resonance_count=int(Resonance::Count);
enum SkillRole : uint32_t {Cold=1,Thunder=2,Poison=4,Fire=8,Armor=16,Heavy=32,Mark=64,Companion=128,DashAttack=256,Shield=512,Counter=1024,MultiShot=2048,Focus=4096,Landing=8192,ArcaneField=16384,Bolt=32768,GroundRift=65536,Holy=131072,Rainfall=262144,Shadow=1u<<19,Blade=1u<<20,CombatCoin=1u<<21,Gun=1u<<22,Energy=1u<<23,Fist=1u<<24,Dragon=1u<<25,Elemental=1u<<26,Machine=1u<<27,Node=1u<<28,Turret=1u<<29,Card=1u<<30,Rose=1u<<31};
struct ResonanceDefinition {Resonance id;const char* key;const char* name;uint32_t first_role,second_role;Skill first_example,second_example;float cooldown;const char* description;};
const std::array<ResonanceDefinition,resonance_count>& resonance_definitions();
struct ResonanceUpgrade {Resonance id;int direction;const char* name;const char* description;};
const std::array<ResonanceUpgrade,12>& resonance_upgrades();
struct ResonanceStatus {Skill first=Skill::Count,second=Skill::Count;bool equipped=false,usable=false;float cooldown=0;std::string hint;};
struct ResonanceStatistics {std::array<int,resonance_count> triggers{},suppressed{};std::array<double,resonance_count> damage{},shield{},support{};};
struct FusionRecipe { Skill first,second,result; CharacterClass character; int material_level=5,other_class=-1; uint32_t theme_tags=0;int weight=100;uint32_t form_mask=1;bool released=true; Skill third=Skill::Count,fourth=Skill::Count;int priority=100;std::string id;std::vector<Skill> sources; };
const std::vector<FusionRecipe>& fusion_recipes();
struct FusionScroll {uint32_t id=0;Skill result=Skill::Count;int form=0;bool study_used=false,tracked=false;};
struct ScrollOffer {Skill result=Skill::Count;int form=0;std::string recipe_id;int investment=0;};
struct ScrollState {
 std::vector<FusionScroll> held;std::vector<ScrollOffer> offers,completed;int foreign_class=-1;
 uint64_t authorized_foreign=0,learned_foreign=0;std::vector<Skill> studied_results;uint32_t next_id=1;
 int failed_rewards=0,eligible_rewards=0,opportunities=0,studies_used=0,replacements=0,pending_opportunities=0;
 float first_opportunity_time=-1;bool challenge_offered=false;
};
struct StudyReplacement {bool active=false;size_t choice_index=0;uint32_t scroll_id=0;Skill learn=Skill::Count,replace=Skill::Count;};
enum class UpgradeKind { Skill, Damage, AttackSpeed, Area, Projectiles, MoveSpeed, Health, Magnet, Cooldown, Duration, CritChance, CritDamage, Recover, Overcharge, Vitality, Relic, Fusion=16, Study=17, Resonance=18, Count };
constexpr int passive_count=static_cast<int>(UpgradeKind::Recover);
enum class EffectKind { Hit, Nova, Lightning, Zone, Summon, Dash, Death, Warning, Cleave, Orbit, Rain, Frost, Meteor, Rune, Charge, Retaliate, EnemyShot, FrostStorm, HunterMark, ArrowTrap, OrbitalOrb, GravityWell, HeavyStrike, Bulwark, PhoenixRain, HunterVolley, MoltenStar, ArcaneRing, SwordCyclone, FlameDance, FeatherStorm, StarBow, BattleFocus, SpearCombo, HolyShield, Judgement, DragonThrust, EarthShatter, PoisonMist, IceLance, HolyLight, HolyDragon, FireBeast, MagicVortex, StormShot, Sniper, IceArrow, BlastArrow, SilverEagle, Decoy, BattleBlades, DragonFormation, EchoShield, MountainBreaker, SpiritJudgement, AzureRift, Sanctuary, PoisonFlare, ThunderLance, HeavenDragon, InfernoArray, VenomVortex, FrostFire, PurityWell, ThousandArrows, EagleExecution, FrozenBarrage, SiegeBolt, TrickDecoy, TwinHunt, PrismShot, ThunderPrison, FrostArmor, MiasmaDragon, DivineVerdict, FrozenDragonSpear, PhoenixAssault, MirrorShield, DragonBreaker, FissureArrows, SpearBolt, StormArrowStream, PoisonHuntingGround, DragonPhoenix, GlacialSniper, FlowingFire, Resonance, Adventure };
enum class EventKind { Shot, Hit, Hurt, Kill, LevelUp, Dash, Boss, Victory, Defeat, ItemPickup, SpecialEvent, EventComplete, Choice, Scroll, Fusion, BossAttack };
enum class ItemKind { Health, Magnet, Fury, Shield, Bomb, DoubleXP, Power, Reset, Chest, Count };
struct ItemDrop {ItemKind kind=ItemKind::Health;Vec2 pos;float age=0;};
struct GearDrop {std::string id;Vec2 pos;int level=1;uint64_t count=1;};
enum class SpecialEventKind { None, GuardedChest, BountyElite, Blessing, Merchant, MushroomRush, EliteHunt, Fountain, Rift };
struct SpecialEvent {SpecialEventKind kind=SpecialEventKind::None;Vec2 pos;float remaining=0;std::vector<uint32_t> guardians;float progress=0,tick=0;int target=0,spawned=0;bool activated=false,scroll_challenge=false;};
enum class PressureKind { None, DarkTide, Invasion, LastWords, Disarm, Frenzy, Exhaustion, ThinAir, Distraction, Hunt, FireRain, DeathGrudge, BloodCurse, Undying, Altar, Encirclement, Shackles, StandPunishment, SoulLink, ShadowTrial, MapleDisorder, Count };
constexpr int pressure_kind_count=int(PressureKind::Count);
struct PressureDefinition {PressureKind kind;const char* key;const char* name;const char* hint;float duration,weight;bool boss_allowed;};
const std::array<PressureDefinition,20>& pressure_definitions();
enum class EnemyAbility { Promise, DeathBlast, Smoke, Refraction, Distortion, NaturalOrder, Regression, Count };
constexpr int enemy_ability_count=int(EnemyAbility::Count);
const char* enemy_ability_name(EnemyAbility ability);
enum class DangerKind { Explosion, Smoke, Distortion, NaturalOrder, Regression, Altar, Revival };
const char* danger_name(DangerKind kind);
struct PressureEvent {PressureKind kind=PressureKind::None;uint32_t id=0;float started=0,remaining=0,tick=0,stationary=0,internal_cooldown=0;int waves=0;bool scroll_reward_assigned=false;Skill sealed=Skill::Count;Vec2 anchor;float escape_angle=0;};
struct DangerZone {uint32_t id=0,source_id=0,owner_id=0;DangerKind kind=DangerKind::Explosion;Vec2 pos;float radius=80,age=0,warning=1.2f,duration=8,damage=0,hp=0,max_hp=0;bool following=false,triggered=false,from_boss=false;uint64_t root_attack_id=0;};
struct SoulLink {uint32_t first=0,second=0,source_id=0;float max_distance=320;bool broken=false;};
struct EnemyAbilityState {uint32_t mask=0;int refraction=0,shadow_class=-1;bool elite=false,native_stability=false,promise_used=false,pending_death=false,blast_used=false,revived=false,reward_claimed=false,scroll_reward=true;float promise_remaining=0,field_cooldown=6,dark_scale=1,feedback=0;int feedback_kind=0;};
struct PressureRecord {PressureKind kind=PressureKind::None;float started=0,ended=0;double damage_taken=0;};
struct PressureDiagnostics {std::vector<PressureRecord> sequence;uint64_t normal_spawn_budget=0,event_spawn_budget=0,requested_spawns=0,actual_spawns=0,capacity_limited=0;std::array<uint64_t,enemy_ability_count> abilities{};std::array<uint64_t,skill_count> attempts{},failures{},protected_casts{},successful_casts{};size_t max_enemies=0,max_projectiles=0,max_zones=0;uint64_t displacement_blocked=0,promise_triggers=0,refractions=0,death_blasts=0,revivals=0;std::string death_cause;};
struct Input { Vec2 move; bool dash=false,interact=false; };
enum class BossProfile { MushroomKing=0, ForestGuardian=1, KingSlime=2, SnailKing=3, Golem=4, Balrog=5, Snowman=6, Pirate=7, Clockwork=8, Leviathan=9, Cactus=10, Faust=11, GiantCrab=12, DollRobot=13, Zeno=14, TaeRoon=15, NineTail=16, Eliza=17, Chimera=18, Dodo=19, Lilynouch=20, Lyka=21, Manon=22, Griffey=23 };
constexpr int boss_profile_count=24,enemy_visual_count=60;
enum class SceneObjectKind { Crate, Barrel, Mud, HealingWell };
enum class SceneObjectState { Active, Armed, Broken, Spent };
struct SceneObject { uint32_t id=0;SceneObjectKind kind=SceneObjectKind::Crate;SceneObjectState state=SceneObjectState::Active;Vec2 pos;float radius=25,hp=40,max_hp=40,age=0,hit_flash=0,countdown=0,lifetime=90;Skill source=Skill::Count; };
struct CombatDiagnostics { uint64_t enemy_reroutes=0,spawn_relocations=0,boss_nodes_destroyed=0,boss_nodes_expired=0,boss_attacks_cancelled=0; uint64_t scene_objects_created=0,objects_destroyed=0,barrels_exploded=0,healing_uses=0,collision_resolutions=0,escape_resolutions=0,boss_phase_changes=0,boss_summons=0,actual_hits=0,critical_hits=0;double object_damage=0; };
struct Player { Vec2 pos; float hp=100,max_hp=100,speed=160,xp=0,xp_next=8,dash_cooldown=0,invulnerable=0,animation_time=0; int level=1; bool facing_left=false,moving=false; };
struct Enemy { uint32_t id=0; EnemyKind kind=EnemyKind::Slime; Vec2 pos,velocity; float hp=1,max_hp=1,speed=20,radius=14,damage=8,age=0,hit_flash=0,attack_clock=0; bool facing_left=false;float slow_remaining=0,slow_factor=1,burn_remaining=0,burn_tick=0,burn_damage=0,windup=0,action_remaining=0,shield_remaining=0,buff_remaining=0;Vec2 attack_target,attack_direction;int phase=0;Skill burn_source=Skill::Count;BossProfile boss_profile=BossProfile::MushroomKing;int boss_stage=1,boss_attack=0;float attack_cooldown=0,recovery_remaining=0;int visual=0;float attack_animation_time=0,action_duration=0,action_tick=0;int action_step=0;Vec2 attack_start,attack_end;float mark_remaining=0,frost_remaining=0,armor_break_remaining=0;Skill mark_source=Skill::Count,frost_source=Skill::Count,armor_source=Skill::Count;int frost_layers=0;float poison_remaining=0,poison_tick=0,poison_damage=0;int poison_layers=0;bool poison_spread=false;Skill poison_source=Skill::Count;float lure_remaining=0;Vec2 lure_point;Skill lure_source=Skill::Count;int thermal_side=0;float thermal_lock=0;Skill thermal_source=Skill::Count; EnemyAbilityState abilities; bool final_boss=true; uint32_t boss_owner=0;int boss_node=0;float node_lifetime=0,boss_exposure=0;uint64_t boss_attack_serial=0;bool boss_contact_hit=false;float navigation_stall=0,navigation_detour=0;Vec2 navigation_direction; };
struct Projectile { Vec2 pos,velocity; Skill skill=Skill::Homing; float damage=10,radius=7,age=0,lifetime=3; int pierce=1; uint32_t target=0,altar_target_id=0; std::vector<uint32_t> hit_ids; bool evolved=false,returned=false;int bounces=0;bool proc_generated=false;int blocks=0;int level_snapshot=0;float power_snapshot=0,gear_snapshot=1;uint64_t root_attack_id=0;std::array<Vec2,4> visual_trail{};int visual_trail_count=0;float visual_trail_clock=0; };
struct Gem { Vec2 pos; float value=1,age=0; };
enum class ImpactResult { Damage, Shield, Immune, Wall, Expired };
struct HostileImpact { Vec2 pos; EnemyKind enemy_kind=EnemyKind::Ranger; BossProfile boss_profile=BossProfile::MushroomKing; int enemy_visual=0; ImpactResult result=ImpactResult::Expired;float age=0,radius=10; };
struct Effect { EffectKind kind=EffectKind::Hit; Vec2 pos,end; float radius=20,age=0,duration=.3f,damage=0,tick=0; bool evolved=false; EnemyKind enemy_kind=EnemyKind::Slime; float angle=0; Skill source=Skill::Count;bool hostile=false;float delay=0;Vec2 velocity;bool triggered=false;int pattern=0;bool environment=false,proc_generated=false;BossProfile boss_profile=BossProfile::MushroomKing;int enemy_visual=0;int count=0;uint32_t target_id=0,altar_target_id=0;float secondary_tick=0,energy=0;int effective_level_snapshot=0;uint64_t root_attack_id=0;int resonance_id=-1;float gear_snapshot=1; float visual_attack_age=100;Vec2 visual_aim{-1,0},visual_origin;std::array<Vec2,4> visual_trail{};int visual_trail_count=0;float visual_trail_clock=0;  };
enum class CombatRecordKind { Damage, Shield, Status, CastFailed, HealthCost, Phase };
struct CombatRecord {CombatRecordKind kind=CombatRecordKind::Damage;float time=0,amount=0,hp=0;Vec2 position;uint32_t states=0;int boss_attack=-1;Skill skill=Skill::Count;std::string source;};
struct DamageNumber { Vec2 pos; int value=0; float age=0; bool critical=false; uint32_t target_id=0; };
enum class FxStage { Cast, Disk, Line, Cone, Impact, End, Fusion };
enum class FxStyle { Steel, Shadow, Poison, Gun, Machine, Fist, Dragon, Frost, Thunder, Fire, Holy, Arcane, Card, Rose };
struct SkillVisual {Skill source=Skill::Count;FxStage stage=FxStage::Cast;Vec2 pos,end;float radius=0,half_angle=0,age=0,duration=.3f;uint32_t elements=0;int tier=1;bool heavy=false;uint64_t serial=0;};
FxStyle skill_visual_style(Skill skill,uint32_t elements=0);
struct GameEvent { EventKind kind; Vec2 pos;float actual_damage=0;bool critical=false;Skill source=Skill::Count;uint32_t target_id=0;EnemyKind target_kind=EnemyKind::Slime;bool secondary=false;uint64_t root_attack_id=0;int resonance_id=-1; };
struct UpgradeChoice { UpgradeKind kind=UpgradeKind::Damage; Skill skill=Skill::Homing; std::string title,description;RelicId relic=RelicId::Count;uint32_t scroll_id=0;int expected_rank=-1;bool selected=false;Resonance resonance=Resonance::Count;int direction=0; };
struct SkillStatus {Skill skill=Skill::Count;int level=0,effective=0,cap=0;bool sealed=false,smoked=false;float cooldown=0;bool usable()const{return level>0&&effective>0&&!sealed&&!smoked;}};
struct FusionStatus {const FusionRecipe* recipe=nullptr;int first=0,second=0;bool known=false,tracked=false,equipped=false,ready=false;std::string hint;};
// A copied, clamped snapshot of camp talents. Bonuses are never accumulated by refresh_stats.
struct RunBonuses {
 float damage_bonus=0,crit_bonus=0,health_bonus=0,move_speed_bonus=0,xp_bonus=0,cooldown_reduction=0;
 int upgrade_rerolls=0,death_protections=0,first_elite_reward=0;
};
struct RunMetrics {
 ResonanceStatistics resonance;
 ContentStatistics content;
 int fusions_created=0;float first_fusion_time=-1;std::array<int,synergy_count> synergy_triggers{};
 CharacterClass character=CharacterClass::Archer;
 std::array<int,enemy_kind_count> enemy_kills_by_kind{};
 int dash_uses=0,upgrade_rerolls_used=0,death_protections_used=0,first_elite_rewards=0;
 int upgrade_choices=0,bonus_choices=0;
 std::array<double,skill_count> skill_damage{};
 double other_damage=0,damage_taken=0,altar_health_spent=0,xp_acquired=0,boss_damage_taken=0;
 std::array<double,xp_bucket_count> xp_by_minute{};
 int events_started=0,events_succeeded=0,events_failed=0,items_collected=0,boss_attacks=0;
 uint32_t enemy_seen_mask=0;int endless_cycle=0;
 int relics_collected=0;std::array<int,relic_count> relic_procs{};std::array<int,equipment_count> equipment_procs{};
 int chapters_cleared=0,bosses_defeated=0,hazards_spawned=0,environment_hits=0;
 double environment_damage_taken=0;
 float boss_spawn_time=-1,boss_defeat_time=-1;
};
// Only a chapter boundary is exportable. No live enemies, projectiles or arbitrary combat saves.
struct RunBuild {
 std::vector<GearStack> run_gear;
 std::array<int,resonance_count> resonance_directions{};
 ScrollState scrolls;EncounterPlan encounter_plan;RunTiming timing;std::string rng_state;
 uint64_t consumed_skills=0;float dragon_soul=0,guard_charge=0,battle_focus=0;
 int version=1,chapter_index=0,intermission_choice=-1;
 uint32_t seed=1; CharacterClass character=CharacterClass::Archer;
 float completed_elapsed=0,hp=100,xp=0;
 int level=1,kills=0,coins=0,overcharge_levels=0,vitality_levels=0;
 std::array<int,skill_count> skills{}; std::array<int,passive_count> passives{};
 RunBonuses bonuses;
 int upgrade_rerolls_remaining=0,death_protections_remaining=0;
 bool first_elite_reward_claimed=false,boss_defeated=false;
 float next_chapter_shield_seconds=0;
 bool enable_loot=false;uint32_t unlocked_relic_mask=0,owned_relic_mask=0;std::array<EquipmentId,equipment_slot_count> equipment{};
 Difficulty difficulty=Difficulty::Normal;uint32_t daily_key=0;int daily_variant=0;CharacterClass daily_character=CharacterClass::Archer;uint32_t starting_relic_mask=0;
 RunMetrics metrics;
};
struct Config {
 bool enable_pressure=true;
 float pressure_first_min=45,pressure_first_max=60,pressure_gap_min=15,pressure_gap_max=25;
 float exhaustion_factor=.65f,thin_air_factor=1.6f,distraction_chance=.25f,shackles_factor=2,maple_disorder_factor=.4f;
 size_t max_danger_zones=24,max_major_zones=2;
 uint32_t available_boss_mask=0xffffffffu;uint64_t available_enemy_mask=~uint64_t(0);
 float duration=1200, boss_time=1110, elite_interval=80,kill_xp_multiplier=1.2f, spawn_radius=520, despawn_radius=1600;
 size_t max_enemies=500,max_projectiles=512,max_gems=600,max_effects=300,max_damage_numbers=100;
 float cleave_half_angle=.8f,blessing_health_cost=15;
 float first_event_time=35,event_interval_min=55,event_interval_max=80,event_duration=45,item_drop_chance=.045f,item_lifetime=45,fury_duration=12,magnet_duration=8;
 size_t max_items=32;
 float dash_duration=.15f,dash_cooldown=1.2f,dash_speed=650,hit_invulnerability=.5f;
 int enabled_events=8;
 bool enable_director=false,enable_scene_objects=false,enable_boss_phases=false;
 float director_boss_health_scale=.30f; // Calibrated for the director's lower kill/XP density.
 size_t max_scene_objects=48;
 DirectorConfig director_config;
};
class Game {
public:
 std::vector<GearStack> run_gear;std::vector<GearDrop> gear_drops;
 uint64_t gear_count() const;void gain_gear(const std::string& id,int level=1,uint64_t count=1);
 static int skill_level_cap(Skill skill);
 static uint32_t skill_roles(Skill skill);
 const char* resonance_direction_name(Resonance id) const;float resonance_shield() const;
 std::array<int,resonance_count> resonance_directions{};
 ResonanceStatus resonance_status(Resonance id) const;
 std::string resonance_change_preview(Skill removed,Skill learned,Skill second_removed=Skill::Count) const;
 float resonance_flash(Resonance id) const {return resonance_flash_[int(id)];}
 SkillStatus skill_status(Skill skill) const;
 FusionStatus fusion_status(Skill result) const;
 const std::array<Skill,8>& skill_slots() const;
 Skill main_fusion_route(Skill skill) const;
 explicit Game(Config config={});
 void start(CharacterClass cls=CharacterClass::Archer,uint32_t seed=1,RunBonuses bonuses={},RunSetup setup={});
 void update(float dt,Input input={});
 bool choose_upgrade(size_t index);
 bool reroll_upgrade();
 bool skip_upgrade();
 std::vector<UpgradeChoice> available_upgrade_choices() const;
 bool upgrade_choice_available(const UpgradeChoice& choice) const;
 int upgrade_picks_taken=0,upgrade_resets_remaining=0;
 int upgrade_picks_required() const {return relic_selection_||bonus_selection_?1:2;}
 int pending_upgrade_levels() const {return pending_levels_;}
 ScrollState scrolls;StudyReplacement pending_study;EncounterPlan encounter_plan;RunTiming timing;
 void prepare_encounter_plan();
 int tracked_scroll_count() const;bool set_scroll_tracking(uint32_t id,bool tracked);
 bool choose_scroll(size_t index);bool decline_scroll_offer();bool discard_scroll(uint32_t id);bool confirm_scroll_fusion(uint32_t id);
 bool scroll_reachable(Skill result) const;std::string scroll_status(uint32_t id) const;std::string scroll_conflicts(Skill result) const;
 const FusionScroll* held_scroll(Skill result) const;
 int fusion_count() const;int cross_fusion_count() const;
 static CharacterClass skill_owner(Skill skill);static bool is_cross_fusion(Skill skill);static uint64_t material_skill_mask();
 static int form_count(Skill result);int fusion_form(Skill result) const;static const char* scroll_form_key(Skill result,int form);
 static const char* scroll_form_name(Skill result,int form);static const char* scroll_form_description(Skill result,int form);
 bool select_study_replacement(Skill skill);bool confirm_study_replacement();void cancel_study_replacement();
 const RunBonuses& run_bonuses() const {return run_bonuses_;}
 RunMode run_mode() const {return run_mode_;}
 RunSetup run_setup() const;
 Difficulty difficulty() const {return difficulty_;}
 int endless_cycle() const {return metrics.endless_cycle;}
 int region_index() const {return run_mode_==RunMode::Endless?metrics.endless_cycle%3:run_mode_==RunMode::Daily?daily_variant_:run_mode_==RunMode::Campaign?chapter_index_:0;}
 static const char* mode_name(RunMode mode);
 static const char* difficulty_name(Difficulty difficulty);
 static const char* enemy_name(EnemyKind kind);
 static const char* enemy_description(EnemyKind kind);
 std::string rules_description() const;
 bool owns_relic(RelicId id) const {int i=int(id);return i>=0&&i<relic_count&&(owned_relic_mask_&(1u<<i))!=0;}
 uint32_t owned_relic_mask() const {return owned_relic_mask_;}
 float relic_haste_remaining() const {return relic_haste_remaining_;}
 bool choosing_relic() const {return relic_selection_;}
 int chapter_index() const {return chapter_index_;}
 float total_elapsed() const {return float(completed_elapsed_+elapsed);}
 int encounter_pace() const {return timing.speed==2?2:1;}
 float remaining_combat_seconds() const {return float(std::max(0.0,config.duration-elapsed)/encounter_pace());}
 int intermission_choice() const {return intermission_choice_;}
 static const char* chapter_name(int index);
 static const char* chapter_objective(int index);
 std::string chapter_reward_description(int index) const;
 bool export_build(RunBuild& build) const;
 bool restore_build(const RunBuild& build);
 bool apply_chapter_reward(int index);
 bool advance_chapter();
 void toggle_pause(); void reset_to_menu();
 bool evolved(Skill skill) const;
 static const char* skill_name(Skill skill);
 static const char* skill_key(Skill skill);
 static const char* synergy_name(int index);static const char* synergy_description(int index);
 static bool is_fusion(Skill skill);
 bool fusion_ready(Skill result) const;void auto_fuse_skills();
 std::string fusion_preview(Skill learned,Skill removed=Skill::Count) const;static int fusion_tier(Skill skill);
 int fusion_result_level(Skill result) const;
 std::string fusion_requirement(Skill result) const;
 float skill_power(Skill skill,int level=0) const;
 static constexpr int active_skill_limit=8;
 int active_skill_count() const;
 bool can_gain_skill(Skill skill) const;
 float skill_cooldown_remaining(Skill skill) const;
 float skill_cooldown_total(Skill skill) const;
 float skill_interval(Skill skill) const;
 float skill_active_remaining(Skill skill) const;
 std::string skill_parameters(Skill skill,int level=0) const;
 std::string skill_upgrade_description(Skill skill) const;
 static const char* evolution_name(Skill skill);
 static const char* evolution_requirement(Skill skill);
 static const char* skill_description(Skill skill);
 bool skill_allowed(Skill skill)const;
 static const char* passive_name(UpgradeKind kind);
 static const char* special_event_name(SpecialEventKind kind);
 static const char* item_name(ItemKind kind);
 static const char* scene_object_key(SceneObjectKind kind);
 static const char* scene_animation_key(const SceneObject& object);
 static const char* boss_profile_name(BossProfile profile);
 static const char* boss_sprite_key(BossProfile profile);
 static float boss_render_scale(BossProfile profile);
 static const char* boss_attack_name(int attack);
 static const char* enemy_visual_key(int visual);
 static const char* enemy_visual_name(int visual);
 static float enemy_visual_scale(int visual);
 BossProfile next_boss_profile() const;
 BossProfile planned_boss_profile(int encounter) const;
 int scheduled_boss_count() const;
 int bosses_spawned() const {return boss_encounters_spawned_;}
 float next_boss_time() const;
 void set_combat_view(float half_width,float half_height);
 static const char* boss_stage_name(int stage);
 std::string event_objective()const;
 std::string special_event_objective()const{return event_objective();}
 std::string passive_value(UpgradeKind kind)const;
 Config config; State state=State::Menu; CharacterClass character=CharacterClass::Archer; Player player;
 double elapsed=0;float combat_elapsed=0; // Encounter progress and local combat animation time.
 int kills=0; bool boss_spawned=false,boss_defeated=false;
 std::array<int,skill_count> skills{}; std::array<int,passive_count> passives{};
 uint64_t consumed_skills=0;float dragon_soul=0,guard_charge=0,battle_focus=0;
 std::vector<HostileImpact> hostile_impacts;
 std::vector<CombatRecord> combat_records;
 void record_combat(CombatRecordKind kind,float amount,const std::string& source,int boss_attack=-1,Skill skill=Skill::Count);
 bool boss_pressure_busy() const;
 std::string gear_description(const GearStack&) const;
 std::string upgrade_pool_diagnostics() const;
 std::vector<SkillVisual> skill_visuals;uint64_t visual_serial=0;size_t visual_peak=0,visual_merged=0,visual_dropped=0;
 void emit_skill_visual(FxStage,Skill,Vec2,Vec2={},float radius=0,float half_angle=0,bool heavy=false,uint32_t elements=0);
 void update_skill_visuals(float dt);
 int skill_visual_charge(Skill skill) const;
 std::vector<Enemy> enemies; std::vector<Projectile> projectiles; std::vector<Gem> gems; std::vector<Effect> effects; std::vector<DamageNumber> damage_numbers; std::vector<UpgradeChoice> choices; std::vector<GameEvent> events;
 std::vector<ItemDrop> items;SpecialEvent special_event;float next_event_in=35,fury_remaining=0,magnet_remaining=0,notice_remaining=0;std::string notice_text;
 float damage_multiplier=1,attack_speed=1,area_multiplier=1,pickup_radius=65; int extra_projectiles=0;
 float cooldown_multiplier=1,duration_multiplier=1,crit_chance=.1f,crit_multiplier=1.7f;
 float shield_remaining=0,double_xp_remaining=0,power_remaining=0,rift_remaining=0,curse_remaining=0;
 int coins=0,overcharge_levels=0,vitality_levels=0;
 RunMetrics metrics;std::array<float,synergy_count> synergy_flash{};
 std::vector<SceneObject> scene_objects;
 CombatDiagnostics combat_diagnostics;
 DirectorOutput director_state;
 int upgrade_rerolls_remaining=0,death_protections_remaining=0;
 bool first_elite_reward_claimed=false;
 // Diagnostics are measured by the simulation, not FPS estimates.
 uint64_t broadphase_candidates=0,simulation_steps=0;
 // Pressure: real build data stays in skills/equipment; these sources affect runtime only.
 PressureEvent pressure_event;std::vector<DangerZone> danger_zones;std::vector<SoulLink> soul_links;
 PressureDiagnostics pressure_metrics;
 bool pressure_active(PressureKind kind) const {return pressure_event.kind==kind&&pressure_event.remaining>0;}
 int effective_skill_level(Skill skill) const;
 bool skill_sealed(Skill skill) const;
 bool smoke_at(Vec2 position) const;
 bool equipment_suppressed() const;bool buffs_suppressed() const;bool regression_active() const;
 bool skill_attack_available(Skill skill,Vec2 origin) const;
 bool displacement_immune(const Enemy& enemy) const;
 float skill_failure_feedback(Skill skill) const;
 float dash_recovery_factor() const;float cast_recovery_factor() const;
 size_t enemy_capacity() const;
 bool begin_pressure_event(PressureKind kind);void end_pressure_event();
 bool add_danger_zone(DangerZone zone);
private:
 float gear_shield_seconds_=0;
 bool gear_dirty_=true,gear_cache_suppressed_=false,gear_cache_regressed_=false;
 std::array<float,12> gear_stats_{};std::array<float,32> gear_roles_{};std::array<float,16> gear_mechanisms_{},gear_cooldowns_{};std::array<uint64_t,16> gear_roots_{};
 void refresh_gear();void update_gear(float dt);void drop_gear(const Enemy&);void gear_hit(size_t,float,Skill);float gear_skill_bonus(Skill) const;
 void refresh_build_cache() const;
 void update_resonances(float dt);void flush_resonance_effects();
 Skill resonance_partner(Resonance id,Skill source,bool source_second=true) const;
 bool begin_resonance(Resonance id);void resonance_visual(Resonance id,Skill source,Vec2 from,Vec2 to,float radius);
 void resonance_hit(size_t target,float actual,Skill source);void resonance_absorb(float amount,Skill source);
 void resonance_cast(Skill source);void resonance_dash(Vec2 from,Vec2 to,bool ended);
 void resonance_area(Resonance id,Skill source,Vec2 origin,float radius,float damage,int limit);
 size_t resonance_target(Skill source,Vec2 origin,float range);
 Vec2 resonance_landing(Skill source,Vec2 fallback);void resonance_add_shield(Resonance id,float amount);float resonance_take_shield(float damage);
 std::array<float,resonance_count> resonance_cooldown_{},resonance_flash_{},resonance_meter_{};
 std::array<uint64_t,resonance_count> resonance_last_root_{};
 std::array<float,resonance_count> resonance_shields_{},resonance_shield_time_{};std::array<uint32_t,skill_count> hunt_previous_target_{};
 uint64_t resonance_dash_root_=0;
 std::vector<Effect> pending_resonance_effects_;
 Skill resonance_dash_source_=Skill::Count;float resonance_trail_distance_=0;int resonance_trail_count_=0;
 uint64_t next_attack_id_=1,current_attack_id_=0;

 mutable std::array<int,skill_count> cached_build_levels_{};
 mutable std::array<Skill,8> ordered_skills_{{Skill::Count,Skill::Count,Skill::Count,Skill::Count,Skill::Count,Skill::Count,Skill::Count,Skill::Count}};
 mutable std::array<Skill,skill_count> main_routes_{};
 std::mt19937 pressure_rng_{1},ability_rng_{1};uint32_t next_pressure_source_=1;
 float next_pressure_in_=50,next_map_zone_in_=28,last_output_denial_end_=-100;
 PressureKind last_pressure_=PressureKind::None,previous_pressure_=PressureKind::None;Skill last_sealed_=Skill::Count;
 std::array<bool,skill_count> cast_failed_{};std::array<float,skill_count> cast_failure_flash_{};
 bool gear_suppressed_=false,buff_suppressed_=false,regressed_=false;
 std::vector<std::pair<Enemy,float>> reviving_enemies_;
 void reset_pressure();void update_pressure(float encounter_dt,float combat_dt);void update_pressure_sources();
 bool pressure_legal(PressureKind kind) const;void apply_dark_tide(Enemy& enemy);
 void pressure_spawn(EnemyKind kind,Vec2 offset,bool elite=false,int shadow=-1);
 void assign_enemy_abilities(Enemy& enemy);bool update_enemy_abilities(Enemy& enemy,float dt);
 float resolve_ability_damage(Enemy& enemy,float candidate);void on_enemy_real_death(Enemy& enemy);
 void update_danger_zones(float dt);void schedule_map_zone(float dt);bool major_zone_legal(DangerKind kind) const;
 Vec2 projectile_motion(Vec2 from,Vec2 velocity,float dt) const;
 bool control_immune(const Enemy& enemy) const;
 void displace_enemy(Enemy& enemy,Vec2 displacement,bool impulse=false);
 bool try_skill_cast(Skill skill);bool cast_has_conditions(Skill skill) const;
 bool effect_attack_available(const Effect& effect) const;
 bool effect_is_summon(const Effect& effect) const;
 void damage_pressure_facilities(Vec2 center,float radius,float damage);
 RunBonuses run_bonuses_;
 bool loot_enabled_=false,relic_selection_=false;uint32_t unlocked_relic_mask_=0,owned_relic_mask_=0;
 std::array<EquipmentId,equipment_slot_count> equipment_{};
 std::array<float,relic_count> relic_cooldowns_{};std::array<float,equipment_count> equipment_cooldowns_{};
 int pending_relic_rewards_=0,wand_casts_=0;bool thunder_pending_=false;float relic_haste_remaining_=0;
 bool equipped(EquipmentId id) const;
 void reset_loot_timers();void update_loot(float dt);void on_enemy_hit(size_t index,Skill source,Vec2 direction);void on_player_hit();void on_kill();void on_dash();void on_skill_cast(Skill skill,float damage,Vec2 direction);
 bool generate_relic_choices();void continue_choices();
 Difficulty difficulty_=Difficulty::Normal;uint32_t daily_key_=0,starting_relic_mask_=0;int daily_variant_=0,last_boss_cycle_=-1,boss_encounters_spawned_=0;CharacterClass daily_character_=CharacterClass::Archer;
 void record_boss_spawn(Enemy& enemy);
 float healing_multiplier() const;float heal_player(float amount);float enemy_attack_multiplier() const;
 RunMode run_mode_=RunMode::Classic;uint32_t run_seed_=1;
 int chapter_index_=0,intermission_choice_=-1;
 float completed_elapsed_=0,next_chapter_shield_seconds_=0,hazard_clock_=12;
 bool chapter_finish_pending_=false;
 float scene_spawn_clock_=0;uint32_t next_scene_id_=1;
 EncounterDirector director_;double director_last_damage_=0;
 void update_director(float dt,bool boss_due,bool boss_alive);
 void reset_combat_scene();void update_scene_objects(float dt,bool interact);void populate_scene();
 void hurt_scene_object(size_t index,float damage,Skill source);void damage_scene_area(Vec2 pos,float radius,float damage,Skill source);
 bool scene_solid(const SceneObject& object) const;float scene_slow(Vec2 pos,float radius) const;
 void resolve_scene_collision(Vec2& pos,float radius);bool scene_projectile_collision(Vec2 from,Vec2 to,float radius,float damage,Skill source);
 struct SummonRequest {Vec2 pos;bool from_boss=false;int visual=-1;};
 float combat_half_width_=300,combat_half_height_=200;
 bool enemy_attack_visible(const Enemy& enemy) const;
 bool update_extra_enemy(Enemy& enemy,float dt);
 EnemyKind budget_enemy_role(EnemyKind kind,bool event_target) const;
 int elite_population() const;int elite_capacity() const;
 bool can_start_enemy_attack(const Enemy& enemy);
 bool enemy_budget_boss_present_=false;std::vector<size_t> active_enemy_attacks_;
 Vec2 enemy_attack_endpoint(Vec2 from,Vec2 wanted,float radius) const;
 int select_enemy_visual(EnemyKind kind,bool event_target);
 static float enemy_visual_radius(int visual);
 static float enemy_death_duration(int visual);
 static float boss_death_duration(BossProfile profile);
 void update_phased_boss(Enemy& enemy,float dt,std::vector<SummonRequest>& summons);
 void update_boss_node(Enemy&,float);void spawn_boss_nodes();void cancel_boss_attack(Enemy&);
 std::vector<Enemy> pending_boss_nodes_;uint32_t previous_combat_states_=0;float last_failed_record_=-10;
 Vec2 cooperative_enemy_direction(const Enemy& enemy,Vec2 direction);void move_ordinary_enemy(Enemy& enemy,Vec2 direction,float speed,float dt);
 Vec2 safe_enemy_position(Vec2 wanted,float radius,float safe_distance);bool enemy_targetable(const Enemy& enemy) const;
 size_t target_by_id(uint32_t id,Vec2 origin,float range) const;size_t skill_role_target(Skill source,Vec2 origin,float range,uint32_t preferred=0);
 const DangerZone* altar_target(Skill source,Vec2 origin,float range,size_t enemy_target,uint32_t held=0) const;
 void clear_battlefield();void finish_chapter();void update_environment(float dt);
 int chapter_reward_skill() const;
 int boss_pattern(const Enemy& enemy) const;
 float boss_interval() const;
 std::mt19937 rng_; std::array<float,skill_count> cooldown_{}; float spawn_clock_=0,elite_clock_=0,dash_remaining_=0; Vec2 dash_direction_{1,0}; uint32_t next_id_=1; int pending_levels_=0;
 std::array<float,skill_count> cooldown_period_{};
 float soul_hit_cooldown_=0,soul_absorb_cooldown_=0,focus_hit_cooldown_=0,holy_support_cooldown_=0;
 bool apply_fusion(Skill result);bool apply_fusion(const FusionRecipe& recipe);
 bool cast_adventure(Skill skill);bool update_adventure_effect(Effect&,float);void on_adventure_hit(size_t,float,Skill);void on_adventure_projectile_hit(Projectile&,size_t);void update_adventure_projectile(Projectile&,float);void adventure_dash(Vec2,Vec2,bool);
 std::array<int,skill_count> adventure_phase_{};uint64_t adventure_root_=0;float shadow_ready_=0;int combat_coins_=0,energy_hits_=0,bat_hits_=0;Vec2 adventure_dash_start_;bool adventure_dashing_=false;bool cast_fusion(Skill skill);
 void roll_scroll_reward(bool guaranteed=false);bool generate_scroll_offer();bool apply_scroll_study(uint32_t id,Skill learn,Skill replace=Skill::Count);
 bool can_learn_material(Skill skill) const;void remove_skill_runtime(Skill skill,bool keep_projectiles=false);bool validate_scroll_state() const;
 bool apply_skill_replacement(Skill learn,Skill replace);
 void complete_upgrade_choice(size_t index);void finish_upgrade_round();void refresh_upgrade_choices();int upgrade_weight(const UpgradeChoice& choice) const;
 int pending_skill_rewards_=0;bool bonus_selection_=false;
 Skill fusion_dash_source_=Skill::Count;bool fusion_dash_active_=false;std::vector<uint32_t> fusion_dash_hits_;float fusion_dash_trail_=0;
 void on_fusion_dash();void update_fusion_dash(Vec2 from,Vec2 to,bool ended);
 std::array<float,synergy_count> synergy_cooldown_{};
 bool cast_cross_fusion(Skill skill);bool update_cross_effect(Effect&,float,std::vector<Effect>&);
 void update_cross_projectile(Projectile&,float);void on_cross_projectile_hit(Projectile&,size_t);
 void on_cross_defense();void on_cross_dash();void update_cross_dash(Vec2,Vec2,bool);
 bool cast_advanced(Skill skill);bool cast_advanced_fusion(Skill skill);
 bool update_advanced_effect(Effect& effect,float dt,std::vector<Effect>& spawned);
 bool update_advanced_fusion_effect(Effect& effect,float dt,std::vector<Effect>& spawned);
 void update_advanced_projectile(Projectile& projectile,float dt);void on_advanced_projectile_hit(Projectile& projectile,size_t target);
 void update_skill_status(size_t target,float dt);void on_advanced_hit(Skill source);
 void apply_poison(size_t target,float damage,float duration,Skill source,int layers=1);
 void apply_frost(Enemy& enemy,float duration,Skill source,int layers=1);void apply_burn(Enemy& enemy,float damage,float duration,Skill source);
 size_t priority_enemy(Vec2 origin,float range,uint32_t omit=0);
 void skill_area(Vec2 origin,float radius,float damage,Skill source,float knockback=.3f,bool can_proc=true);
 void skill_line(Vec2 from,Vec2 to,float width,float damage,Skill source,float knockback=.3f,bool can_proc=true);
 void skill_cone(Vec2 origin,Vec2 direction,float radius,float half_angle,float damage,Skill source,float knockback=.3f);
 bool skill_bolt(Skill source,Vec2 origin,Vec2 direction,float damage,float speed,float radius,int pierce,uint32_t target=0,int bounces=0,int blocks=0);
 void lure_enemies(Effect& effect);void pull_enemies(const Effect& effect,float dt,float speed,int limit=12);
 std::string advanced_skill_parameters(Skill skill,int level) const;
 bool cast_expansion(Skill skill);bool update_expansion_effect(Effect& effect,float dt,std::vector<Effect>& spawned);
 void record_synergy(int index);void trigger_burst_synergy(size_t index,float damage,Skill source,std::vector<Effect>& spawned);
 void on_shield_absorb();
 bool update_fusion_effect(Effect& effect,float dt,std::vector<Effect>& spawned);
 bool retaliation_pending_=false;std::array<bool,skill_count> announced_evolutions_{};
 std::unordered_map<int64_t,std::vector<size_t>> grid_;
 void begin_special_event(bool scroll_challenge=false);void update_content(float dt);void finish_special_event(bool success);void collect_item(ItemKind kind);void drop_item(ItemKind kind,Vec2 pos);void notify(const std::string& text);
 void step(float dt,Input input); float random(float low,float high); void spawn(EnemyKind kind,bool event_target=false); void rebuild_grid(); std::vector<size_t> nearby(Vec2 p,float radius,size_t limit=0); size_t nearest(Vec2 p,float radius);
 void hurt_enemy(size_t index,float damage,Vec2 direction,float knockback=1.f,Skill source=Skill::Count,bool can_crit=true,bool can_proc=true,int resonance_id=-1);
 void hurt_player(float damage,bool from_boss=false,bool from_environment=false,int boss_attack=-1,const char* source=nullptr);void update_enemies(float dt);void update_effects(float dt);void update_projectiles(float dt);
 void add_projectile(Projectile projectile);void cast(Skill skill); void remove_dead(); void gain_xp(float amount); void generate_choices(const std::vector<UpgradeChoice>* previous=nullptr); void add_effect(Effect e); void add_event(EventKind kind,Vec2 pos,Skill source=Skill::Count); void refresh_stats();
};
}


#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include "encounter_pool.hpp"
#include "boss_combat.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>

using namespace maple;
namespace {
Config sample_config(){
 Config c;c.duration=10000;c.boss_time=.02f;c.elite_interval=c.first_event_time=1e6f;c.spawn_radius=320;c.max_enemies=80;c.item_drop_chance=0;
 c.enable_pressure=false;c.enable_director=c.enable_boss_phases=true;c.director_config.boss_telegraph=.4f;c.director_config.first_encounter=600;c.director_config.ambient_interval=10;
 return c;
}
void tick(Game& game,float seconds){for(int n=0;n<int(std::ceil(seconds/.01f));++n)game.update(.01f);}
Enemy& boss(Game& game){auto found=std::find_if(game.enemies.begin(),game.enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss;});assert(found!=game.enemies.end());return *found;}
void observe_only(Game& game){game.skills.fill(0);game.player.invulnerable=1000;}
}

int main(){
 // Every released Boss enters through the director with a singleton
 // legal resource mask. This is targeted observation, not a natural encounter sample.
 uint32_t profiles=0;
 for(uint32_t id=0;id<boss_profile_count;++id){
  const uint32_t seed=20261120+id;auto config=sample_config();config.available_boss_mask=1u<<id;
  Game game(config);game.start(CharacterClass::Archer,seed);observe_only(game);
  tick(game,.6f);assert(game.boss_spawned&&game.metrics.boss_spawn_time>=.4f);
  assert(boss(game).boss_profile==game.next_boss_profile());profiles|=1u<<int(boss(game).boss_profile);
  std::set<int> attacks;const Vec2 spawned_at=boss(game).pos;float first_attack=-1;
  // Immunity does not disable contact push; pin this stationary observer so
  // ordinary escorts cannot carry it offscreen from a correctly gated boss.
  for(int frame=0;frame<4000;++frame){game.player.pos={};game.update(.01f);if(boss(game).windup>0){attacks.insert(boss(game).boss_attack);if(first_attack<0)first_attack=game.elapsed;}}
  if(attacks.size()<3){const auto& e=boss(game);std::cerr<<"Boss sample seed="<<seed<<" profile="<<int(e.boss_profile)<<" spawn="<<spawned_at.x<<","<<spawned_at.y<<" end="<<e.pos.x<<","<<e.pos.y<<" player="<<game.player.pos.x<<","<<game.player.pos.y<<" first_attack="<<first_attack<<" clock="<<e.attack_cooldown<<" windup="<<e.windup<<" action="<<e.action_remaining<<" recovery="<<e.recovery_remaining<<" attacks:";for(int attack:attacks)std::cerr<<" "<<attack;std::cerr<<std::endl;}
  assert(attacks.size()==3);
  assert(game.metrics.content.boss_encounters[id]==1&&game.metrics.content.boss_moves_seen[id]==7);
  assert(game.displacement_immune(boss(game)));
  // Mechanism targets use independent HP and never pay the boss reward.
  if(boss_combat_definition(boss(game).boss_profile).node_visual>=0){
   bool spawned=false;for(const auto& n:game.enemies)spawned|=n.boss_owner==boss(game).id;
   assert(spawned||game.combat_diagnostics.boss_summons>0);
   int coins=game.coins,kills=game.kills;for(auto& n:game.enemies)if(n.boss_owner)n.hp=0;
   game.update(.01f);assert(game.coins==coins&&game.kills==kills);
  }
  // Direct HP changes below are phase/death fixtures, not claimed playthroughs.
  boss(game).hp=boss(game).max_hp*(boss_combat_definition(boss(game).boss_profile).phase_two-.01f);game.update(.01f);assert(boss(game).boss_stage==2);
  boss(game).hp=boss(game).max_hp*(boss_combat_definition(boss(game).boss_profile).phase_three-.01f);game.update(.01f);assert(boss(game).boss_stage==3&&game.combat_diagnostics.boss_phase_changes==2);
  const BossProfile profile=boss(game).boss_profile;boss(game).hp=0;game.update(.01f);
  auto death=std::find_if(game.effects.begin(),game.effects.end(),[](const Effect& e){return e.kind==EffectKind::Death&&e.enemy_kind==EnemyKind::Boss;});
  assert(death!=game.effects.end()&&death->boss_profile==profile&&death->duration>=.9f);
  assert(game.metrics.content.boss_kills[int(profile)]==1&&game.boss_defeated&&game.coins>=30);
  std::cout<<"Boss "<<int(profile)<<" "<<Game::boss_profile_name(profile)<<": three attacks, stages, death and reward observed\n";
 }
 assert(profiles==0xffffff);

 // Reproduce phase cancellation with real authored node volleys and clock fields.
 for(BossProfile profile:{BossProfile::ForestGuardian,BossProfile::Clockwork}){
  auto config=sample_config();config.available_boss_mask=1u<<int(profile);Game game(config);game.start(CharacterClass::Mage,20261112);observe_only(game);tick(game,.6f);
  const int move=profile==BossProfile::ForestGuardian?2:1;bool started=false;
  for(int frame=0;frame<6000;++frame){game.player.pos={};game.update(.01f);if(boss(game).boss_attack==boss_move_id(profile,move)&&boss(game).windup>0){started=true;break;}}
  assert(started);const auto attack=boss(game).boss_attack_serial;boss(game).hp=boss(game).max_hp*.5f;game.update(.01f);
  for(const auto& effect:game.effects)if(effect.hostile&&effect.root_attack_id==attack&&effect.kind==EffectKind::Warning)assert(effect.triggered||effect.age>=effect.duration);
  for(const auto& zone:game.danger_zones)if(zone.root_attack_id==attack)assert(zone.age>=zone.duration);
 }
 // A real hostile cone must not hurt someone behind its origin.
 {Config config;config.max_enemies=0;config.enable_pressure=false;config.first_event_time=config.boss_time=10000;Game game(config);game.start(CharacterClass::Mage,10);game.skills.fill(0);game.player.pos={-60,0};
  Effect attack;attack.kind=EffectKind::Warning;attack.hostile=true;attack.pattern=4;attack.pos={};attack.end={1,0};attack.radius=100;attack.angle=.4f;attack.delay=.1f;attack.duration=.5f;attack.damage=20;game.effects.push_back(attack);
  float hp=game.player.hp;tick(game,.2f);assert(game.player.hp==hp);
  game.effects.clear();game.player.pos={60,0};game.effects.push_back(attack);tick(game,.2f);assert(game.player.hp==hp-20);
  assert(!game.combat_records.empty()&&game.combat_records.back().amount==20);
 }

 // A small ordinary seed sample establishes variation without selecting winning seeds.
 {std::set<int> selected;std::set<std::vector<int>> rosters;
  for(uint32_t seed=20261120;seed<20261128;++seed){Game game;game.start(CharacterClass::Mage,seed);selected.insert(int(game.next_boss_profile()));rosters.insert(game.encounter_plan.roster);const auto plan=game.encounter_plan;game.toggle_pause();game.timing.speed=2;game.prepare_encounter_plan();assert(game.encounter_plan.boss==plan.boss&&game.encounter_plan.roster==plan.roster);}
  assert(selected.size()>=2&&rosters.size()>=2);std::cout<<"Normal seeds: "<<selected.size()<<" Boss IDs / "<<rosters.size()<<" chapter rosters\n";
 }
 // Short chapter timing fixture uses the real supply/advance path to inspect legal pools.
 {Config config;config.duration=.15f;config.boss_time=1e6f;config.first_event_time=1e6f;Game game(config);RunSetup setup;setup.mode=RunMode::Campaign;game.start(CharacterClass::Mage,20261121,{},setup);
  for(int chapter=0;chapter<3;++chapter){assert(game.chapter_index()==chapter);auto pool=legal_boss_pool(game);assert(pool.size()>=3);assert(std::find(pool.begin(),pool.end(),game.next_boss_profile())!=pool.end());for(auto profile:pool)assert(boss_definition(profile).threat<=chapter+1);std::cout<<"Chapter "<<chapter<<" legal Bosses="<<pool.size()<<"\n";if(chapter<2){tick(game,.2f);assert(game.state==State::ChapterComplete);assert(game.apply_chapter_reward(0)&&game.advance_chapter());}}
 }

 // Three short representative director encounters exercise real behavior roles.
 // Selection weights isolate each ordinary encounter; no enemies are injected.
 uint32_t roles=0;
 const EncounterKind encounters[]={EncounterKind::Rush,EncounterKind::Encirclement,EncounterKind::EliteHunt,EncounterKind::Crisis};
 for(int sample=0;sample<4;++sample){
  Config c=sample_config();c.boss_time=1e6f;c.director_config.first_encounter=0;
  for(auto& rule:c.director_config.rules){rule.weight=rule.kind==encounters[sample]?1.f:0.f;rule.unlock_progress=0;rule.telegraph=.4f;rule.duration=8;rule.spawn_interval=.5f;rule.base_budget=18;rule.batch_size=3;}
  RunSetup setup;setup.mode=RunMode::Daily;setup.daily_variant=sample%3;setup.daily_character=CharacterClass::Archer;
  Game game(c);game.start(CharacterClass::Archer,100+sample,{},setup);observe_only(game);tick(game,4);
  for(const auto& enemy:game.enemies){roles|=1u<<int(enemy.kind);assert(enemy.visual>=0&&enemy.visual<enemy_visual_count);}
 }
 for(auto role:{EnemyKind::Pig,EnemyKind::BlueMushroom,EnemyKind::Ranger,EnemyKind::Splitter,EnemyKind::Support,EnemyKind::Elite,EnemyKind::GroundController,EnemyKind::WindupCharger})assert(roles&(1u<<int(role)));

 // A still-living boss must survive a cycle boundary without a duplicate spawn.
 {Config c=sample_config();c.duration=.7f;Game game(c);RunSetup setup;setup.mode=RunMode::Endless;game.start(CharacterClass::Archer,4,{},setup);observe_only(game);tick(game,2.6f);assert(game.endless_cycle()>=3);assert(std::count_if(game.enemies.begin(),game.enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss;})==1);assert(int(game.next_boss_profile())>=0&&int(game.next_boss_profile())<boss_profile_count);}
 // Daily prediction depends on the run seed, never prior runs in the process.
 {RunSetup setup;setup.mode=RunMode::Daily;setup.daily_variant=2;Game a(sample_config()),b(sample_config());a.start(CharacterClass::Mage,902,{},setup);b.start(CharacterClass::Warrior,17);b.start(CharacterClass::Mage,902,{},setup);assert(a.next_boss_profile()==b.next_boss_profile());}
 std::cout<<"PASS Expansion: 24 Boss attacks/phases/death, normal pool variation, chapter filters, eight roles, no duplicate living Boss and daily determinism\n";
}

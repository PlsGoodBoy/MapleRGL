#ifdef NDEBUG
#undef NDEBUG
#endif
#include "progression.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
using namespace maple;
namespace fs=std::filesystem;
using json=nlohmann::json;
namespace {
struct TempRoot {fs::path path;TempRoot(){static unsigned n=0;path=fs::temp_directory_path()/("MapleRGL_unlock_test_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"_"+std::to_string(++n));assert(fs::create_directory(path));}~TempRoot(){std::error_code ec;auto parent=fs::temp_directory_path(ec).lexically_normal();auto target=path.lexically_normal();if(!ec&&target.parent_path()==parent&&target.filename().u8string().find("MapleRGL_unlock_test_")==0)fs::remove_all(target,ec);}};
std::string bytes(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write(const fs::path& path,const std::string& value){std::ofstream f(path,std::ios::binary|std::ios::trunc);assert(f);f<<value;f.close();assert(!f.fail());}
Config quiet(){Config c;c.max_enemies=0;c.item_drop_chance=0;c.first_event_time=1e6f;c.elite_interval=1e6f;c.boss_time=1e6f;c.duration=61;return c;}
void resolve(Game& g){int guard=0;while(g.state==State::Upgrade){assert(++guard<1000&&!g.choices.empty());assert(g.choose_upgrade(0));}}
void actual_combat(Game& game,int count){game.enemies.clear();for(int i=0;i<count;++i){Enemy e;e.id=1000+i;e.pos={float(i%10),float(i/10%10)};e.hp=e.max_hp=1;e.damage=0;e.speed=0;game.enemies.push_back(e);}Effect field;field.kind=EffectKind::Zone;field.pos=game.player.pos;field.radius=300;field.damage=5;field.duration=.2f;field.source=Skill::Arrow;game.effects.push_back(field);for(int i=0;i<1400&&game.state!=State::Victory;++i){game.update(.05f);resolve(game);}assert(game.state==State::Victory&&game.kills==count&&game.metrics.enemy_kills_by_kind[int(EnemyKind::Slime)]==count);assert(game.metrics.skill_damage[int(Skill::Arrow)]>0);}
void real_combat_unlock_reload_and_equipment_proc(){
 TempRoot t;RunReport report;uint64_t id=0;int64_t balance=0;int reward=0;
 {
  ProgressionStore store(t.path);assert(!store.read_only()&&store.profile().unlocks["classes"].size()==3&&store.profile().unlocks["skills"].size()==18);int gear=0,relics=0;for(const auto& d:equipment_definitions())gear+=store.owns_equipment(d.id)?1:0;for(const auto& d:relic_definitions())relics+=store.relic_unlocked(d.id)?1:0;assert(gear==3&&relics==6&&!store.owns_equipment(EquipmentId::ScoutCloak));
  for(int cls=0;cls<3;++cls){int legal=0;for(const auto& d:relic_definitions())if(store.relic_unlocked(d.id)&&relic_allowed(d.id,CharacterClass(cls)))++legal;assert(legal>=4);}
  auto begin=store.begin_run(CharacterClass::Archer,101);assert(begin);id=begin.run_id;Game game(quiet());game.start(CharacterClass::Archer,101,store.run_bonuses(),store.profile().active_run->setup);actual_combat(game,500);report=make_run_report(game,RunOutcome::Victory,101);
  // Force a failed disk transaction before granting any account reward.
  assert(fs::create_directory(t.path/"profile.json.bak"));auto before=bytes(store.profile_path());assert(!store.settle(id,report)&&store.profile().crystals==0&&!store.owns_equipment(EquipmentId::ScoutCloak)&&store.profile().achievement_stats.kills==0&&bytes(store.profile_path())==before);assert(fs::remove(t.path/"profile.json.bak"));
  auto result=store.settle(id,report);assert(result&&result.progress_crystals_awarded==settlement_crystals(report)&&result.achievement_crystals_awarded>0&&result.crystals_awarded==result.progress_crystals_awarded+result.achievement_crystals_awarded);reward=result.crystals_awarded;balance=store.profile().crystals;assert(balance==reward&&store.owns_equipment(EquipmentId::ScoutCloak)&&store.owns_equipment(EquipmentId::ReedBow)&&store.relic_unlocked(RelicId::BarbedQuiver));assert(store.profile().achievement_stats.kills==500&&store.profile().achievement_stats.enemy_kills[int(EnemyKind::Slime)]==500);
  assert(store.profile().history.back().report.skills==game.skills&&store.profile().history.back().report.owned_relic_mask==game.owned_relic_mask());before=bytes(store.profile_path());auto repeated=store.settle(id,report);assert(repeated&&repeated.already_claimed&&repeated.crystals_awarded==0&&bytes(store.profile_path())==before);
 }
 {
  ProgressionStore store(t.path);assert(store.profile().crystals==balance&&store.owns_equipment(EquipmentId::ScoutCloak)&&store.profile().achievement_completed[int(AchievementId::SeasonedHunter)]);assert(store.settle(id,report).already_claimed&&store.profile().crystals==balance);
  auto old=store.equipped(EquipmentSlot::Armor);assert(!store.equip(EquipmentSlot::Weapon,EquipmentId::ScoutCloak,CharacterClass::Archer,store.equipped(EquipmentSlot::Weapon)));assert(!store.equip(EquipmentSlot::Armor,EquipmentId::ScholarRobe,CharacterClass::Archer,old));assert(!store.equip(EquipmentSlot::Weapon,EquipmentId::ReedBow,CharacterClass::Mage,store.equipped(EquipmentSlot::Weapon)));assert(store.equip(EquipmentSlot::Armor,EquipmentId::ScoutCloak,CharacterClass::Archer,old));assert(!store.equip(EquipmentSlot::Armor,EquipmentId::None,CharacterClass::Archer,old));
  auto run=store.begin_run(CharacterClass::Archer,102);assert(run);Game next(quiet());next.start(CharacterClass::Archer,102,store.run_bonuses(),store.profile().active_run->setup);assert(next.run_setup().equipment[int(EquipmentSlot::Armor)]==EquipmentId::ScoutCloak&&(next.run_setup().unlocked_relic_mask&(1u<<int(RelicId::BarbedQuiver))));
  next.enemies.clear();Enemy target;target.id=50;target.pos={50,0};target.hp=target.max_hp=10000;target.damage=0;target.speed=0;next.enemies.push_back(target);next.update(.01f,{{1,0},true});assert(next.metrics.equipment_procs[int(EquipmentId::ScoutCloak)]==1&&next.enemies[0].slow_remaining>0&&next.enemies[0].slow_factor<1);
 }
}
void catalog_and_reward_requirements(){
 TempRoot t;ProgressionStore p(t.path);std::set<std::string> keys;auto status=p.achievement_status();assert(status.size()==20);for(const auto& d:achievement_definitions()){assert(keys.insert(d.key).second&&d.target>0&&d.crystal_reward>0&&!achievement_reward_text(d.id).empty());auto s=status[int(d.id)];assert(s.id==d.id&&!s.completed&&s.target==d.target&&s.progress<=s.target);}
 for(const auto& d:equipment_definitions())if(d.id!=EquipmentId::None){assert(!equipment_unlock_requirement(d.id).empty());if(!d.initially_owned){bool reward=false;for(const auto& a:achievement_definitions())reward|=a.equipment_reward==d.id;assert(reward);}}
 for(const auto& d:relic_definitions()){assert(!relic_unlock_requirement(d.id).empty());if(!d.initially_unlocked){bool reward=false;for(const auto& a:achievement_definitions())reward|=(a.relic_reward_mask&(1u<<int(d.id)))!=0;assert(reward);}}
}
void chapter_achievement_delta_and_legacy_snapshot(){
 TempRoot t;uint64_t id=0;int earned=0;json old;
 {
  ProgressionStore p(t.path);auto run=p.begin_run(CharacterClass::Archer,9,RunMode::Campaign);assert(run);id=run.run_id;Game g;g.start(CharacterClass::Archer,9,p.run_bonuses(),p.profile().active_run->setup);g.elapsed=600;g.kills=100;g.metrics.enemy_kills_by_kind[int(EnemyKind::Slime)]=100;g.metrics.chapters_cleared=1;g.state=State::ChapterComplete;RunBuild b;assert(g.export_build(b));auto r=make_run_report(g,RunOutcome::Victory,9);auto claim=p.claim_chapter(id,0,r,b);assert(claim&&p.profile().achievement_stats.kills==100&&p.profile().achievement_stats.chapters==1);earned=claim.crystals_awarded;assert(p.claim_chapter(id,0,r,b).already_claimed&&p.profile().achievement_stats.kills==100);old=json::parse(bytes(p.profile_path()));r.outcome=RunOutcome::Abandoned;auto end=p.settle(id,r);assert(end&&end.crystals_awarded==0&&p.profile().achievement_stats.kills==100&&p.profile().achievement_stats.chapters==1&&p.profile().history.back().crystals==earned);
 }
 // Strip only M3 fields to model a genuine schema-2 boundary. Migration must not add gear to it.
 old["schema_version"]=2;for(const char* key:{"owned_equipment","equipped","unlocked_relic_mask","challenge_eligible","achievement_completed","achievement_stats"})old.erase(key);for(const char* key:{"setup","accounted_report","achievement_crystals"})old["active_run"].erase(key);auto& build=old["chapter_checkpoint"]["build"];for(const char* key:{"enable_loot","unlocked_relic_mask","owned_relic_mask","equipment"})build.erase(key);for(const char* key:{"relics_collected","relic_procs","equipment_procs"})build["metrics"].erase(key);auto& report=old["active_run"]["claimed_report"];for(const char* key:{"skills","passives","skill_damage","enemy_kills","dash_uses","items_collected","rerolls_used","death_protections_used","evolved_count","equipment","owned_relic_mask","detailed_metrics"})report.erase(key);write(t.path/"profile.json",old.dump());
 ProgressionStore migrated(t.path);assert(migrated.load_state()==ProfileLoadState::Migrated&&!migrated.read_only());RunBuild restored;assert(migrated.checkpoint_build(restored)&&!restored.enable_loot&&restored.owned_relic_mask==0);for(auto item:restored.equipment)assert(item==EquipmentId::None);assert(!migrated.profile().active_run->setup.enable_loot&&migrated.run_setup(CharacterClass::Archer).enable_loot);assert(migrated.save_settings(migrated.profile().settings));assert(json::parse(bytes(migrated.profile_path()))["schema_version"]==profile_schema_version);
}
}
int main(){real_combat_unlock_reload_and_equipment_proc();catalog_and_reward_requirements();chapter_achievement_delta_and_legacy_snapshot();std::cout<<"unlock_progression: real combat -> atomic unlock -> restart -> equipped dash effect, catalogs, class/slot guards, one-time rewards, chapter deltas and legacy snapshot migration passed (isolated roots only)\n";}

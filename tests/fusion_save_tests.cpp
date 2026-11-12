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
using namespace maple;
namespace fs=std::filesystem;
using json=nlohmann::json;
namespace {
struct TempRoot {
 fs::path path;
 TempRoot(){static unsigned serial=0;path=fs::temp_directory_path()/("MapleRGL_fusion_save_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"_"+std::to_string(++serial));assert(fs::create_directory(path));}
 ~TempRoot(){std::error_code ec;const auto parent=fs::absolute(fs::temp_directory_path(ec)).lexically_normal();const auto target=fs::absolute(path).lexically_normal();if(!ec&&target.parent_path()==parent&&target.filename().u8string().find("MapleRGL_fusion_save_")==0)fs::remove_all(target,ec);}
};
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::string& data){std::ofstream out(path,std::ios::binary|std::ios::trunc);assert(out);out<<data;out.close();assert(!out.fail());}
bool near(double a,double b){return std::abs(a-b)<.001;}
uint64_t flag(Skill skill){return uint64_t(1)<<int(skill);}
// Persistence fixture at an already-completed chapter. Fusion combat and natural
// unlock timing are validated separately; no formal player profile is touched.
void boundary(Game& game){game.elapsed=120;game.kills=20;game.metrics.enemy_kills_by_kind[int(EnemyKind::Slime)]=20;game.metrics.chapters_cleared=1;game.state=State::ChapterComplete;}
void fusion_boundary_roundtrip(){
 TempRoot root;write(root.path/"profile.json",json{{"schema_version",0},{"crystals",1000}}.dump());
 uint64_t run=0;const uint64_t consumed=flag(Skill::Nova)|flag(Skill::Retaliate);int64_t crystals=0;
 const std::array<int,synergy_count> synergies={1,2,3,4,5,6};
 {
  ProgressionStore store(root.path);assert(store.purchase(TalentId::Damage,0)&&store.purchase(TalentId::Health,0));
  const auto begun=store.begin_run(CharacterClass::Warrior,501,RunMode::Campaign);assert(begun);run=begun.run_id;
  Game game;game.start(CharacterClass::Warrior,501,store.run_bonuses(),store.profile().active_run->setup);
  game.skills[int(Skill::Nova)]=0;game.skills[int(Skill::Retaliate)]=0;game.skills[int(Skill::DragonArmor)]=2;
  game.consumed_skills=consumed;game.dragon_soul=62.5f;game.metrics.fusions_created=1;game.metrics.first_fusion_time=60;
  game.skills[int(Skill::Bulwark)]=3;game.guard_charge=27.5f;game.metrics.synergy_triggers=synergies;
  game.metrics.skill_damage[int(Skill::Nova)]=17.25;game.metrics.skill_damage[int(Skill::DragonArmor)]=91.5;boundary(game);
  RunBuild build;assert(game.export_build(build));const auto report=make_run_report(game,RunOutcome::Victory,501);
  assert(store.claim_chapter(run,0,report,build)&&store.choose_chapter_supply(run,2));
  const auto saved=json::parse(bytes(store.profile_path()));const auto& snapshot=saved.at("chapter_checkpoint").at("build");
  assert(saved.at("schema_version")==profile_schema_version&&snapshot.at("skills").is_object());
  assert(snapshot.at("skills").at("dragon_armor")==2&&snapshot.at("skills").at("nova")==0);
  assert(snapshot.at("consumed_skills")==json::array({"nova","retaliate"})&&near(snapshot.at("dragon_soul").get<double>(),62.5));
  assert(near(snapshot.at("metrics").at("skill_damage").at("dragon_armor").get<double>(),91.5));
  assert(near(snapshot.at("guard_charge").get<double>(),27.5)&&snapshot.at("metrics").at("synergy_triggers")==json(synergies));
 }
 {
  ProgressionStore store(root.path);RunBuild restored;assert(!store.read_only()&&store.checkpoint_build(restored));
  assert(restored.consumed_skills==consumed&&restored.skills[int(Skill::DragonArmor)]==2&&restored.skills[int(Skill::Nova)]==0&&restored.skills[int(Skill::Retaliate)]==0);
  assert(near(restored.dragon_soul,62.5)&&restored.metrics.fusions_created==1&&near(restored.metrics.first_fusion_time,60));
  assert(near(restored.guard_charge,27.5)&&restored.metrics.synergy_triggers==synergies);
  Game game;assert(game.restore_build(restored)&&game.restore_build(restored));
  assert(near(game.player.max_hp,129)&&near(game.damage_multiplier,1.02)&&near(game.dragon_soul,62.5));
  assert(near(game.guard_charge,27.5)&&game.metrics.synergy_triggers==synergies);
  assert(!game.can_gain_skill(Skill::Nova)&&!game.can_gain_skill(Skill::Retaliate));
  assert(game.advance_chapter()&&game.consumed_skills==consumed&&near(game.dragon_soul,62.5));
  assert(near(game.guard_charge,27.5)&&game.metrics.synergy_triggers==synergies);
  const auto report=make_run_report(game,RunOutcome::Abandoned,501);assert(store.settle(run,report));crystals=store.profile().crystals;
  const auto& history=store.profile().history.back().report;assert(history.consumed_skills==consumed&&history.fusions_created==1&&near(history.first_fusion_time,60));
  assert(history.skills[int(Skill::DragonArmor)]==2&&near(history.skill_damage[int(Skill::DragonArmor)],91.5)&&near(history.skill_damage[int(Skill::Nova)],17.25));
  assert(history.synergy_triggers==synergies);
  const auto saved=bytes(store.profile_path());assert(store.settle(run,report).already_claimed&&bytes(store.profile_path())==saved);
 }
 ProgressionStore reopened(root.path);assert(reopened.profile().crystals==crystals&&reopened.profile().history.back().report.consumed_skills==consumed&&reopened.profile().history.back().report.synergy_triggers==synergies);
}
void as_legacy_skills(json& owner,const char* key){
 const auto table=owner.at(key);assert(table.is_object());json old=json::array();
 const Skill order[]={Skill::Homing,Skill::Arrow,Skill::Nova,Skill::Pierce,Skill::Lightning,Skill::Zone,Skill::Summon,Skill::Cleave,Skill::Orbit,Skill::Spread,Skill::Rain,Skill::Ricochet,Skill::Frost,Skill::Meteor,Skill::Rune,Skill::Charge,Skill::BladeWave,Skill::Retaliate};
 for(Skill skill:order)old.push_back(table.at(Game::skill_key(skill)));owner[key]=std::move(old);
}
void remove_fusion_fields(json& value){for(const char* key:{"consumed_skills","dragon_soul","fusions_created","first_fusion_time","guard_charge","synergy_triggers"})value.erase(key);}
void schema_four_array_migration(){
 TempRoot root;json old;uint64_t run=0;
 {
  ProgressionStore store(root.path);const auto begun=store.begin_run(CharacterClass::Archer,504,RunMode::Campaign);assert(begun);run=begun.run_id;
  Game game;game.start(CharacterClass::Archer,504,store.run_bonuses(),store.profile().active_run->setup);game.skills[int(Skill::Arrow)]=2;game.skills[int(Skill::Pierce)]=3;game.metrics.skill_damage[int(Skill::Arrow)]=70;game.metrics.skill_damage[int(Skill::Pierce)]=30;boundary(game);
  RunBuild build;assert(game.export_build(build)&&store.claim_chapter(run,0,make_run_report(game,RunOutcome::Victory,504),build));old=json::parse(bytes(store.profile_path()));
 }
 old["schema_version"]=4;old["unlocks"]["skills"]=json::array();for(int i=0;i<18;++i)old["unlocks"]["skills"].push_back(i);
 for(const char* key:{"claimed_report","accounted_report"}){auto& report=old["active_run"][key];as_legacy_skills(report,"skills");as_legacy_skills(report,"skill_damage");remove_fusion_fields(report);}
 auto& snapshot=old["chapter_checkpoint"]["build"];as_legacy_skills(snapshot,"skills");as_legacy_skills(snapshot["metrics"],"skill_damage");remove_fusion_fields(snapshot);remove_fusion_fields(snapshot["metrics"]);
 const auto old_bytes=old.dump();write(root.path/"profile.json",old_bytes);
 ProgressionStore migrated(root.path);assert(migrated.load_state()==ProfileLoadState::Migrated&&!migrated.read_only());RunBuild restored;assert(migrated.checkpoint_build(restored));
 assert(restored.skills[int(Skill::Arrow)]==2&&restored.skills[int(Skill::Pierce)]==3&&restored.skills[int(Skill::StarReturn)]==0);
 assert(restored.consumed_skills==0&&restored.metrics.fusions_created==0&&restored.metrics.first_fusion_time==-1&&restored.dragon_soul==0);
 assert((restored.guard_charge==0&&restored.metrics.synergy_triggers==std::array<int,synergy_count>{}));
 assert(near(restored.metrics.skill_damage[int(Skill::Arrow)],70)&&near(restored.metrics.skill_damage[int(Skill::Pierce)],30));
 assert(migrated.save_settings(migrated.profile().settings));const auto current=json::parse(bytes(migrated.profile_path()));
 assert(current.at("schema_version")==profile_schema_version&&current.at("chapter_checkpoint").at("build").at("skills").is_object());
 assert(bytes(root.path/"profile.json.bak")==old_bytes&&migrated.choose_chapter_supply(run,2));
}
void schema_five_optional_state(){
 TempRoot root;json early;
 {
  ProgressionStore store(root.path);const auto begun=store.begin_run(CharacterClass::Archer,505,RunMode::Campaign);assert(begun);
  Game game;game.start(CharacterClass::Archer,505,store.run_bonuses(),store.profile().active_run->setup);boundary(game);
  RunBuild build;assert(game.export_build(build)&&store.claim_chapter(begun.run_id,0,make_run_report(game,RunOutcome::Victory,505),build));early=json::parse(bytes(store.profile_path()));
 }
 // A real schema-5 predecessor has fusion fields but no M4 guard/synergy fields.
 early["chapter_checkpoint"]["build"].erase("guard_charge");early["chapter_checkpoint"]["build"]["metrics"].erase("synergy_triggers");
 for(const char* key:{"claimed_report","accounted_report"})early["active_run"][key].erase("synergy_triggers");write(root.path/"profile.json",early.dump());
 ProgressionStore loaded(root.path);RunBuild build;assert(loaded.load_state()==ProfileLoadState::Loaded&&loaded.checkpoint_build(build));
 assert((build.guard_charge==0&&build.metrics.synergy_triggers==std::array<int,synergy_count>{}));
 assert((loaded.profile().active_run->claimed_report.synergy_triggers==std::array<int,synergy_count>{}));
}
}
int main(){fusion_boundary_roundtrip();schema_four_array_migration();schema_five_optional_state();std::cout<<"fusion_save: keyed fusion/history, retained dragon soul and guard, synergy counters, once-only settlement, legacy schema-4 arrays and schema-5 optional defaults passed (isolated profiles)\n";}

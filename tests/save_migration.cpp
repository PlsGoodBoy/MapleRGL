#include "progression.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace maple;
using nlohmann::json;
namespace fs=std::filesystem;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
uint64_t bit(Skill s){return uint64_t(1)<<int(s);}
void old_fields(json& value){
 if(value.is_array()){for(auto& entry:value)old_fields(entry);return;}if(!value.is_object())return;
 for(const char* key:{"adventurer","upgrade_choices","bonus_choices","combat_seconds","tracked","pending_opportunities"})value.erase(key);
 if(value.contains("held")&&value.contains("completed")){auto& held=value["held"];held.erase(std::remove_if(held.begin(),held.end(),[](const json& row){return row["result"]=="dragon_armor"||row["result"]=="flame_dance";}),held.end());}
 for(auto& entry:value.items())old_fields(entry.value());
}
int main(){try{
 const fs::path root=fs::current_path()/"artifacts"/"build"/("migration_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);uint64_t run_id=0;std::array<int,skill_count> original{};
 {
  ProgressionStore store(root);auto begin=store.begin_run(CharacterClass::Warrior,80806,RunMode::Campaign);check(begin.ok,begin.message.c_str());run_id=begin.run_id;
  Game game;game.start(CharacterClass::Warrior,80806,store.run_bonuses(),store.profile().active_run->setup);game.skills.fill(0);
  for(Skill s:{Skill::Cleave,Skill::BladeWave,Skill::Bulwark,Skill::HeavyStrike,Skill::DragonArmor,Skill::FlameDance})game.skills[int(s)]=2;original=game.skills;
  game.consumed_skills=bit(Skill::Nova)|bit(Skill::Retaliate)|bit(Skill::Orbit)|bit(Skill::Zone);game.metrics.fusions_created=2;game.metrics.first_fusion_time=60;
  game.scrolls.held={{1,Skill::BattleBlades,0,false,true},{2,Skill::AzureRift,0,false,true},{3,Skill::DragonArmor,1,false,false},{4,Skill::FlameDance,0,true,false}};game.scrolls.completed={{Skill::DragonArmor,1},{Skill::FlameDance,0}};game.scrolls.studied_results={Skill::FlameDance};game.scrolls.studies_used=1;game.scrolls.next_id=5;game.scrolls.foreign_class=int(CharacterClass::Mage);game.scrolls.authorized_foreign=game.scrolls.learned_foreign=bit(Skill::Zone);game.scrolls.eligible_rewards=game.scrolls.opportunities=4;game.scrolls.first_opportunity_time=30;
  game.elapsed=120;game.kills=20;game.metrics.enemy_kills_by_kind[int(EnemyKind::Slime)]=20;game.metrics.chapters_cleared=1;game.state=State::ChapterComplete;game.timing.speed=2;
  RunBuild build;check(game.export_build(build),"create chapter fixture");auto report=make_run_report(game,RunOutcome::Victory,80806);auto claim=store.claim_chapter(run_id,0,report,build);check(claim.ok,claim.message.c_str());check(store.choose_chapter_supply(run_id,2).ok,"save chapter supply");auto settings=store.profile().settings;settings.game_speed=2;check(store.save_settings(settings).ok,"save original speed");
 }
 json old;{std::ifstream file(root/"profile.json");file>>old;}old_fields(old);old["schema_version"]=6;old["chapter_checkpoint"]["build"]["hp"]=125;
 {std::ofstream file(root/"profile.json");file<<old.dump(2);}
 {
  ProgressionStore store(root);check(!store.read_only()&&store.load_state()==ProfileLoadState::Migrated,"load v6 without touching formal profile");RunBuild build;check(store.checkpoint_build(build),"old chapter remains resumable");Game resumed;check(resumed.restore_build(build),"restore old six-skill build");
  check(resumed.skills==original&&resumed.active_skill_count()==6&&Game::active_skill_limit==8,"six retained skills and two free slots");check(resumed.can_gain_skill(Skill::Zone)&&resumed.can_gain_skill(Skill::Nova),"legacy foreign and consumed flags do not block reacquisition");check(resumed.scrolls.held.size()==4&&resumed.tracked_scroll_count()==2&&resumed.fusion_form(Skill::DragonArmor)==1,"knowledge and forms migrate while keeping two tracked routes");
  check(resumed.timing.speed==2&&store.profile().settings.game_speed==2&&resumed.player.hp<=resumed.player.max_hp,"old speed maps to encounter pace and HP maps to common stats");check(!store.profile().active_run->claimed_report.adventurer,"old class receipt is preserved");check(resumed.advance_chapter(),"continue into the next chapter");
  auto report=make_run_report(resumed,RunOutcome::Abandoned,80806);auto settled=store.settle(run_id,report);check(settled.ok,settled.message.c_str());check(store.profile().history.back().report.adventurer,"new report identifies adventurer and retains starter");check(fs::exists(root/"profile.json.bak"),"atomic backup retained");
 }
 ProgressionStore reopened(root);check(!reopened.read_only()&&reopened.profile().schema_version==profile_schema_version&&reopened.profile().history.back().report.skills==original,"current profile schema reopens with final build intact");
 std::cout<<"PASS Build v6 migration: six slots, public reacquisition, recipe knowledge, forms, pace mapping, campaign resume, atomic backup and current-schema reopen.\n"<<root.u8string()<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

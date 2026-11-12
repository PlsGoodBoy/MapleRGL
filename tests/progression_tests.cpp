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
#include <limits>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
using namespace maple;
namespace fs=std::filesystem;
using json=nlohmann::json;
namespace {
struct TempDirectory {
 fs::path path;
 TempDirectory(){static unsigned serial=0;auto root=fs::temp_directory_path();auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();path=root/("MapleRGL_progression_test_"+std::to_string(nonce)+"_"+std::to_string(++serial));assert(fs::create_directory(path));}
 ~TempDirectory(){std::error_code ec;auto root=fs::temp_directory_path(ec).lexically_normal();auto target=path.lexically_normal();if(!ec&&target.parent_path()==root&&target.filename().u8string().find("MapleRGL_progression_test_")==0)fs::remove_all(target,ec);}
};
std::string bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write_bytes(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::binary|std::ios::trunc);assert(f);f<<text;f.close();assert(!f.fail());}
void funded(const fs::path& dir,int crystals=2000){write_bytes(dir/"profile.json",json{{"schema_version",0},{"crystals",crystals}}.dump());}
RunReport victory(CharacterClass c=CharacterClass::Archer,uint32_t seed=7){RunReport r;r.outcome=RunOutcome::Victory;r.character=c;r.seed=seed;r.elapsed=600;r.kills=1000;r.elite_kills=4;r.events_succeeded=5;r.level=40;r.boss_defeated=true;r.damage_dealt=80000;r.damage_taken=70;r.xp_acquired=4000;return r;}
bool near_value(float a,float b){return std::abs(a-b)<.0001f;}
void new_profile_and_settlement(){
 TempDirectory temp;auto unicode=temp.path/fs::u8path(u8"冒险者_存档");RunReport report;int earned=0;
 {
  ProgressionStore p(unicode);assert(!p.read_only()&&p.load_state()==ProfileLoadState::NewProfile);assert(p.profile().crystals==0&&p.profile().unlocks["classes"].size()==3&&p.profile().unlocks["skills"].size()==size_t(skill_count));assert(!fs::exists(p.profile_path()));
  auto begin=p.begin_run(CharacterClass::Archer,7);assert(begin.ok&&begin.run_id==1&&p.profile().active_run->id==1);ProgressionStore spectator(unicode);assert(spectator.read_only()&&spectator.profile().active_run->id==1&&spectator.profile().next_run_serial==2);
  Game measured;measured.start(CharacterClass::Archer,7,p.run_bonuses(),p.profile().active_run->setup);measured.elapsed=600;measured.kills=1000;measured.metrics.enemy_kills_by_kind[int(EnemyKind::Elite)]=4;measured.metrics.events_succeeded=5;measured.player.level=40;measured.boss_defeated=true;measured.metrics.skill_damage[0]=80000;measured.metrics.damage_taken=70;measured.metrics.xp_acquired=4000;report=make_run_report(measured,RunOutcome::Victory,7);assert(report.damage_dealt==80000&&report.elite_kills==4&&report.events_succeeded==5);earned=settlement_crystals(report);assert(earned>50);auto paid=p.settle(begin.run_id,report);assert(paid.ok&&paid.progress_crystals_awarded==earned&&p.profile().crystals==paid.crystals_awarded);earned=paid.crystals_awarded;assert(!p.profile().active_run&&p.profile().history.size()==1&&p.profile().records.victories==1);
  const auto saved=bytes(p.profile_path());auto again=p.settle(begin.run_id,report);assert(again.ok&&again.already_claimed&&again.crystals_awarded==0&&bytes(p.profile_path())==saved);
  assert(p.purchase(TalentId::Damage,0));assert(!p.purchase(TalentId::Damage,0));assert(p.profile().crystals==earned-8&&p.talent_rank(TalentId::Damage)==1);assert(p.purchase(TalentId::Damage,1)&&p.profile().crystals==earned-21);
 }
 ProgressionStore restart(unicode);assert(!restart.read_only()&&restart.talent_rank(TalentId::Damage)==2&&restart.profile().crystals==earned-21);Game next_adventure;next_adventure.start(CharacterClass::Archer,9,restart.run_bonuses());assert(near_value(next_adventure.damage_multiplier,1.04f)&&near_value(next_adventure.run_bonuses().damage_bonus,.04f));assert(restart.settle(1,report).already_claimed&&!restart.settle(0,report));
}
void talents_and_settings(){
 TempDirectory t;funded(t.path);ProgressionStore p(t.path);std::array<int,3> routes{};std::set<std::string> keys;
 for(auto& d:talent_definitions()){assert(d.route>=0&&d.route<3&&d.max_rank>0&&d.base_cost>0);routes[d.route]++;assert(keys.insert(d.key).second);for(int rank=0;rank<d.max_rank;++rank){auto before=p.profile().crystals;int cost=p.talent_cost(d.id);assert(p.purchase(d.id,rank)&&p.profile().crystals==before-cost);}assert(p.talent_cost(d.id)==-1&&!p.purchase(d.id,d.max_rank));}
 for(int count:routes)assert(count==3);auto b=p.run_bonuses();assert(near_value(b.damage_bonus,.10f)&&near_value(b.crit_bonus,.06f)&&near_value(b.health_bonus,20)&&near_value(b.move_speed_bonus,.06f)&&near_value(b.xp_bonus,.10f)&&near_value(b.cooldown_reduction,.06f));assert(b.upgrade_rerolls==2&&b.death_protections==1&&b.first_elite_reward==1);assert(!p.purchase(TalentId(999),0)&&p.talent_cost(TalentId(999))==-1);
 ProfileSettings s;s.music=false;s.sound=false;s.master_volume=.7f;s.sound_volume=.2f;s.music_volume=.6f;s.zoom=2.25f;s.shake=false;s.show_stats=true;assert(p.save_settings(s));ProgressionStore restored(t.path);auto r=restored.profile().settings;assert(!r.music&&!r.sound&&near_value(r.master_volume,.7f)&&near_value(r.sound_volume,.2f)&&near_value(r.music_volume,.6f)&&near_value(r.zoom,2.25f)&&!r.shake&&r.show_stats);assert(restored.profile().talent_ranks==p.profile().talent_ranks);
 s.zoom=std::numeric_limits<float>::quiet_NaN();assert(!p.save_settings(s));s.zoom=99;assert(!p.save_settings(s));
 auto begun=p.begin_run(CharacterClass::Mage,8);assert(begun&& !p.purchase(TalentId::Health,5));auto mismatched=victory(CharacterClass::Warrior,8);assert(!p.settle(begun.run_id,mismatched));mismatched=victory(CharacterClass::Mage,9);assert(!p.settle(begun.run_id,mismatched));assert(p.profile().active_run->id==begun.run_id);
}
void reward_guards(){
 auto r=victory();r.elapsed=0;assert(settlement_crystals(r)==0);r.elapsed=59.99f;assert(settlement_crystals(r)==0);r.elapsed=600;r.kills=0;assert(settlement_crystals(r)==0);r=victory();int full=settlement_crystals(r);r.outcome=RunOutcome::Defeat;int earned_without_victory=settlement_crystals(r);assert(earned_without_victory==full-10);r.outcome=RunOutcome::Abandoned;assert(settlement_crystals(r)==earned_without_victory/2);r.outcome=RunOutcome::Interrupted;assert(settlement_crystals(r)==0);
 TempDirectory t;ProgressionStore p(t.path);for(int i=0;i<10;++i){auto begin=p.begin_run(CharacterClass::Archer,7);RunReport instant;instant.seed=7;instant.outcome=RunOutcome::Abandoned;auto result=p.settle(begin.run_id,instant);assert(result&&result.crystals_awarded==0);}assert(p.profile().crystals==0&&p.profile().records.runs==10);
 auto start=p.begin_run(CharacterClass::Archer,7);r=victory();r.elapsed=std::numeric_limits<float>::quiet_NaN();assert(!p.settle(start.run_id,r));r=victory();r.kills=-1;assert(!p.settle(start.run_id,r));r=victory();r.elite_kills=r.kills+1;assert(!p.settle(start.run_id,r));assert(p.profile().active_run&&p.profile().crystals==0);
}
void failed_writes_are_transactional(){
 TempDirectory t;ProgressionStore p(t.path);auto start=p.begin_run(CharacterClass::Archer,7);assert(start);const auto primary=bytes(p.profile_path());assert(fs::create_directory(t.path/"profile.json.bak"));auto failed=p.settle(start.run_id,victory());assert(!failed&&p.profile().crystals==0&&p.profile().active_run->id==start.run_id&&bytes(p.profile_path())==primary);assert(fs::remove(t.path/"profile.json.bak"));assert(p.settle(start.run_id,victory()));
 auto before=p.profile().crystals;const auto saved=bytes(p.profile_path());assert(fs::remove(t.path/"profile.json.bak"));assert(fs::create_directory(t.path/"profile.json.bak"));assert(!p.purchase(TalentId::Damage,0));assert(p.profile().crystals==before&&p.talent_rank(TalentId::Damage)==0&&bytes(p.profile_path())==saved);assert(fs::remove(t.path/"profile.json.bak"));assert(p.purchase(TalentId::Damage,0));assert(p.profile().crystals==before-8);
#ifdef _WIN32
 // OS-level competing process lock must fail safely, even within the same process.
 HANDLE lock=CreateFileW((t.path/L"profile.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);assert(lock!=INVALID_HANDLE_VALUE);before=p.profile().crystals;assert(!p.purchase(TalentId::Damage,1)&&p.profile().crystals==before);CloseHandle(lock);assert(p.purchase(TalentId::Damage,1));
#endif
}
void corrupt_recovery_preserves_backup(){
 TempDirectory t;funded(t.path,100);{ProgressionStore p(t.path);assert(p.purchase(TalentId::Damage,0));}const auto primary=t.path/"profile.json";const auto good_backup=bytes(t.path/"profile.json.bak");const std::string corrupt="{\"schema_version\":1,\"crystals\":";write_bytes(primary,corrupt);
 {
  ProgressionStore recovered(t.path);assert(!recovered.read_only()&&recovered.load_state()==ProfileLoadState::RecoveredBackup&&recovered.profile().crystals==100);ProfileSettings s;s.music=false;assert(recovered.save_settings(s));assert(bytes(t.path/"profile.json.bak")==good_backup);bool archive=false;for(auto& entry:fs::directory_iterator(t.path)){if(entry.path().filename().u8string().find("profile.json.corrupt") ==0){archive=true;assert(bytes(entry.path())==corrupt);}}assert(archive);ProgressionStore loaded(t.path);assert(loaded.read_only()&&loaded.load_state()==ProfileLoadState::Loaded&&loaded.profile().crystals==100&&!loaded.profile().settings.music);
 }
 write_bytes(primary,"broken-again");ProgressionStore second(t.path);assert(!second.read_only()&&second.load_state()==ProfileLoadState::RecoveredBackup&&second.profile().crystals==100);
 TempDirectory bad;write_bytes(bad.path/"profile.json","bad-main");write_bytes(bad.path/"profile.json.bak","bad-backup");ProgressionStore readonly(bad.path);assert(readonly.read_only()&&readonly.load_state()==ProfileLoadState::Corrupt);assert(!readonly.begin_run(CharacterClass::Archer,1)&&!readonly.save_settings({}));assert(bytes(bad.path/"profile.json")=="bad-main"&&bytes(bad.path/"profile.json.bak")=="bad-backup");
}
void oversized_primary_recovery(){
 TempDirectory t;funded(t.path,123);const auto primary=t.path/"profile.json",backup=t.path/"profile.json.bak";{ProgressionStore seed(t.path);assert(seed.save_settings({}));assert(seed.save_settings({}));}const auto good_backup=bytes(backup);std::string corrupt(4*1024*1024+127,'x');corrupt[123]='\0';write_bytes(primary,corrupt);
 {
  ProgressionStore recovered(t.path);assert(!recovered.read_only()&&recovered.load_state()==ProfileLoadState::RecoveredBackup&&recovered.profile().crystals==123);assert(bytes(primary)==corrupt&&bytes(backup)==good_backup);bool preserved=false;for(auto& entry:fs::directory_iterator(t.path))if(entry.path().filename().u8string().find("profile.json.corrupt")==0){preserved=true;assert(bytes(entry.path())==corrupt);}assert(preserved);
  ProfileSettings settings;settings.music=false;assert(recovered.save_settings(settings));assert(bytes(backup)==good_backup&&json::parse(bytes(primary))["crystals"]==123);
 }
 // Same length and timestamp must not hide an externally changed damaged primary.
 write_bytes(primary,corrupt);ProgressionStore stale(t.path);assert(!stale.read_only()&&stale.load_state()==ProfileLoadState::RecoveredBackup);auto original_time=fs::last_write_time(primary);corrupt[65540]='y';write_bytes(primary,corrupt);fs::last_write_time(primary,original_time);assert(!stale.save_settings({})&&stale.profile().crystals==123);assert(bytes(primary)==corrupt&&bytes(backup)==good_backup);assert(stale.load()&&!stale.read_only());assert(stale.save_settings({})&&bytes(backup)==good_backup);
 TempDirectory no_backup;write_bytes(no_backup.path/"profile.json",corrupt);ProgressionStore blocked(no_backup.path);assert(blocked.read_only()&&blocked.load_state()==ProfileLoadState::Corrupt&&!blocked.save_settings({}));assert(bytes(no_backup.path/"profile.json")==corrupt&&!fs::exists(no_backup.path/"profile.json.bak"));
}
void migration_and_future_version(){
 TempDirectory t;json old={{"version",0},{"crystals",50},{"talents",{{"damage",2},{"future_talent",3}}},{"settings",{{"music",false},{"controller_layout","southpaw"}}},{"unlocks",{{"classes",{"archer","mage","warrior"}},{"skills",{0,1,2}},{"equipment",{"kept_sword"}},{"relics",{"kept_relic"}}}},{"future_section",{{"value",99}}}};write_bytes(t.path/"profile.json",old.dump());ProgressionStore p(t.path);assert(p.load_state()==ProfileLoadState::Migrated&&p.profile().crystals==50&&p.talent_rank(TalentId::Damage)==2);assert(near_value(p.profile().settings.zoom,1.6f)&&near_value(p.profile().settings.sound_volume,.35f));assert(p.save_settings(p.profile().settings));auto after=json::parse(bytes(p.profile_path()));assert(after["schema_version"]==profile_schema_version&&after["future_section"]==old["future_section"]&&after["talents"]["future_talent"]==3&&after["settings"]["controller_layout"]=="southpaw"&&after["unlocks"]["equipment"][0]=="kept_sword");assert(json::parse(bytes(t.path/"profile.json.bak"))==old);
 auto future=after;future["schema_version"]=99;future["crystals"]=7777;auto text=future.dump();write_bytes(p.profile_path(),text);auto backup=bytes(t.path/"profile.json.bak");ProgressionStore next(t.path);assert(next.read_only()&&next.load_state()==ProfileLoadState::FutureVersion);assert(!next.begin_run(CharacterClass::Archer,7)&&!next.save_settings({})&&!next.purchase(TalentId::Damage,0));assert(bytes(p.profile_path())==text&&bytes(t.path/"profile.json.bak")==backup);
 TempDirectory future_backup;write_bytes(future_backup.path/"profile.json","bad");write_bytes(future_backup.path/"profile.json.bak",text);ProgressionStore blocked(future_backup.path);assert(blocked.read_only()&&blocked.load_state()==ProfileLoadState::FutureVersion&&bytes(future_backup.path/"profile.json.bak")==text);
}
void complete_schema_and_unbounded_future(){
 TempDirectory t;funded(t.path,123);{ProgressionStore p(t.path);assert(p.save_settings({}));assert(p.save_settings({}));}const auto primary=t.path/"profile.json";const auto full=json::parse(bytes(primary));const auto backup=bytes(t.path/"profile.json.bak");
 // Syntactically valid but incomplete v1 saves are corrupt, never silent zero-balance profiles.
 for(const char* key:{"schema_version","crystals","revision","next_run_serial","closed_through_serial","talents","settings","records","unlocks","history","active_run"}){auto partial=full;partial.erase(key);write_bytes(primary,partial.dump());ProgressionStore recovered(t.path);assert(!recovered.read_only()&&recovered.load_state()==ProfileLoadState::RecoveredBackup&&recovered.profile().crystals==123);assert(bytes(t.path/"profile.json.bak")==backup);}
 for(const auto& unversioned:std::array<json,2>{json::object(),json{{"crystals",123}}}){write_bytes(primary,unversioned.dump());ProgressionStore recovered(t.path);assert(recovered.load_state()==ProfileLoadState::RecoveredBackup&&recovered.profile().crystals==123);assert(bytes(t.path/"profile.json.bak")==backup);}
 for(const char* section:{"talents","records","unlocks"}){auto partial=full;partial[section]=json::object();write_bytes(primary,partial.dump());ProgressionStore recovered(t.path);assert(recovered.load_state()==ProfileLoadState::RecoveredBackup&&recovered.profile().crystals==123);}
 auto optional_settings=full;optional_settings["settings"].erase("zoom");optional_settings["settings"].erase("sound_volume");write_bytes(primary,optional_settings.dump());ProgressionStore defaults(t.path);assert(defaults.load_state()==ProfileLoadState::Loaded&&near_value(defaults.profile().settings.zoom,1.6f)&&near_value(defaults.profile().settings.sound_volume,.35f));
 for(uint64_t version:std::array<uint64_t,3>{1000001ULL,9223372036854775808ULL,UINT64_MAX}){auto future=full;future["schema_version"]=version;const auto original=future.dump();write_bytes(primary,original);ProgressionStore readonly(t.path);assert(readonly.read_only()&&readonly.load_state()==ProfileLoadState::FutureVersion&&!readonly.begin_run(CharacterClass::Archer,7));assert(bytes(primary)==original&&bytes(t.path/"profile.json.bak")==backup);}
}
void single_writer_and_interrupted_runs(){
 TempDirectory t;funded(t.path,100);std::unique_ptr<ProgressionStore> owner=std::make_unique<ProgressionStore>(t.path);auto first=owner->begin_run(CharacterClass::Archer,7);assert(first);ProgressionStore secondary(t.path);assert(secondary.read_only()&&secondary.profile().active_run->id==first.run_id);const auto saved=bytes(owner->profile_path());
 RunReport interrupted;interrupted.outcome=RunOutcome::Interrupted;interrupted.seed=7;
 assert(!secondary.begin_run(CharacterClass::Mage,8)&&!secondary.settle(first.run_id,interrupted)&&!secondary.settle(first.run_id,victory())&&!secondary.purchase(TalentId::Damage,0)&&!secondary.save_settings({})&&!secondary.save_chapter_checkpoint(first.run_id,nullptr));assert(bytes(owner->profile_path())==saved&&owner->profile().active_run->id==first.run_id&&owner->profile().crystals==100);
 auto reward=owner->settle(first.run_id,victory());assert(reward&&reward.progress_crystals_awarded==settlement_crystals(victory()));assert(secondary.load()&&secondary.read_only()&&!secondary.settle(first.run_id,victory()));auto second=owner->begin_run(CharacterClass::Mage,8);assert(second&&second.run_id==2);
 // Only releasing the original instance permits explicit reload to acquire ownership.
 owner.reset();assert(secondary.load()&&!secondary.read_only()&&secondary.profile().active_run->id==second.run_id);const auto before=secondary.profile().crystals;auto third=secondary.begin_run(CharacterClass::Warrior,9);assert(third&&third.run_id==3&&secondary.profile().crystals==before);assert(secondary.profile().history.back().run_id==second.run_id&&secondary.profile().history.back().report.outcome==RunOutcome::Interrupted&&secondary.profile().history.back().crystals==0);
}
void external_change_settlement_retry(){
 TempDirectory t;const auto primary=t.path/"profile.json";const auto report=victory();uint64_t id=0;int award=settlement_crystals(report);
 {
  ProgressionStore owner(t.path);auto begin=owner.begin_run(CharacterClass::Archer,7);assert(begin);id=begin.run_id;
  // External edits remain possible even with a game-to-game ownership lease.
  auto external=json::parse(bytes(primary));external["revision"]=owner.profile().revision+1;external["settings"]["music"]=false;write_bytes(primary,external.dump());
  auto stale=owner.settle(id,report);assert(!stale&&!stale.already_claimed&&owner.profile().crystals==0&&owner.profile().active_run->id==id&&owner.profile().history.empty());assert(owner.load()&&!owner.read_only()&&owner.profile().active_run->id==id&&!owner.profile().settings.music);
  auto mismatched=owner.settle(id+1,report);assert(!mismatched&&!mismatched.already_claimed&&owner.profile().active_run->id==id&&owner.profile().history.empty());auto paid=owner.settle(id,report);assert(paid&&!paid.already_claimed&&paid.progress_crystals_awarded==award&&owner.profile().crystals==paid.crystals_awarded&&owner.profile().history.size()==1&&owner.profile().records.runs==1);award=paid.crystals_awarded;const auto saved=bytes(primary);auto again=owner.settle(id,report);assert(again&&again.already_claimed&&again.crystals_awarded==0&&bytes(primary)==saved);
 }
 ProgressionStore restarted(t.path);assert(!restarted.read_only()&&restarted.settle(id,report).already_claimed&&restarted.profile().crystals==award&&restarted.profile().records.total_crystals==uint64_t(award)&&restarted.profile().history.size()==1);
}
void checkpoint_and_history(){
 TempDirectory t;uint64_t first_id=0;int64_t balance=0;
 {
  ProgressionStore p(t.path);auto first=p.begin_run(CharacterClass::Archer,7);assert(first);first_id=first.run_id;assert(!p.save_chapter_checkpoint(first_id,json{{"boundary","chapter"},{"run_id",first_id},{"next_chapter",2}}));assert(p.profile().chapter_checkpoint.is_null());assert(p.settle(first_id,victory()));
  for(int i=0;i<70;++i){auto run=p.begin_run(CharacterClass::Archer,7);RunReport r;r.seed=7;r.outcome=RunOutcome::Abandoned;assert(run&&p.settle(run.run_id,r));}assert(p.profile().history.size()==64);balance=p.profile().crystals;assert(p.settle(first_id,victory()).already_claimed&&p.profile().crystals==balance);
 }
 ProgressionStore last(t.path);assert(!last.read_only()&&last.profile().history.size()==64&&last.settle(first_id,victory()).already_claimed&&last.profile().crystals==balance);
 TempDirectory orphan;write_bytes(orphan.path/"profile.json.tmp.123.1",json{{"schema_version",1},{"crystals",999999}}.dump());ProgressionStore newgame(orphan.path);assert(newgame.load_state()==ProfileLoadState::NewProfile&&newgame.profile().crystals==0);
}
}
int main(){new_profile_and_settlement();talents_and_settings();reward_guards();failed_writes_are_transactional();corrupt_recovery_preserves_backup();oversized_primary_recovery();migration_and_future_version();complete_schema_and_unbounded_future();single_writer_and_interrupted_runs();external_change_settlement_retry();checkpoint_and_history();std::cout<<"progression: settlement, purchases, reload, corruption, future schema, atomic failures, lifetime ownership, external edit retries, oversized recovery, Unicode, settings and checkpoints passed (isolated temp profiles only)\n";}

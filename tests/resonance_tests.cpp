#include "game.hpp"
#include "progression.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <chrono>
using namespace maple;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
Game battle(Skill a,Skill b){
 Config c;c.enable_pressure=false;c.first_event_time=c.boss_time=10000;c.spawn_radius=2000;c.max_enemies=16;
 Game g(c);g.start(CharacterClass::Mage,20261112);g.skills.fill(0);g.skills[int(a)]=g.skills[int(b)]=3;g.enemies.clear();g.player.xp_next=1e9f;g.player.invulnerable=10000;
 for(int i=0;i<7;++i){float angle=i*6.2831853f/7;Enemy e;e.id=900+i;e.pos={std::cos(angle)*65,std::sin(angle)*65};e.hp=e.max_hp=100000;e.speed=0;e.damage=7;g.enemies.push_back(e);}return g;
}
void resonance_scenarios(const std::string& filter){
 for(const auto& rule:resonance_definitions()){
  if(filter=="new"?int(rule.id)<6:!filter.empty()&&filter!=rule.key)continue;
  Game g=battle(rule.first_example,rule.second_example);
  if(rule.id==Resonance::ShieldEcho||rule.id==Resonance::HolyShelter){g.player.invulnerable=0;for(auto& enemy:g.enemies){enemy.kind=EnemyKind::Boss;enemy.pos={15,0};enemy.speed=20;}}
  if(rule.id==Resonance::DashFlame){for(auto& enemy:g.enemies){enemy.kind=EnemyKind::Boss;enemy.pos={50,0};}}
  for(int i=0;i<600&&g.state==State::Playing;++i){Input input;if(rule.id==Resonance::DashFlame&&i==60){input.move={1,0};input.dash=true;}g.update(1.f/60,input);}
  int id=int(rule.id);std::cout<<rule.key<<" triggers="<<g.metrics.resonance.triggers[id]<<" damage="<<g.metrics.resonance.damage[id]<<" shield="<<g.metrics.resonance.shield[id]<<" support="<<g.metrics.resonance.support[id]<<" suppressed="<<g.metrics.resonance.suppressed[id]<<"\n";
  if(rule.id==Resonance::ShieldEcho)std::cout<<"shield scene: hp="<<g.player.hp<<" guard="<<g.guard_charge<<" invulnerable="<<g.player.invulnerable<<" taken="<<g.metrics.damage_taken<<" casts="<<g.pressure_metrics.successful_casts[int(Skill::Bulwark)]<<" pair="<<g.resonance_status(rule.id).usable<<" target_distance="<<length(g.enemies.front().pos-g.player.pos)<<"\n";
  check(g.scrolls.held.empty(),"base resonance needs no scroll");check(g.metrics.resonance.triggers[id]>0&&g.metrics.resonance.damage[id]+g.metrics.resonance.support[id]+g.metrics.resonance.shield[id]>0,"the equipped pair must produce actual damage or effective support");
  const auto report=make_run_report(g,RunOutcome::Abandoned);double total=g.metrics.other_damage;for(double d:g.metrics.skill_damage)total+=d;for(double d:g.metrics.resonance.damage)total+=d;check(std::abs(report.damage_dealt-total)<.01,"damage is counted once across primary and resonance contributions");
 }
 auto solo=battle(Skill::FrostStorm,Skill::FrostStorm);check(!solo.resonance_status(Resonance::FrostRelay).equipped,"one fusion with two tags is not two participants");
 auto pair=battle(Skill::Frost,Skill::Lightning);check(pair.resonance_status(Resonance::FrostRelay).usable,"two equipped participants are usable");pair.skills[int(Skill::Frost)]=0;check(!pair.resonance_status(Resonance::FrostRelay).equipped,"replacement removes qualification immediately");
 pair.skills[int(Skill::Frost)]=1;pair.pressure_event.kind=PressureKind::LastWords;pair.pressure_event.sealed=Skill::Frost;pair.pressure_event.remaining=10;check(!pair.resonance_status(Resonance::FrostRelay).usable,"sealing pauses new resonance attacks without deleting ownership");
 std::cout<<"PASS selected resonance scenarios, distinct participants, live suppression and exclusive damage totals.\n";
}
void step(Game& game,int count){for(int i=0;i<count&&game.state==State::Playing;++i){game.events.clear();game.update(1.f/60);for(const auto& event:game.events)if(event.resonance_id>=0)check(event.secondary&&event.root_attack_id>0&&event.actual_damage>0,"resonance damage keeps a valid root and secondary attribution");}}
void directions(){
 for(const auto& upgrade:resonance_upgrades()){
  const auto& rule=resonance_definitions()[int(upgrade.id)];Game g=battle(rule.first_example,rule.second_example);
  auto pool=g.available_upgrade_choices();auto found=std::find_if(pool.begin(),pool.end(),[&](const UpgradeChoice& c){return c.kind==UpgradeKind::Resonance&&c.resonance==upgrade.id&&c.direction==upgrade.direction;});check(found!=pool.end(),"eligible direction enters ordinary pool");
  g.state=State::Upgrade;g.choices={*found};check(g.choose_upgrade(0),"direction is chosen through ordinary upgrade");check(g.skip_upgrade(),"remaining choice can be skipped");
  if(upgrade.id==Resonance::ShieldEcho){g.player.invulnerable=0;for(auto& e:g.enemies){e.kind=EnemyKind::Boss;e.pos={15,0};e.speed=20;}}
  if(upgrade.id==Resonance::DashFlame){for(auto& e:g.enemies){e.kind=EnemyKind::Boss;e.pos={50,0};}step(g,60);Input dash;dash.move={1,0};dash.dash=true;g.update(1.f/60,dash);}
  step(g,600);int id=int(upgrade.id);check(g.metrics.resonance.triggers[id]>0,"each branch retains real battle behavior");check(g.metrics.resonance.damage[id]+g.metrics.resonance.shield[id]>0,"each branch contributes actual damage or shield");
  std::cout<<rule.key<<" direction="<<upgrade.direction<<" damage="<<g.metrics.resonance.damage[id]<<" shield="<<g.metrics.resonance.shield[id]<<"\n";
  pool=g.available_upgrade_choices();check(std::none_of(pool.begin(),pool.end(),[&](const UpgradeChoice& c){return c.kind==UpgradeKind::Resonance&&c.resonance==upgrade.id;}),"opposite direction cannot stack");
  g.skills[int(rule.first_example)]=0;check(!g.resonance_status(upgrade.id).usable&&g.resonance_directions[id]==upgrade.direction,"lost participant suspends, retains direction");g.skills[int(rule.first_example)]=3;check(g.resonance_status(upgrade.id).usable,"re-equipped participant restores direction eligibility");
 }
 auto g=battle(Skill::Frost,Skill::Lightning);g.player.xp_next=8;g.gems.push_back({g.player.pos,8});g.update(1.f/60);check(g.state==State::Upgrade,"ordinary level choices open");check(std::count_if(g.choices.begin(),g.choices.end(),[](const UpgradeChoice& c){return c.kind==UpgradeKind::Resonance;})<=1,"at most one resonance card");
 std::cout<<"PASS twelve authored direction scenarios, one-card limit and suspension.\n";
}
void compatibility(){
 auto blocked=battle(Skill::Frost,Skill::Lightning);for(auto& e:blocked.enemies)e.abilities.refraction=50;step(blocked,120);check(blocked.metrics.resonance.triggers[int(Resonance::FrostRelay)]==0,"fully refracted hits cannot trigger resonance");
 auto immune=battle(Skill::Frost,Skill::Lightning);for(auto& e:immune.enemies)e.abilities.promise_remaining=10;step(immune,120);check(immune.metrics.resonance.triggers[int(Resonance::FrostRelay)]==0,"full immunity grants no hit resonance");
 auto smoke=battle(Skill::Frost,Skill::Lightning);DangerZone zone;zone.kind=DangerKind::Smoke;zone.warning=0;zone.radius=150;zone.duration=10;check(smoke.add_danger_zone(zone),"smoke fixture uses normal zone creation");step(smoke,120);check(!smoke.resonance_status(Resonance::FrostRelay).usable&&smoke.metrics.resonance.triggers[0]==0,"smoke cannot create new resonance attacks");
 auto regression=battle(Skill::Frost,Skill::Lightning);regression.skills[int(Skill::Frost)]=1;zone.kind=DangerKind::Regression;check(regression.add_danger_zone(zone),"regression zone opens");step(regression,60);check(regression.skills[int(Skill::Frost)]==1&&regression.effective_skill_level(Skill::Frost)==0&&!regression.resonance_status(Resonance::FrostRelay).usable,"zero effective level suspends without deleting ranks");
 auto fused=battle(Skill::Frost,Skill::Lightning);step(fused,180);fused.resonance_directions[0]=1;fused.scrolls.held={{1,Skill::FrostStorm,0,false,true}};fused.scrolls.next_id=2;check(fused.confirm_scroll_fusion(1),"normal fusion consumes two participants");double old=fused.metrics.skill_damage[int(Skill::Frost)]+fused.metrics.skill_damage[int(Skill::Lightning)];int count=fused.metrics.resonance.triggers[0];step(fused,120);check(fused.metrics.resonance.triggers[0]==count&&!fused.resonance_status(Resonance::FrostRelay).equipped&&old==fused.metrics.skill_damage[int(Skill::Frost)]+fused.metrics.skill_damage[int(Skill::Lightning)],"fusion removes old hidden output and does not self-qualify");
 auto boss=battle(Skill::Frost,Skill::Lightning);boss.enemies.resize(1);boss.enemies[0].kind=EnemyKind::Boss;boss.enemies[0].pos={65,0};boss.enemies[0].speed=0;boss.enemies[0].attack_cooldown=100;const Vec2 origin=boss.enemies[0].pos;step(boss,600);check(length(boss.enemies[0].pos-origin)<.01f&&boss.metrics.resonance.triggers[0]>0,"Boss retains useful frost and permanent displacement immunity");
 std::cout<<"PASS smoke, regression, blocked hits, fusion cleanup, secondary attribution and Boss stability.\n";
}
void save_roundtrip(){
 namespace fs=std::filesystem;using nlohmann::json;auto root=fs::current_path()/"artifacts"/"resonance"/("save_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
 {
  ProgressionStore store(root);auto started=store.begin_run(CharacterClass::Mage,909,RunMode::Campaign);check(started.ok,"begin isolated campaign");Game game;game.start(CharacterClass::Mage,909,store.run_bonuses(),store.profile().active_run->setup);game.skills.fill(0);game.skills[int(Skill::Frost)]=game.skills[int(Skill::Lightning)]=3;game.resonance_directions[0]=2;game.metrics.resonance.triggers[0]=4;game.metrics.resonance.damage[0]=80;game.metrics.chapters_cleared=1;game.elapsed=120;game.state=State::ChapterComplete;
  RunBuild build;check(game.export_build(build),"chapter exports directions");auto result=store.claim_chapter(started.run_id,0,make_run_report(game,RunOutcome::Victory,909),build);check(result.ok,result.message.c_str());auto settings=store.profile().settings;settings.ui_volume=.42f;settings.ui_sound=false;check(store.save_settings(settings).ok,"save third audio group");
 }
 {
  ProgressionStore store(root);RunBuild build;check(!store.read_only()&&store.checkpoint_build(build),"saved chapter reloads");Game restored;check(restored.restore_build(build)&&restored.resonance_directions[0]==2&&restored.metrics.resonance.damage[0]==80,"direction and contribution persist");check(std::abs(store.profile().settings.ui_volume-.42f)<.001&&!store.profile().settings.ui_sound,"third audio group persists");
 }
 json legacy;{std::ifstream file(root/"profile.json");file>>legacy;}
 auto strip=[&](auto&& self,json& node)->void{if(node.is_object()){if(node.contains("resonance")&&node.contains("damage_dealt")){double damage=0;for(const auto& entry:node["resonance"].items())damage+=entry.value().value("damage",0.0);node["damage_dealt"]=node["damage_dealt"].get<double>()-damage;}node.erase("resonance");node.erase("resonance_directions");node.erase("ui_volume");node.erase("ui_sound");for(auto& item:node.items())self(self,item.value());}else if(node.is_array())for(auto& item:node)self(self,item);};strip(strip,legacy);{std::ofstream file(root/"profile.json");file<<legacy.dump(2);}
 {ProgressionStore store(root);RunBuild build;check(!store.read_only()&&store.checkpoint_build(build),"pre-Resonance v7 chapter still loads");check(build.resonance_directions[0]==0&&build.metrics.resonance.damage[0]==0&&store.profile().settings.ui_sound,"missing new fields use defaults");}
 std::cout<<"PASS isolated v7 compatibility and Resonance chapter/audio roundtrip.\n";
}

}
int main(int argc,char** argv){try{std::string group=argc>1?argv[1]:"new";if(group=="branches")directions();else if(group=="compat")compatibility();else if(group=="save")save_roundtrip();else resonance_scenarios(group);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

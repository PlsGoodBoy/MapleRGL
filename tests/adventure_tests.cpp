#include "adventure_content.hpp"
#include "encounter_pool.hpp"
#include "progression.hpp"
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace maple;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
Game fixture(){Config c;c.enable_pressure=false;c.first_event_time=c.boss_time=10000;c.max_enemies=12;Game g(c);g.start(CharacterClass::Mage,20261112);g.skills.fill(0);g.enemies.clear();g.player.xp_next=1e9f;g.player.invulnerable=10000;return g;}
void rank(Game& g,Skill s,int value=5){g.skills[int(s)]=value;}
void catalogue(){
 std::set<std::string> ids,keys;std::set<Skill> results[5];int bases=0;
 for(int i=0;i<skill_count;++i){check(keys.insert(Game::skill_key(Skill(i))).second,"unique skill keys");if(!Game::is_fusion(Skill(i)))++bases;}
 for(const auto& r:fusion_recipes()){
  check(ids.insert(r.id).second,"unique entry IDs");check(r.material_level==5,"all inputs require true rank five");check(r.sources.size()>=2&&r.sources.size()<=4,"source count");
  std::set<Skill> used,bases;for(Skill s:fusion_inputs(r)){check(used.insert(s).second,"no repeated instance");check(Game::skill_level_cap(s)>=5,"rank reachable");if(Game::is_fusion(s)){auto it=std::find_if(fusion_recipes().begin(),fusion_recipes().end(),[&](const FusionRecipe& p){return p.result==s;});check(it->sources.size()<r.sources.size(),"strict source growth");for(Skill base:it->sources)check(bases.insert(base).second,"no overlapping ancestry");}else check(bases.insert(s).second,"unique sources");}
  check(bases==std::set<Skill>(r.sources.begin(),r.sources.end()),"entry sources agree");results[r.sources.size()].insert(r.result);
 }
 check(bases==70&&results[3].size()>=12&&results[4].size()>=6,"formal content targets");check(gear_definitions().size()==40&&enemy_visual_count==60&&boss_profile_count==24,"pool counts");
 // One explicit staged witness per high-order result, without enumerating playthrough combinations.
 for(int tier:{3,4})for(Skill result:results[tier]){bool witnessed=false;for(const auto& r:fusion_recipes())if(r.result==result){auto inputs=fusion_inputs(r);if(inputs.size()!=2)continue;Game g=fixture();for(Skill s:inputs)rank(g,s);auto plan=fusion_plan(g.skills);g.auto_fuse_skills();if(g.skills[int(result)]){check(!plan.empty()&&plan.front()->result==result,"preview and arbitration");witnessed=true;break;}}check(witnessed,"high fusion has executable continuation");}
 std::cout<<"catalogue: "<<bases<<" base / "<<results[2].size()<<" two / "<<results[3].size()<<" three / "<<results[4].size()<<" four; "<<ids.size()<<" entries\n";
 std::ofstream doc("docs/FUSIONS.md");doc<<"# 多源融合图谱\n\n所有材料真实等级必须达到 5；结果等级按当前消耗项数为 Lv.3 / 4 / 5，最高 Lv.5。优先四源、三源、二源，同层 priority 降序、稳定配方 ID 升序。\n\n## 基础技能与覆盖\n\n| ID | 技能 | 获取 | 不同融合结果数 |\n|---|---|---|---|\n";
 for(int i=0;i<skill_count;++i)if(!Game::is_fusion(Skill(i))){std::set<Skill> routes;for(const auto& r:fusion_recipes())if(std::find(r.sources.begin(),r.sources.end(),Skill(i))!=r.sources.end())routes.insert(r.result);doc<<"| "<<Game::skill_key(Skill(i))<<" | "<<Game::skill_name(Skill(i))<<" | 公共五选二 / 满槽替换 | "<<routes.size()<<" |\n";check(routes.size()>=2,"base skill coverage");}
 for(int tier:{2,3,4}){doc<<"\n## "<<tier<<"源结果与允许入口\n\n";for(Skill s:results[tier]){doc<<"### "<<Game::skill_name(s)<<" (`"<<Game::skill_key(s)<<"`)\n\n"<<Game::skill_description(s)<<"\n\n";for(const auto& r:fusion_recipes())if(r.result==s){doc<<"- ";bool first=true;for(Skill input:fusion_inputs(r)){if(!first)doc<<" + ";doc<<Game::skill_name(input)<<" Lv.5";first=false;}doc<<" → Lv."<<fusion_rank(r)<<"\n";}}}
}
void transaction(){
 Game g=fixture();rank(g,Skill::TripleStar);rank(g,Skill::ShadowClone,4);g.auto_fuse_skills();check(!g.skills[int(Skill::ThousandStars)],"rank four cannot fuse");rank(g,Skill::ShadowClone);g.auto_fuse_skills();check(g.skills[int(Skill::ThousandStars)]==3,"pair growth");rank(g,Skill::ThousandStars);rank(g,Skill::Lightning);g.auto_fuse_skills();check(g.skills[int(Skill::ThunderShadow)]==3,"three-source continuation");rank(g,Skill::ThunderShadow);rank(g,Skill::PoisonDart);g.auto_fuse_skills();check(g.skills[int(Skill::ToxicThunder)]==3&&g.active_skill_count()==1,"four-source continuation");
 Game grouped=fixture();rank(grouped,Skill::SkySwarm);rank(grouped,Skill::DeepFleet);grouped.auto_fuse_skills();check(grouped.skills[int(Skill::SeaFortress)]==3,"AB plus CD");
 Game direct=fixture();for(Skill s:{Skill::Missiles,Skill::Engineer,Skill::Cannon,Skill::OctopusTurret})rank(direct,s);direct.auto_fuse_skills();check(direct.skills[int(Skill::SeaFortress)]==5&&direct.metrics.fusions_created==1,"four source wins shared candidates");
 Game choices=fixture();rank(choices,Skill::ThousandStars);rank(choices,Skill::Lightning,4);choices.state=State::Upgrade;choices.upgrade_resets_remaining=1;UpgradeChoice a{UpgradeKind::Skill,Skill::Lightning,"up",""};a.expected_rank=4;UpgradeChoice b{UpgradeKind::Skill,Skill::PoisonDart,"learn",""};b.expected_rank=0;choices.choices={a,b,{UpgradeKind::Recover,Skill::Count,"heal",""}};const auto preview=choices.fusion_preview(Skill::Lightning);check(preview==choices.fusion_preview(Skill::Lightning),"pure preview");check(choices.choose_upgrade(0)&&choices.upgrade_picks_taken==1&&choices.skills[int(Skill::ThunderShadow)],"first pick auto fusion retains second pick");check(choices.choose_upgrade(1)&&choices.skills[int(Skill::PoisonDart)]==1,"second pick uses live slots");
 rank(direct,Skill::Missiles);rank(direct,Skill::Engineer);rank(direct,Skill::Cannon);rank(direct,Skill::OctopusTurret);direct.auto_fuse_skills();check(direct.skills[int(Skill::SeaFortress)]==5,"same result is not consumed");
 Game cooldown=fixture();rank(cooldown,Skill::Nova);rank(cooldown,Skill::Retaliate);const float remaining=std::max(cooldown.skill_cooldown_remaining(Skill::Nova),cooldown.skill_cooldown_remaining(Skill::Retaliate));cooldown.auto_fuse_skills();check(cooldown.skill_cooldown_remaining(Skill::DragonArmor)>=remaining,"fusion retains material recovery instead of resetting it");
 rank(cooldown,Skill::DragonArmor);rank(cooldown,Skill::HolyShield);cooldown.auto_fuse_skills();check(cooldown.skills[int(Skill::DragonOath)]>0&&!cooldown.skills[int(Skill::DragonArmor)],"higher route consumes dragon armor");cooldown.shield_remaining=0;cooldown.dragon_soul=0;rank(cooldown,Skill::Nova);rank(cooldown,Skill::Retaliate);cooldown.auto_fuse_skills();check(cooldown.skills[int(Skill::DragonArmor)]>0&&cooldown.dragon_soul==0&&cooldown.shield_remaining==0,"recrafting does not repeat first-creation soul or shield");
 const auto diagnostics=direct.upgrade_pool_diagnostics();check(diagnostics.find("automatic_fusion_only")!=std::string::npos&&diagnostics.find("maximum_rank")!=std::string::npos,"candidate diagnostics explain actual exclusions without changing rolls");
 std::cout<<"transactions: 5-level threshold, sequential chain, pair-pair, priority, preview and picks passed\n";
}
void combat(){
 for(Skill skill:{Skill::TripleStar,Skill::GunFeast,Skill::ThunderShadow,Skill::ToxicThunder,Skill::MechanicalSiege,Skill::SeaFortress,Skill::ColdPrison,Skill::DragonOath,Skill::FrostFlame,Skill::HolyPhoenix,Skill::NightBanquet,Skill::VenomLock,Skill::TidalFist,Skill::PolarDragon,Skill::DragonOrrery,Skill::StarfallPrison,Skill::DragonFistArmor,Skill::StarRain,Skill::ElementSanctum,Skill::EternalMask}){
  Game g=fixture();rank(g,skill,3);for(int i=0;i<6;++i){Enemy e;e.id=800+i;e.pos={70.f+25*(i%3),float(i/3)*30};e.hp=e.max_hp=100000;e.radius=20;e.speed=0;g.enemies.push_back(e);}for(int i=0;i<600;++i)g.update(1.f/60);check(g.metrics.skill_damage[int(skill)]>0,"representative attack deals attributed damage");
 }
 std::cout<<"combat: representative base, two-source and high results deal attributed damage\n";
}
void remaining_content(){
 // One short combat sample per authored rule; no skill/equipment/Boss cross product.
 for(int i=12;i<resonance_count+2;++i){
  Game g=fixture();
  if(i<resonance_count){const auto& r=resonance_definitions()[i];rank(g,r.first_example,3);rank(g,r.second_example,3);}
  else rank(g,i==resonance_count?Skill::BreachFireweb:Skill::PirateAssault,3);
  for(int n=0;n<6;++n){Enemy e;e.id=900+n;e.pos={65.f+20*(n%3),float(n/3)*25};e.hp=e.max_hp=100000;e.radius=20;e.speed=0;g.enemies.push_back(e);}
  for(int step=0;step<900;++step)g.update(1.f/60);
  if(i<resonance_count){std::cout<<resonance_definitions()[i].key<<": "<<g.metrics.resonance.triggers[i]<<" triggers\n";check(g.metrics.resonance.triggers[i]>0,"new resonance fires in representative combat");}
  else check(g.metrics.skill_damage[int(i==resonance_count?Skill::BreachFireweb:Skill::PirateAssault)]>0,"remaining high result deals damage");
 }
 std::cout<<"remaining content: eight new resonances and two remaining high results passed\n";
}
void gear_and_time(){
 Game g=fixture();g.player.hp=40;g.gain_gear("maple_badge",2,250);g.gain_gear("adventurer_cape",2,50);check(g.gear_count()==300&&g.player.hp==40&&std::isfinite(g.damage_multiplier),"equipment aggregation without free heal");float power=g.damage_multiplier;g.config.enable_pressure=true;check(g.begin_pressure_event(PressureKind::Disarm),"disarm recognizes run gear");g.update(.1f);g.gain_gear("maple_badge",1,1);check(g.gear_count()==301&&g.damage_multiplier<power,"pickup during suppression");g.end_pressure_event();g.update(.1f);check(g.damage_multiplier>=power&&g.gear_count()==301,"suppression restores contribution only");
 for(int speed:{1,2}){Game clock=fixture();clock.timing.speed=speed;clock.config.max_enemies=0;for(int i=0;i<60*10;++i)clock.update(1.f/60);check(std::abs(clock.elapsed-speed*10.f)<.03,"encounter pace");check(std::abs(clock.combat_elapsed-10.f)<.03,"normal combat time");clock.elapsed=1199.98;clock.update(.04f);check(clock.state==State::Victory&&!clock.boss_defeated,"time success not boss kill");}
 Game normal;normal.start();check(normal.config.duration==1200&&normal.scheduled_boss_count()==4,"twenty minute classic");RunSetup setup;setup.mode=RunMode::Campaign;normal.start(CharacterClass::Mage,1,{},setup);check(normal.config.duration==600&&normal.scheduled_boss_count()==1,"campaign duration retained");
 std::cout<<"equipment and clocks: aggregation, suppression, 1x/2x, time victory and mode scope passed\n";
}
void save_and_status(){
 const auto dir=std::filesystem::path("artifacts/adventure")/("save-check-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
 ProgressionStore store(dir);auto started=store.begin_run(CharacterClass::Mage,19,RunMode::Campaign);check(bool(started),"isolated run begins");Game g;g.start(CharacterClass::Mage,19,store.run_bonuses(),store.profile().active_run->setup);
 g.skills.fill(0);rank(g,Skill::TripleStar);rank(g,Skill::ShadowClone);g.auto_fuse_skills();rank(g,Skill::ThousandStars);rank(g,Skill::Lightning);g.auto_fuse_skills();g.gain_gear("maple_badge",2,150);g.gain_gear("life_thread",1,8);
 g.elapsed=120;g.kills=1;g.boss_defeated=true;g.metrics.enemy_kills_by_kind.fill(0);g.metrics.enemy_kills_by_kind[int(EnemyKind::Boss)]=1;g.metrics.bosses_defeated=1;g.metrics.chapters_cleared=1;g.metrics.boss_spawn_time=80;g.metrics.boss_defeat_time=120;g.state=State::ChapterComplete;
 g.metrics.content.bosses_seen=1;g.metrics.content.boss_encounters[0]=1;g.metrics.content.boss_kills[0]=1;g.metrics.content.boss_moves_seen[0]=5;
 RunBuild build;check(g.export_build(build),"export boundary");auto report=make_run_report(g,RunOutcome::Victory,19);auto saved=store.claim_chapter(started.run_id,0,report,build);check(bool(saved),"save actual JSON boundary");
 check(store.profile().lifetime_content.boss_encounters[0]==1&&store.profile().lifetime_content.boss_kills[0]==1&&store.profile().lifetime_content.boss_moves_seen[0]==5,"actual Boss encounters and observed moves accumulated");check(store.claim_chapter(started.run_id,0,report,build).already_claimed&&store.profile().lifetime_content.boss_kills[0]==1,"duplicate chapter claim does not duplicate Boss statistics");
 auto settings=store.profile().settings;settings.flash_strength=.3f;settings.ui_scale=1.1f;settings.guidance=false;settings.guide_seen=0x41;settings.boss_hints_seen=(1u<<23)|1;check(bool(store.save_settings(settings)),"display and guidance settings save");
 ProgressionStore reopened(dir);RunBuild restored;check(reopened.checkpoint_build(restored),"reopen saved boundary");Game resumed;check(resumed.restore_build(restored),"restore saved build");check(resumed.skills==g.skills&&resumed.gear_count()==158,"skills and unlimited equipment roundtrip");
 check(restored.metrics.content.boss_encounters[0]==1&&restored.metrics.content.boss_moves_seen[0]==5&&reopened.profile().lifetime_content.boss_kills[0]==1,"stable Boss statistics survive actual JSON reopening");const auto& kept=reopened.profile().settings;check(std::abs(kept.flash_strength-.3f)<.001&&std::abs(kept.ui_scale-1.1f)<.001&&!kept.guidance&&kept.guide_seen==0x41&&kept.boss_hints_seen==((1u<<23)|1),"display and stable guidance IDs roundtrip");
 auto lineage=std::find_if(resumed.scrolls.completed.begin(),resumed.scrolls.completed.end(),[](const ScrollOffer& o){return o.result==Skill::ThunderShadow;});check(lineage!=resumed.scrolls.completed.end()&&!lineage->recipe_id.empty()&&lineage->investment>=15,"recipe identity and investment roundtrip");
 Game status=fixture();status.config.enable_pressure=true;rank(status,Skill::Frost);rank(status,Skill::Lightning);check(status.begin_pressure_event(PressureKind::LastWords),"seal fixture");status.pressure_event.sealed=Skill::Frost;status.pressure_event.remaining=9;status.auto_fuse_skills();check(status.skill_sealed(Skill::FrostStorm)&&status.pressure_event.remaining==9,"seal transferred without reset");
 Game regressed=fixture();rank(regressed,Skill::Frost);rank(regressed,Skill::Lightning);DangerZone zone;zone.kind=DangerKind::Regression;zone.pos=regressed.player.pos;zone.radius=90;zone.warning=0;zone.duration=12;check(regressed.add_danger_zone(zone),"legal regression fixture");regressed.update(.02f);regressed.auto_fuse_skills();regressed.update(.02f);check(regressed.skills[int(Skill::FrostStorm)]==3&&regressed.effective_skill_level(Skill::FrostStorm)==2,"qualification uses true rank and result remains regressed");
 Game smoked=fixture();rank(smoked,Skill::Frost);rank(smoked,Skill::Lightning);zone.kind=DangerKind::Smoke;check(smoked.add_danger_zone(zone),"legal smoke fixture");smoked.auto_fuse_skills();smoked.update(.02f);check(smoked.skills[int(Skill::FrostStorm)]==3&&smoked.metrics.skill_damage[int(Skill::FrostStorm)]==0,"smoke permits construction without free damage");
 auto legacy=build;legacy.run_gear.clear();legacy.skills.fill(0);legacy.skills[int(Skill::FrostStorm)]=2;legacy.consumed_skills=(uint64_t(1)<<int(Skill::Frost))|(uint64_t(1)<<int(Skill::Lightning));legacy.scrolls={};legacy.scrolls.completed.push_back({Skill::FrostStorm,0});legacy.metrics.fusions_created=1;legacy.metrics.first_fusion_time=0;legacy.timing.rules_version=1;legacy.hp=100;Game old;check(old.restore_build(legacy)&&old.skills[int(Skill::FrostStorm)]==2,"old fusion kept without recipe history or new gear");
 std::cout<<"save/status: real JSON roundtrip, lineage, 158 gear, legacy fusion, seal and regression passed\n";
}
}
int main(int argc,char** argv){try{std::string part=argc>1?argv[1]:"all";if(part=="all"){catalogue();transaction();combat();}if(part=="all"||part=="content")remaining_content();if(part=="all"||part=="gear")gear_and_time();if(part=="all"||part=="save")save_and_status();std::cout<<"Adventure directed checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

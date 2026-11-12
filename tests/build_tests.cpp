#include "game.hpp"
#include "simulation_clock.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace maple;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b,double tolerance=.05){return std::abs(a-b)<=tolerance;}
Game fixture(int pace=1){Config c;c.enable_pressure=false;c.first_event_time=10000;c.max_enemies=2;c.boss_time=10000;c.spawn_radius=2000;Game g(c);g.start(CharacterClass::Mage,20261112);g.timing.speed=pace;g.player.invulnerable=10000;g.enemies.clear();return g;}
void ticks(Game& g,int count,Input input={}){for(int i=0;i<count&&g.state==State::Playing;++i)g.update(1.f/60,input);}
void time_checks(){
 SimulationClock clock;int calls=0;for(int i=0;i<60;++i)clock.advance(1.0/60,2,[&](float){++calls;return true;});check(calls==60,"pace must not accelerate the fixed combat clock");
 auto normal=fixture(),compact=fixture(2);Enemy target;target.id=900;target.pos={300,0};target.hp=target.max_hp=100000;target.speed=20;
 normal.enemies.push_back(target);compact.enemies.push_back(target);ticks(normal,60,{{0,1}});ticks(compact,60,{{0,1}});
 check(near(normal.player.pos.y,compact.player.pos.y)&&near(normal.enemies.front().pos.y,compact.enemies.front().pos.y),"player and enemy motion are combat-time based");
 check(near(normal.skill_cooldown_remaining(Skill::Homing),compact.skill_cooldown_remaining(Skill::Homing)),"skill cooldown stays at normal speed");
 check(near(normal.elapsed*2,compact.elapsed)&&near(normal.combat_elapsed,compact.combat_elapsed),"two distinct clocks");
 compact.toggle_pause();const double progress=compact.elapsed,combat=compact.combat_elapsed;compact.update(.2f);check(compact.elapsed==progress&&compact.combat_elapsed==combat,"pause freezes both clocks");
 compact.toggle_pause();compact.timing.speed=1;ticks(compact,60);check(near(compact.elapsed,progress+1)&&near(compact.combat_elapsed,combat+1),"pace change preserves progress");
 for(int pace:{1,2}){
  auto g=fixture(pace);g.config.boss_time=510;g.skills.fill(0);g.config.max_enemies=8;for(int i=0;i<36100&&g.state==State::Playing;++i)g.update(1.f/60);
  check(g.state==State::Victory&&near(g.elapsed,600)&&near(g.timing.combat_seconds,600.0/pace,.2),"classic reaches its timed endpoint with a living Boss");
  check(g.boss_spawned&&!g.boss_defeated,"survival does not invent a Boss kill");
  std::cout<<"endpoint pace="<<pace<<" combat="<<g.timing.combat_seconds<<" encounter="<<g.elapsed<<" planned="<<g.pressure_metrics.normal_spawn_budget<<" actual="<<g.pressure_metrics.actual_spawns<<"\n";
 }
 auto mixed=fixture();mixed.skills.fill(0);ticks(mixed,120*60);mixed.timing.speed=2;ticks(mixed,241*60);check(mixed.state==State::Victory&&near(mixed.timing.combat_seconds,360,.05),"120 seconds standard plus the remainder compact takes 360 combat seconds");
 auto early=fixture(2);early.boss_spawned=early.boss_defeated=true;ticks(early,60);check(early.state==State::Playing,"early Boss kill keeps timed battle running");
 for(int pace:{1,2}){
  auto g=fixture(pace);g.config.max_enemies=64;g.config.enable_pressure=true;g.config.pressure_first_min=g.config.pressure_first_max=10000;g.skills.fill(0);
  check(g.begin_pressure_event(PressureKind::Invasion),"invasion starts");ticks(g,24*60/pace);check(g.pressure_event.waves==3,"all three invasion batches fit the compressed window");
  g.end_pressure_event();check(g.begin_pressure_event(PressureKind::Frenzy),"global event starts");ticks(g,15*60/pace);check(near(g.pressure_event.remaining,15,.1),"global event window uses encounter time");
  g.end_pressure_event();g.skills[int(Skill::Homing)]=1;g.enemies.clear();Enemy e=target;e.pos={70,0};e.hp=e.max_hp=100;e.speed=0;e.abilities.mask=1u<<int(EnemyAbility::Promise);g.enemies.push_back(e);
  Projectile p;p.skill=Skill::Homing;p.pos=e.pos;p.velocity={1,0};p.damage=10000;g.projectiles.push_back(p);ticks(g,1);check(g.enemies.front().abilities.pending_death,"lethal hit starts Promise");
  ticks(g,174);check(g.kills==0,"Promise remains for 2.9 combat seconds in both paces");ticks(g,8);check(g.kills==1,"Promise commits death after three combat seconds");
 }
}
std::string offer_key(const Game& g){std::string key;for(const auto& c:g.choices)key+=std::to_string(int(c.kind))+":"+std::to_string(int(c.skill))+":"+std::to_string(c.expected_rank)+":"+std::to_string(c.selected)+";";return key;}
UpgradeChoice card(UpgradeKind kind,Skill skill=Skill::Count,int rank=-1){UpgradeChoice c;c.kind=kind;c.skill=skill;c.expected_rank=rank;return c;}
void level_up(Game& g,float xp=8){g.gems.push_back({g.player.pos,xp});ticks(g,1);check(g.state==State::Upgrade,"pickup enters a real upgrade round");}
size_t first_available(const Game& g){for(size_t i=0;i<g.choices.size();++i)if(g.upgrade_choice_available(g.choices[i]))return i;throw std::runtime_error("expected an available choice");}
void choice_checks(){
 auto full=fixture();full.skills.fill(0);for(Skill skill:{Skill::Homing,Skill::Zone,Skill::Meteor,Skill::Rune,Skill::PoisonMist,Skill::IceLance,Skill::HolyLight,Skill::FrostStorm})full.skills[int(skill)]=1;
 full.config.enable_pressure=true;check(full.begin_pressure_event(PressureKind::LastWords),"seal starts before the upgrade");full.pressure_event.sealed=Skill::Rune;
 Effect old;old.kind=EffectKind::Rune;old.source=Skill::Rune;old.duration=30;old.damage=3;full.effects.push_back(old);level_up(full);
 check(full.choices.size()==5,"five candidates at eight occupied slots");bool fresh=false;for(const auto& c:full.choices){check(full.upgrade_choice_available(c),"each displayed candidate is currently legal");fresh|=(c.kind==UpgradeKind::Skill||c.kind==UpgradeKind::Study)&&full.skills[int(c.skill)]==0;}check(fresh,"full slots still offer new skills");
 full.choices={card(UpgradeKind::Skill,Skill::FireBeast,0),card(UpgradeKind::Skill,Skill::HolyDragon,0),card(UpgradeKind::Skill,Skill::Homing,1),card(UpgradeKind::Damage,Skill::Count,0),card(UpgradeKind::Area,Skill::Count,0)};
 const auto original=offer_key(full);check(full.choose_upgrade(0)&&full.pending_study.active&&full.upgrade_picks_taken==0,"full new skill opens preview without charging a pick");full.cancel_study_replacement();check(offer_key(full)==original&&full.upgrade_picks_taken==0,"cancel retains the offer and remaining picks");
 check(full.choose_upgrade(0)&&full.select_study_replacement(Skill::Rune)&&full.confirm_study_replacement(),"first replacement commits");check(full.state==State::Upgrade&&full.upgrade_picks_taken==1&&full.skills[int(Skill::FireBeast)]==1&&full.skill_sealed(Skill::FireBeast),"first choice applies and inherits the seal");
 check(std::none_of(full.effects.begin(),full.effects.end(),[](const Effect& e){return e.source==Skill::Rune;}),"replaced skill leaves no sustained output");check(!full.choose_upgrade(0),"same card cannot be selected twice");
 check(full.choose_upgrade(1)&&full.select_study_replacement(Skill::FrostStorm)&&full.confirm_study_replacement(),"second new skill may replace a fusion");check(full.state==State::Playing&&full.active_skill_count()==8&&full.metrics.upgrade_choices==2,"exactly two choices close one level");
 auto queue=fixture();level_up(queue,40.01f);check(queue.player.level==4&&queue.pending_upgrade_levels()==3,"three levels queued separately");
 for(int round=0;round<3;++round){check(queue.choices.size()==5&&queue.upgrade_picks_taken==0,"next level receives a new five-card round");check(queue.choose_upgrade(first_available(queue))&&queue.state==State::Upgrade&&queue.upgrade_picks_taken==1,"first selection keeps the round paused");check(queue.choose_upgrade(first_available(queue)),"second selection commits");}
 check(queue.state==State::Playing&&queue.metrics.upgrade_choices==6,"three levels award six choices without extra experience");
 auto fusion=fixture();fusion.skills.fill(0);for(Skill skill:{Skill::Homing,Skill::Rune,Skill::Lightning,Skill::Frost,Skill::Meteor,Skill::Zone,Skill::PoisonMist,Skill::IceLance})fusion.skills[int(skill)]=1;
 fusion.skills[int(Skill::Rune)]=2;level_up(fusion);
 fusion.choices={card(UpgradeKind::Skill,Skill::Homing,1),card(UpgradeKind::Skill,Skill::HolyLight,0),card(UpgradeKind::Skill,Skill::Rune,2),card(UpgradeKind::Damage,Skill::Count,0),card(UpgradeKind::Area,Skill::Count,0)};
 check(fusion.choose_upgrade(0)&&fusion.active_skill_count()==7&&fusion.skills[int(Skill::ArcaneRing)]==1&&fusion.upgrade_picks_taken==1,"material upgrade auto-fuses without a scroll or extra pick");check((fusion.choices[2].skill!=Skill::Rune||fusion.choices[2].expected_rank==0)&&fusion.choices[3].kind==UpgradeKind::Damage&&fusion.choices[4].kind==UpgradeKind::Area,"only the invalid material card is replaced");
 check(fusion.choose_upgrade(1)&&!fusion.pending_study.active&&fusion.active_skill_count()==8&&fusion.state==State::Playing,"second learning uses the slot freed by fusion");
 std::cout<<"PASS Build M2: five distinct choices, two commits, cancellation, fusion replacement, seal transfer and queued levels.\n";
}
void auto_fusion_checks(){
 auto game=fixture();game.skills.fill(0);game.skills[int(Skill::Frost)]=2;game.skills[int(Skill::Lightning)]=1;
 game.config.enable_pressure=true;check(game.begin_pressure_event(PressureKind::LastWords),"seal can start");game.pressure_event.sealed=Skill::Frost;
 Effect old;old.source=Skill::Frost;old.duration=30;game.effects.push_back(old);level_up(game);
 game.choices[0]=card(UpgradeKind::Skill,Skill::Lightning,1);check(game.choose_upgrade(0),"last material reaches fusion level");
 check(game.scrolls.held.empty()&&game.skills[int(Skill::FrostStorm)]==1&&game.skills[int(Skill::Frost)]==0&&game.skills[int(Skill::Lightning)]==0,"material levels alone trigger fusion");
 check(game.upgrade_picks_taken==1&&game.metrics.fusions_created==1&&game.skill_sealed(Skill::FrostStorm),"automatic fusion preserves the pick count and remaining seal");
 check(std::none_of(game.effects.begin(),game.effects.end(),[](const Effect& e){return e.source==Skill::Frost;}),"consumed materials leave no sustained output");
 check(std::any_of(game.events.begin(),game.events.end(),[](const GameEvent& e){return e.kind==EventKind::Fusion&&e.source==Skill::FrostStorm;}),"fusion feedback identifies its result");
 const int form=game.fusion_form(Skill::FrostStorm);game.auto_fuse_skills();check(game.metrics.fusions_created==1&&game.fusion_form(Skill::FrostStorm)==form,"existing fusion keeps its form and is not made twice");
 for(const auto& c:game.available_upgrade_choices())check(c.kind!=UpgradeKind::Fusion&&c.kind!=UpgradeKind::Study,"no separate fusion or scroll-study cards");
 auto shared=fixture();shared.skills.fill(0);shared.skills[int(Skill::Frost)]=shared.skills[int(Skill::BladeWave)]=2;shared.skills[int(Skill::Lightning)]=1;level_up(shared);
 shared.choices[0]=card(UpgradeKind::Skill,Skill::Lightning,1);check(shared.choose_upgrade(0)&&shared.skills[int(Skill::FrostStorm)]==1&&shared.skills[int(Skill::ThunderPrison)]==0&&shared.skills[int(Skill::BladeWave)]==2,"shared material follows catalogue order without consuming it twice");
 auto rewards=fixture();rewards.scrolls.failed_rewards=3;Enemy elite;elite.id=901;elite.kind=EnemyKind::Elite;elite.hp=0;elite.pos={1000,0};rewards.enemies.push_back(elite);ticks(rewards,1);
 check(rewards.scrolls.offers.empty()&&rewards.scrolls.pending_opportunities==0&&rewards.scrolls.opportunities==0,"elite rewards no longer open or queue scroll choices");
 Game chapter;RunSetup setup;setup.mode=RunMode::Campaign;chapter.start(CharacterClass::Mage,20261112,{},setup);chapter.skills[int(Skill::Rune)]=2;chapter.state=State::ChapterComplete;
 check(chapter.apply_chapter_reward(1)&&chapter.skills[int(Skill::ArcaneRing)]==1,"chapter skill reward auto-fuses at the same level requirement");
 std::cout<<"PASS automatic fusion: no scroll, material upgrade, free fusion, cleanup, seal, feedback, stable form, shared materials, reward flow and chapter reward.\n";
}
void staged_boss_checks(){
 std::array<int,3> first_plan{};
 for(int pace:{1,2}){
  auto game=fixture(pace);game.config.enable_director=true;game.config.max_enemies=8;game.config.boss_time=510;game.skills.fill(0);
  std::array<int,3> plan;for(int i=0;i<3;++i)plan[i]=int(game.planned_boss_profile(i));
  check(plan[0]!=plan[1]&&plan[0]!=plan[2]&&plan[1]!=plan[2],"three scheduled Bosses use distinct legal profiles");
  if(pace==1)first_plan=plan;else check(plan==first_plan,"Boss sequence is unchanged by encounter pace");
  std::array<double,3> spawned{};int seen=0;
  for(int i=0;i<36100&&game.state==State::Playing;++i){game.update(1.f/60);if(game.bosses_spawned()>seen){spawned[seen]=game.elapsed;++seen;}}
  check(seen==3&&near(spawned[0],204,.08)&&near(spawned[1],404,.08)&&near(spawned[2],514,.08),"director delivers all three milestones with its four-second arrival warning");
  check(game.state==State::Victory&&near(game.timing.combat_seconds,600.0/pace,.05)&&!game.boss_defeated,"living earlier Bosses neither block later arrivals nor extend the timed ending");
  check(std::count_if(game.enemies.begin(),game.enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss&&e.hp>0;})==3,"all three un-killed Bosses stay present");
  std::cout<<"Boss pace="<<pace<<" arrivals="<<spawned[0]<<","<<spawned[1]<<","<<spawned[2]<<" combat_end="<<game.timing.combat_seconds<<"\n";
 }
 auto kills=fixture();kills.config.boss_time=510;kills.skills.fill(0);
 for(double moment:{200.,400.,510.}){kills.elapsed=moment;kills.update(1.f/60);for(auto& enemy:kills.enemies)if(enemy.kind==EnemyKind::Boss)enemy.hp=0;kills.update(1.f/60);if(moment<510)check(!kills.boss_defeated,"mid-stage Boss kills do not claim the final Boss reward");}
 check(kills.metrics.bosses_defeated==3&&kills.boss_defeated&&kills.state==State::Playing,"three actual kills are counted and only the final kill sets final-Boss completion");
 for(int pace:{1,2}){
  auto game=fixture(pace);game.skills.fill(0);Enemy caster;caster.id=901;caster.pos={60,0};caster.hp=caster.max_hp=100000;caster.speed=0;caster.abilities.mask=1u<<int(EnemyAbility::Smoke);caster.abilities.field_cooldown=0;game.enemies.push_back(caster);ticks(game,1);
  check(game.danger_zones.size()==1&&near(game.danger_zones[0].duration-game.danger_zones[0].warning,10),"enemy smoke lasts ten combat seconds after warning");
  ticks(game,8*60);check(!game.danger_zones.empty(),"smoke persists beyond the old six-second effect at both paces");ticks(game,4*60);check(game.danger_zones.empty(),"smoke expires normally rather than lingering indefinitely");
 }
 std::cout<<"PASS staged Bosses and longer combat-time smoke.\n";
}
void pool_checks(){
 for(int starter=0;starter<3;++starter){
  Game g;g.start(CharacterClass(starter),20261112);
  check(g.active_skill_count()==1&&near(g.player.max_hp,110)&&near(g.player.speed,170),"starters share stats and grant exactly one initial skill");
  g.consumed_skills=Game::material_skill_mask();g.scrolls.foreign_class=(starter+1)%3;g.scrolls.learned_foreign=Game::material_skill_mask();
  int public_skills=0;for(int i=0;i<skill_count;++i)if(!Game::is_fusion(Skill(i))){check(g.can_gain_skill(Skill(i)),"all implemented base skills remain learnable despite legacy locks");++public_skills;}
  check(public_skills==42,"all 42 implemented base skills are public");
  for(const auto& recipe:fusion_recipes())check(g.scroll_reachable(recipe.result),"every released recipe is reachable from every starter");
  for(const auto& gear:equipment_definitions())if(gear.id!=EquipmentId::None)check(equipment_allowed(gear.id,gear.slot,CharacterClass(starter)),"gear follows its slot, not starter class");
 }
 std::cout<<"PASS Build M3: three equal-stat starters, 42 public skills, all released fusion routes, legacy locks ignored and unrestricted gear.\n";
}
void upgrade_controls_checks(){
 auto game=fixture();level_up(game,40.01f);const auto initial_skills=game.skills;const auto initial_passives=game.passives;
 check(game.upgrade_resets_remaining==1&&game.reroll_upgrade()&&game.upgrade_resets_remaining==0,"each level provides one free reset without talent");
 check(game.upgrade_picks_taken==0&&game.metrics.upgrade_choices==0&&!game.reroll_upgrade(),"reset does not select a card and the free charge is finite");
 check(game.skip_upgrade()&&game.pending_upgrade_levels()==2&&game.player.level==4&&game.skills==initial_skills&&game.passives==initial_passives,"skip all keeps the gained level and moves to exactly one next queued level");
 check(game.upgrade_resets_remaining==1,"the next level restores one free reset");
 game.choices={card(UpgradeKind::Skill,Skill::Homing,1),card(UpgradeKind::Skill,Skill::FireBeast,0),card(UpgradeKind::Area,Skill::Count,0),card(UpgradeKind::Damage,Skill::Count,0),card(UpgradeKind::MoveSpeed,Skill::Count,0)};
 check(game.choose_upgrade(3)&&game.reroll_upgrade(),"reset remains available after the first selection");
 check(game.choices.size()==5&&game.choices[3].selected&&game.choices[3].kind==UpgradeKind::Damage&&game.upgrade_picks_taken==1&&game.passives[int(UpgradeKind::Damage)]==1,"reset preserves the selected card position, applied effect and pick count");
 for(size_t i=0;i<game.choices.size();++i)if(i!=3){const auto& c=game.choices[i];check(game.upgrade_choice_available(c)&&c.skill!=Skill::Homing&&c.skill!=Skill::FireBeast&&c.kind!=UpgradeKind::Area&&c.kind!=UpgradeKind::MoveSpeed&&c.kind!=UpgradeKind::Damage,"unselected cards are replaced with different legal options");}
 check(game.skip_upgrade()&&game.pending_upgrade_levels()==1&&game.metrics.upgrade_choices==1&&game.passives[int(UpgradeKind::Damage)]==1,"skip the remaining pick retains exactly the first reward");
 check(game.skip_upgrade()&&game.state==State::Playing&&game.pending_upgrade_levels()==0&&game.metrics.upgrade_choices==1,"skipping the final queued level resumes combat without extra rewards");
 auto talent=fixture();RunBonuses bonuses;bonuses.upgrade_rerolls=1;talent.start(CharacterClass::Mage,20261112,bonuses);level_up(talent);
 check(talent.reroll_upgrade()&&talent.upgrade_rerolls_remaining==1&&talent.metrics.upgrade_rerolls_used==0,"free reset is spent before the run's talent charge");
 check(talent.reroll_upgrade()&&talent.upgrade_rerolls_remaining==0&&talent.metrics.upgrade_rerolls_used==1,"talent charge remains an additional reset and retains its save accounting");
 std::cout<<"PASS Build upgrade controls: skip zero/one choices, queued levels, one free reset per level, retained selection and extra talent reset.\n";
}

void book_checks(){
 auto g=fixture();g.scrolls.held={{1,Skill::ArcaneRing,0,false},{2,Skill::FrostStorm,0,false},{3,Skill::ThunderPrison,0,false}};g.scrolls.next_id=4;
 check(g.set_scroll_tracking(1,true)&&g.set_scroll_tracking(2,true)&&!g.set_scroll_tracking(3,true),"knowledge is uncapped while tracking is limited to two");
 check(g.set_scroll_tracking(1,false)&&g.set_scroll_tracking(3,true)&&g.scrolls.held.size()==3,"switching tracking retains every known recipe");
 for(int i=0;i<skill_count;++i)if(!Game::is_fusion(Skill(i)))check(g.can_gain_skill(Skill(i)),"tracked routes never whitelist the public skill pool");
 g.skills[int(Skill::Homing)]=g.skills[int(Skill::Rune)]=2;check(g.confirm_scroll_fusion(1)&&g.held_scroll(Skill::ArcaneRing)&&g.can_gain_skill(Skill::Homing),"fusion retains knowledge and lets consumed material return");
 g.skills[int(Skill::ArcaneRing)]=0;g.skills[int(Skill::Homing)]=g.skills[int(Skill::Rune)]=2;check(g.confirm_scroll_fusion(1)&&g.metrics.fusions_created==2&&g.scrolls.completed.size()==1,"remaking requires fresh materials and retains one completed-route record");
 auto study=fixture();study.scrolls.held={{1,Skill::ArcaneRing,0,false,true}};study.scrolls.next_id=2;level_up(study);
 auto choice=card(UpgradeKind::Study,Skill::Rune,0);choice.scroll_id=1;study.choices[0]=choice;check(study.choose_upgrade(0)&&study.scrolls.studies_used==1&&study.upgrade_picks_taken==1,"study costs exactly one of the two selections");study.choose_upgrade(first_available(study));
 check(study.set_scroll_tracking(1,false)&&study.set_scroll_tracking(1,true)&&study.scrolls.held[0].study_used,"retracking never refreshes study");
 auto reward=fixture();reward.scrolls.failed_rewards=3;Enemy e;e.id=901;e.kind=EnemyKind::Elite;e.hp=0;e.pos=reward.player.pos;reward.enemies.push_back(e);ticks(reward,1);
 check(reward.scrolls.eligible_rewards==1&&reward.scrolls.opportunities==1&&reward.scrolls.failed_rewards==0,"the fourth qualified reward is guaranteed after three misses");
 check(near(reward.metrics.xp_acquired,30),"elite kill XP is increased once from 25 to 30");
 auto twice=fixture(2);e.id=902;twice.enemies.push_back(e);ticks(twice,1);check(near(twice.metrics.xp_acquired,reward.metrics.xp_acquired),"the same kill yields the same XP at both paces");
 DirectorConfig dc;dc.first_encounter=0;for(auto& rule:dc.rules)rule.weight=rule.kind==EncounterKind::EliteHunt?1.f:0.f;EncounterDirector director(dc);DirectorInput input;input.elapsed=300;input.max_enemies=1000;int hunts=0,elites=0;
 for(int i=0;i<180&&hunts<2;++i){auto out=director.update(1,input);for(const auto& spawn:out.spawns)if(spawn.role==DirectorEnemyRole::Elite)++elites;if(out.ended_kind==EncounterKind::EliteHunt)++hunts;}
 check(hunts==2&&elites==3,"two ordinary hunts provide three elite entries instead of two");
 std::cout<<"PASS Build M4: recipe knowledge, tracking, repeat fusion, one-time study, fourth-reward guarantee, equal kill XP and 50% more regular elite entries.\n";
}

}
int main(int argc,char** argv){try{const std::string group=argc>1?argv[1]:"all";if(group=="all"||group=="time"){time_checks();std::cout<<"PASS Build M1: combat/encounter clocks, timed endings, pause/switch, event batches and Promise.\n";}if(group=="all"||group=="choices")choice_checks();if(group=="all"||group=="upgrade_controls")upgrade_controls_checks();if(group=="all"||group=="auto_fusion")auto_fusion_checks();if(group=="all"||group=="staged_bosses")staged_boss_checks();if(group=="all"||group=="pool")pool_checks();if(group=="all"||group=="book")book_checks();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

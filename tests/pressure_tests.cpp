#include "game.hpp"
#include "simulation_clock.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace maple;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool near(float a,float b,float epsilon=.02f){return std::abs(a-b)<epsilon;}
Game fixture(){Config c;c.duration=c.boss_time=c.first_event_time=c.elite_interval=10000;c.pressure_first_min=c.pressure_first_max=10000;c.item_drop_chance=0;Game g(c);RunSetup setup;setup.equipment={EquipmentId::ApprenticeWand,EquipmentId::ScholarRobe,EquipmentId::AmberPendant};g.start(CharacterClass::Mage,20261112,{},setup);g.enemies.clear();g.config.spawn_radius=2000;g.skills.fill(0);g.player.invulnerable=10000;return g;}
Enemy foe(uint32_t id,Vec2 pos={70,0}){Enemy e;e.id=id;e.pos=pos;e.hp=e.max_hp=1000;e.speed=0;e.damage=1;return e;}
void tick(Game& g,float seconds){for(int i=0;i<int(std::round(seconds*60));++i)g.update(1.f/60);}
void hit(Game& g,uint32_t id,float damage){auto it=std::find_if(g.enemies.begin(),g.enemies.end(),[&](const Enemy& e){return e.id==id;});check(it!=g.enemies.end(),"fixture target exists");Projectile p;p.skill=Skill::Homing;p.pos=it->pos;p.damage=damage;p.velocity={1,0};g.projectiles.push_back(p);g.update(1.f/60);}
Enemy& get(Game& g,uint32_t id){for(auto& e:g.enemies)if(e.id==id)return e;throw std::runtime_error("expected live enemy");}
DangerZone zone(DangerKind kind,Vec2 pos={}){DangerZone z;z.kind=kind;z.pos=pos;z.radius=100;z.warning=0;z.duration=30;return z;}
}
int main(){try{
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=1;auto b=foe(900);b.kind=EnemyKind::Boss;b.speed=25;b.abilities.native_stability=true;g.enemies.push_back(b);hit(g,900,10);
  check(get(g,900).hp<1000&&length(get(g,900).velocity)<.001f&&length(get(g,900).pos-b.pos)>0,"Boss takes damage without knockback");
  check(g.begin_pressure_event(PressureKind::Frenzy),"frenzy starts");g.end_pressure_event();check(g.displacement_immune(get(g,900)),"ending frenzy preserves Boss/native stability");
 }
 {
  auto g=fixture();auto e=foe(900);e.max_hp=100;e.hp=50;e.speed=20;e.damage=8;g.enemies.push_back(e);check(g.begin_pressure_event(PressureKind::DarkTide),"dark tide starts");
  check(near(get(g,900).hp,150)&&near(get(g,900).max_hp,300)&&near(get(g,900).speed,60)&&near(get(g,900).damage,24),"dark tide four exact baselines");
  const size_t capacity=g.enemy_capacity();tick(g,1);check(capacity==g.config.max_enemies*3&&g.pressure_metrics.event_spawn_budget==g.pressure_metrics.normal_spawn_budget*3,"3x budget and capacity");
  get(g,900).hp=90;g.end_pressure_event();check(near(get(g,900).hp,30)&&near(get(g,900).max_hp,100)&&near(get(g,900).speed,20),"tide removal maps current health ratio");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=2;g.skills[int(Skill::Lightning)]=2;g.enemies.push_back(foe(900));tick(g,.4f);const float cd=g.skill_cooldown_remaining(Skill::Homing),total=g.skill_cooldown_total(Skill::Homing);
  check(g.begin_pressure_event(PressureKind::ThinAir),"thin air starts");check(near(g.skill_cooldown_remaining(Skill::Homing),cd*1.6f)&&near(g.skill_cooldown_remaining(Skill::Homing)/g.skill_cooldown_total(Skill::Homing),cd/total),"cooldown fraction preserved");
  tick(g,.1f);check(near(g.skill_cooldown_remaining(Skill::Homing),cd*1.6f-.1f),"cooldown slows once");g.end_pressure_event();
  g=fixture();g.skills[int(Skill::Homing)]=2;g.skills[int(Skill::Lightning)]=2;g.enemies.push_back(foe(900));tick(g,.4f);check(g.begin_pressure_event(PressureKind::LastWords),"last words starts");auto sealed=g.pressure_event.sealed;float before=g.skill_cooldown_remaining(sealed);tick(g,.2f);check(near(g.skill_cooldown_remaining(sealed),before),"sealed cooldown pauses");g.end_pressure_event();
 }
 {
  auto g=fixture();g.skills[int(Skill::Lightning)]=5;g.skills[int(Skill::Frost)]=5;check(g.begin_pressure_event(PressureKind::LastWords),"fusion seal starts");g.pressure_event.sealed=Skill::Lightning;float remaining=g.pressure_event.remaining;
  g.auto_fuse_skills();check(g.skills[int(Skill::FrostStorm)]>0,"level-five automatic fusion while sealed");check(g.skill_sealed(Skill::FrostStorm)&&near(g.pressure_event.remaining,remaining),"fusion inherits seal end time");
 }
 {
  auto g=fixture();g.fury_remaining=10;g.shield_remaining=10;g.player.hp=40;tick(g,.05f);float damage=g.damage_multiplier;check(g.begin_pressure_event(PressureKind::Disarm),"disarm starts");tick(g,.05f);check(g.equipment_suppressed()&&g.buffs_suppressed()&&g.damage_multiplier<damage&&near(g.player.hp,40),"disarm contribution only");tick(g,.2f);float remaining=g.fury_remaining;g.end_pressure_event();tick(g,1.f/60);check(!g.equipment_suppressed()&&g.fury_remaining<remaining&&near(g.player.hp,40),"disarm restoration no heal or time rollback");
  g=fixture();g.fury_remaining=10;check(g.add_danger_zone(zone(DangerKind::NaturalOrder)),"natural field");tick(g,1.f/60);check(g.equipment_suppressed()&&!g.buffs_suppressed(),"natural suppresses gear only");g.player.pos={300,0};tick(g,1.f/60);check(!g.equipment_suppressed(),"natural exits");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=1;check(g.add_danger_zone(zone(DangerKind::Regression)),"regression field");tick(g,.5f);check(g.skills[int(Skill::Homing)]==1&&g.effective_skill_level(Skill::Homing)==0&&g.pressure_metrics.attempts[int(Skill::Homing)]==0,"level one becomes runtime zero");g.skills[int(Skill::Homing)]=2;tick(g,1.f/60);check(g.effective_skill_level(Skill::Homing)==1,"upgrade uses latest real rank");g.player.pos={300,0};tick(g,1.f/60);check(g.effective_skill_level(Skill::Homing)==2,"regression exits to latest real rank");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=2;g.config.distraction_chance=1;check(g.begin_pressure_event(PressureKind::Distraction),"distraction starts");tick(g,1);check(g.pressure_metrics.attempts[int(Skill::Homing)]==0,"no target no failure roll");g.enemies.push_back(foe(900));tick(g,1);auto i=int(Skill::Homing);check(g.pressure_metrics.failures[i]==1&&g.pressure_metrics.protected_casts[i]==1&&g.pressure_metrics.successful_casts[i]==1,"one failure followed by protected cast");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=1;auto e=foe(900);e.hp=e.max_hp=100;e.abilities.mask=1u<<int(EnemyAbility::Promise);g.enemies.push_back(e);hit(g,900,10000);check(get(g,900).abilities.pending_death&&get(g,900).hp>0&&near(get(g,900).abilities.promise_remaining,3),"lethal hit intercepted for three seconds");auto kills=g.kills;tick(g,2.9f);check(get(g,900).hp>0&&g.kills==kills,"promise no premature reward");tick(g,.12f);check(g.kills==kills+1,"promise commits true death once");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=1;auto e=foe(900);e.abilities.refraction=2;e.abilities.mask=1u<<int(EnemyAbility::Refraction);g.enemies.push_back(e);hit(g,900,10);hit(g,900,10);check(get(g,900).hp==1000&&get(g,900).abilities.refraction==0,"refraction consumes per hit");hit(g,900,10);check(get(g,900).hp<1000,"damage after layers exhausted");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=1;auto e=foe(900);e.abilities.mask=1u<<int(EnemyAbility::DeathBlast);g.enemies.push_back(e);check(g.begin_pressure_event(PressureKind::DeathGrudge),"death grudge starts");hit(g,900,10000);check(std::count_if(g.danger_zones.begin(),g.danger_zones.end(),[](const DangerZone& z){return z.kind==DangerKind::Explosion;})==1,"one death one visible blast");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=1;g.enemies.push_back(foe(900));check(g.add_danger_zone(zone(DangerKind::Smoke)),"smoke field");tick(g,1);check(g.pressure_metrics.attempts[int(Skill::Homing)]==0,"smoke forbids new attacks");g.player.pos={170,0};tick(g,.05f);check(g.pressure_metrics.attempts[int(Skill::Homing)]==1,"leaving smoke no attack backlog");
 }
 {
  auto g=fixture();g.skills[int(Skill::Homing)]=1;auto z=zone(DangerKind::Distortion);z.radius=50;check(g.add_danger_zone(z),"distortion field");Projectile p;p.skill=Skill::Homing;p.pos={-60,0};p.velocity={300,0};p.lifetime=3;g.projectiles.push_back(p);tick(g,.2f);check(near(g.projectiles.front().pos.x,-25,.2f),"swept crossing uses half speed only inside");
 }
 {
  auto g=fixture();check(g.begin_pressure_event(PressureKind::Shackles),"shackles starts");g.update(1.f/60,{{},true});check(g.metrics.dash_uses==1&&g.player.dash_cooldown>1,"ready dash preserved");g.toggle_pause();float remaining=g.pressure_event.remaining;g.update(.2f);check(near(remaining,g.pressure_event.remaining),"pause stops event clock");g.toggle_pause();g.end_pressure_event();g=fixture();check(g.begin_pressure_event(PressureKind::StandPunishment),"standing starts");tick(g,2.1f);check(!g.danger_zones.empty(),"stationary world position creates telegraph");g.reset_to_menu();check(g.danger_zones.empty()&&g.pressure_event.kind==PressureKind::None,"menu clears temporary sources");
 }
 check(pressure_definitions().size()==20,"all twenty event definitions");
 std::cout<<"Pressure targeted runtime checks passed (13 scenarios; no combination sweep).\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

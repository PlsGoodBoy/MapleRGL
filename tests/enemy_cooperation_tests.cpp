#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace maple;
namespace {
Config quiet(){Config c;c.duration=10000;c.boss_time=c.elite_interval=c.first_event_time=1e6f;c.max_enemies=0;c.enable_pressure=false;c.item_drop_chance=0;return c;}
Enemy enemy(uint32_t id,Vec2 pos,EnemyKind kind=EnemyKind::Slime){Enemy e;e.id=id;e.kind=kind;e.pos=pos;e.hp=e.max_hp=100000;e.speed=e.damage=0;e.attack_clock=-1000;return e;}
void prime(Game& game){game.start(CharacterClass::Archer,318);game.skills.fill(0);game.enemies.clear();game.effects.clear();game.projectiles.clear();game.scene_objects.clear();}
void tick(Game& game,float seconds){for(int i=0;i<int(std::ceil(seconds*100));++i)game.update(.01f);}
}
int main(){
 // The real ordinary attack path retains its simultaneous-attack cap but
 // starts a group of ready rangers on separate combat frames.
 {Game g(quiet());prime(g);for(int i=0;i<3;++i){auto e=enemy(100+i,{220.f,float(i*20)},EnemyKind::Ranger);e.attack_clock=3.3f;g.enemies.push_back(e);}g.update(.01f);
  assert(std::count_if(g.enemies.begin(),g.enemies.end(),[](const Enemy& e){return e.windup>0;})==1);
  tick(g,.19f);assert(std::count_if(g.enemies.begin(),g.enemies.end(),[](const Enemy& e){return e.windup>0;})==2);
  tick(g,.2f);assert(std::count_if(g.enemies.begin(),g.enemies.end(),[](const Enemy& e){return e.windup>0;})==3&&g.metrics.damage_taken==0);}
 // A support unit follows a living group behind the frontline, rather than
 // walking toward the player until all supports stack on the same point.
 {Game g(quiet());prime(g);auto support=enemy(100,{-200,0},EnemyKind::Support);support.speed=50;g.enemies.push_back(support);g.enemies.push_back(enemy(101,{-260,90}));g.enemies.push_back(enemy(102,{-275,100}));tick(g,.6f);assert(g.enemies[0].pos.y>6&&g.enemies[0].pos.x<-200);}
 // Navigation deals with a real circle obstacle by a short lateral route.
 // It does not teleport the enemy, damage the player, or delete the obstacle.
 {Config c=quiet();c.enable_scene_objects=true;Game g(c);prime(g);g.player.pos={100,0};SceneObject crate;crate.id=1;crate.pos={};crate.radius=25;crate.hp=crate.max_hp=1000;g.scene_objects.push_back(crate);auto e=enemy(100,{-60,0});e.speed=60;g.enemies.push_back(e);tick(g,2.f);assert(g.combat_diagnostics.enemy_reroutes>0&&std::abs(g.enemies[0].pos.y)>12);assert(g.scene_objects[0].hp>900&&g.metrics.damage_taken==0);}
 // A melee cast faces a hittable nearby enemy even when a remote Boss has a
 // valid mark. Its actual local damage is evidence of the selected direction.
 {Game g(quiet());prime(g);g.skills[int(Skill::Cleave)]=1;g.skills[int(Skill::HunterMark)]=1;g.enemies.push_back(enemy(100,{90,0}));auto boss=enemy(101,{-400,0},EnemyKind::Boss);boss.mark_remaining=5;boss.mark_source=Skill::HunterMark;g.enemies.push_back(boss);tick(g,.4f);assert(g.enemies[0].hp<g.enemies[0].max_hp&&g.enemies[1].hp==g.enemies[1].max_hp);}
 // A destroyable Boss node is a genuine target of high-value sniper casts.
 {Game g(quiet());prime(g);g.skills[int(Skill::Sniper)]=1;g.enemies.push_back(enemy(100,{300,0},EnemyKind::Boss));auto node=enemy(101,{130,30},EnemyKind::Minion);node.boss_owner=100;node.boss_node=1;node.node_lifetime=30;g.enemies.push_back(node);tick(g,.4f);auto cast=std::find_if(g.effects.begin(),g.effects.end(),[](const Effect& e){return e.source==Skill::Sniper&&e.pattern==0;});assert(cast!=g.effects.end()&&cast->target_id==101);}
 // A missile keeps a valid existing target, then retargets immediately when
 // that enemy enters Promise immunity; a reused position is not an identity.
 {Game g(quiet());prime(g);g.skills[int(Skill::Homing)]=1;g.enemies.push_back(enemy(100,{220,0}));g.enemies.push_back(enemy(101,{210,50}));Projectile p;p.skill=Skill::Homing;p.pos={0,0};p.velocity={330,0};p.target=100;p.damage=1;p.root_attack_id=7;g.projectiles.push_back(p);g.update(.01f);assert(g.projectiles[0].target==100);g.enemies[0].abilities.promise_remaining=2;g.update(.01f);assert(g.projectiles[0].target==101);g.enemies[1].pos={2000,0};g.update(.01f);assert(g.projectiles[0].target==0);}
 // A ground spell chooses the nearby cluster instead of spending its full
 // duration on the single closest straggler.
 {Game g(quiet());prime(g);g.skills[int(Skill::Zone)]=1;g.enemies.push_back(enemy(100,{50,0}));for(int i=0;i<5;++i)g.enemies.push_back(enemy(101+i,{230.f+i*7,90.f+i*4}));tick(g,.4f);auto field=std::find_if(g.effects.begin(),g.effects.end(),[](const Effect& e){return e.source==Skill::Zone&&e.kind==EffectKind::Zone;});assert(field!=g.effects.end()&&field->pos.x>180);}
 // With no enemies and the player facing away, each common attack category
 // must deliberately aim at the living altar and reduce its real facility HP.
 // Facility hits must not fabricate enemy kills or enemy damage contributions.
 for(Skill skill:{Skill::Cleave,Skill::Homing,Skill::Zone,Skill::OctopusTurret,Skill::StormArrowStream,Skill::FrostFire,Skill::TwinHunt}){
  Game g(quiet());prime(g);g.player.facing_left=true;g.skills[int(skill)]=1;
  DangerZone altar;altar.id=700;altar.kind=DangerKind::Altar;altar.pos=skill==Skill::Cleave?Vec2{90,30}:Vec2{240,70};altar.radius=150;altar.age=.2f;altar.warning=.1f;altar.duration=30;altar.hp=altar.max_hp=5000;g.danger_zones.push_back(altar);
  tick(g,3.f);const auto target=std::find_if(g.danger_zones.begin(),g.danger_zones.end(),[](const DangerZone& zone){return zone.id==700;});
  if(target==g.danger_zones.end()||target->hp>=altar.max_hp)std::cerr<<"altar targeting failed: "<<Game::skill_name(skill)<<", attempts="<<g.pressure_metrics.attempts[int(skill)]<<", projectiles="<<g.projectiles.size()<<", effects="<<g.effects.size()<<'\n';
  assert(target!=g.danger_zones.end()&&target->hp<altar.max_hp);
  assert(g.kills==0&&g.metrics.other_damage==0&&std::all_of(g.metrics.skill_damage.begin(),g.metrics.skill_damage.end(),[](double damage){return damage==0;}));
 }
 std::cout<<"PASS ordinary cooperation: staggered attacks, support formation, local obstacle detour, melee reach, Boss nodes, stable valid homing and field clusters\n";
 std::cout<<"PASS active altar targets: melee, homing projectile, ground field, octopus turret and legacy fusion shots/field/companions, with facility-only damage accounting\n";
}

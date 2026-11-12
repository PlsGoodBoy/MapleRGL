#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
bool Game::enemy_targetable(const Enemy& enemy) const {
 return enemy.hp>0&&enemy.abilities.promise_remaining<=0;
}
size_t Game::target_by_id(uint32_t id,Vec2 origin,float range) const {
 if(!id)return std::numeric_limits<size_t>::max();
 for(size_t i=0;i<enemies.size();++i)if(enemies[i].id==id&&enemy_targetable(enemies[i])&&length(enemies[i].pos-origin)<=range)return i;
 return std::numeric_limits<size_t>::max();
}
Vec2 Game::safe_enemy_position(Vec2 wanted,float radius,float safe_distance){
 auto legal=[&](Vec2 p){
  if(length(p-player.pos)<safe_distance)return false;
  if(config.enable_scene_objects)for(const auto& object:scene_objects)if(scene_solid(object)&&length(p-object.pos)<radius+object.radius+2)return false;
  for(const auto& zone:danger_zones)if(zone.age<zone.duration&&!(zone.kind==DangerKind::Altar&&zone.hp<=0)&&length(p-zone.pos)<radius+zone.radius+8)return false;
  return true;
 };
 if(legal(wanted))return wanted;
 Vec2 result=wanted;
 // The battlefield is an open plane. Local projections preserve the authored
 // entry direction and avoid both solid scenery and already announced fields.
 for(int pass=0;pass<6;++pass){
  Vec2 away=result-player.pos;float distance=length(away);
  if(distance<safe_distance)result=player.pos+(distance>.001f?away*(1/distance):Vec2{1,0})*(safe_distance+2);
  if(config.enable_scene_objects)for(const auto& object:scene_objects)if(scene_solid(object)){
   away=result-object.pos;distance=length(away);const float limit=radius+object.radius+3;
   if(distance<limit)result=object.pos+(distance>.001f?away*(1/distance):Vec2{1,0})*limit;
  }
  for(const auto& zone:danger_zones)if(zone.age<zone.duration&&!(zone.kind==DangerKind::Altar&&zone.hp<=0)){
   away=result-zone.pos;distance=length(away);const float limit=radius+zone.radius+9;
   if(distance<limit)result=zone.pos+(distance>.001f?away*(1/distance):Vec2{1,0})*limit;
  }
  if(legal(result)){++combat_diagnostics.spawn_relocations;return result;}
 }
 // Overlapping fields can defeat projection. A finite outer position is legal
 // on this unbounded map; it never moves a failed spawn onto the player.
 float distance=std::max(safe_distance,length(wanted-player.pos))+radius+16;
 if(config.enable_scene_objects)for(const auto& object:scene_objects)if(scene_solid(object))distance=std::max(distance,length(object.pos-player.pos)+object.radius+radius+16);
 for(const auto& zone:danger_zones)if(zone.age<zone.duration)distance=std::max(distance,length(zone.pos-player.pos)+zone.radius+radius+16);
 Vec2 direction=normalized(wanted-player.pos);if(length(direction)<.001f)direction={1,0};
 ++combat_diagnostics.spawn_relocations;return player.pos+direction*distance;
}
Vec2 Game::cooperative_enemy_direction(const Enemy& enemy,Vec2 direction){
 if(enemy.boss_owner||enemy.kind==EnemyKind::Boss)return direction;
 const float distance=length(player.pos-enemy.pos);
 if(enemy.kind==EnemyKind::Support){
  Vec2 center{};int count=0;
  for(size_t index:nearby(enemy.pos,260,24)){const auto& ally=enemies[index];if(ally.id==enemy.id||ally.boss_owner||ally.kind==EnemyKind::Support||ally.kind==EnemyKind::Boss)continue;center+=ally.pos;++count;}
  if(count){center=center*(1.f/count);Vec2 rear=normalized(center-player.pos);Vec2 anchor=center+rear*70;const float gap=length(anchor-enemy.pos);direction=gap>40?normalized(anchor-enemy.pos):Vec2{};}
  if(distance<130)direction=normalized(enemy.pos-player.pos);
 }else if((enemy.kind==EnemyKind::Pig||(enemy.kind==EnemyKind::Minion&&enemy.id%3==0))&&enemy.lure_remaining<=0&&distance>120&&distance<360){
  const float side=enemy.id%2?1.f:-1.f;Vec2 flank={-direction.y*side,direction.x*side};direction=normalized(direction+flank*.48f);
 }
 // Only the heavy/support ranks separate locally; the basic growth swarm
 // keeps its inexpensive direct chase and still leaves corridors between ranks.
 if(enemy.kind==EnemyKind::BlueMushroom||enemy.kind==EnemyKind::Support){
  Vec2 separation{};int checked=0;
  for(size_t index:nearby(enemy.pos,enemy.radius+50,13)){const auto& ally=enemies[index];if(ally.id==enemy.id||ally.boss_owner)continue;Vec2 away=enemy.pos-ally.pos;float gap=length(away),limit=(enemy.radius+ally.radius)*.85f;if(gap>.01f&&gap<limit)separation+=away*((limit-gap)/(limit*gap));if(++checked==12)break;}
  if(length(separation)>0)direction=normalized(direction+separation*.45f);
 }
 return direction;
}
void Game::move_ordinary_enemy(Enemy& enemy,Vec2 direction,float speed,float dt){
 const Vec2 before=enemy.pos;
 if(enemy.navigation_detour>0&&length(direction)>.01f){enemy.navigation_detour=std::max(0.f,enemy.navigation_detour-dt);direction=normalized(enemy.navigation_direction+direction*.3f);}
 enemy.pos+=direction*(speed*dt)+enemy.velocity*dt;
 resolve_scene_collision(enemy.pos,enemy.radius);
 const float expected=speed*dt*length(direction);
 if(expected>.001f&&length(enemy.pos-before)<expected*.3f)enemy.navigation_stall+=dt;
 else enemy.navigation_stall=std::max(0.f,enemy.navigation_stall-dt*2);
 if(enemy.navigation_stall>=.45f){
  Vec2 outward=direction*(-1);
  for(const auto& object:scene_objects)if(scene_solid(object)&&length(enemy.pos-object.pos)<enemy.radius+object.radius+16){outward=normalized(enemy.pos-object.pos);break;}
  Vec2 tangent={-outward.y,outward.x};float alignment=tangent.x*direction.x+tangent.y*direction.y;if(alignment<0||(std::abs(alignment)<.05f&&enemy.id%2))tangent=tangent*(-1);
  enemy.navigation_direction=normalized(tangent+outward*.35f);enemy.navigation_detour=.9f;enemy.navigation_stall=0;++combat_diagnostics.enemy_reroutes;
 }
}
int Game::elite_population() const {return int(std::count_if(enemies.begin(),enemies.end(),[](const Enemy& e){return e.hp>0&&(e.kind==EnemyKind::Elite||e.abilities.elite);}));}
int Game::elite_capacity() const {const bool boss=std::any_of(enemies.begin(),enemies.end(),[](const Enemy& e){return e.hp>0&&e.kind==EnemyKind::Boss;});return (boss?2:6)*encounter_pace();}
EnemyKind Game::budget_enemy_role(EnemyKind requested,bool event_target) const {
 // Objectives keep their authored identities and reserved capacity.
 if(requested==EnemyKind::Elite&&elite_population()>=elite_capacity())return EnemyKind::Mushroom;
 if(event_target||requested==EnemyKind::Boss)return requested;
 int counts[enemy_kind_count]={};int living=0,common=0;bool boss=false;
 for(const auto& enemy:enemies)if(enemy.hp>0){++living;const int id=int(enemy.kind);if(id>=0&&id<enemy_kind_count)++counts[id];if(enemy.kind==EnemyKind::Boss)boss=true;if(enemy.kind==EnemyKind::Slime||enemy.kind==EnemyKind::Mushroom)++common;}
 if(requested==EnemyKind::Minion)return counts[int(requested)]<18?requested:static_cast<EnemyKind>(-1);
 int cap=500;
 switch(requested){
 case EnemyKind::Ranger:cap=boss?2:6;break;
 case EnemyKind::Support:cap=boss?1:2;break;
 case EnemyKind::GroundController:cap=boss?1:3;break;
 case EnemyKind::WindupCharger:cap=boss?1:4;break;
 case EnemyKind::Pig:cap=boss?2:8;break;
 case EnemyKind::Splitter:cap=boss?1:5;break;
 case EnemyKind::Elite:return requested;
 case EnemyKind::BlueMushroom:cap=boss?3:12;break;
 default:return requested;
 }
 // Keep a majority of the ordinary growth-producing swarm, even in mixed waves.
 if(counts[int(requested)]>=cap||(living>=12&&common*100<living*55))return EnemyKind::Slime;
 return requested;
}
bool Game::can_start_enemy_attack(const Enemy& enemy) {
 int charging=0,ranged=0,ground=0;const bool boss=enemy_budget_boss_present_;
 const bool charge_role=enemy.kind==EnemyKind::Pig||enemy.kind==EnemyKind::WindupCharger||enemy.kind==EnemyKind::Elite;
 // Read only current starters, not every member of a dense growth swarm.
 // Their live state preserves cancellations, movement and same-frame starts.
 for(size_t index:active_enemy_attacks_){const auto& other=enemies[index];
  Vec2 delta=other.pos-enemy.pos;if(other.hp<=0||other.id==enemy.id||delta.x*delta.x+delta.y*delta.y>420*420||!(other.windup>0||other.action_remaining>0))continue;
  // A new local special starts at least 180 ms after another windup. This
  // preserves simultaneous-attack budgets without same-frame hidden volleys.
  if(other.kind!=EnemyKind::Boss&&other.attack_animation_time<.18f)return false;
  if(charge_role&&(other.kind==EnemyKind::Pig||other.kind==EnemyKind::WindupCharger||other.kind==EnemyKind::Elite)&&++charging>=(boss?1:2))return false;
  if(enemy.kind==EnemyKind::Ranger&&other.kind==EnemyKind::Ranger&&++ranged>=(boss?1:3))return false;
  if(enemy.kind==EnemyKind::GroundController&&other.kind==EnemyKind::GroundController&&++ground>=1)return false;
 }
 if(enemy.kind==EnemyKind::GroundController){
  int hazards=0;for(const auto& effect:effects)if(effect.hostile&&effect.kind==EffectKind::Warning&&effect.damage>0&&++hazards>=(boss?4:9))return false;
 }
 // All callers start the attack immediately after this reservation succeeds.
 active_enemy_attacks_.push_back(size_t(&enemy-enemies.data()));
 return true;
}
bool Game::update_extra_enemy(Enemy& enemy,float dt){
 if(enemy.kind!=EnemyKind::GroundController&&enemy.kind!=EnemyKind::WindupCharger)return false;
 const bool ground=enemy.kind==EnemyKind::GroundController;
 Vec2 direction=normalized(player.pos-enemy.pos);float distance=length(player.pos-enemy.pos);
 float speed=enemy.speed*scene_slow(enemy.pos,enemy.radius)*(enemy.slow_remaining>0?enemy.slow_factor:1.f)*(enemy.buff_remaining>0?1.3f:1.f);
 if(enemy.windup>0||enemy.action_remaining>0||enemy.recovery_remaining>0)enemy.attack_animation_time+=dt;
 if(enemy.windup>0){
  enemy.windup=std::max(0.f,enemy.windup-dt);speed=0;
  if(enemy.windup<=0){
   if(!ground)enemy.action_remaining=enemy.action_duration;
   enemy.recovery_remaining=ground?1.f:1.25f;
  }
 }else if(enemy.action_remaining>0){
  enemy.action_remaining=std::max(0.f,enemy.action_remaining-dt);speed=0;
  enemy.pos=enemy.attack_start+(enemy.attack_end-enemy.attack_start)*(1-enemy.action_remaining/std::max(.001f,enemy.action_duration));
 }else if(enemy.recovery_remaining>0){enemy.recovery_remaining=std::max(0.f,enemy.recovery_remaining-dt);speed*=.15f;}
 else{
  if(ground){if(distance<145)direction=direction*(-1);else if(distance<235)direction={};}
  enemy.attack_clock+=dt;
  const float interval=(ground?5.5f:4.8f)*enemy_attack_multiplier();
  const size_t effects_needed=ground?3:2;
  if(enemy.attack_clock>=interval&&distance<(ground?350:300)&&enemy_attack_visible(enemy)&&effects.size()+effects_needed<=config.max_effects&&can_start_enemy_attack(enemy)){
   enemy.attack_clock=0;enemy.attack_target=player.pos;enemy.attack_start=enemy.pos;enemy.attack_direction=normalized(player.pos-enemy.pos);enemy.attack_animation_time=0;enemy.windup=ground?1.1f:.95f;speed=0;
   Effect warning;warning.kind=EffectKind::Warning;warning.enemy_kind=enemy.kind;warning.enemy_visual=enemy.visual;warning.hostile=true;warning.target_id=enemy.id;warning.pos=enemy.attack_target;warning.delay=enemy.windup;warning.duration=warning.delay+.3f;
   if(ground){
    // Three visibly announced pulses control one small patch; no invisible lasting hitbox.
    warning.radius=52;warning.damage=enemy.damage*.55f;
    for(int pulse=0;pulse<3;++pulse){Effect patch=warning;patch.delay+=pulse*.7f;patch.duration+=pulse*.7f;add_effect(patch);}
   }else{
    enemy.action_duration=.72f;enemy.attack_end=enemy_attack_endpoint(enemy.pos,enemy.pos+enemy.attack_direction*265,enemy.radius);
    warning.pos=enemy.attack_start;warning.end=enemy.attack_end;warning.radius=enemy.radius+8;warning.pattern=1;warning.duration+=enemy.action_duration;add_effect(warning);
    Effect impact=warning;impact.pattern=0;impact.pos=enemy.attack_end;impact.radius=56;impact.delay+=enemy.action_duration;impact.duration=impact.delay+.3f;impact.damage=enemy.damage*.85f;add_effect(impact);
   }
  }
 }
 if(enemy.windup<=0&&enemy.action_remaining<=0)move_ordinary_enemy(enemy,cooperative_enemy_direction(enemy,direction),speed,dt);
 else resolve_scene_collision(enemy.pos,enemy.radius);
 enemy.velocity=enemy.velocity*std::max(0.f,1-dt*9);damage_scene_area(enemy.pos,enemy.radius,8*dt,Skill::Count);
 if(length(enemy.pos-player.pos)<enemy.radius+13)hurt_player(enemy.damage);
 return true;
}
}

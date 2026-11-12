#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
namespace {
constexpr float pi=3.14159265f;
constexpr size_t none=std::numeric_limits<size_t>::max();
bool continuous(EffectKind kind){return kind==EffectKind::Zone||kind==EffectKind::Rune||kind==EffectKind::Rain||kind==EffectKind::FrostStorm||kind==EffectKind::MoltenStar;}
}
const char* Game::synergy_name(int index){
 static const char* names[]={"寒霜传导","燃烧扩散","标记追猎","破甲重击","护盾反击","牵引入阵"};
 return index>=0&&index<synergy_count?names[index]:"";
}
const char* Game::synergy_description(int index){
 static const char* descriptions[]={
  "连锁闪电命中寒霜目标后，向65范围最多3个敌人碎冰；间隔0.8秒。霜雷领域自身每1.2秒产生受限碎冰。次生伤害不再触发联动。",
  "陨石、蓄势重斩或熔星陨落命中燃烧目标，向90范围最多3个敌人扩散30%伤害；间隔1.2秒，不递归。",
  "箭矢、法球和凤凰优先瞄准有效标记目标。拥有猎手标记时，投射物命中标记目标追加25%伤害，间隔0.25秒，归属猎手标记。",
  "劈空斩和剑气命中施加3秒破甲；蓄势重斩消耗破甲，使该次伤害提高35%。",
  "护盾实际吸收伤害才触发反击准备或龙魂积累，最多每0.5秒一次；不会按每帧接触叠加。",
  "牵引魔井优先锚定已有持续区域，将范围内最多24名敌人拉入阵中；首领永久免疫牵引，突进中不拉动。进入伤害区每秒最多记录一次。"
 };return index>=0&&index<synergy_count?descriptions[index]:"";
}
void Game::record_synergy(int index){if(index<0||index>=synergy_count)return;++metrics.synergy_triggers[index];synergy_flash[index]=1.2f;}
void Game::on_shield_absorb(){
 on_cross_defense();
 if(soul_absorb_cooldown_>0||smoke_at(player.pos))return;
 soul_absorb_cooldown_=.5f;
 if(skill_attack_available(Skill::DragonArmor,player.pos)){dragon_soul=std::min(100.f,dragon_soul+(fusion_form(Skill::DragonArmor)==0?15.f:8.f));record_synergy(4);}
 else if(skill_attack_available(Skill::Retaliate,player.pos)){retaliation_pending_=true;record_synergy(4);}
}
void Game::trigger_burst_synergy(size_t index,float damage,Skill source,std::vector<Effect>& spawned){
 if(!skill_attack_available(source,player.pos)||index>=enemies.size()||enemies[index].burn_remaining<=0||synergy_cooldown_[1]>0)return;
 synergy_cooldown_[1]=1.2f;record_synergy(1);Vec2 origin=enemies[index].pos;int hits=0;
 for(auto target:nearby(origin,90)){if(target==index)continue;if(hits++>=3)break;hurt_enemy(target,damage*.30f,enemies[target].pos-origin,.35f,source,false,false);}
 Effect burst;burst.kind=EffectKind::Meteor;burst.pos=origin;burst.radius=90;burst.duration=.5f;burst.triggered=true;burst.source=source;burst.proc_generated=true;spawned.push_back(burst);
}
bool Game::cast_expansion(Skill skill){
 const int level=effective_skill_level(skill);const float damage=skill_power(skill);
 auto add=[&](EffectKind kind,Vec2 point,float radius,float duration){Effect effect;effect.kind=kind;effect.source=skill;effect.pos=point;effect.radius=radius;effect.duration=duration;effect.damage=damage;return effect;};
 if(skill==Skill::HunterMark){
  size_t target=none;float strength=-1;for(auto n:nearby(player.pos,500*area_multiplier))if(enemies[n].max_hp>strength){strength=enemies[n].max_hp;target=n;}
  if(target==none)return false;
  for(auto& enemy:enemies)enemy.mark_remaining=0;
  effects.erase(std::remove_if(effects.begin(),effects.end(),[](const Effect& e){return e.kind==EffectKind::HunterMark;}),effects.end());
  auto& enemy=enemies[target];enemy.mark_source=Skill::HunterMark;enemy.mark_remaining=(5+level*.5f)*duration_multiplier;
  auto effect=add(EffectKind::HunterMark,enemy.pos,enemy.radius+12,enemy.mark_remaining);effect.target_id=enemy.id;effect.damage=0;add_effect(effect);return true;
 }
 if(skill==Skill::ArrowTrap){
  int traps=0;for(const auto& effect:effects)if(effect.kind==EffectKind::ArrowTrap)++traps;if(traps>=3)return false;
  Vec2 behind=player.pos+Vec2{player.facing_left?35.f:-35.f,20};auto effect=add(EffectKind::ArrowTrap,behind,(48+level*5)*area_multiplier,6*duration_multiplier);effect.delay=.45f;effect.damage*=1.6f;add_effect(effect);return true;
 }
 if(skill==Skill::OrbitalOrb){
  effects.erase(std::remove_if(effects.begin(),effects.end(),[](const Effect& e){return e.kind==EffectKind::OrbitalOrb;}),effects.end());
  auto effect=add(EffectKind::OrbitalOrb,player.pos,(72+level*3)*area_multiplier,skill_interval(skill)+.15f);effect.count=std::min(5,2+level/2+extra_projectiles/2);effect.secondary_tick=.15f;add_effect(effect);return true;
 }
 if(skill==Skill::GravityWell){
  Vec2 point=player.pos;size_t target=nearest(player.pos,380);const DangerZone* altar=altar_target(skill,player.pos,380,target);if(altar)point=altar->pos;else if(target!=none)point=enemies[target].pos;
  float nearest_field=1e6f;if(!altar)for(const auto& field:effects)if(!field.hostile&&continuous(field.kind)&&field.damage>0&&field.duration-field.age>.8f){float distance=length(field.pos-player.pos);if(distance<400&&distance<nearest_field){nearest_field=distance;point=field.pos;}}
  effects.erase(std::remove_if(effects.begin(),effects.end(),[](const Effect& e){return e.kind==EffectKind::GravityWell;}),effects.end());
  auto effect=add(EffectKind::GravityWell,point,(155+level*9)*area_multiplier,3.5f*duration_multiplier);effect.damage*=.35f;add_effect(effect);return true;
 }
 if(skill==Skill::HeavyStrike){
  size_t target=nearest(player.pos,230*area_multiplier);const DangerZone* altar=altar_target(skill,player.pos,230*area_multiplier,target);if(target==none&&!altar)return false;
  Vec2 direction=normalized((altar?altar->pos:enemies[target].pos)-player.pos);auto effect=add(EffectKind::HeavyStrike,player.pos,(145+level*10)*area_multiplier,1.05f);effect.end=player.pos+direction*effect.radius;effect.angle=.85f;effect.delay=.7f;effect.damage*=4.5f;add_effect(effect);return true;
 }
 if(skill==Skill::Bulwark){guard_charge=std::max(guard_charge,20.f+4*level);Effect pulse=add(EffectKind::Bulwark,player.pos,30,.45f);pulse.damage=0;add_effect(pulse);return true;}
 return false;
}
bool Game::update_expansion_effect(Effect& e,float dt,std::vector<Effect>& spawned){
 const int level=e.source==Skill::Count?0:effective_skill_level(e.source);
 auto bolt=[&](Vec2 from,Vec2 direction,float damage,int pierce){if(projectiles.size()>=config.max_projectiles)return false;Projectile projectile;projectile.pos=from;projectile.velocity=direction*350;projectile.skill=e.source;projectile.damage=damage;projectile.radius=8;projectile.pierce=pierce;projectile.lifetime=2;add_projectile(projectile);return true;};
 if(e.kind==EffectKind::HunterMark){auto found=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& enemy){return enemy.id==e.target_id&&enemy.hp>0;});if(found==enemies.end())e.age=e.duration;else e.pos=found->pos;return true;}
 if(e.kind==EffectKind::ArrowTrap){
  if(e.triggered||e.age<e.delay)return true;
  size_t target=nearest(e.pos,e.radius+35);if(target==none||length(enemies[target].pos-e.pos)>e.radius+enemies[target].radius)return true;
  e.triggered=true;e.duration=e.age+.28f;
  for(auto n:nearby(e.pos,e.radius+50))if(length(enemies[n].pos-e.pos)<=e.radius+enemies[n].radius)hurt_enemy(n,e.damage,enemies[n].pos-e.pos,1.8f,e.source);
  float axis=std::atan2(enemies[target].pos.y-e.pos.y,enemies[target].pos.x-e.pos.x);int count=3+level/2+extra_projectiles;
  for(int i=0;i<std::min(10,count);++i){float angle=axis+(i-(count-1)*.5f)*.24f;bolt(e.pos,{std::cos(angle),std::sin(angle)},e.damage*.6f,2);}
  Effect flash;flash.kind=EffectKind::Cleave;flash.source=e.source;flash.pos=e.pos;flash.end=enemies[target].pos;flash.radius=e.radius;flash.angle=1.4f;flash.duration=.25f;spawned.push_back(flash);return true;
 }
 if(e.kind==EffectKind::OrbitalOrb){
  e.pos=player.pos;e.tick-=dt;e.secondary_tick-=dt;const bool release=e.tick<=0,contact=e.secondary_tick<=0;
  if(release)e.tick=1.25f;if(contact)e.secondary_tick=.3f;
  for(int i=0;i<e.count;++i){float angle=combat_elapsed*1.65f+i*2*pi/e.count;Vec2 point=player.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius;
   if(release&&!smoke_at(point)){auto target=nearest(point,300);const DangerZone* altar=altar_target(e.source,point,300,target);if(target!=none||altar){if(bolt(point,normalized((altar?altar->pos:enemies[target].pos)-point),e.damage*.85f,1)&&altar)projectiles.back().altar_target_id=altar->id;}}
   if(contact)for(auto n:nearby(point,45))if(length(enemies[n].pos-point)<=enemies[n].radius+12)hurt_enemy(n,e.damage*.35f,enemies[n].pos-point,.2f,e.source);
  }return true;
 }
 if(e.kind==EffectKind::GravityWell){
  int count=0;bool entered_field=false;e.tick-=dt;const bool pulse=e.tick<=0;if(pulse){e.tick=.5f;damage_pressure_facilities(e.pos,e.radius,e.damage);}
  for(auto n:nearby(e.pos,e.radius+50)){auto& enemy=enemies[n];float distance=length(enemy.pos-e.pos);if(distance>e.radius+enemy.radius)continue;if(count++>=24)break;
   float speed=displacement_immune(enemy)?0.f:90.f;Vec2 move=normalized(e.pos-enemy.pos)*std::min(std::max(0.f,distance-15),speed*dt);displace_enemy(enemy,move);
   if(pulse)hurt_enemy(n,e.damage,{},0,e.source);
   if(length(move)>.001f&&!entered_field)for(const auto& field:effects)if(!field.hostile&&continuous(field.kind)&&field.damage>0&&field.age>=field.delay&&length(enemy.pos-field.pos)<field.radius){entered_field=true;break;}
  }
  if(entered_field&&synergy_cooldown_[5]<=0){synergy_cooldown_[5]=1;record_synergy(5);}return true;
 }
 if(e.kind==EffectKind::HeavyStrike){
  if(e.triggered||e.age<e.delay)return true;e.triggered=true;Vec2 direction=normalized(e.end-e.pos);
  for(auto n:nearby(e.pos,e.radius+55)){Vec2 delta=enemies[n].pos-e.pos,axis=normalized(delta);if(length(delta)>e.radius+enemies[n].radius||axis.x*direction.x+axis.y*direction.y<std::cos(e.angle))continue;hurt_enemy(n,e.damage,delta,3.2f,e.source);trigger_burst_synergy(n,e.damage,e.source,spawned);}
  if(config.enable_scene_objects)for(size_t index=0;index<scene_objects.size();++index){Vec2 delta=scene_objects[index].pos-e.pos,axis=normalized(delta);if(length(delta)<e.radius+scene_objects[index].radius&&axis.x*direction.x+axis.y*direction.y>=std::cos(e.angle))hurt_scene_object(index,e.damage,e.source);}
  for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone.hp>0){Vec2 delta=zone.pos-e.pos,axis=normalized(delta);if(length(delta)<e.radius+24&&axis.x*direction.x+axis.y*direction.y>=std::cos(e.angle))damage_pressure_facilities(zone.pos,0,e.damage);}
  return true;
 }
 if(e.kind==EffectKind::Bulwark){e.pos=player.pos;return true;}
 return false;
}
}

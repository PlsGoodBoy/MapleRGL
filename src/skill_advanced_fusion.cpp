#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr size_t none=std::numeric_limits<size_t>::max();
EffectKind effect_for(Skill skill){return EffectKind(int(EffectKind::BattleFocus)+int(skill)-int(Skill::BattleFocus));}
}
bool Game::cast_advanced_fusion(Skill skill){
 if(skill<Skill::BattleBlades||skill>Skill::PrismShot||skill==Skill::MountainBreaker)return false;
 if(effects.size()>=config.max_effects||std::any_of(effects.begin(),effects.end(),[&](const Effect& effect){return effect.source==skill&&effect.kind==effect_for(skill)&&effect.pattern<10&&effect.age<effect.duration;}))return false;
 const int level=effective_skill_level(skill);const float base=skill_power(skill);size_t target=skill_role_target(skill,player.pos,520);const DangerZone* altar=altar_target(skill,player.pos,520,target);
 Vec2 point=altar?altar->pos:target==none?player.pos:enemies[target].pos;
 Vec2 direction=target==none&&!altar?Vec2{player.facing_left?-1.f:1.f,0}:normalized(point-player.pos);
 Effect effect;effect.kind=effect_for(skill);effect.source=skill;effect.pos=player.pos;effect.end=target==none&&!altar?player.pos+direction*180:point;effect.damage=base;effect.duration=1;effect.target_id=altar||target==none?0:enemies[target].id;effect.altar_target_id=altar?altar->id:0;
 switch(skill){
 case Skill::BattleBlades:if(target==none&&!altar)return false;effect.radius=(130+8*level)*area_multiplier;effect.angle=.85f;effect.end=player.pos+direction*effect.radius;effect.count=3;effect.duration=2;break;
 case Skill::DragonFormation:if(target==none&&!altar)return false;effect.end=player.pos+direction*320*area_multiplier;effect.radius=30*area_multiplier;effect.delay=.2f;effect.duration=1.05f;break;
 case Skill::EchoShield:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base*1.15f,400,18*area_multiplier,5,effect.target_id,0,3);
 case Skill::SpiritJudgement:if(target==none&&!altar)return false;effect.pos=effect.end;effect.radius=(80+5*level)*area_multiplier;effect.duration=4.2f;effect.count=0;break;
 case Skill::AzureRift:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base*1.7f,470,22*area_multiplier,7,effect.target_id,0,4);
 case Skill::Sanctuary:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base,400,18*area_multiplier,5,effect.target_id,0,2);
 case Skill::PoisonFlare:effect.pos=effect.end;effect.radius=(130+7*level)*area_multiplier*(fusion_form(skill)==0?1.18f:.75f);effect.duration=5.2f*duration_multiplier;effect.delay=1.6f;break;
 case Skill::ThunderLance:{if(target==none&&!altar)return false;const Vec2 side{-direction.y*18,direction.x*18};bool made=skill_bolt(skill,player.pos+side,direction,base*1.05f,460,12*area_multiplier,5,effect.target_id,0,3);made=skill_bolt(skill,player.pos-side,direction,base*1.05f,460,12*area_multiplier,5,effect.target_id,0,3)||made;return made;}
 case Skill::HeavenDragon:effect.radius=100;effect.duration=4.6f*duration_multiplier;effect.tick=.2f;break;
 case Skill::InfernoArray:if(target==none&&!altar)return false;effect.radius=55*area_multiplier;effect.count=3;effect.duration=3.3f;effect.tick=.1f;break;
 case Skill::VenomVortex:effect.pos=effect.end;effect.radius=(150+6*level)*area_multiplier;effect.duration=4*duration_multiplier+.3f;break;
 case Skill::FrostFire:{Vec2 middle=target==none&&!altar?player.pos:effect.end;Vec2 axis={-direction.y,direction.x};effect.pos=middle-axis*70*area_multiplier;effect.end=middle+axis*70*area_multiplier;effect.angle=std::atan2(axis.y,axis.x);effect.radius=(90+4*level)*area_multiplier;effect.duration=4.8f*duration_multiplier;effect.secondary_tick=1.3f;break;}
 case Skill::PurityWell:effect.pos=effect.end;effect.radius=(145+6*level)*area_multiplier;effect.delay=.9f;effect.duration=4*duration_multiplier;break;
 case Skill::ThousandArrows:if(target==none&&!altar)return false;effect.count=6;effect.energy=6;effect.duration=1.65f;effect.radius=8;break;
 case Skill::EagleExecution:if(target==none&&!altar)return false;effect.radius=90;effect.delay=.95f;effect.duration=1.75f;break;
 case Skill::FrozenBarrage:effect.pos=effect.end;effect.radius=(130+7*level)*area_multiplier;effect.delay=.25f;effect.duration=2.65f;effect.count=4;break;
 case Skill::SiegeBolt:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base*1.5f,470,13*area_multiplier,8,effect.target_id,0,4);
 case Skill::TrickDecoy:effect.pos=player.pos+direction*65;effect.end=effect.pos;effect.radius=180*area_multiplier;effect.duration=2.5f*duration_multiplier+.25f;break;
 case Skill::TwinHunt:effect.radius=95;effect.duration=4.6f*duration_multiplier;effect.tick=.2f;effect.count=2;break;
 case Skill::PrismShot:if(target==none&&!altar)return false;effect.radius=13*area_multiplier;effect.delay=.8f;effect.duration=1.15f;effect.count=3;break;
 default:return false;
 }
 add_effect(effect);return true;
}
bool Game::update_advanced_fusion_effect(Effect& effect,float dt,std::vector<Effect>& spawned){
 const Skill skill=effect.source;if(skill<Skill::BattleBlades||skill>Skill::PrismShot||effect.kind!=effect_for(skill))return false;
 const int level=effective_skill_level(skill);
 auto append=[&](Effect child){if(effects.size()+spawned.size()<config.max_effects)spawned.push_back(child);};
 auto target_by_id=[&](){return this->target_by_id(effect.target_id,player.pos,650);};
 const DangerZone* altar=nullptr;
 auto aim=[&](Vec2 origin,float range,bool strong=true){size_t index=this->target_by_id(effect.target_id,origin,range);if(index==none)index=strong?priority_enemy(origin,range):nearest(origin,range);altar=altar_target(skill,origin,range,index,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;effect.target_id=altar||index==none?0:enemies[index].id;if(altar||index!=none)effect.end=altar?altar->pos:enemies[index].pos;return index;};
 switch(skill){
 case Skill::BattleBlades:{
  if(effect.pattern>=10)break;effect.pos=player.pos;size_t target=aim(player.pos,effect.radius+120,false);if(target!=none||altar)effect.end=player.pos+normalized(effect.end-player.pos)*effect.radius;effect.tick-=dt;
  if(effect.tick<=0&&effect.count>0){bool heavy=effect.pattern==1;effect.tick=heavy?.2f:.4f;Vec2 direction=normalized(effect.end-effect.pos);bool hit=false;
   for(auto index:nearby(effect.pos,effect.radius+60)){Vec2 delta=enemies[index].pos-effect.pos,axis=normalized(delta);if(length(delta)<=effect.radius+enemies[index].radius&&axis.x*direction.x+axis.y*direction.y>=std::cos(effect.angle)){hit=true;break;}}
   skill_cone(effect.pos,direction,effect.radius,effect.angle,effect.damage*(heavy?1.9f:.85f),skill,heavy?1.6f:.6f);--effect.count;
   Effect flash=effect;flash.age=0;flash.duration=heavy?.2f:.25f;flash.damage=0;flash.pattern=heavy?11:10;flash.triggered=true;append(flash);
   if(!heavy&&hit)battle_focus=std::min(100.f,battle_focus+25);
   if(!heavy&&battle_focus>=100){battle_focus=0;effect.pattern=1;effect.count=3;effect.tick=.2f;effect.duration=effect.age+.8f;}
   else if(effect.count==0)effect.duration=effect.age+.25f;
  }break;}
 case Skill::DragonFormation:{
  const Vec2 direction=normalized(effect.end-effect.pos);const float axis=std::atan2(direction.y,direction.x);
  if(effect.pattern==0&&effect.age>=effect.delay){effect.pattern=1;for(int lane=-1;lane<=1;++lane){const float angle=axis+lane*.46f;skill_line(effect.pos,effect.pos+Vec2{std::cos(angle),std::sin(angle)}*320*area_multiplier,effect.radius,effect.damage*2.6f,skill,.7f);}}
  if(effect.pattern==1&&effect.age>=.55f){effect.pattern=2;effect.triggered=true;effect.end=effect.pos+direction*80*area_multiplier;effect.radius=180*area_multiplier;skill_area(effect.end,effect.radius,effect.damage*4,skill,1.2f);}break;}
 case Skill::EchoShield:if(effect.pattern==1&&!effect.triggered){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage,skill,1.5f);}break;
 case Skill::MountainBreaker:
  if(effect.pattern==1&&effect.age>=effect.delay){effect.triggered=true;effect.tick-=dt;if(effect.tick<=0){effect.tick=.4f;skill_line(effect.pos,effect.end,effect.radius,effect.damage*.65f,skill,.5f);}}break;
 case Skill::SpiritJudgement:{
  if(effect.pattern==0){effect.tick-=dt;if(effect.tick<=0){effect.tick=.6f;size_t target=target_by_id();if(target==none)target=priority_enemy(player.pos,520);if(target!=none){auto& enemy=enemies[target];effect.target_id=enemy.id;effect.pos=effect.end=enemy.pos;enemy.mark_remaining=std::max(enemy.mark_remaining,1.3f);enemy.mark_source=skill;hurt_enemy(target,effect.damage*.35f,{},0,skill);battle_focus=std::min(100.f,battle_focus+25);effect.count=std::min(4,effect.count+1);}
    if(battle_focus>=100){battle_focus=0;effect.pattern=1;effect.delay=effect.age+.6f;effect.duration=effect.delay+.4f;}
   }}else if(effect.pattern==1&&effect.age>=effect.delay){effect.pattern=2;effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage*(4+.4f*effect.count),skill,1.7f);}break;}
 case Skill::AzureRift:case Skill::SiegeBolt:if(effect.pattern==1&&!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage,skill,.8f,false);}break;
 case Skill::Sanctuary:
  if(effect.pattern==1){effect.tick-=dt;if(effect.tick<=0){effect.tick=.75f;size_t target=aim(effect.pos,effect.radius+24);if(altar||(target!=none&&length(enemies[target].pos-effect.pos)<=effect.radius+enemies[target].radius)){Effect strike=effect;strike.age=0;strike.pos=strike.end=effect.end;strike.radius=45*area_multiplier;strike.pattern=2;strike.delay=.4f;strike.duration=.75f;strike.damage=effect.damage*1.5f;strike.triggered=false;append(strike);}}}
  else if(effect.pattern==2&&!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage,skill,.7f);}break;
 case Skill::PoisonFlare:{
  const bool consume=fusion_form(skill)==1;
  if(effect.pattern>=10){if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;damage_pressure_facilities(effect.pos,effect.radius,effect.damage*(consume?1.8f:.9f));for(auto index:nearby(effect.pos,effect.radius+60)){
   auto& enemy=enemies[index];if(enemy.hp<=0||length(enemy.pos-effect.pos)>effect.radius+enemy.radius)continue;
   const int layers=enemy.poison_source==skill?std::clamp(enemy.poison_layers,0,3):0;
   if(layers>0){enemy.poison_layers=consume?0:layers-1;if(enemy.poison_layers==0){enemy.poison_remaining=enemy.poison_tick=enemy.poison_damage=0;enemy.poison_source=Skill::Count;}}
   hurt_enemy(index,effect.damage*(consume?1.8f+.8f*layers:.9f+.2f*layers),enemy.pos-effect.pos,.8f,skill,false,false);apply_burn(enemy,effect.damage*(consume?.3f:.2f),1.2f,skill);
  }}break;}
  if(effect.pattern==0){effect.tick-=dt;if(effect.tick<=0){effect.tick=.5f;for(auto index:nearby(effect.pos,effect.radius+60))if(length(enemies[index].pos-effect.pos)<=effect.radius+enemies[index].radius){apply_poison(index,effect.damage*(consume?.25f:.2f),2.4f,skill);if(consume)enemies[index].poison_spread=true;}}
   if(effect.age>=effect.delay&&effect.count<2){effect.pattern=1;effect.delay=effect.age+.65f;++effect.count;const int parts=consume?1:3;for(int part=0;part<parts;++part){Effect burst=effect;burst.pos=effect.pos+Vec2{consume?0.f:float(part-1)*90*area_multiplier,0};burst.end=burst.pos;burst.radius=(consume?85.f:75.f)*area_multiplier;burst.age=0;burst.delay=part*.15f;burst.duration=burst.delay+.4f;burst.pattern=10+part;burst.triggered=false;append(burst);}}}
  else if(effect.age>=effect.delay){effect.pattern=0;effect.delay=effect.age+1.6f;effect.tick=0;}break;}
 case Skill::ThunderLance:if(effect.pattern==1&&!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_line(effect.pos,effect.end,effect.radius,effect.damage*1.1f,skill,.3f);skill_area(effect.end,45*area_multiplier,effect.damage*.65f,skill,.3f,false);record_synergy(0);}break;
 case Skill::HeavenDragon:{
  if(effect.pattern==1){if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;effect.tick=.6f;skill_line(effect.pos,effect.end,effect.radius,effect.damage*1.2f,skill,.3f);}else if(effect.triggered){effect.tick-=dt;if(effect.tick<=0){effect.tick=.6f;skill_line(effect.pos,effect.end,effect.radius,effect.damage*.55f,skill,.1f,false);}}break;}
  effect.angle+=dt*1.2f;effect.pos=player.pos+Vec2{std::cos(effect.angle)*100,std::sin(effect.angle)*70};effect.tick-=dt;if(effect.tick<=0){effect.tick=1.4f;size_t target=aim(effect.pos,260*area_multiplier+24);int active=0;for(const auto& child:effects)if(child.source==skill&&child.pattern==1&&child.age<child.duration)++active;if((target!=none||altar)&&active<2){effect.visual_attack_age=0;effect.visual_aim=effect.end-effect.pos;Effect corridor=effect;corridor.age=0;corridor.end=effect.pos+normalized(effect.end-effect.pos)*260*area_multiplier;corridor.radius=22*area_multiplier;corridor.pattern=1;corridor.delay=.15f;corridor.duration=1.6f;corridor.triggered=false;append(corridor);}}break;}
 case Skill::InfernoArray:{
  if(effect.pattern>0){if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;effect.pattern=2;effect.tick=.4f;skill_area(effect.pos,effect.radius,effect.damage*2.1f,skill,1.3f);for(auto index:nearby(effect.pos,effect.radius+60))if(length(enemies[index].pos-effect.pos)<=effect.radius+enemies[index].radius)apply_burn(enemies[index],effect.damage*.3f,1,skill);}else if(effect.triggered){effect.tick-=dt;if(effect.tick<=0){effect.tick=.4f;skill_area(effect.pos,effect.radius,effect.damage*.35f,skill,.1f,false);}}break;}
  effect.pos=player.pos+Vec2{player.facing_left?70.f:-70.f,35};effect.tick-=dt;if(effect.tick<=0&&effect.count>0){effect.tick=.9f;size_t target=aim(player.pos,520);if(target!=none||altar){if(!altar){enemies[target].mark_remaining=1.5f;enemies[target].mark_source=skill;}float angle=(3-effect.count)*2*pi/3;Effect meteor=effect;meteor.pos=effect.end+Vec2{std::cos(angle),std::sin(angle)}*55*area_multiplier;meteor.end=meteor.pos;meteor.radius=65*area_multiplier;meteor.age=0;meteor.delay=.6f;meteor.duration=1.75f;meteor.pattern=1;meteor.triggered=false;append(meteor);--effect.count;}}break;}
 case Skill::VenomVortex:{
  if(effect.triggered)break;pull_enemies(effect,dt,80);effect.tick-=dt;if(effect.tick<=0){effect.tick=.5f;damage_pressure_facilities(effect.pos,effect.radius,effect.damage*.25f);int count=0;for(auto index:nearby(effect.pos,effect.radius)){if(count++>=12)break;hurt_enemy(index,effect.damage*.25f,{},0,skill);apply_poison(index,effect.damage*.3f,2.2f,skill);}}
  if(effect.age>=effect.duration-.3f){effect.triggered=true;effect.pattern=1;damage_pressure_facilities(effect.pos,effect.radius,effect.damage*.8f);int count=0;for(auto index:nearby(effect.pos,effect.radius+60)){auto& enemy=enemies[index];if(length(enemy.pos-effect.pos)>effect.radius+enemy.radius)continue;if(count++>=12)break;int layers=std::clamp(enemy.poison_layers,0,3);enemy.poison_remaining=0;enemy.poison_layers=0;hurt_enemy(index,effect.damage*(.8f+.65f*layers),enemy.pos-effect.pos,1.f,skill,false,false);}}break;}
 case Skill::FrostFire:{
  if(effect.pattern==2)break;effect.secondary_tick-=dt;if(effect.secondary_tick<=0){effect.secondary_tick=1.3f;effect.pattern=1-effect.pattern;}effect.tick-=dt;if(effect.tick>0)break;effect.tick=.4f;Vec2 middle=(effect.pos+effect.end)*.5f,axis=normalized(effect.end-effect.pos);int shocks=0;
  for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone.hp>0&&std::min(length(zone.pos-effect.pos),length(zone.pos-effect.end))<=effect.radius+24){int side=((zone.pos-middle).x*axis.x+(zone.pos-middle).y*axis.y)<0?-1:1;bool cold=effect.pattern==0?side<0:side>0;damage_pressure_facilities(zone.pos,0,effect.damage*(cold?.4f:.55f));}
  for(auto index:nearby(middle,effect.radius+length(effect.end-effect.pos)*.5f+60)){auto& enemy=enemies[index];float left=length(enemy.pos-effect.pos),right=length(enemy.pos-effect.end);if(std::min(left,right)>effect.radius+enemy.radius)continue;int side=((enemy.pos-middle).x*axis.x+(enemy.pos-middle).y*axis.y)<0?-1:1;bool cold=(effect.pattern==0?side<0:side>0);
   if(enemy.thermal_source==skill&&enemy.thermal_side&&enemy.thermal_side!=side&&enemy.thermal_lock<=0&&shocks<3){++shocks;enemy.thermal_lock=1;Effect shock=effect;shock.pos=shock.end=enemy.pos;shock.radius=55*area_multiplier;shock.age=0;shock.duration=.35f;shock.damage=0;shock.pattern=2;shock.triggered=true;append(shock);int hits=0;for(auto other:nearby(enemy.pos,55)){if(hits++>=3)break;hurt_enemy(other,effect.damage*.65f,enemies[other].pos-enemy.pos,.4f,skill,false,false);}}
   enemy.thermal_side=side;enemy.thermal_source=skill;if(cold){apply_frost(enemy,1.1f,skill);hurt_enemy(index,effect.damage*.4f,{},0,skill);}else{hurt_enemy(index,effect.damage*.55f,{},0,skill);apply_burn(enemy,effect.damage*.25f,1.1f,skill);}
  }break;}
 case Skill::PurityWell:pull_enemies(effect,dt,65);if(effect.age>=effect.delay){effect.pattern=1;effect.tick-=dt;if(effect.tick<=0){effect.tick=.75f;skill_area(effect.pos,effect.radius,effect.damage,skill,.2f);if(holy_support_cooldown_<=0&&length(player.pos-effect.pos)<=effect.radius+13){heal_player(2);holy_support_cooldown_=3;}}}break;
 case Skill::ThousandArrows:{
  effect.pos=player.pos;effect.tick-=dt;if(effect.tick>0||effect.count<=0)break;effect.tick=.22f;size_t target=aim(player.pos,550,false);if(target==none&&!altar)break;effect.pattern=(6-effect.count)%2;float axis=std::atan2(effect.end.y-effect.pos.y,effect.end.x-effect.pos.x);int bolts=effect.pattern==0?3:std::min(8,5+level/2+extra_projectiles/2);float spread=effect.pattern==0?.035f:.18f;
  for(int bolt=0;bolt<bolts;++bolt){float angle=axis+(bolt-(bolts-1)*.5f)*spread;skill_bolt(skill,effect.pos,{std::cos(angle),std::sin(angle)},effect.damage*.65f,460,8,2,effect.target_id);}--effect.count;break;}
 case Skill::EagleExecution:{
  effect.angle+=dt*2;effect.pos=player.pos+Vec2{std::cos(effect.angle)*90,std::sin(effect.angle)*60};if(effect.age<.6f){size_t target=aim(player.pos,550);if(target!=none&&!altar){enemies[target].mark_remaining=2;enemies[target].mark_source=skill;}}
  if(effect.pattern==0&&effect.age>=.55f)effect.pattern=1;
  if(effect.pattern==1&&effect.age>=effect.delay){effect.pattern=2;skill_bolt(skill,player.pos,effect.end-player.pos,effect.damage*5,620,12,3,effect.target_id);}
  if(effect.pattern==2&&effect.age>=1.35f){effect.pattern=3;effect.triggered=true;skill_bolt(skill,player.pos,effect.end-player.pos,effect.damage*2,620,12,3,effect.target_id);}break;}
 case Skill::FrozenBarrage:{
  if(effect.pattern>=10)break;if(effect.age<effect.delay)break;effect.tick-=dt;
  if(effect.count>0&&effect.tick<=0){effect.tick=.4f;int count=0;altar=altar_target(skill,effect.pos,effect.radius+24,none,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;if(altar){Vec2 origin=altar->pos+Vec2{-18,-180};skill_bolt(skill,origin,altar->pos-origin,effect.damage*.5f,600,8,1);++count;}for(auto index:nearby(effect.pos,effect.radius+40)){if(length(enemies[index].pos-effect.pos)>effect.radius+enemies[index].radius)continue;if(count++>=6)break;Vec2 origin=enemies[index].pos+Vec2{-18,-180};skill_bolt(skill,origin,enemies[index].pos-origin,effect.damage*.5f,600,8,1,enemies[index].id);}--effect.count;if(effect.count==0)effect.pattern=1;}
  if(!effect.triggered&&effect.age>=2.25f){effect.triggered=true;effect.pattern=2;int shattered=0;for(auto index:nearby(effect.pos,effect.radius+60)){auto& enemy=enemies[index];if(enemy.frost_layers<3||length(enemy.pos-effect.pos)>effect.radius+enemy.radius)continue;if(shattered++>=8)break;enemy.frost_layers=0;enemy.frost_remaining=0;hurt_enemy(index,effect.damage*1.8f,{},.5f,skill,false,false);int splinters=0;for(auto other:nearby(enemy.pos,45)){if(other==index)continue;if(splinters++>=3)break;hurt_enemy(other,effect.damage*.45f,enemies[other].pos-enemy.pos,.3f,skill,false,false);}Effect shard=effect;shard.pos=shard.end=enemy.pos;shard.radius=45*area_multiplier;shard.age=0;shard.duration=.3f;shard.damage=0;shard.pattern=10;append(shard);}}break;}
 case Skill::TrickDecoy:{
  if(effect.pattern==2){if(effect.age>=effect.delay){size_t target=nearest(effect.pos,effect.radius+40);if(target!=none&&length(enemies[target].pos-effect.pos)<=effect.radius+enemies[target].radius){effect.pattern=3;effect.triggered=true;effect.radius=75*area_multiplier;effect.duration=effect.age+.3f;skill_area(effect.pos,effect.radius,effect.damage*2.2f,skill,1.f,false);}}break;}
  if(effect.pattern!=0)break;effect.tick-=dt;if(effect.tick<=0){effect.tick=.5f;lure_enemies(effect);}if(effect.age>=effect.duration-.25f){effect.pattern=1;effect.triggered=true;for(int trap=0;trap<4;++trap){float angle=trap*pi*.5f;Effect mine=effect;mine.pos=effect.pos+Vec2{std::cos(angle),std::sin(angle)}*55*area_multiplier;mine.end=mine.pos;mine.radius=45*area_multiplier;mine.age=0;mine.delay=.35f;mine.duration=2.85f;mine.pattern=2;mine.triggered=false;append(mine);}}break;}
 case Skill::TwinHunt:{
  const bool headhunt=fusion_form(skill)==1;effect.angle+=dt*1.3f;Vec2 center=player.pos,point=effect.end;size_t target=none;
  if(headhunt){target=aim(player.pos,550);point=effect.end;if(target!=none||altar){Vec2 delta=point-player.pos;center+=normalized(delta)*std::min(220.f,length(delta)*.55f);}}
  effect.pos=center+Vec2{std::cos(effect.angle)*(headhunt?50.f:95.f),std::sin(effect.angle)*(headhunt?38.f:65.f)};
  effect.end=center+Vec2{std::cos(-effect.angle-pi)*(headhunt?60.f:110.f),std::sin(-effect.angle-pi)*(headhunt?42.f:75.f)};
  effect.tick-=dt;if(effect.tick<=0){effect.tick=headhunt?1.2f:.85f;Vec2 from=effect.pattern%2==0?effect.pos:effect.end;
   if(!headhunt){target=priority_enemy(from,550,effect.target_id);if(target==none)target=priority_enemy(from,550);altar=altar_target(skill,from,550,target,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;effect.target_id=altar||target==none?0:enemies[target].id;if(altar||target!=none)point=altar?altar->pos:enemies[target].pos;}
   if(target!=none||altar){if(!altar){auto& enemy=enemies[target];enemy.mark_remaining=std::max(enemy.mark_remaining,headhunt?2.f:1.8f);enemy.mark_source=skill;}
    if(headhunt){if(skill_bolt(skill,effect.pos,point-effect.pos,effect.damage*1.15f,430,8,1,effect.target_id))projectiles.back().altar_target_id=effect.altar_target_id;if(skill_bolt(skill,effect.end,point-effect.end,effect.damage*1.15f,430,8,1,effect.target_id))projectiles.back().altar_target_id=effect.altar_target_id;}
    else if(skill_bolt(skill,from,point-from,effect.damage*1.35f,400,10,2,effect.target_id))projectiles.back().altar_target_id=effect.altar_target_id;
    effect.pattern=1-effect.pattern;
   }
  }break;}
 case Skill::PrismShot:effect.pos=player.pos;if(effect.age<.5f)aim(player.pos,650);if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;effect.pattern=1;skill_bolt(skill,effect.pos,effect.end-effect.pos,effect.damage*4,600,effect.radius,1,effect.target_id,3);}break;
 default:break;
 }
 return true;
}
}

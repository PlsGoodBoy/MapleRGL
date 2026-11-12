#include "battle_art.hpp"
#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr size_t none=std::numeric_limits<size_t>::max();
float line_distance(Vec2 point,Vec2 from,Vec2 to){Vec2 delta=to-from;float square=delta.x*delta.x+delta.y*delta.y;float t=std::clamp(((point-from).x*delta.x+(point-from).y*delta.y)/std::max(.001f,square),0.f,1.f);return length(point-(from+delta*t));}
EffectKind effect_for(Skill skill){return EffectKind(int(EffectKind::BattleFocus)+int(skill)-int(Skill::BattleFocus));}
bool lureable(EnemyKind kind){return kind==EnemyKind::Slime||kind==EnemyKind::Mushroom||kind==EnemyKind::BlueMushroom||kind==EnemyKind::Splitter||kind==EnemyKind::Minion;}
}
size_t Game::priority_enemy(Vec2 origin,float range,uint32_t omit){
 size_t best=none;float score=-std::numeric_limits<float>::max();
 for(auto index:nearby(origin,range)){const auto& enemy=enemies[index];if(enemy.id==omit||!enemy_targetable(enemy))continue;
  // High-value shots favor exposed mechanisms and major threats, while
  // distance still lets the player bring a nearby node into priority.
  float value=(enemy.boss_owner?270.f:enemy.kind==EnemyKind::Boss?220.f:enemy.kind==EnemyKind::Elite?170.f:enemy.kind==EnemyKind::Support?140.f:enemy.kind==EnemyKind::Ranger||enemy.kind==EnemyKind::GroundController?110.f:0.f)+(enemy.mark_remaining>0&&skill_status(enemy.mark_source).usable()?150.f:0.f)+std::min(50.f,enemy.max_hp*.02f)-length(enemy.pos-origin)*.7f;
  if(value>score){score=value;best=index;}}
 return best;
}
void Game::skill_area(Vec2 origin,float radius,float damage,Skill source,float knockback,bool can_proc){
 emit_skill_visual(FxStage::Disk,source,origin,origin,radius,0,knockback>=1.5f);
 damage_scene_area(origin,radius,damage,source);
 for(auto index:nearby(origin,radius+70))if(length(enemies[index].pos-origin)<=radius+enemies[index].radius)hurt_enemy(index,damage,enemies[index].pos-origin,knockback,source,can_proc,can_proc);
}
void Game::skill_line(Vec2 from,Vec2 to,float width,float damage,Skill source,float knockback,bool can_proc){
 emit_skill_visual(FxStage::Line,source,from,to,width,0,knockback>=1.5f);
 Vec2 midpoint=(from+to)*.5f;float range=length(to-from)*.5f+width+70;
 for(auto index:nearby(midpoint,range))if(line_distance(enemies[index].pos,from,to)<=width+enemies[index].radius)hurt_enemy(index,damage,to-from,knockback,source,can_proc,can_proc);
 for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone.hp>0&&line_distance(zone.pos,from,to)<=width+24)damage_pressure_facilities(zone.pos,0,damage);
 if(config.enable_scene_objects)for(size_t index=0;index<scene_objects.size();++index)if(line_distance(scene_objects[index].pos,from,to)<=width+scene_objects[index].radius)hurt_scene_object(index,damage,source);
}
void Game::skill_cone(Vec2 origin,Vec2 direction,float radius,float half_angle,float damage,Skill source,float knockback){
 emit_skill_visual(FxStage::Cone,source,origin,origin+direction*radius,radius,half_angle);
 direction=normalized(direction);
 for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone.hp>0){Vec2 delta=zone.pos-origin,axis=normalized(delta);if(length(delta)<=radius+24&&axis.x*direction.x+axis.y*direction.y>=std::cos(half_angle))damage_pressure_facilities(zone.pos,0,damage);}
 for(auto index:nearby(origin,radius+70)){Vec2 delta=enemies[index].pos-origin,axis=normalized(delta);if(length(delta)<=radius+enemies[index].radius&&(length(delta)<enemies[index].radius||axis.x*direction.x+axis.y*direction.y>=std::cos(half_angle)))hurt_enemy(index,damage,delta,knockback,source);}
 if(config.enable_scene_objects)for(size_t index=0;index<scene_objects.size();++index){Vec2 delta=scene_objects[index].pos-origin,axis=normalized(delta);if(length(delta)<=radius+scene_objects[index].radius&&axis.x*direction.x+axis.y*direction.y>=std::cos(half_angle))hurt_scene_object(index,damage,source);}
}
bool Game::skill_bolt(Skill source,Vec2 origin,Vec2 direction,float damage,float speed,float radius,int pierce,uint32_t target,int bounces,int blocks){
 if(projectiles.size()>=config.max_projectiles||length(direction)<.001f||(source==Skill::TwinHunt&&(smoke_at(player.pos)||smoke_at(origin))))return false;
 Projectile projectile;projectile.skill=source;projectile.pos=origin;projectile.velocity=normalized(direction)*speed;projectile.damage=damage;projectile.radius=radius;projectile.pierce=pierce;projectile.target=target;projectile.bounces=bounces;projectile.blocks=blocks;projectile.lifetime=2.5f;add_projectile(projectile);return true;
}
void Game::apply_poison(size_t index,float damage,float duration,Skill source,int layers){
 auto& enemy=enemies[index];if(enemy.hp<=0)return;
 if(enemy.poison_remaining<=0||enemy.poison_source!=source){enemy.poison_layers=0;enemy.poison_tick=.65f;enemy.poison_spread=false;}
 enemy.poison_layers=std::min(3,enemy.poison_layers+layers);enemy.poison_damage=damage;enemy.poison_remaining=std::max(enemy.poison_remaining,duration);enemy.poison_source=source;
}
void Game::apply_frost(Enemy& enemy,float duration,Skill source,int layers){
 if(enemy.hp<=0)return;if(enemy.frost_remaining<=0)enemy.frost_layers=0;
 enemy.frost_layers=std::min(3,enemy.frost_layers+layers);enemy.frost_source=source;enemy.frost_remaining=std::max(enemy.frost_remaining,duration);if(control_immune(enemy))return;enemy.slow_remaining=std::max(enemy.slow_remaining,duration);enemy.slow_factor=enemy.kind==EnemyKind::Boss?.8f:.5f;
}
void Game::apply_burn(Enemy& enemy,float damage,float duration,Skill source){
 if(enemy.hp<=0)return;if(enemy.burn_remaining<=0||damage>=enemy.burn_damage){enemy.burn_damage=damage;enemy.burn_source=source;}enemy.burn_remaining=std::max(enemy.burn_remaining,duration);
}
void Game::on_advanced_hit(Skill source){
 if(source==Skill::Count||!skill_attack_available(source,player.pos)||source==Skill::BattleFocus||source==Skill::BattleBlades||source==Skill::SpiritJudgement||focus_hit_cooldown_>0)return;
 if(effective_skill_level(Skill::BattleFocus)>0||effective_skill_level(Skill::BattleBlades)>0||effective_skill_level(Skill::SpiritJudgement)>0){battle_focus=std::min(100.f,battle_focus+12);focus_hit_cooldown_=.18f;}
}
void Game::update_skill_status(size_t index,float dt){
 auto& enemy=enemies[index];enemy.lure_remaining=std::max(0.f,enemy.lure_remaining-dt);enemy.thermal_lock=std::max(0.f,enemy.thermal_lock-dt);
 if(enemy.frost_remaining<=0){enemy.frost_layers=0;enemy.frost_source=Skill::Count;}
 if(enemy.hp<=0||enemy.poison_remaining<=0)return;
 const Skill source=enemy.poison_source;
 if(source==Skill::Count||skills[int(source)]<=0||!skill_allowed(source)){enemy.poison_remaining=0;enemy.poison_layers=0;return;}
 enemy.poison_remaining=std::max(0.f,enemy.poison_remaining-dt);enemy.poison_tick-=dt;
 if(enemy.poison_tick<=0){enemy.poison_tick=.65f;if(!skill_sealed(source)&&effective_skill_level(source)>0)hurt_enemy(index,enemy.poison_damage*enemy.poison_layers,{},0,source,false,false);
  // One transmission per original poison. A transmitted infection cannot transmit again.
  if(skill_attack_available(source,player.pos)&&enemy.poison_layers==3&&!enemy.poison_spread){enemy.poison_spread=true;size_t target=none;float best=90;
   for(size_t other=0;other<enemies.size();++other){const auto& candidate=enemies[other];if(other==index||candidate.hp<=0||candidate.poison_remaining>0)continue;float distance=length(candidate.pos-enemy.pos);if(distance<best){best=distance;target=other;}}
   if(target!=none){apply_poison(target,enemy.poison_damage,1.5f,source,1);enemies[target].poison_spread=true;}
  }
 }
 if(enemy.poison_remaining<=0){enemy.poison_layers=0;enemy.poison_source=Skill::Count;}
}
void Game::lure_enemies(Effect& effect){
 int count=0;for(auto index:nearby(effect.pos,effect.radius)){auto& enemy=enemies[index];if(displacement_immune(enemy)||!lureable(enemy.kind)||enemy.windup>0||enemy.action_remaining>0)continue;if(count++>=8)break;enemy.lure_remaining=.65f;enemy.lure_point=effect.pos;enemy.lure_source=effect.source;}effect.count=std::min(count,8);
}
void Game::pull_enemies(const Effect& effect,float dt,float speed,int limit){
 int count=0;for(auto index:nearby(effect.pos,effect.radius)){auto& enemy=enemies[index];if(count++>=limit)break;float distance=length(effect.pos-enemy.pos);float actual=speed;if(enemy.action_remaining>0)actual=0;displace_enemy(enemy,normalized(effect.pos-enemy.pos)*std::min(std::max(0.f,distance-18),actual*dt));}
}
bool Game::cast_advanced(Skill skill){
 if(skill>=Skill::BattleBlades)return cast_advanced_fusion(skill);
 const int level=effective_skill_level(skill);const float base=skill_power(skill);if(effects.size()>=config.max_effects)return false;
 if(std::any_of(effects.begin(),effects.end(),[&](const Effect& effect){return effect.source==skill&&effect.pattern==0&&effect.age<effect.duration;}))return false;
 const float range=(skill==Skill::Judgement||skill==Skill::Sniper||skill==Skill::SilverEagle)?520.f:420.f;size_t target=resonance_target(skill,player.pos,range);const DangerZone* altar=altar_target(skill,player.pos,range,target);
 Vec2 point=altar?altar->pos:target==none?player.pos:enemies[target].pos;Vec2 direction=target==none&&!altar?Vec2{player.facing_left?-1.f:1.f,0}:normalized(point-player.pos);
 Effect effect;effect.kind=effect_for(skill);effect.source=skill;effect.pos=player.pos;effect.end=target==none&&!altar?player.pos+direction*180:point;effect.damage=base;effect.duration=.5f;effect.target_id=altar||target==none?0:enemies[target].id;effect.altar_target_id=altar?altar->id:0;
 switch(skill){
 case Skill::BattleFocus:if(battle_focus<100||(target==none&&!altar))return false;battle_focus=0;effect.radius=(145+8*level)*area_multiplier;effect.angle=.9f;effect.delay=.2f;effect.end=player.pos+direction*effect.radius;break;
 case Skill::SpearCombo:if(target==none&&!altar)return false;effect.end=player.pos+direction*(150+12*level)*area_multiplier;effect.radius=14*area_multiplier;effect.count=3;effect.duration=.7f;break;
 case Skill::HolyShield:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base*1.25f,400,16*area_multiplier,5,effect.target_id,0,3);
 case Skill::Judgement:if(target==none&&!altar)return false;effect.pos=effect.end;effect.radius=(65+5*level)*area_multiplier;effect.delay=.65f;effect.duration=1;break;
 case Skill::DragonThrust:if(target==none&&!altar)return false;if(!skill_bolt(skill,player.pos,direction,base*2,470,22*area_multiplier,6,effect.target_id))return false;effect.end=player.pos+direction*330;effect.radius=22*area_multiplier;effect.damage=0;effect.duration=.3f;break;
 case Skill::EarthShatter:effect.end=player.pos+direction*240*area_multiplier;effect.radius=38*area_multiplier;effect.duration=.25f;effect.triggered=true;for(int part=1;part<=4&&effects.size()+1<config.max_effects;++part){Effect node=effect;node.pos=player.pos+direction*(part*60*area_multiplier);node.end=node.pos;node.pattern=1;node.delay=(part-1)*.18f;node.duration=node.delay+1.4f;node.triggered=false;add_effect(node);}break;
 case Skill::PoisonMist:effect.pos=target==none&&!altar?player.pos:effect.end;effect.radius=(105+7*level)*area_multiplier;effect.duration=4*duration_multiplier;break;
 case Skill::IceLance:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base*1.4f,430,10*area_multiplier,5,effect.target_id);
 case Skill::HolyLight:effect.radius=(110+8*level)*area_multiplier;effect.delay=.3f;effect.duration=.65f;break;
 case Skill::HolyDragon:effect.radius=100;effect.duration=skill_interval(skill)+.1f;effect.tick=.25f;break;
 case Skill::FireBeast:effect.radius=160*area_multiplier;effect.duration=skill_interval(skill)+.1f;effect.tick=.3f;break;
 case Skill::MagicVortex:effect.pos=target==none&&!altar?player.pos:effect.end;effect.radius=(140+8*level)*area_multiplier;effect.duration=2.6f*duration_multiplier+.3f;break;
 case Skill::StormShot:if(target==none&&!altar)return false;effect.count=std::min(16,7+level+extra_projectiles);effect.energy=float(effect.count);effect.radius=7;effect.duration=effect.count*.13f+.25f;break;
 case Skill::Sniper:if(target==none&&!altar)return false;effect.radius=11*area_multiplier;effect.delay=.85f;effect.duration=1.15f;break;
 case Skill::IceArrow:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base*1.2f,450,8,2+level/3,effect.target_id);
 case Skill::BlastArrow:if(target==none&&!altar)return false;return skill_bolt(skill,player.pos,direction,base*.8f,400,10,1,effect.target_id);
 case Skill::SilverEagle:effect.radius=90;effect.duration=skill_interval(skill)+.1f;effect.tick=.35f;break;
 case Skill::Decoy:effect.pos=player.pos+direction*70;effect.end=effect.pos;effect.radius=(160+8*level)*area_multiplier;effect.duration=3.5f*duration_multiplier+.25f;break;
 default:return false;
 }
 add_effect(effect);return true;
}
bool Game::update_advanced_effect(Effect& effect,float dt,std::vector<Effect>& spawned){
 if(effect.source<Skill::BattleFocus||effect.source>=Skill::Count)return false;
 if(effect.source>=Skill::BattleBlades)return update_advanced_fusion_effect(effect,dt,spawned);
 if(effect.kind!=effect_for(effect.source))return false;
 const Skill skill=effect.source;const int level=effective_skill_level(skill);
 auto append=[&](Effect child){if(effects.size()+spawned.size()<config.max_effects)spawned.push_back(child);};
 auto target_by_id=[&](){return this->target_by_id(effect.target_id,player.pos,520*area_multiplier);};
 switch(skill){
 case Skill::BattleFocus:if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_cone(effect.pos,effect.end-effect.pos,effect.radius,effect.angle,effect.damage*3.6f,skill,2);}break;
 case Skill::SpearCombo:if(effect.pattern>0)break;effect.tick-=dt;if(effect.tick<=0&&effect.count>0){effect.tick=.2f;int beat=3-effect.count--;skill_line(effect.pos,effect.end,effect.radius,effect.damage*(.9f+.15f*beat),skill,.8f);Effect stab=effect;stab.age=0;stab.duration=.16f;stab.pattern=beat+1;stab.triggered=true;stab.damage=0;append(stab);}break;
 case Skill::Judgement:if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage*3.5f,skill,1.5f);}break;
 case Skill::EarthShatter:if(effect.pattern==1&&effect.age>=effect.delay){effect.triggered=true;effect.tick-=dt;if(effect.tick<=0){effect.tick=.45f;skill_area(effect.pos,effect.radius,effect.damage*.6f,skill,.5f);}}break;
 case Skill::PoisonMist:effect.tick-=dt;if(effect.tick<=0){effect.tick=.65f;damage_pressure_facilities(effect.pos,effect.radius,effect.damage*.2f);for(auto index:nearby(effect.pos,effect.radius+60))if(length(enemies[index].pos-effect.pos)<=effect.radius+enemies[index].radius){hurt_enemy(index,effect.damage*.2f,{},0,skill);apply_poison(index,effect.damage*.35f,3,skill);}}break;
 case Skill::IceLance:if(effect.pattern==1){effect.tick-=dt;if(effect.tick<=0){effect.tick=.5f;skill_area(effect.pos,effect.radius,effect.damage*.35f,skill,.1f);for(auto index:nearby(effect.pos,effect.radius+50))if(length(enemies[index].pos-effect.pos)<=effect.radius+enemies[index].radius)apply_frost(enemies[index],.8f,skill);}}break;
 case Skill::HolyLight:if(!effect.triggered){effect.pos=player.pos;if(effect.age>=effect.delay){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage*1.8f,skill,.3f);if(holy_support_cooldown_<=0){heal_player(2);holy_support_cooldown_=3;}}}break;
 case Skill::HolyDragon:case Skill::FireBeast:{
  if(effect.pattern==1){if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;if(skill==Skill::HolyDragon)skill_line(effect.pos,effect.end,effect.radius,effect.damage*1.2f,skill,.3f);else{skill_cone(effect.pos,effect.end-effect.pos,effect.radius,effect.angle,effect.damage*1.3f,skill,.3f);Vec2 direction=normalized(effect.end-effect.pos);for(auto index:nearby(effect.pos,effect.radius+60)){Vec2 delta=enemies[index].pos-effect.pos,axis=normalized(delta);if(length(delta)<=effect.radius+enemies[index].radius&&axis.x*direction.x+axis.y*direction.y>=std::cos(effect.angle))apply_burn(enemies[index],effect.damage*.3f,2,skill);}}}break;}
  effect.angle+=dt*1.2f;effect.pos=skill==Skill::HolyDragon?player.pos+Vec2{std::cos(effect.angle)*100,std::sin(effect.angle)*70}:player.pos+Vec2{player.facing_left?65.f:-65.f,35};effect.tick-=dt;
  if(effect.tick<=0){effect.tick=skill==Skill::HolyDragon?1.25f:1.4f;size_t target=resonance_target(skill,effect.pos,340);const DangerZone* altar=altar_target(skill,effect.pos,(skill==Skill::HolyDragon?230.f:160.f)*area_multiplier+24,target,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;if(target!=none||altar){effect.end=altar?altar->pos:enemies[target].pos;Vec2 direction=normalized(effect.end-effect.pos);effect.visual_attack_age=0;effect.visual_aim=direction;Effect breath=effect;breath.age=0;breath.pattern=1;breath.pos=effect.pos;breath.end=effect.pos+direction*(skill==Skill::HolyDragon?230.f:160.f)*area_multiplier;breath.radius=(skill==Skill::HolyDragon?18.f:160.f)*area_multiplier;breath.angle=.65f;breath.delay=skill==Skill::HolyDragon?.15f:.2f;breath.duration=skill==Skill::HolyDragon?.45f:.65f;breath.triggered=false;append(breath);}}
  break;}
 case Skill::MagicVortex:if(!effect.triggered){pull_enemies(effect,dt,65);effect.tick-=dt;if(effect.tick<=0){effect.tick=.45f;skill_area(effect.pos,effect.radius,effect.damage*.25f,skill,0);}if(effect.age>=effect.duration-.3f){effect.triggered=true;effect.pattern=1;skill_area(effect.pos,effect.radius,effect.damage*2.2f,skill,1.2f);}}break;
 case Skill::StormShot:{effect.pos=player.pos;effect.tick-=dt;if(effect.tick>0||effect.count<=0)break;effect.tick=.13f;int fired=int(effect.energy)-effect.count;size_t target=target_by_id();if(target==none||fired%4==0)target=nearest(player.pos,520);const DangerZone* altar=altar_target(skill,player.pos,520,target,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;effect.target_id=altar||target==none?0:enemies[target].id;if(target!=none||altar){effect.end=altar?altar->pos:enemies[target].pos;if(skill_bolt(skill,player.pos,effect.end-player.pos,effect.damage*.65f,470,7,1,effect.target_id))--effect.count;}break;}
 case Skill::Sniper:effect.pos=player.pos;if(effect.age<.5f){size_t target=target_by_id();if(target!=none)effect.end=enemies[target].pos;}if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_bolt(skill,effect.pos,effect.end-effect.pos,effect.damage*5,620,effect.radius,2,effect.target_id);}break;
 case Skill::SilverEagle:{effect.angle+=dt*1.8f;effect.pos=player.pos+Vec2{std::cos(effect.angle)*90,std::sin(effect.angle)*65};effect.tick-=dt;if(effect.tick<=0){effect.tick=1.5f;size_t target=priority_enemy(effect.pos,500);const DangerZone* altar=altar_target(skill,effect.pos,500,target,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;if(target!=none||altar){effect.target_id=altar?0:enemies[target].id;effect.end=altar?altar->pos:enemies[target].pos;if(!altar){enemies[target].mark_remaining=std::max(enemies[target].mark_remaining,2.f);enemies[target].mark_source=skill;}effect.visual_attack_age=0;effect.visual_aim=effect.end-effect.pos;Vec2 muzzle=battle_muzzle(skill,effect.pos,effect.visual_aim);skill_bolt(skill,muzzle,effect.end-muzzle,effect.damage*1.45f,400,9,1,effect.target_id);}}break;}
 case Skill::Decoy:if(!effect.triggered){effect.tick-=dt;if(effect.tick<=0){effect.tick=.5f;lure_enemies(effect);}if(effect.age>=effect.duration-.25f){effect.triggered=true;effect.pattern=1;effect.radius=70*area_multiplier;skill_area(effect.pos,effect.radius,effect.damage*1.4f,skill,.8f);}}break;
 default:break;
 }
 return true;
}
void Game::update_advanced_projectile(Projectile& projectile,float){
 const Skill skill=projectile.skill;const float snapshot=projectile.power_snapshot>0?projectile.power_snapshot:skill_power(skill);const int snapshot_level=projectile.level_snapshot>0?projectile.level_snapshot:effective_skill_level(skill);
 bool shield=skill==Skill::HolyShield||skill==Skill::EchoShield||skill==Skill::Sanctuary;
 if(shield){
  for(auto& hostile:effects)if(projectile.blocks>0&&hostile.hostile&&hostile.kind==EffectKind::EnemyShot&&hostile.damage>0&&hostile.age<hostile.duration&&length(hostile.pos-projectile.pos)<=hostile.radius+projectile.radius+8){resonance_absorb(hostile.damage,skill);hostile.damage=0;hostile.triggered=true;hostile.age=hostile.duration;--projectile.blocks;}
  if(!projectile.returned&&projectile.age>=.45f){projectile.returned=true;projectile.hit_ids.clear();projectile.pierce=5;projectile.lifetime=1.7f;
   if(skill==Skill::Sanctuary){Effect field;field.kind=EffectKind::Sanctuary;field.source=skill;field.pos=projectile.pos;field.radius=(125+6*snapshot_level)*area_multiplier;field.duration=3.4f*duration_multiplier;field.damage=snapshot;field.pattern=1;add_effect(field);}
  }
  if(projectile.returned){projectile.velocity=normalized(player.pos-projectile.pos)*450;if(length(projectile.pos-player.pos)<projectile.radius+13||projectile.age>=1.65f){
    if(skill==Skill::EchoShield){Effect wave;wave.kind=EffectKind::EchoShield;wave.source=skill;wave.pos=player.pos;wave.radius=(90+7*snapshot_level)*area_multiplier;wave.duration=.4f;wave.damage=snapshot*(1.8f+.45f*(3-projectile.blocks));wave.pattern=1;add_effect(wave);}
    projectile.pierce=0;
   }}
 }
 if(skill==Skill::AzureRift){if(!projectile.returned&&projectile.age>=.6f){projectile.returned=true;projectile.hit_ids.clear();projectile.pierce=7;projectile.damage=snapshot*1.35f;projectile.lifetime=1.5f;}if(projectile.returned){projectile.velocity=normalized(player.pos-projectile.pos)*470;if(length(projectile.pos-player.pos)<20)projectile.pierce=0;}}
 if(skill==Skill::SilverEagle||skill==Skill::TwinHunt){
  const float range=550*area_multiplier;size_t target=target_by_id(projectile.target,projectile.pos,range);
  if(target!=none&&std::find(projectile.hit_ids.begin(),projectile.hit_ids.end(),enemies[target].id)!=projectile.hit_ids.end())target=none;
  if(target==none){float best=range;for(auto index:nearby(projectile.pos,range))if(enemy_targetable(enemies[index])&&std::find(projectile.hit_ids.begin(),projectile.hit_ids.end(),enemies[index].id)==projectile.hit_ids.end()){float distance=length(enemies[index].pos-projectile.pos);if(distance<best){best=distance;target=index;}}}
  const DangerZone* altar=altar_target(skill,projectile.pos,range,target,projectile.altar_target_id);projectile.altar_target_id=altar?altar->id:0;projectile.target=altar||target==none?0:enemies[target].id;
  if(altar||target!=none)projectile.velocity=normalized(projectile.velocity*.75f+normalized((altar?altar->pos:enemies[target].pos)-projectile.pos)*400*.25f)*400;
 }
}
void Game::on_advanced_projectile_hit(Projectile& projectile,size_t index){
 const Skill skill=projectile.skill;auto& enemy=enemies[index];const float base=projectile.power_snapshot>0?projectile.power_snapshot:skill_power(skill);const int level=projectile.level_snapshot>0?projectile.level_snapshot:effective_skill_level(skill);
 Effect effect;effect.source=skill;effect.pos=enemy.pos;effect.end=enemy.pos;effect.damage=base;effect.pattern=1;effect.duration=.3f;effect.radius=35*area_multiplier;
 if(skill==Skill::DragonThrust){enemy.armor_break_remaining=3;enemy.armor_source=skill;return;}
 if(skill==Skill::IceArrow||skill==Skill::IceLance||skill==Skill::ThunderLance||skill==Skill::FrozenBarrage){apply_frost(enemy,2.3f,skill);effect.kind=effect_for(skill);
  if(skill==Skill::IceLance){effect.radius=24*area_multiplier;effect.duration=1.4f;add_effect(effect);}else if(skill==Skill::ThunderLance&&projectile.blocks>0){--projectile.blocks;effect.end=enemy.pos;effect.pos=player.pos;if(projectile.hit_ids.size()>1){uint32_t previous=projectile.hit_ids[projectile.hit_ids.size()-2];for(const auto& other:enemies)if(other.id==previous){effect.pos=other.pos;break;}}effect.radius=18*area_multiplier;effect.delay=.35f+.1f*(3-projectile.blocks);effect.duration=effect.delay+.4f;add_effect(effect);}else if(skill==Skill::IceArrow){effect.damage=0;add_effect(effect);}return;
 }
 if(skill==Skill::BlastArrow){effect.kind=EffectKind::BlastArrow;effect.radius=(55+5*level)*area_multiplier;effect.triggered=true;effect.damage=0;skill_area(enemy.pos,effect.radius,base*1.5f,skill,1.f,false);add_effect(effect);return;}
 if((skill==Skill::SiegeBolt||skill==Skill::AzureRift)&&projectile.blocks>0){--projectile.blocks;effect.kind=effect_for(skill);effect.radius=(skill==Skill::SiegeBolt?48.f:38.f)*area_multiplier;effect.delay=.25f+.14f*(3-projectile.blocks);effect.duration=effect.delay+.35f;effect.damage=base*(skill==Skill::SiegeBolt?1.25f:.65f);add_effect(effect);}
}
}

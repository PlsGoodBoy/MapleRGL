#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
namespace {
constexpr float cross_pi=3.14159265358979323846f;
constexpr size_t cross_none=std::numeric_limits<size_t>::max();
Vec2 turn_cross(Vec2 vector,float angle){return {vector.x*std::cos(angle)-vector.y*std::sin(angle),vector.x*std::sin(angle)+vector.y*std::cos(angle)};}
float cross_segment(Vec2 point,Vec2 from,Vec2 to){Vec2 delta=to-from;float square=delta.x*delta.x+delta.y*delta.y;float t=std::clamp(((point-from).x*delta.x+(point-from).y*delta.y)/std::max(.001f,square),0.f,1.f);return length(point-(from+delta*t));}
}

bool Game::cast_cross_fusion(Skill skill){
 if(skill<Skill::ThunderPrison||skill>Skill::FlowingFire)return false;
 if(skill==Skill::PhoenixAssault)return false;
 if(std::any_of(effects.begin(),effects.end(),[&](const Effect& effect){return effect.source==skill&&effect.pattern==0&&effect.age<effect.duration;}))return false;
 const int level=effective_skill_level(skill);const float damage=skill_power(skill);const size_t target=skill_role_target(skill,player.pos,560*area_multiplier);const DangerZone* altar=altar_target(skill,player.pos,560*area_multiplier,target);
 if(target==cross_none&&!altar&&skill!=Skill::FrostArmor)return false;
 Vec2 point=altar?altar->pos:target==cross_none?player.pos:enemies[target].pos;Vec2 direction=normalized(point-player.pos);if(length(direction)<.01f)direction={player.facing_left?-1.f:1.f,0};
 Effect effect;effect.source=skill;effect.pos=player.pos;effect.end=point;effect.angle=std::atan2(direction.y,direction.x);effect.damage=damage;effect.radius=90*area_multiplier;effect.duration=4;effect.target_id=altar||target==cross_none?0:enemies[target].id;effect.altar_target_id=altar?altar->id:0;
 switch(skill){
 case Skill::ThunderPrison:effect.kind=EffectKind::ThunderPrison;effect.duration=4.6f;effect.count=3;effect.energy=float(combat_diagnostics.actual_hits);effect.secondary_tick=.7f;break;
 case Skill::FrostArmor:effect.kind=EffectKind::FrostArmor;effect.duration=3600;effect.radius=(115+6*level)*area_multiplier;effect.tick=0;effect.energy=0;break;
 case Skill::MiasmaDragon:effect.kind=EffectKind::MiasmaDragon;effect.radius=(255+10*level)*area_multiplier;effect.duration=3.1f;effect.delay=2.5f;break;
 case Skill::DivineVerdict:effect.kind=EffectKind::DivineVerdict;effect.pos=point;effect.radius=(120+6*level)*area_multiplier;effect.duration=3.6f;effect.count=0;break;
 case Skill::FrozenDragonSpear:effect.kind=EffectKind::FrozenDragonSpear;effect.duration=2.7f;effect.count=6;effect.radius=175*area_multiplier;break;
 case Skill::MirrorShield:{bool made=skill_bolt(skill,player.pos,direction,damage*1.15f,340,18,6,effect.target_id,0,3);if(made){projectiles.back().lifetime=1.9f;projectiles.back().altar_target_id=effect.altar_target_id;return true;}return false;}
 case Skill::DragonBreaker:effect.kind=EffectKind::DragonBreaker;effect.radius=110*area_multiplier;effect.delay=.85f;effect.duration=1.25f;break;
 case Skill::FissureArrows:effect.kind=EffectKind::FissureArrows;effect.end=effect.pos+direction*(330*area_multiplier);effect.radius=28*area_multiplier;effect.delay=.5f;effect.duration=2.8f;effect.count=0;break;
 case Skill::SpearBolt:effect.kind=EffectKind::SpearBolt;effect.duration=2.4f;effect.count=6;effect.radius=170*area_multiplier;break;
 case Skill::StormArrowStream:effect.kind=EffectKind::StormArrowStream;effect.duration=2;effect.count=10;effect.energy=0;break;
 case Skill::PoisonHuntingGround:effect.kind=EffectKind::PoisonHuntingGround;effect.pos=player.pos+Vec2{player.facing_left?35.f:-35.f,18};effect.radius=(105+5*level)*area_multiplier;effect.duration=5*duration_multiplier;effect.delay=effect.duration-.15f;break;
 case Skill::DragonPhoenix:effect.kind=EffectKind::DragonPhoenix;effect.pos=point;effect.radius=(fusion_form(skill)==1?80.f:160.f)*area_multiplier;effect.duration=4.8f;effect.count=0;effect.secondary_tick=.25f;break;
 case Skill::GlacialSniper:effect.kind=EffectKind::GlacialSniper;effect.radius=15*area_multiplier;effect.delay=1.05f;effect.duration=1.45f;break;
 case Skill::FlowingFire:effect.kind=EffectKind::FlowingFire;effect.pos=point;effect.radius=(110+6*level)*area_multiplier;effect.duration=3.6f;effect.count=3;effect.secondary_tick=.2f;break;
 default:return false;
 }
 if(effects.size()>=config.max_effects)return false;add_effect(effect);return true;
}

bool Game::update_cross_effect(Effect& effect,float dt,std::vector<Effect>& spawned){
 if(effect.kind<EffectKind::ThunderPrison||effect.kind>EffectKind::FlowingFire)return false;
 const Skill skill=effect.source;const int level=effective_skill_level(skill);
 auto visual=[&](EffectKind kind,Vec2 from,Vec2 to,float radius,float duration,int pattern=1){Effect flash;flash.kind=kind;flash.source=skill;flash.pos=from;flash.end=to;flash.radius=radius;flash.duration=duration;flash.pattern=pattern;flash.proc_generated=true;if(effects.size()+spawned.size()<config.max_effects)spawned.push_back(flash);};
 const DangerZone* altar=nullptr;
 auto target=[&](bool strong=true){size_t index=target_by_id(effect.target_id,player.pos,600*area_multiplier);if(index==cross_none)index=strong?skill_role_target(skill,player.pos,600*area_multiplier):nearest(player.pos,550*area_multiplier);altar=altar_target(skill,player.pos,600*area_multiplier,index,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;effect.target_id=altar||index==cross_none?0:enemies[index].id;if(altar||index!=cross_none)effect.end=altar?altar->pos:enemies[index].pos;return index;};
 auto bolt=[&](Vec2 origin,Vec2 aim,float factor,float speed=480,int pierce=3,int bounces=0){if(!skill_bolt(skill,origin,normalized(aim-origin),effect.damage*factor,speed,9,pierce,effect.target_id,bounces))return false;projectiles.back().altar_target_id=effect.altar_target_id;return true;};
 if(effect.kind==EffectKind::ThunderPrison){
  if(effect.pattern==1){size_t index=target_by_id(effect.target_id,player.pos,650*area_multiplier);if(index!=cross_none)effect.pos=enemies[index].pos;return true;}
  if(effect.pattern!=0)return true;
  effect.tick-=dt;effect.secondary_tick-=dt;
  if(effect.tick<=0&&effect.count>0){effect.tick=.65f;const size_t index=target();if((index!=cross_none||altar)&&bolt(player.pos,effect.end,1.15f,390,4))--effect.count;}
  if(effect.secondary_tick<=0&&combat_diagnostics.actual_hits>uint64_t(effect.energy)){
   effect.secondary_tick=.6f;std::vector<Vec2> nodes;for(const auto& node:effects)if(node.source==skill&&node.pattern==1&&node.age<node.duration&&nodes.size()<4)nodes.push_back(node.pos);
   for(size_t i=1;i<nodes.size();++i){skill_line(nodes[i-1],nodes[i],14*area_multiplier,effect.damage*.9f,skill,.15f,false);visual(EffectKind::Lightning,nodes[i-1],nodes[i],14*area_multiplier,.28f);}
   effect.energy=float(combat_diagnostics.actual_hits);
  }return true;
 }
 if(effect.kind==EffectKind::FrostArmor){
  if(effect.pattern!=0)return true;effect.pos=player.pos;effect.tick-=dt;effect.secondary_tick=std::max(0.f,effect.secondary_tick-dt);
  if(effect.tick<=0){effect.tick=5.f;guard_charge=std::max(guard_charge,8.f+2*level);}
  if(effect.energy>=60){effect.energy-=60;skill_area(player.pos,effect.radius,effect.damage*2.7f,skill,1.4f);for(auto index:nearby(player.pos,effect.radius+55))if(length(enemies[index].pos-player.pos)<=effect.radius+enemies[index].radius)apply_frost(enemies[index],2.4f,skill,2);visual(EffectKind::Frost,player.pos,{},effect.radius,.65f);shield_remaining=std::max(shield_remaining,.4f);}
  return true;
 }
 if(effect.kind==EffectKind::MiasmaDragon){
  if(effect.pattern!=0)return true;const Vec2 direction{std::cos(effect.angle),std::sin(effect.angle)};
  if(effect.age<effect.delay){effect.tick-=dt;if(effect.tick<=0){effect.tick=.45f;skill_cone(effect.pos,direction,effect.radius,.58f,effect.damage*.4f,skill,.2f);int count=0;for(auto index:nearby(effect.pos,effect.radius+55)){auto& enemy=enemies[index];Vec2 delta=enemy.pos-effect.pos;float distance=length(delta);if(distance>effect.radius+enemy.radius||distance>.01f&&(delta.x*direction.x+delta.y*direction.y)/distance<std::cos(.58f))continue;if(count++>=14)break;apply_poison(index,effect.damage*.15f,2.6f,skill);}}}
  else if(!effect.triggered){effect.triggered=true;int count=0;for(auto index:nearby(effect.pos,effect.radius+55)){auto& enemy=enemies[index];if(enemy.poison_source!=skill||enemy.poison_layers<=0||count>=12)continue;const int layers=enemy.poison_layers;enemy.poison_remaining=0;enemy.poison_layers=0;++count;hurt_enemy(index,effect.damage*(.8f+.35f*layers),{},.25f,skill,false,false);visual(EffectKind::MiasmaDragon,enemy.pos,{},35*area_multiplier,.4f,1);}}
  return true;
 }
 if(effect.kind==EffectKind::DivineVerdict){
  if(effect.pattern!=0)return true;effect.tick-=dt;if(effect.tick>0)return true;effect.tick=.75f;const size_t index=target();if(index==cross_none&&!altar)return true;effect.pos=effect.end;
  if(effect.count<3){skill_area(effect.pos,effect.radius,effect.damage*.65f,skill,.2f);if(!altar){enemies[index].mark_source=skill;enemies[index].mark_remaining=3;effect.energy+=1;}++effect.count;visual(EffectKind::HolyLight,effect.pos,{},effect.radius,.4f);}
  else if(!effect.triggered){effect.triggered=true;skill_area(effect.pos,55*area_multiplier,effect.damage*(2+effect.energy*.55f),skill,1.5f);if(!altar)enemies[index].mark_remaining=0;visual(EffectKind::Judgement,effect.pos,{},55*area_multiplier,.6f,1);effect.duration=effect.age+.6f;}
  return true;
 }
 if(effect.kind==EffectKind::FrozenDragonSpear||effect.kind==EffectKind::SpearBolt){
  if(effect.pattern!=0)return true;effect.tick-=dt;if(effect.tick>0||effect.count<=0)return true;effect.tick=effect.kind==EffectKind::SpearBolt?.3f:.35f;
  const size_t index=target();if(index==cross_none&&!altar)return true;Vec2 point=effect.end,direction=normalized(point-player.pos);effect.pos=player.pos;effect.end=player.pos+direction*effect.radius;
  if(effect.count%2==0){skill_line(effect.pos,effect.end,18*area_multiplier,effect.damage*.95f,skill,.5f);visual(EffectKind::SpearCombo,effect.pos,effect.end,18*area_multiplier,.23f,1);}
  else if(bolt(effect.pos,point,effect.count==1?2.05f:1.2f,540,6)){projectiles.back().evolved=effect.count==1;}
  --effect.count;return true;
 }
 if(effect.kind==EffectKind::PhoenixAssault){
  if(effect.pattern!=0)return true;effect.tick-=dt;if(effect.tick>0)return true;effect.tick=.10f;
  const float progress=std::clamp((effect.age-.08f)/.42f,0.f,1.f);Vec2 direction=normalized(effect.end-effect.pos);if(length(direction)<.01f)return true;Vec2 side{-direction.y*35,direction.x*35};
  const Vec2 first=effect.pos+(effect.end-effect.pos)*progress+side,second=effect.end+(effect.pos-effect.end)*progress-side;
  skill_area(first,29*area_multiplier,effect.damage*.65f,skill,.2f);skill_area(second,29*area_multiplier,effect.damage*.65f,skill,.2f);return true;
 }
 if(effect.kind==EffectKind::MirrorShield){
  if(effect.pattern==1&&!effect.triggered){effect.triggered=true;const size_t index=target();if((index!=cross_none||altar)&&skill_bolt(skill,effect.pos,normalized(effect.end-effect.pos),effect.damage,470,8,1,effect.target_id,0,-1)){projectiles.back().proc_generated=true;projectiles.back().altar_target_id=effect.altar_target_id;}}
  return true;
 }
 if(effect.kind==EffectKind::DragonBreaker){
  if(effect.pattern!=0)return true;const size_t index=target();
  if(!effect.count&&effect.age>=.25f){effect.count=1;if(index!=cross_none||altar){if(!altar){enemies[index].mark_source=skill;enemies[index].mark_remaining=3;}skill_area(effect.end,effect.radius,effect.damage*.6f,skill,1.2f);}}
  if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;if(index!=cross_none||altar)bolt(player.pos,effect.end,4.3f,580,9);}
  return true;
 }
 if(effect.kind==EffectKind::FissureArrows){
  if(effect.pattern==1){if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage,skill,.4f);}return true;}
  if(effect.pattern!=0||effect.age<effect.delay)return true;
  if(!effect.triggered){effect.triggered=true;skill_line(effect.pos,effect.end,effect.radius,effect.damage*.85f,skill,.6f);}
  effect.tick-=dt;if(effect.tick<=0&&effect.count<5){effect.tick=.32f;Effect rain;rain.kind=effect.kind;rain.source=skill;rain.pattern=1;rain.pos=effect.pos+(effect.end-effect.pos)*((effect.count+.5f)/5);rain.radius=48*area_multiplier;rain.damage=effect.damage*1.35f;rain.delay=.32f;rain.duration=.72f;if(effects.size()+spawned.size()<config.max_effects)spawned.push_back(rain);++effect.count;}return true;
 }
 if(effect.kind==EffectKind::StormArrowStream){
  if(effect.pattern!=0)return true;effect.tick-=dt;if(effect.tick>0||effect.count<=0)return true;effect.tick=.13f;
  const size_t index=target(false);if((index!=cross_none||altar)&&bolt(player.pos,effect.end,.72f,520,1)){effect.energy+=1;projectiles.back().evolved=int(effect.energy)%3==0;--effect.count;}return true;
 }
 if(effect.kind==EffectKind::PoisonHuntingGround){
  if(effect.pattern!=0)return true;
  if(effect.age<effect.delay){lure_enemies(effect);effect.tick-=dt;if(effect.tick<=0){effect.tick=.5f;int count=0;for(auto index:nearby(effect.pos,effect.radius+55))if(length(enemies[index].pos-effect.pos)<=effect.radius+enemies[index].radius&&count++<12)apply_poison(index,effect.damage*.16f,2.2f,skill);}}
  else if(!effect.triggered){effect.triggered=true;int count=0;for(auto index:nearby(effect.pos,effect.radius+55)){auto& enemy=enemies[index];if(length(enemy.pos-effect.pos)>effect.radius+enemy.radius||count++>=12)continue;const int layers=enemy.poison_source==skill?enemy.poison_layers:0;hurt_enemy(index,effect.damage*(1.2f+.5f*layers),enemy.pos-effect.pos,.6f,skill,false,false);if(enemy.poison_source==skill){enemy.poison_layers=0;enemy.poison_remaining=0;}}visual(EffectKind::PoisonHuntingGround,effect.pos,{},effect.radius,.55f,1);}
  return true;
 }
 if(effect.kind==EffectKind::DragonPhoenix){
  if(effect.pattern!=0){if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_line(effect.pos,effect.end,effect.radius,effect.damage,skill,.4f);if(effect.pattern>=3){skill_area(effect.end,48*area_multiplier,effect.damage*.65f,skill,.5f,false);visual(EffectKind::HolyLight,effect.end,{},48*area_multiplier,.4f);}if(effect.pattern%2==0)for(auto index:nearby((effect.pos+effect.end)*.5f,length(effect.end-effect.pos)*.5f+effect.radius+55))if(cross_segment(enemies[index].pos,effect.pos,effect.end)<=effect.radius+enemies[index].radius)apply_burn(enemies[index],effect.damage*.15f,1.5f,skill);}return true;}
  effect.secondary_tick-=dt;if(effect.secondary_tick>0||effect.count>=4)return true;effect.secondary_tick=1.05f;
  const bool dive=fusion_form(skill)==1;
  {const size_t index=target();if(index==cross_none&&!altar)return true;effect.pos=effect.end;}
  const float angle=(effect.count%2)*cross_pi*.5f;Vec2 direction{std::cos(angle),std::sin(angle)};Effect sweep;sweep.kind=effect.kind;sweep.source=skill;sweep.pattern=effect.count%2+(dive?3:1);sweep.pos=effect.pos-direction*effect.radius;sweep.end=dive?effect.pos:effect.pos+direction*effect.radius;sweep.radius=(dive?20.f:32.f)*area_multiplier;sweep.damage=effect.damage*(dive?1.7f:1.5f);sweep.delay=.38f;sweep.duration=.9f;
  if(effects.size()+spawned.size()<config.max_effects)spawned.push_back(sweep);
  if(!dive&&effect.count%2){skill_area(effect.pos,75*area_multiplier,effect.damage*2.4f,skill,.6f,false);visual(EffectKind::HolyLight,effect.pos,{},75*area_multiplier,.5f);}
  ++effect.count;return true;
 }
 if(effect.kind==EffectKind::GlacialSniper){
  if(effect.pattern==1){if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage,skill,.8f,false);for(auto index:nearby(effect.pos,effect.radius+55))if(length(enemies[index].pos-effect.pos)<=effect.radius+enemies[index].radius)apply_frost(enemies[index],2,skill,2);}return true;}
  if(effect.pattern!=0)return true;const size_t index=target();if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;if((index!=cross_none||altar)&&bolt(player.pos,effect.end,3.6f,650,10)){auto& projectile=projectiles.back();projectile.radius=13*area_multiplier;projectile.lifetime=std::clamp(length(effect.end-player.pos)/650+.08f,.2f,.8f);projectile.blocks=1;}}
  return true;
 }
 if(effect.kind==EffectKind::FlowingFire){
  if(effect.pattern==0){effect.secondary_tick-=dt;effect.tick-=dt;if(effect.tick<=0){effect.tick=.6f;skill_area(effect.pos,effect.radius,effect.damage*.32f,skill,.05f);}
   if(effect.secondary_tick<=0&&effect.count>0){effect.secondary_tick=.85f;const size_t index=nearest(effect.pos,effect.radius+120);altar=altar_target(skill,effect.pos,effect.radius+120,index,effect.altar_target_id);effect.altar_target_id=altar?altar->id:0;effect.target_id=altar||index==cross_none?0:enemies[index].id;if((index!=cross_none||altar)&&bolt(player.pos,altar?altar->pos:enemies[index].pos,1.f,440,1))--effect.count;}
  }else if(!effect.triggered&&effect.age>=effect.delay){effect.triggered=true;skill_area(effect.pos,effect.radius,effect.damage,skill,.45f,false);int affected=0;for(auto index:nearby(effect.pos,effect.radius+55))if(length(enemies[index].pos-effect.pos)<=effect.radius+enemies[index].radius&&affected++<10)apply_burn(enemies[index],effect.damage*.18f,1.5f,skill);
   if(effect.count>0){size_t index=priority_enemy(effect.pos,180,effect.target_id);altar=altar_target(skill,effect.pos,180,index);if((index!=cross_none||altar)&&effects.size()+spawned.size()<config.max_effects){Effect next=effect;next.pos=altar?altar->pos:enemies[index].pos;next.target_id=altar?0:enemies[index].id;next.altar_target_id=altar?altar->id:0;next.pattern=2;next.count=0;next.age=0;next.delay=.45f;next.duration=.8f;next.triggered=false;next.damage*=.6f;spawned.push_back(next);}}
  }return true;
 }
 return true;
}

void Game::update_cross_projectile(Projectile& projectile,float dt){
 if(projectile.skill==Skill::MirrorShield&&projectile.blocks<0){size_t index=target_by_id(projectile.target,projectile.pos,550*area_multiplier);if(index==cross_none)index=priority_enemy(projectile.pos,550*area_multiplier);const DangerZone* altar=altar_target(projectile.skill,projectile.pos,550*area_multiplier,index,projectile.altar_target_id);projectile.altar_target_id=altar?altar->id:0;projectile.target=altar||index==cross_none?0:enemies[index].id;if(index!=cross_none||altar)projectile.velocity=normalized(projectile.velocity*.8f+normalized((altar?altar->pos:enemies[index].pos)-projectile.pos)*470*.2f)*470;}
 if(projectile.skill==Skill::MirrorShield&&projectile.blocks>=0){
  if(!projectile.returned&&projectile.age>=.55f){projectile.returned=true;projectile.hit_ids.clear();projectile.pierce=5;}
  if(projectile.returned){projectile.velocity=normalized(player.pos-projectile.pos)*420;if(length(player.pos-projectile.pos)<20){projectile.pierce=0;return;}}
  bool reflected=false;Effect reflection;
  if(projectile.blocks>0)for(auto& shot:effects){if(!shot.hostile||shot.kind!=EffectKind::EnemyShot||shot.age>=shot.duration||length(shot.pos-projectile.pos)>shot.radius+projectile.radius+9)continue;
   size_t target=priority_enemy(projectile.pos,550);const DangerZone* altar=altar_target(projectile.skill,projectile.pos,550,target);if(target==cross_none&&!altar)continue;shot.age=shot.duration;shot.damage=0;--projectile.blocks;
   reflection.kind=EffectKind::MirrorShield;reflection.source=projectile.skill;reflection.pattern=1;reflection.pos=projectile.pos;reflection.end=altar?altar->pos:enemies[target].pos;reflection.target_id=altar?0:enemies[target].id;reflection.altar_target_id=altar?altar->id:0;reflection.damage=projectile.damage*1.15f;reflection.duration=.12f;reflection.radius=20;reflected=true;break;
  }
  if(reflected&&skill_attack_available(projectile.skill,projectile.pos))add_effect(reflection);
 }
 if(projectile.skill==Skill::GlacialSniper&&projectile.blocks>0&&projectile.age>=projectile.lifetime){projectile.blocks=0;Effect crystal;crystal.kind=EffectKind::GlacialSniper;crystal.source=projectile.skill;crystal.pattern=1;crystal.pos=projectile.pos;crystal.radius=75*area_multiplier;crystal.damage=projectile.damage*.55f;crystal.delay=.18f;crystal.duration=.6f;add_effect(crystal);}
}
void Game::on_cross_projectile_hit(Projectile& projectile,size_t index){
 if(index>=enemies.size())return;auto& enemy=enemies[index];
 if(projectile.skill==Skill::ThunderPrison){int count=0;bool refreshed=false;for(auto& effect:effects)if(effect.source==projectile.skill&&effect.pattern==1&&effect.age<effect.duration){++count;if(effect.target_id==enemy.id){effect.age=0;effect.pos=enemy.pos;refreshed=true;}}if(!refreshed&&count<4){Effect node;node.kind=EffectKind::ThunderPrison;node.source=projectile.skill;node.pattern=1;node.pos=enemy.pos;node.target_id=enemy.id;node.radius=20;node.duration=2.3f;add_effect(node);}enemy.mark_source=projectile.skill;enemy.mark_remaining=2.3f;}
 if(projectile.skill==Skill::FrozenDragonSpear){const int layers=enemy.frost_layers;apply_frost(enemy,2,projectile.skill);if(projectile.evolved&&layers>0){hurt_enemy(index,projectile.damage*.45f,{},.2f,projectile.skill,false,false);enemy.frost_layers=0;}}
 if(projectile.skill==Skill::StormArrowStream&&projectile.evolved){int count=0;Vec2 from=enemy.pos;for(auto other:nearby(enemy.pos,175*area_multiplier)){if(other==index||enemies[other].hp<=0||count>=3)continue;++count;Vec2 to=enemies[other].pos;hurt_enemy(other,projectile.damage*.8f,to-from,.2f,projectile.skill,false,false);Effect spark;spark.kind=EffectKind::Lightning;spark.source=projectile.skill;spark.pos=from;spark.end=to;spark.duration=.25f;spark.proc_generated=true;add_effect(spark);from=to;}}
 if(projectile.skill==Skill::GlacialSniper){apply_frost(enemy,2,projectile.skill,2);if(projectile.blocks>0&&enemy.id==projectile.target){projectile.blocks=0;projectile.pierce=0;Effect crystal;crystal.kind=EffectKind::GlacialSniper;crystal.source=projectile.skill;crystal.pattern=1;crystal.pos=enemy.pos;crystal.radius=75*area_multiplier;crystal.damage=projectile.damage*.55f;crystal.delay=.18f;crystal.duration=.6f;add_effect(crystal);}}
 if(projectile.skill==Skill::FlowingFire){bool in_field=false;for(const auto& field:effects)if(field.source==projectile.skill&&field.pattern==0&&field.age<field.duration&&length(enemy.pos-field.pos)<=field.radius+enemy.radius){in_field=true;break;}if(!in_field)return;int active=0;for(const auto& effect:effects)if(effect.source==projectile.skill&&effect.pattern>0&&effect.age<effect.duration)++active;if(active<6){Effect fire;fire.kind=EffectKind::FlowingFire;fire.source=projectile.skill;fire.pattern=1;fire.pos=enemy.pos;fire.target_id=enemy.id;fire.radius=68*area_multiplier;fire.damage=projectile.damage*.9f;fire.delay=.28f;fire.duration=.7f;fire.count=1;add_effect(fire);}}
}
void Game::on_cross_defense(){
 if(!skill_attack_available(Skill::FrostArmor,player.pos))return;
 for(auto& effect:effects)if(effect.source==Skill::FrostArmor&&effect.pattern==0&&effect.secondary_tick<=0){effect.energy=std::min(90.f,effect.energy+30);effect.secondary_tick=.5f;}
}
void Game::on_cross_dash(){
 if(!skill_attack_available(Skill::PhoenixAssault,player.pos)||cooldown_[int(Skill::PhoenixAssault)]>0||effects.size()>=config.max_effects||!try_skill_cast(Skill::PhoenixAssault))return;
 Effect flight;flight.kind=EffectKind::PhoenixAssault;flight.source=Skill::PhoenixAssault;flight.pos=flight.end=player.pos;flight.radius=29*area_multiplier;flight.damage=skill_power(flight.source);flight.duration=.65f;flight.tick=.10f;add_effect(flight);on_skill_cast(flight.source,flight.damage,dash_direction_);add_event(EventKind::Shot,player.pos);
}
void Game::update_cross_dash(Vec2,Vec2 to,bool){for(auto& effect:effects)if(effect.kind==EffectKind::PhoenixAssault&&effect.pattern==0&&effect.age<.3f)effect.end=to;}
}

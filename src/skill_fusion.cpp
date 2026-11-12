#include "game.hpp"
#include "adventure_content.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
namespace {
constexpr float fusion_pi=3.14159265358979323846f;
constexpr size_t no_target=std::numeric_limits<size_t>::max();
float path_distance(Vec2 point,Vec2 from,Vec2 to){Vec2 delta=to-from;float square=delta.x*delta.x+delta.y*delta.y;float t=std::clamp(((point-from).x*delta.x+(point-from).y*delta.y)/std::max(.001f,square),0.f,1.f);return length(point-(from+delta*t));}
void apply_fusion_burn(Enemy& enemy,float damage,float duration,Skill source){
 if(enemy.burn_remaining<=0||damage>=enemy.burn_damage){enemy.burn_damage=damage;enemy.burn_source=source;}
 enemy.burn_remaining=std::max(enemy.burn_remaining,duration);
}
}
const std::vector<FusionRecipe>& fusion_recipes(){
 static const std::vector<FusionRecipe> recipes=[](){std::vector<FusionRecipe> values={
  {Skill::Pierce,Skill::Ricochet,Skill::StarReturn,CharacterClass::Archer,5},
  {Skill::Frost,Skill::Lightning,Skill::FrostStorm,CharacterClass::Mage,5},
  {Skill::Nova,Skill::Retaliate,Skill::DragonArmor,CharacterClass::Warrior,5},
  {Skill::Summon,Skill::Rain,Skill::PhoenixRain,CharacterClass::Archer,5},
  {Skill::Arrow,Skill::Spread,Skill::HunterVolley,CharacterClass::Archer,5},
  {Skill::Zone,Skill::Meteor,Skill::MoltenStar,CharacterClass::Mage,5},
  {Skill::Homing,Skill::Rune,Skill::ArcaneRing,CharacterClass::Mage,5},
  {Skill::Orbit,Skill::BladeWave,Skill::SwordCyclone,CharacterClass::Warrior,5},
  {Skill::Charge,Skill::Cleave,Skill::DashBreaker,CharacterClass::Warrior,5},
  {Skill::Orbit,Skill::Zone,Skill::FlameDance,CharacterClass::Warrior,5,int(CharacterClass::Mage)},
  {Skill::Orbit,Skill::Spread,Skill::FeatherStorm,CharacterClass::Warrior,5,int(CharacterClass::Archer)},
  {Skill::Rune,Skill::Ricochet,Skill::StarBow,CharacterClass::Mage,5,int(CharacterClass::Archer)},
  {Skill::Cleave,Skill::BattleFocus,Skill::BattleBlades,CharacterClass::Warrior,5},
  {Skill::Nova,Skill::SpearCombo,Skill::DragonFormation,CharacterClass::Warrior,5},
  {Skill::Retaliate,Skill::HolyShield,Skill::EchoShield,CharacterClass::Warrior,5},
  {Skill::Charge,Skill::EarthShatter,Skill::MountainBreaker,CharacterClass::Warrior,5},
  {Skill::Judgement,Skill::BattleFocus,Skill::SpiritJudgement,CharacterClass::Warrior,5},
  {Skill::DragonThrust,Skill::BladeWave,Skill::AzureRift,CharacterClass::Warrior,5},
  {Skill::HolyShield,Skill::Judgement,Skill::Sanctuary,CharacterClass::Warrior,5},
  {Skill::PoisonMist,Skill::Zone,Skill::PoisonFlare,CharacterClass::Mage,5},
  {Skill::IceLance,Skill::Lightning,Skill::ThunderLance,CharacterClass::Mage,5},
  {Skill::HolyLight,Skill::HolyDragon,Skill::HeavenDragon,CharacterClass::Mage,5},
  {Skill::FireBeast,Skill::Meteor,Skill::InfernoArray,CharacterClass::Mage,5},
  {Skill::MagicVortex,Skill::PoisonMist,Skill::VenomVortex,CharacterClass::Mage,5},
  {Skill::Frost,Skill::FireBeast,Skill::FrostFire,CharacterClass::Mage,5},
  {Skill::MagicVortex,Skill::HolyLight,Skill::PurityWell,CharacterClass::Mage,5},
  {Skill::StormShot,Skill::Spread,Skill::ThousandArrows,CharacterClass::Archer,5},
  {Skill::Sniper,Skill::SilverEagle,Skill::EagleExecution,CharacterClass::Archer,5},
  {Skill::IceArrow,Skill::Rain,Skill::FrozenBarrage,CharacterClass::Archer,5},
  {Skill::BlastArrow,Skill::Pierce,Skill::SiegeBolt,CharacterClass::Archer,5},
  {Skill::Decoy,Skill::BlastArrow,Skill::TrickDecoy,CharacterClass::Archer,5},
  {Skill::Summon,Skill::SilverEagle,Skill::TwinHunt,CharacterClass::Archer,5},
  {Skill::Sniper,Skill::Ricochet,Skill::PrismShot,CharacterClass::Archer,5},
  {Skill::BladeWave,Skill::Lightning,Skill::ThunderPrison,CharacterClass::Warrior,5,int(CharacterClass::Mage)},
  {Skill::Retaliate,Skill::Frost,Skill::FrostArmor,CharacterClass::Warrior,5,int(CharacterClass::Mage)},
  {Skill::Nova,Skill::PoisonMist,Skill::MiasmaDragon,CharacterClass::Warrior,5,int(CharacterClass::Mage)},
  {Skill::Judgement,Skill::HolyLight,Skill::DivineVerdict,CharacterClass::Warrior,5,int(CharacterClass::Mage)},
  {Skill::SpearCombo,Skill::IceLance,Skill::FrozenDragonSpear,CharacterClass::Warrior,5,int(CharacterClass::Mage)},
  {Skill::Charge,Skill::Summon,Skill::PhoenixAssault,CharacterClass::Warrior,5,int(CharacterClass::Archer)},
  {Skill::HolyShield,Skill::Ricochet,Skill::MirrorShield,CharacterClass::Warrior,5,int(CharacterClass::Archer)},
  {Skill::Nova,Skill::Sniper,Skill::DragonBreaker,CharacterClass::Warrior,5,int(CharacterClass::Archer)},
  {Skill::EarthShatter,Skill::Rain,Skill::FissureArrows,CharacterClass::Warrior,5,int(CharacterClass::Archer)},
  {Skill::SpearCombo,Skill::Pierce,Skill::SpearBolt,CharacterClass::Warrior,5,int(CharacterClass::Archer)},
  {Skill::Lightning,Skill::StormShot,Skill::StormArrowStream,CharacterClass::Mage,5,int(CharacterClass::Archer)},
  {Skill::PoisonMist,Skill::Decoy,Skill::PoisonHuntingGround,CharacterClass::Mage,5,int(CharacterClass::Archer)},
  {Skill::HolyDragon,Skill::Summon,Skill::DragonPhoenix,CharacterClass::Mage,5,int(CharacterClass::Archer)},
  {Skill::IceLance,Skill::Sniper,Skill::GlacialSniper,CharacterClass::Mage,5,int(CharacterClass::Archer)},
  {Skill::Zone,Skill::BlastArrow,Skill::FlowingFire,CharacterClass::Mage,5,int(CharacterClass::Archer)}
 };
  append_adventure_recipes(values);
  for(auto& recipe:values){
   // Theme tags describe actual mechanics; weighting never removes a legal route.
   switch(recipe.result){
   case Skill::DragonArmor:case Skill::DashBreaker:case Skill::BattleBlades:case Skill::DragonFormation:recipe.theme_tags=1;break;
   case Skill::EchoShield:case Skill::MountainBreaker:case Skill::Sanctuary:case Skill::ThunderPrison:case Skill::FrostArmor:case Skill::MiasmaDragon:case Skill::FrozenDragonSpear:recipe.theme_tags=1|2;break;
   case Skill::SwordCyclone:case Skill::FeatherStorm:case Skill::SpiritJudgement:case Skill::AzureRift:case Skill::DivineVerdict:case Skill::MirrorShield:case Skill::DragonBreaker:case Skill::SpearBolt:recipe.theme_tags=1|16;break;
   case Skill::FlameDance:recipe.theme_tags=1|4;break;
   case Skill::PhoenixAssault:recipe.theme_tags=1|4|8;break;
   case Skill::FissureArrows:recipe.theme_tags=1|2|16;break;
   case Skill::FrostStorm:case Skill::VenomVortex:case Skill::PurityWell:recipe.theme_tags=2;break;
   case Skill::PoisonFlare:case Skill::FrostFire:recipe.theme_tags=2|4;break;
   case Skill::ThunderLance:case Skill::FrozenBarrage:case Skill::StormArrowStream:case Skill::GlacialSniper:recipe.theme_tags=2|16;break;
   case Skill::PoisonHuntingGround:recipe.theme_tags=2|8;break;
   case Skill::MoltenStar:recipe.theme_tags=4;break;
   case Skill::InfernoArray:case Skill::DragonPhoenix:recipe.theme_tags=4|8;break;
   case Skill::SiegeBolt:case Skill::FlowingFire:recipe.theme_tags=4|16;break;
   case Skill::TrickDecoy:recipe.theme_tags=2|4|8;break;
   case Skill::HeavenDragon:recipe.theme_tags=8;break;
   case Skill::EagleExecution:recipe.theme_tags=8|16;break;
   case Skill::PhoenixRain:case Skill::TwinHunt:recipe.theme_tags=4|8|16;break;
   default:recipe.theme_tags=16;break;
   }
   recipe.form_mask=Game::form_count(recipe.result)==2?3u:1u;
  }
  return values;
 }();return recipes;
}
bool Game::is_fusion(Skill skill){for(const auto& r:fusion_recipes())if(r.result==skill)return true;return false;}
bool Game::cast_fusion(Skill skill){
 const int level=effective_skill_level(skill);const float damage=skill_power(skill);const size_t none=std::numeric_limits<size_t>::max();
 if(skill==Skill::StarReturn){
  size_t target=nearest(player.pos,500*area_multiplier);const DangerZone* altar=altar_target(skill,player.pos,500*area_multiplier,target);if(target==none&&!altar)return false;
  Vec2 direction=normalized((altar?altar->pos:enemies[target].pos)-player.pos);float axis=std::atan2(direction.y,direction.x);const int count=2+extra_projectiles/2;
  for(int j=0;j<count&&projectiles.size()<config.max_projectiles;++j){float a=axis+(j-(count-1)*.5f)*.16f;Projectile p;p.skill=skill;p.pos=player.pos;p.velocity={std::cos(a)*440,std::sin(a)*440};p.damage=damage*1.65f;p.radius=10;p.pierce=8+level;p.bounces=2+level;p.lifetime=2;p.target=altar?0:enemies[target].id;p.altar_target_id=altar?altar->id:0;add_projectile(p);}return true;
 }
 if(skill==Skill::FrostStorm){
  size_t target=nearest(player.pos,420*area_multiplier);const DangerZone* altar=altar_target(skill,player.pos,420*area_multiplier,target);Vec2 point=altar?altar->pos:target==none?player.pos:enemies[target].pos;
  effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect& e){return e.source==skill&&e.kind==EffectKind::FrostStorm;}),effects.end());
  Effect field;field.kind=EffectKind::FrostStorm;field.source=skill;field.pos=point;field.radius=(135+8*level)*area_multiplier*(fusion_form(skill)==0?1.18f:.8f);field.duration=5.2f*duration_multiplier;field.damage=damage;add_effect(field);return true;
 }
 if(skill==Skill::DragonArmor){
  if(dragon_soul<100)return false;dragon_soul=0;
  const float radius=(155+12*level)*area_multiplier;
  for(auto index:nearby(player.pos,radius+55))if(length(enemies[index].pos-player.pos)<=radius+enemies[index].radius)hurt_enemy(index,damage*5,enemies[index].pos-player.pos,2.6f,skill);
  damage_scene_area(player.pos,radius,damage*5,skill);const bool guard=fusion_form(skill)==0;shield_remaining=std::max(shield_remaining,guard?std::min(2.f,1.8f*duration_multiplier):std::min(.9f,.65f*duration_multiplier));
  Effect roar;roar.kind=EffectKind::Nova;roar.source=skill;roar.pos=player.pos;roar.radius=radius;roar.duration=.7f;add_effect(roar);return true;
 }
 if(skill==Skill::FlameDance||skill==Skill::FeatherStorm||skill==Skill::StarBow){
  if(effects.size()>=config.max_effects||std::any_of(effects.begin(),effects.end(),[&](const Effect& e){return e.source==skill&&e.pattern==0&&e.age<e.duration;}))return false;
  Effect field;field.source=skill;field.pos=field.end=player.pos;field.damage=damage;field.radius=(72+4*level)*area_multiplier;field.count=3;field.duration=4.4f*duration_multiplier;
  if(skill==Skill::FlameDance){field.kind=EffectKind::FlameDance;field.secondary_tick=.4f;}
  else if(skill==Skill::FeatherStorm){field.kind=EffectKind::FeatherStorm;field.secondary_tick=1.1f;}
  else{field.kind=EffectKind::StarBow;field.count=std::min(6,4+level/2);field.energy=float(field.count);field.delay=.65f;field.duration=.65f+field.count*.28f+.3f;}
  add_effect(field);return true;
 }
 if(skill==Skill::DashBreaker||skill==Skill::MountainBreaker)return false; // Player dash is its only trigger.
 if(skill==Skill::PhoenixRain||skill==Skill::HunterVolley||skill==Skill::MoltenStar||skill==Skill::ArcaneRing||skill==Skill::SwordCyclone){
  // Complete the marked/charged sequence even when cooldown bonuses are large.
  if(std::any_of(effects.begin(),effects.end(),[&](const Effect& e){return e.source==skill&&e.damage>0&&e.age<e.duration;}))return false;
  if(effects.size()>=config.max_effects)return false;
  Effect field;field.source=skill;field.damage=damage;field.pos=player.pos;
  if(skill==Skill::PhoenixRain){
   size_t target=nearest(player.pos,500*area_multiplier);const DangerZone* altar=altar_target(skill,player.pos,500*area_multiplier,target);if(target==none&&!altar)return false;
   field.kind=EffectKind::PhoenixRain;field.pos=altar?altar->pos:enemies[target].pos;field.target_id=altar?0:enemies[target].id;field.altar_target_id=altar?altar->id:0;field.end=player.pos;field.radius=(105+8*level)*area_multiplier;
   field.delay=.6f;field.secondary_tick=.6f+2.5f*duration_multiplier;field.duration=field.secondary_tick+1.8f*duration_multiplier;
   for(auto index:nearby(field.pos,field.radius+55))if(length(enemies[index].pos-field.pos)<=field.radius+enemies[index].radius)enemies[index].mark_remaining=std::max(enemies[index].mark_remaining,field.secondary_tick);
  }else if(skill==Skill::HunterVolley){
   size_t target=priority_enemy(player.pos,550*area_multiplier);const DangerZone* altar=altar_target(skill,player.pos,550*area_multiplier,target);if(target==none&&!altar)return false;
   field.kind=EffectKind::HunterVolley;field.target_id=altar?0:enemies[target].id;field.altar_target_id=altar?altar->id:0;field.end=altar?altar->pos:enemies[target].pos;field.count=std::clamp(6+level+extra_projectiles,1,18);field.energy=float(field.count);
   field.delay=.35f;field.radius=28;field.duration=.35f+field.count*.12f+.25f;if(!altar)enemies[target].mark_remaining=std::max(enemies[target].mark_remaining,field.duration);
  }else if(skill==Skill::MoltenStar){
   size_t target=nearest(player.pos,440*area_multiplier);const DangerZone* altar=altar_target(skill,player.pos,440*area_multiplier,target);if(target==none&&!altar)return false;
   field.kind=EffectKind::MoltenStar;field.pos=altar?altar->pos:enemies[target].pos;field.radius=(100+8*level)*area_multiplier;field.duration=4.6f*duration_multiplier;
  }else if(skill==Skill::ArcaneRing){
   field.kind=EffectKind::ArcaneRing;field.radius=(72+4*level)*area_multiplier;field.count=std::clamp(4+level+extra_projectiles/2,1,10);field.energy=float(field.count);
   field.delay=.8f;field.duration=.8f+((field.count+1)/2)*.3f+.3f;
  }else{
   const bool focus=fusion_form(skill)==1;field.kind=EffectKind::SwordCyclone;field.radius=(75+5*level)*area_multiplier*(focus?.75f:1.f);field.count=std::clamp(3+level/2+extra_projectiles/2,1,focus?3:6);field.duration=4.6f*duration_multiplier;field.secondary_tick=.2f;
  }
  add_effect(field);return true;
 }
 return false;
}
bool Game::update_fusion_effect(Effect& e,float dt,std::vector<Effect>& spawned){
 if(e.kind==EffectKind::FlameDance||e.kind==EffectKind::FeatherStorm){
  if(e.kind==EffectKind::FlameDance&&e.pattern==1){e.tick-=dt;if(e.tick>0)return true;e.tick=.4f;for(auto index:nearby((e.pos+e.end)*.5f,length(e.end-e.pos)*.5f+e.radius+55))if(enemies[index].hp>0&&path_distance(enemies[index].pos,e.pos,e.end)<=e.radius+enemies[index].radius){hurt_enemy(index,e.damage,enemies[index].pos-e.pos,.1f,e.source);if(enemies[index].hp>0)apply_fusion_burn(enemies[index],e.damage*.45f,.9f,e.source);}return true;}
  e.pos=player.pos;e.angle+=dt*3.6f;e.tick-=dt;e.secondary_tick-=dt;
  if(e.tick<=0){e.tick=.23f;bool touched=false;for(auto index:nearby(e.pos,e.radius+65)){if(enemies[index].hp<=0)continue;bool hit=false;for(int blade=0;blade<e.count&&!hit;++blade){float angle=e.angle+blade*2*fusion_pi/e.count;hit=length(enemies[index].pos-(e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius))<=enemies[index].radius+19;}if(hit){touched=true;hurt_enemy(index,e.damage*(e.kind==EffectKind::FlameDance?.8f:.7f),enemies[index].pos-e.pos,.3f,e.source);}}if(touched)e.energy=std::min(6.f,e.energy+1);}
  if(e.secondary_tick<=0){
   if(e.kind==EffectKind::FlameDance){e.secondary_tick=.4f;int active=0;for(const auto& field:effects)if(field.kind==e.kind&&field.pattern==1&&field.age<field.duration)++active;if(length(e.pos-e.end)>=18&&active<6&&effects.size()+spawned.size()<config.max_effects){Effect path;path.kind=e.kind;path.source=e.source;path.pattern=1;path.pos=e.end;path.end=e.pos;path.radius=22*area_multiplier;path.duration=1.3f;path.damage=e.damage*.45f;spawned.push_back(path);e.end=e.pos;}}
   else{e.secondary_tick=1.1f;int count=std::clamp(3+int(e.energy),3,9);e.energy=0;for(int arrow=0;arrow<count&&projectiles.size()<config.max_projectiles;++arrow){float angle=e.angle+arrow*2*fusion_pi/count;Vec2 direction{std::cos(angle),std::sin(angle)};Projectile feather;feather.skill=e.source;feather.pos=e.pos+direction*e.radius;feather.velocity=direction*390;feather.damage=e.damage*1.1f;feather.radius=8;feather.pierce=2;feather.lifetime=1.4f;add_projectile(feather);}}
  }return true;
 }
 if(e.kind==EffectKind::StarBow){e.angle+=dt*3.4f;if(e.age<e.delay)return true;e.tick-=dt;if(e.tick>0||e.count<=0)return true;e.tick=.28f;size_t target=nearest(e.pos,600*area_multiplier);const DangerZone* altar=altar_target(e.source,e.pos,600*area_multiplier,target,e.altar_target_id);e.altar_target_id=altar?altar->id:0;if((target==no_target&&!altar)||projectiles.size()>=config.max_projectiles)return true;float angle=e.angle+(e.energy-e.count)*2*fusion_pi/std::max(1.f,e.energy);Vec2 origin=e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius;Projectile arrow;arrow.skill=e.source;arrow.pos=origin;arrow.velocity=normalized((altar?altar->pos:enemies[target].pos)-origin)*390;arrow.damage=e.damage*1.35f;arrow.radius=9;arrow.pierce=1;arrow.bounces=2+effective_skill_level(e.source)/2;arrow.lifetime=3;arrow.target=altar?0:enemies[target].id;arrow.altar_target_id=e.altar_target_id;add_projectile(arrow);--e.count;return true;}
 if(e.kind==EffectKind::FrostStorm){
 const bool thunder=fusion_form(e.source)==1;e.tick-=dt;if(e.tick>0)return true;e.tick=thunder?.42f:.8f;++e.pattern;damage_pressure_facilities(e.pos,e.radius,e.damage*(thunder?.4f:.65f));
 std::vector<size_t> targets;
 for(auto index:nearby(e.pos,e.radius+55))if(length(enemies[index].pos-e.pos)<=e.radius+enemies[index].radius){
  auto& enemy=enemies[index];apply_frost(enemy,thunder?.6f:1.1f,e.source);if(!control_immune(enemy))enemy.slow_factor=enemy.kind==EnemyKind::Boss?(thunder?.9f:.8f):(thunder?.72f:.35f);
  hurt_enemy(index,e.damage*(thunder?.4f:.65f),enemy.pos-e.pos,.1f,e.source);if(enemy.hp>0)targets.push_back(index);
 }
 // The lightning is part of this skill. Its bounded secondary shatter cannot retrigger relics.
 Vec2 origin=e.pos;int count=0;const int chain_limit=std::min(7,(thunder?4:3)+effective_skill_level(e.source)/2);for(auto index:targets){if(count++>=chain_limit)break;Vec2 point=enemies[index].pos;
  hurt_enemy(index,e.damage*(thunder?.75f:.95f),point-origin,.3f,e.source,false,false);
  if(effects.size()+spawned.size()<config.max_effects){Effect lightning;lightning.kind=EffectKind::Lightning;lightning.source=e.source;lightning.pos=origin;lightning.end=point;lightning.duration=.22f;spawned.push_back(lightning);}
  if(e.pattern%(thunder?3:2)==0){int shards=0;for(auto nearby_index:nearby(point,65)){if(nearby_index==index||enemies[nearby_index].hp<=0)continue;if(shards++>=3)break;hurt_enemy(nearby_index,e.damage*.35f,enemies[nearby_index].pos-point,.2f,e.source,false,false);}if(shards>0)record_synergy(0);}
  origin=point;
 }return true;
 }
 if(e.kind==EffectKind::PhoenixRain){
  if(e.age<e.delay)return true;
  e.pattern=e.age<e.secondary_tick?1:2;e.tick-=dt;if(e.tick>0)return true;e.tick=e.pattern==1?.25f:.45f;
  const float hit=e.damage*(e.pattern==1?.65f:.35f);damage_scene_area(e.pos,e.radius,hit,e.source);
  for(auto index:nearby(e.pos,e.radius+55))if(enemies[index].hp>0&&length(enemies[index].pos-e.pos)<=e.radius+enemies[index].radius){
   auto& enemy=enemies[index];hurt_enemy(index,hit,enemy.pos-e.pos,.1f,e.source);
   if(e.pattern==2&&enemy.hp>0)apply_fusion_burn(enemy,e.damage*.25f,1.2f,e.source);
  }return true;
 }
 if(e.kind==EffectKind::HunterVolley){
  e.pos=player.pos;if(e.age<e.delay)return true;
  e.tick-=dt;if(e.tick>0||e.count<=0)return true;e.tick=.12f;
  size_t target=target_by_id(e.target_id,player.pos,600*area_multiplier);
  if(target==no_target){target=nearest(player.pos,600*area_multiplier);e.target_id=target==no_target?0:enemies[target].id;}
  const DangerZone* altar=altar_target(e.source,player.pos,600*area_multiplier,target,e.altar_target_id);e.altar_target_id=altar?altar->id:0;if(altar)e.target_id=0;
  if((target==no_target&&!altar)||projectiles.size()>=config.max_projectiles)return true;
  e.end=altar?altar->pos:enemies[target].pos;if(!altar){enemies[target].mark_remaining=std::max(enemies[target].mark_remaining,.8f);enemies[target].mark_source=e.source;}
  const float side=(int(e.energy)-e.count)%2==0?-1.f:1.f;Vec2 direction=normalized(e.end-player.pos);Vec2 origin=player.pos+Vec2{-direction.y,direction.x}*(side*16);
  Projectile arrow;arrow.skill=e.source;arrow.pos=origin;arrow.velocity=normalized(e.end-origin)*410;arrow.damage=e.damage*1.1f;arrow.radius=7;arrow.pierce=1;arrow.lifetime=2.4f;arrow.target=e.target_id;arrow.altar_target_id=e.altar_target_id;add_projectile(arrow);--e.count;++e.pattern;
  return true;
 }
 if(e.kind==EffectKind::MoltenStar){
  if(e.pattern>0){
   if(!e.triggered&&e.age>=e.delay){e.triggered=true;e.pattern=2;damage_scene_area(e.pos,e.radius,e.damage,e.source);
    for(auto index:nearby(e.pos,e.radius+55))if(enemies[index].hp>0&&length(enemies[index].pos-e.pos)<=e.radius+enemies[index].radius){
     trigger_burst_synergy(index,e.damage,e.source,spawned);hurt_enemy(index,e.damage,enemies[index].pos-e.pos,2.1f,e.source);
    }
   }return true;
  }
  e.tick-=dt;if(e.tick>0)return true;e.tick=.4f;bool heated=false;
  damage_scene_area(e.pos,e.radius,e.damage*.45f,e.source);
  for(auto index:nearby(e.pos,e.radius+55))if(enemies[index].hp>0&&length(enemies[index].pos-e.pos)<=e.radius+enemies[index].radius){
   auto& enemy=enemies[index];heated=true;hurt_enemy(index,e.damage*.45f,enemy.pos-e.pos,.05f,e.source);if(enemy.hp>0)apply_fusion_burn(enemy,e.damage*.25f,1.2f,e.source);
  }
  if(heated&&e.count<2)e.energy=std::min(4.f,e.energy+1);
  if(e.energy>=4&&e.count<2&&effects.size()+spawned.size()<config.max_effects){
   Effect meteor;meteor.kind=EffectKind::MoltenStar;meteor.source=e.source;meteor.pos=e.pos;meteor.radius=e.radius;meteor.damage=e.damage*3.4f;meteor.pattern=1;meteor.delay=.65f;meteor.duration=1.0f;spawned.push_back(meteor);e.energy=0;++e.count;
  }return true;
 }
 if(e.kind==EffectKind::ArcaneRing){
  e.pos=player.pos;e.angle+=dt*2.6f;if(e.age<e.delay)return true;
  e.tick-=dt;if(e.tick>0||e.count<=0)return true;e.tick=.3f;
  const float pulse_radius=(125+6*effective_skill_level(e.source))*area_multiplier;
  skill_area(player.pos,pulse_radius,e.damage*.75f,e.source,.15f,false);
  Effect pulse;pulse.kind=EffectKind::Rune;pulse.source=e.source;pulse.pos=player.pos;pulse.radius=pulse_radius;pulse.duration=.32f;pulse.triggered=true;spawned.push_back(pulse);
  for(int batch=0;batch<2&&e.count>0&&projectiles.size()<config.max_projectiles;++batch){
   size_t target=nearest(player.pos,600*area_multiplier);const DangerZone* altar=altar_target(e.source,player.pos,600*area_multiplier,target,e.altar_target_id);e.altar_target_id=altar?altar->id:0;if(target==no_target&&!altar)break;
   float angle=e.angle+(e.energy-e.count)*2*fusion_pi/std::max(1.f,e.energy);Vec2 origin=e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius;
   Projectile orb;orb.skill=e.source;orb.pos=origin;orb.velocity=normalized((altar?altar->pos:enemies[target].pos)-origin)*330;orb.damage=e.damage*1.6f;orb.radius=9;orb.pierce=2;orb.lifetime=3;orb.target=altar?0:enemies[target].id;orb.altar_target_id=e.altar_target_id;add_projectile(orb);--e.count;++e.pattern;
  }return true;
 }
 if(e.kind==EffectKind::SwordCyclone){
  const bool focus=fusion_form(e.source)==1;e.pos=player.pos;e.angle+=dt*3.8f;e.tick-=dt;e.secondary_tick-=dt;
  if(e.tick<=0){e.tick=.22f;
   for(int blade=0;blade<e.count;++blade){float angle=e.angle+blade*2*fusion_pi/e.count;Vec2 point=e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius;damage_scene_area(point,19,e.damage*(focus?.65f:.85f),e.source);}
   for(auto index:nearby(e.pos,e.radius+75)){auto& enemy=enemies[index];if(enemy.hp<=0)continue;bool touching=false;
    for(int blade=0;blade<e.count&&!touching;++blade){float angle=e.angle+blade*2*fusion_pi/e.count;Vec2 point=e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius;touching=length(enemy.pos-point)<=enemy.radius+19;}
    if(touching)hurt_enemy(index,e.damage*(focus?.65f:.85f),enemy.pos-e.pos,.35f,e.source);
   }
  }
  if(e.secondary_tick<=0){e.secondary_tick=focus?.95f:1.1f;int count=focus?3:std::clamp(4+effective_skill_level(e.source)/2+extra_projectiles/2,1,8);
   size_t target=priority_enemy(player.pos,550*area_multiplier);
   if(focus){target=target_by_id(e.target_id,player.pos,550*area_multiplier);if(target==no_target)target=priority_enemy(player.pos,550*area_multiplier);}
   const DangerZone* altar=altar_target(e.source,player.pos,550*area_multiplier,target,e.altar_target_id);e.altar_target_id=altar?altar->id:0;e.target_id=altar||target==no_target?0:enemies[target].id;if(altar||target!=no_target)e.end=altar?altar->pos:enemies[target].pos;
   if(focus){if(target==no_target&&!altar)return true;if(!altar){enemies[target].mark_remaining=std::max(enemies[target].mark_remaining,1.3f);enemies[target].mark_source=e.source;}}
   ++e.pattern;
   for(int blade=0;blade<count&&projectiles.size()<config.max_projectiles;++blade){float angle=e.angle+blade*2*fusion_pi/count;Vec2 direction={std::cos(angle),std::sin(angle)};Projectile wave;wave.skill=e.source;wave.pos=e.pos+direction*e.radius;if((target!=no_target||altar)&&(focus||blade==0))direction=normalized(e.end-wave.pos);wave.velocity=direction*(focus?440.f:380.f);wave.damage=e.damage*(focus?.9f:1.15f);wave.radius=focus?12.f:18.f;wave.pierce=focus?1:4;wave.lifetime=focus?1.3f:1.2f;wave.target=focus?e.target_id:0;wave.altar_target_id=focus||blade==0?e.altar_target_id:0;add_projectile(wave);}
  }return true;
 }
 return false;
}
void Game::on_fusion_dash(){
 fusion_dash_active_=false;fusion_dash_source_=Skill::Count;fusion_dash_hits_.clear();fusion_dash_trail_=0;
 for(Skill candidate:{Skill::DashBreaker,Skill::MountainBreaker,Skill::Charge})if(skill_attack_available(candidate,player.pos)&&cooldown_[int(candidate)]<=0){fusion_dash_source_=candidate;break;}
 if(fusion_dash_source_==Skill::Count)return;const Skill source=fusion_dash_source_;if(!try_skill_cast(source))return;current_attack_id_=next_attack_id_++;fusion_dash_active_=true;
 cooldown_[int(source)]=cooldown_period_[int(source)]=skill_interval(source);
 on_skill_cast(source,skill_power(source),dash_direction_);add_event(EventKind::Shot,player.pos);
}
void Game::update_fusion_dash(Vec2 from,Vec2 to,bool ended){
 if(!fusion_dash_active_)return;const Skill source=fusion_dash_source_;
 if(source==Skill::Count||!skill_attack_available(source,player.pos)){fusion_dash_active_=false;fusion_dash_hits_.clear();return;}
 const float damage=skill_power(source),width=(source==Skill::Charge?30.f:source==Skill::MountainBreaker?32.f:26.f)*area_multiplier;Vec2 direction=normalized(to-from);float moved=length(to-from);fusion_dash_trail_+=moved;
 if(moved>.001f){
  for(size_t index=0;index<enemies.size();++index){const auto& enemy=enemies[index];if(enemy.hp<=0||std::find(fusion_dash_hits_.begin(),fusion_dash_hits_.end(),enemy.id)!=fusion_dash_hits_.end()||path_distance(enemy.pos,from,to)>width+enemy.radius)continue;
   fusion_dash_hits_.push_back(enemy.id);hurt_enemy(index,damage*(source==Skill::Charge?2.3f:source==Skill::MountainBreaker?1.2f:1.3f),direction,1.4f,source);
  }
  Effect trail;trail.kind=source==Skill::MountainBreaker?EffectKind::MountainBreaker:EffectKind::Charge;trail.source=source;trail.pos=from;trail.end=to;trail.radius=width;trail.duration=.32f;add_effect(trail);
  if(source==Skill::MountainBreaker&&fusion_dash_trail_>=28){fusion_dash_trail_=0;int active=0;for(const auto& effect:effects)if(effect.source==source&&effect.pattern==1&&effect.age<effect.duration)++active;if(active<8){Effect earth=trail;earth.pattern=1;earth.delay=.35f;earth.duration=1.55f;earth.damage=damage;add_effect(earth);}}
 }
 if(ended){
  fusion_dash_active_=false;
  if(source!=Skill::Charge){const float radius=(100+8*effective_skill_level(source))*area_multiplier;float multiplier=source==Skill::MountainBreaker?3.f:2.5f;
   skill_area(to,radius,damage*multiplier,source,2.2f);Effect strike;strike.kind=source==Skill::MountainBreaker?EffectKind::MountainBreaker:EffectKind::Cleave;strike.pattern=source==Skill::MountainBreaker?2:0;strike.source=source;strike.pos=to;strike.end=to+dash_direction_*radius;strike.radius=radius;strike.angle=fusion_pi;strike.duration=.38f;strike.triggered=true;add_effect(strike);
  }else if(evolved(source)){Effect echo;echo.kind=EffectKind::Rune;echo.source=source;echo.pos=to;echo.radius=90*area_multiplier;echo.delay=.1f;echo.duration=.5f;echo.damage=damage*1.8f;add_effect(echo);}
  fusion_dash_hits_.clear();fusion_dash_source_=Skill::Count;
 }
}
}

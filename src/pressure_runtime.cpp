#include "game.hpp"
#include <algorithm>
#include <cmath>

namespace maple {
const DangerZone* Game::altar_target(Skill source,Vec2 origin,float range,size_t enemy_target,uint32_t held) const {
 // Altars keep their one real health value in DangerZone. They are aim points,
 // not fake enemies, and never grant enemy hit/proc or kill contributions.
 // Pure enemy marks/lures, defense and player-directed dashes keep their
 // own triggers. Direct damage, shots and ground attacks may aim at an altar.
 switch(source){case Skill::HunterMark:case Skill::ArrowTrap:case Skill::DragonArmor:case Skill::Bulwark:case Skill::Charge:case Skill::DashBreaker:case Skill::MountainBreaker:case Skill::PhoenixAssault:case Skill::FlameDance:case Skill::FeatherStorm:case Skill::Decoy:case Skill::TrickDecoy:case Skill::PoisonHuntingGround:case Skill::FrostArmor:case Skill::SpiritJudgement:case Skill::ShadowClone:return nullptr;default:break;}
 const int level=effective_skill_level(source);
 switch(source){
 case Skill::Cleave:range=std::min(range,(100+10*level)*area_multiplier+24);break;
 case Skill::HeavyStrike:case Skill::BattleFocus:range=std::min(range,(145+8*level)*area_multiplier+24);break;
 case Skill::SpearCombo:range=std::min(range,(150+12*level)*area_multiplier+24);break;
 case Skill::HolyShield:case Skill::EchoShield:range=std::min(range,180+18*area_multiplier+24);break;
 case Skill::Sanctuary:range=std::min(range,180+(125+6*level)*area_multiplier+24);break;
 case Skill::AzureRift:range=std::min(range,282+22*area_multiplier+24);break;
 case Skill::MirrorShield:range=std::min(range,187.f+18+24);break;
 case Skill::StarReturn:range=std::min(range,286.f+10+24);break;
 case Skill::BattleBlades:range=std::min(range,(130+8*level)*area_multiplier+24);break;
 case Skill::DragonFormation:range=std::min(range,350*area_multiplier+24);break;
 case Skill::MiasmaDragon:range=std::min(range,(255+10*level)*area_multiplier+24);break;
 case Skill::FissureArrows:range=std::min(range,358*area_multiplier+24);break;
 case Skill::SixBlade:case Skill::Uppercut:case Skill::FrostCombo:case Skill::AsuraBlades:case Skill::BladeFist:range=std::min(range,135*area_multiplier+24);break;
 default:break;
 }
 const DangerZone* selected=nullptr;
 float best=enemy_target<enemies.size()?length(enemies[enemy_target].pos-origin):range+40;
 for(const auto& zone:danger_zones){
  if(zone.kind!=DangerKind::Altar||zone.hp<=0||zone.age<zone.warning||zone.age>=zone.duration)continue;
  const float distance=length(zone.pos-origin);if(distance>range)continue;
  if(held&&zone.id==held)return &zone;
  if(distance-40<best){best=distance-40;selected=&zone;}
 }
 return selected;
}
bool Game::buffs_suppressed() const {return pressure_active(PressureKind::Disarm);}
bool Game::regression_active() const {
 for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Regression&&zone.age>=zone.warning&&zone.age<zone.duration&&length(player.pos-zone.pos)<=zone.radius)return true;
 return false;
}
bool Game::equipment_suppressed() const {
 if(buffs_suppressed())return true;
 for(const auto& zone:danger_zones)if(zone.kind==DangerKind::NaturalOrder&&zone.age>=zone.warning&&zone.age<zone.duration&&length(player.pos-zone.pos)<=zone.radius)return true;
 return false;
}
int Game::effective_skill_level(Skill skill) const {
 const int i=int(skill);return i>=0&&i<skill_count?std::max(0,skills[i]-(regressed_?1:0)):0;
}
bool Game::skill_sealed(Skill skill) const {return pressure_active(PressureKind::LastWords)&&pressure_event.sealed==skill;}
bool Game::skill_attack_available(Skill skill,Vec2 origin) const {
 return skill_allowed(skill)&&effective_skill_level(skill)>0&&!skill_sealed(skill)&&!smoke_at(player.pos)&&!smoke_at(origin);
}
float Game::skill_failure_feedback(Skill skill) const {return int(skill)>=0&&int(skill)<skill_count?cast_failure_flash_[int(skill)]:0;}
float Game::cast_recovery_factor() const {return pressure_active(PressureKind::ThinAir)?config.thin_air_factor:1.f;}
float Game::dash_recovery_factor() const {return pressure_active(PressureKind::Shackles)?config.shackles_factor:1.f;}
void Game::update_pressure_sources(){
 gear_suppressed_=equipment_suppressed();buff_suppressed_=buffs_suppressed();regressed_=regression_active();
}
bool Game::displacement_immune(const Enemy& enemy) const {
 return enemy.kind==EnemyKind::Boss||enemy.abilities.native_stability||enemy.abilities.promise_remaining>0||pressure_active(PressureKind::Frenzy);
}
void Game::displace_enemy(Enemy& enemy,Vec2 delta,bool impulse){
 if(length(delta)<=.0001f)return;
 if(displacement_immune(enemy)){++pressure_metrics.displacement_blocked;if(enemy.abilities.feedback<=0){enemy.abilities.feedback=.35f;enemy.abilities.feedback_kind=4;}return;}
 if(impulse)enemy.velocity+=delta;else{enemy.pos+=delta;resolve_scene_collision(enemy.pos,enemy.radius);}
}
bool Game::cast_has_conditions(Skill skill) const {
 const int i=int(skill);if(i<0||i>=skill_count||!skill_attack_available(skill,player.pos)||cooldown_[i]>0)return false;
 if(effects.size()>=config.max_effects)return false;
 if(skill==Skill::Retaliate&&!retaliation_pending_)return false;
 if(skill==Skill::DragonArmor&&dragon_soul<100)return false;
 if(skill==Skill::BattleFocus&&battle_focus<100)return false;
 if(skill==Skill::ArrowTrap&&std::count_if(effects.begin(),effects.end(),[](const Effect& e){return e.kind==EffectKind::ArrowTrap&&e.age<e.duration;})>=3)return false;
 if((skill>=Skill::PhoenixRain&&skill<=Skill::SwordCyclone)||skill>=Skill::FlameDance){
  if(std::any_of(effects.begin(),effects.end(),[&](const Effect& e){if(e.source!=skill||e.age>=e.duration)return false;if(skill>=Skill::BattleBlades&&skill<=Skill::PrismShot)return e.kind==EffectKind(int(EffectKind::BattleFocus)+int(skill)-int(Skill::BattleFocus))&&e.pattern<10;if(skill>=Skill::PhoenixRain&&skill<=Skill::SwordCyclone)return e.damage>0;return e.pattern==0;}))return false;
 }
 bool target_required=true,projectile=false;float range=420*area_multiplier;
 switch(skill){
 case Skill::Nova:case Skill::Zone:case Skill::Cleave:case Skill::Orbit:case Skill::Frost:case Skill::Rune:case Skill::Retaliate:case Skill::FrostStorm:case Skill::DragonArmor:case Skill::ArrowTrap:case Skill::OrbitalOrb:case Skill::GravityWell:case Skill::Bulwark:case Skill::ArcaneRing:case Skill::SwordCyclone:case Skill::FlameDance:case Skill::FeatherStorm:case Skill::StarBow:case Skill::EarthShatter:case Skill::PoisonMist:case Skill::HolyLight:case Skill::HolyDragon:case Skill::FireBeast:case Skill::MagicVortex:case Skill::SilverEagle:case Skill::Decoy:case Skill::PoisonFlare:case Skill::HeavenDragon:case Skill::VenomVortex:case Skill::FrostFire:case Skill::PurityWell:case Skill::TrickDecoy:case Skill::TwinHunt:case Skill::FrostArmor:target_required=false;break;
 default:break;
 }
 if(skill>=Skill::ThunderPrison&&skill!=Skill::FrostArmor)range=560*area_multiplier;
 else if(skill>=Skill::BattleBlades)range=520;
 else if(skill==Skill::Judgement||skill==Skill::Sniper)range=520;
 else if(skill>=Skill::BattleFocus)range=420;
 else if(skill==Skill::HeavyStrike)range=230*area_multiplier;
 else if(skill==Skill::HunterVolley)range=550*area_multiplier;
 else if(skill==Skill::MoltenStar)range=440*area_multiplier;
 else if(skill==Skill::StarReturn||skill==Skill::HunterMark||skill==Skill::PhoenixRain)range=500*area_multiplier;
 if(target_required&&std::none_of(enemies.begin(),enemies.end(),[&](const Enemy& e){return enemy_targetable(e)&&length(e.pos-player.pos)<range;})&&!altar_target(skill,player.pos,range,enemies.size()))return false;
 switch(skill){case Skill::Homing:case Skill::Arrow:case Skill::Pierce:case Skill::Spread:case Skill::Ricochet:case Skill::BladeWave:case Skill::Summon:case Skill::StarReturn:case Skill::HolyShield:case Skill::DragonThrust:case Skill::IceLance:case Skill::IceArrow:case Skill::BlastArrow:case Skill::EchoShield:case Skill::AzureRift:case Skill::Sanctuary:case Skill::ThunderLance:case Skill::MirrorShield:projectile=true;break;default:break;}
 return !projectile||projectiles.size()<config.max_projectiles;
}
bool Game::try_skill_cast(Skill skill){
 const int i=int(skill);if(!skill_attack_available(skill,player.pos)||cooldown_[i]>0)return false;
 cooldown_[i]=cooldown_period_[i]=skill_interval(skill);++pressure_metrics.attempts[i];
 if(pressure_active(PressureKind::Distraction)){
  if(cast_failed_[i]){cast_failed_[i]=false;++pressure_metrics.protected_casts[i];}
  else if(float(double(pressure_rng_())/4294967296.0)<config.distraction_chance){
   cast_failed_[i]=true;cast_failure_flash_[i]=.7f;++pressure_metrics.failures[i];
   if(combat_elapsed-last_failed_record_>=.5f){record_combat(CombatRecordKind::CastFailed,0,"心猿意马 · 施放失败",-1,skill);last_failed_record_=combat_elapsed;}
   if(skill==Skill::Retaliate)retaliation_pending_=false;
   if(skill==Skill::DragonArmor)dragon_soul=0;
   if(skill==Skill::BattleFocus)battle_focus=0;
   return false;
  }
 }
 ++pressure_metrics.successful_casts[i];return true;
}
bool Game::effect_is_summon(const Effect& e) const {
 if(e.source>=Skill::TripleStar&&e.source!=Skill::Count&&(skill_roles(e.source)&(Companion|Turret)))return true;
 if(e.kind==EffectKind::TwinHunt)return true;
 if(e.pattern!=0)return false;
 switch(e.kind){case EffectKind::Summon:case EffectKind::OrbitalOrb:case EffectKind::HolyDragon:case EffectKind::FireBeast:case EffectKind::SilverEagle:case EffectKind::HeavenDragon:case EffectKind::InfernoArray:case EffectKind::TwinHunt:case EffectKind::DragonPhoenix:return true;default:return false;}
}
bool Game::effect_attack_available(const Effect& effect) const {
 if(effect.source==Skill::Count)return true;
 if(effective_skill_level(effect.source)<=0||skill_sealed(effect.source))return false;
 if(effect.kind==EffectKind::TwinHunt)return !smoke_at(player.pos);
 return !effect_is_summon(effect)||(!smoke_at(player.pos)&&!smoke_at(effect.pos));
}
} // namespace maple

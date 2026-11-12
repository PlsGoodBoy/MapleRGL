#include "game.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace maple {
namespace {
constexpr float pi=3.14159265358979323846f;
uint32_t ability_bit(EnemyAbility ability){return 1u<<int(ability);}
bool has_ability(const Enemy& enemy,EnemyAbility ability){return (enemy.abilities.mask&ability_bit(ability))!=0;}
bool major_kind(DangerKind kind){return kind==DangerKind::Smoke||kind==DangerKind::Distortion||kind==DangerKind::NaturalOrder||kind==DangerKind::Regression||kind==DangerKind::Altar;}
bool denial_kind(DangerKind kind){return kind==DangerKind::Smoke||kind==DangerKind::NaturalOrder||kind==DangerKind::Regression;}
bool zone_alive(const Game& game,const DangerZone& zone){
 if(zone.age>=zone.duration||(zone.kind==DangerKind::Altar&&zone.hp<=0))return false;
 if(zone.following)return std::any_of(game.enemies.begin(),game.enemies.end(),[&](const Enemy& enemy){return enemy.id==zone.owner_id&&enemy.hp>0;});
 return true;
}
bool zone_active(const Game& game,const DangerZone& zone){return zone.age>=zone.warning&&zone_alive(game,zone);}
float dot(Vec2 first,Vec2 second){return first.x*second.x+first.y*second.y;}
float segment_distance(Vec2 point,Vec2 first,Vec2 last){Vec2 delta=last-first;float square=dot(delta,delta);float t=square>0?std::clamp(dot(point-first,delta)/square,0.f,1.f):0;return length(point-(first+delta*t));}
Vec2 rotate(Vec2 vector,float angle){float c=std::cos(angle),s=std::sin(angle);return {vector.x*c-vector.y*s,vector.x*s+vector.y*c};}
float unit_random(std::mt19937& random){return float(double(random())/double(std::mt19937::max()));}
void cancel_pending_attacks(std::vector<Effect>& effects,uint32_t owner,uint64_t attack_serial=0){
 // Already flying shots are independent. Uncommitted previews belong to the
 // cancelled attack and must never turn into a shot or delayed ground hit.
 for(auto& effect:effects)if(effect.hostile&&(effect.target_id==owner||(attack_serial&&effect.root_attack_id==attack_serial))&&effect.kind==EffectKind::Warning&&!effect.triggered){
  effect.damage=effect.energy=0;effect.triggered=true;effect.age=effect.duration;
 }
}
}

const char* enemy_ability_name(EnemyAbility ability){
 static const char* names[]={"虚妄之诺","爆破起飞","烟幕","折光","扭曲力场","自然秩序","退化光环"};
 int index=int(ability);return index>=0&&index<enemy_ability_count?names[index]:"未知能力";
}
const char* danger_name(DangerKind kind){
 switch(kind){case DangerKind::Explosion:return "延迟爆炸";case DangerKind::Smoke:return "烟幕";case DangerKind::Distortion:return "扭曲力场";case DangerKind::NaturalOrder:return "自然秩序";case DangerKind::Regression:return "退化光环";case DangerKind::Altar:return "黑暗祭坛";case DangerKind::Revival:return "复苏预告";}return "危险区域";
}

void Game::assign_enemy_abilities(Enemy& enemy){
 if(!config.enable_pressure||enemy.abilities.mask)return;
 const bool boss=enemy.kind==EnemyKind::Boss;
 const bool elite=enemy.kind==EnemyKind::Elite||enemy.abilities.elite||enemy.abilities.shadow_class>=0;
 enemy.abilities.elite=elite;
 if(!boss&&!elite&&ability_rng_()%100>=15)return;
 int count=boss?1:elite?1+int(ability_rng_()%2):1;
 std::vector<EnemyAbility> candidates;
 for(int index=0;index<enemy_ability_count;++index){
  auto ability=EnemyAbility(index);
  // Authored boss mechanics own the battlefield budget. No random output
  // denial or refraction layer obscures the node/counterplay contract.
  if(boss&&(ability==EnemyAbility::Smoke||ability==EnemyAbility::NaturalOrder||ability==EnemyAbility::Regression||ability==EnemyAbility::Refraction))continue;
  if(ability==EnemyAbility::Promise&&boss&&(enemy.boss_profile==BossProfile::Golem||enemy.boss_profile==BossProfile::Clockwork))continue;
  candidates.push_back(ability);
 }
 bool field_assigned=false;
 for(int slot=0;slot<count&&!candidates.empty();++slot){
  size_t choice=ability_rng_()%candidates.size();auto ability=candidates[choice];candidates.erase(candidates.begin()+choice);
  enemy.abilities.mask|=ability_bit(ability);++pressure_metrics.abilities[int(ability)];
  if(ability==EnemyAbility::Refraction)enemy.abilities.refraction=boss?6:elite?4:2;
  if(ability==EnemyAbility::Smoke||ability==EnemyAbility::Distortion||ability==EnemyAbility::NaturalOrder||ability==EnemyAbility::Regression)field_assigned=true;
  // Two elite affixes may coexist, but one body does not monopolize both
  // major-field slots with two independent denial generators.
  if(field_assigned)candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[](EnemyAbility next){return next==EnemyAbility::Smoke||next==EnemyAbility::Distortion||next==EnemyAbility::NaturalOrder||next==EnemyAbility::Regression;}),candidates.end());
 }
 enemy.abilities.field_cooldown=4.f+unit_random(ability_rng_)*3.f;
}

bool Game::control_immune(const Enemy& enemy) const {return enemy.abilities.promise_remaining>0;}

float Game::resolve_ability_damage(Enemy& enemy,float candidate){
 if(enemy.hp<=0||candidate<=0||!std::isfinite(candidate))return 0;
 auto& ability=enemy.abilities;
 if(ability.promise_remaining>0){ability.feedback=.25f;ability.feedback_kind=1;return 0;}
 if(ability.refraction>0){--ability.refraction;++pressure_metrics.refractions;ability.feedback=.3f;ability.feedback_kind=2;return 0;}
 float multiplier=enemy.shield_remaining>0?.35f:1.f;
 for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone_active(*this,zone)&&length(enemy.pos-zone.pos)<=zone.radius){multiplier*=.8f;break;}
 for(const auto& link:soul_links){
  if(link.broken||(link.first!=enemy.id&&link.second!=enemy.id))continue;
  if(!pressure_active(PressureKind::SoulLink)||link.source_id!=pressure_event.id)continue;
  uint32_t other_id=link.first==enemy.id?link.second:link.first;
  auto other=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& partner){return partner.id==other_id&&partner.hp>0;});
  if(other!=enemies.end()&&length(other->pos-enemy.pos)<=link.max_distance){multiplier*=.65f;break;}
 }
 candidate*=std::max(.35f,multiplier); // All finite reduction sources share a 65% cap.
 const float threshold=enemy.max_hp*(enemy.kind==EnemyKind::Boss?.05f:.10f);
 if(has_ability(enemy,EnemyAbility::Promise)&&!ability.promise_used&&enemy.hp-candidate<=threshold){
  ability.promise_used=true;ability.promise_remaining=3;ability.pending_death=candidate>=enemy.hp;
  ability.feedback=.6f;ability.feedback_kind=3;++pressure_metrics.promise_triggers;
  enemy.velocity={};enemy.windup=enemy.action_remaining=enemy.recovery_remaining=0;
  enemy.attack_clock=0;enemy.attack_cooldown=std::max(enemy.attack_cooldown,1.f);enemy.attack_animation_time=0;
  if(enemy.kind==EnemyKind::Boss)cancel_boss_attack(enemy);else cancel_pending_attacks(effects,enemy.id);
  if(ability.pending_death)candidate=std::max(0.f,enemy.hp-std::min(1.f,enemy.hp));
 }
 return candidate;
}

bool Game::major_zone_legal(DangerKind kind) const {
 if(!major_kind(kind))return true;
 if(boss_pressure_busy()&&denial_kind(kind))return false;
 size_t major=0;bool smoke=false,denial=false;
 for(const auto& zone:danger_zones)if(zone_alive(*this,zone)&&major_kind(zone.kind)){
  ++major;smoke|=zone.kind==DangerKind::Smoke;denial|=denial_kind(zone.kind);
 }
 if(major>=std::min(size_t(2),config.max_major_zones))return false;
 if(kind==DangerKind::Smoke&&smoke)return false;
 if(denial_kind(kind)){
  if(total_elapsed()<last_output_denial_end_+30)return false;
  if(denial||pressure_active(PressureKind::DarkTide)||pressure_active(PressureKind::LastWords)||pressure_active(PressureKind::Disarm)||pressure_active(PressureKind::ThinAir)||pressure_active(PressureKind::Distraction))return false;
  if(kind==DangerKind::NaturalOrder&&pressure_active(PressureKind::BloodCurse))return false;
 }
 return true;
}

bool Game::add_danger_zone(DangerZone zone){
 if(danger_zones.size()>=config.max_danger_zones||!std::isfinite(zone.radius)||zone.radius<=0||!std::isfinite(zone.duration)||!std::isfinite(zone.warning)||zone.warning<0||zone.duration<=zone.warning)return false;
 if(!major_zone_legal(zone.kind))return false;
 if(zone.kind==DangerKind::Explosion){int pending=0;for(const auto& current:danger_zones)if(current.kind==DangerKind::Explosion&&zone_alive(*this,current))++pending;if(pending>=8)return false;}
 zone.radius=std::clamp(zone.radius,12.f,major_kind(zone.kind)?150.f:180.f);
 if(zone.kind==DangerKind::Altar&&zone.hp<=0)return false;
 if(!zone.id)zone.id=next_pressure_source_++;
 if(!zone.source_id)zone.source_id=zone.id;
 danger_zones.push_back(zone);pressure_metrics.max_zones=std::max(pressure_metrics.max_zones,danger_zones.size());return true;
}

bool Game::smoke_at(Vec2 position) const {
 return std::any_of(danger_zones.begin(),danger_zones.end(),[&](const DangerZone& zone){return zone.kind==DangerKind::Smoke&&zone_active(*this,zone)&&length(position-zone.pos)<=zone.radius;});
}

Vec2 Game::projectile_motion(Vec2 from,Vec2 velocity,float dt) const {
 const float speed=length(velocity),budget=speed*dt;
 if(speed<=0||budget<=0)return from;
 const Vec2 direction=velocity*(1.f/speed);
 std::vector<std::pair<float,float>> intervals;intervals.reserve(2);
 // Intersect the entire possible travel segment, including an enter-and-exit
 // in one step. Union the intervals so overlapping fields remain exactly 0.5x.
 for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Distortion&&zone_active(*this,zone)){
  Vec2 relative=zone.pos-from;float along=dot(relative,direction);
  float perpendicular=std::max(0.f,dot(relative,relative)-along*along);
  float square=zone.radius*zone.radius-perpendicular;if(square<=0)continue;
  float reach=std::sqrt(square),first=std::max(0.f,along-reach),last=std::min(budget,along+reach);
  if(last>first)intervals.push_back({first,last});
 }
 if(intervals.empty())return from+velocity*dt;
 std::sort(intervals.begin(),intervals.end());
 float distance=0,remaining=budget;
 for(size_t i=0;i<intervals.size();++i){
  float first=intervals[i].first,last=intervals[i].second;
  while(i+1<intervals.size()&&intervals[i+1].first<=last){last=std::max(last,intervals[++i].second);}
  float outside=std::max(0.f,first-distance);
  if(remaining<=outside)return from+direction*(distance+remaining);
  remaining-=outside;distance=first;
  float inside=last-first;
  if(remaining<=inside*2)return from+direction*(distance+remaining*.5f);
  remaining-=inside*2;distance=last;
 }
 return from+direction*(distance+remaining);
}

void Game::schedule_map_zone(float dt){
 if(!config.enable_pressure)return;
 next_map_zone_in_-=dt;if(next_map_zone_in_>0)return;
 std::vector<DangerKind> candidates;
 for(auto kind:{DangerKind::Smoke,DangerKind::Distortion,DangerKind::NaturalOrder,DangerKind::Regression})if(major_zone_legal(kind))candidates.push_back(kind);
 if(candidates.empty()){next_map_zone_in_=6;return;}
 DangerZone zone;zone.kind=candidates[ability_rng_()%candidates.size()];
 float angle=unit_random(ability_rng_)*2*pi,range=145+unit_random(ability_rng_)*80;
 zone.pos=player.pos+Vec2{std::cos(angle),std::sin(angle)}*range;
 zone.radius=zone.kind==DangerKind::Smoke?105.f:125.f;zone.warning=1.3f;zone.duration=zone.warning+12;
 if(add_danger_zone(zone)){next_map_zone_in_=28+unit_random(ability_rng_)*14;notify(std::string(danger_name(zone.kind))+"正在形成，留意边界。");}
 else next_map_zone_in_=6;
}

void Game::on_enemy_real_death(Enemy& enemy){
 if(enemy.hp>0)return;
 for(auto& zone:danger_zones)if(zone.following&&zone.owner_id==enemy.id){if(zone.age<zone.duration&&denial_kind(zone.kind))last_output_denial_end_=total_elapsed();zone.age=zone.duration;}
 for(auto& link:soul_links)if(link.first==enemy.id||link.second==enemy.id)link.broken=true;
 cancel_pending_attacks(effects,enemy.id,enemy.boss_attack_serial);
 if(enemy.abilities.reward_claimed)return;
 const bool boss=enemy.kind==EnemyKind::Boss;
 const bool elite=enemy.kind==EnemyKind::Elite||enemy.abilities.elite;
 bool blast=has_ability(enemy,EnemyAbility::DeathBlast);
 if(!blast&&!boss&&enemy.kind!=EnemyKind::Minion&&pressure_active(PressureKind::DeathGrudge))blast=ability_rng_()%100<30;
 if(blast&&!enemy.abilities.blast_used){
  enemy.abilities.blast_used=true;
  DangerZone zone;zone.kind=DangerKind::Explosion;zone.owner_id=enemy.id;zone.pos=enemy.pos;zone.from_boss=boss;
  zone.warning=boss?1.8f:elite?1.4f:1.2f;zone.duration=zone.warning+.35f;
  zone.radius=boss?145.f:elite?95.f:70.f;zone.damage=enemy.damage*(boss?1.25f:1.f);
  if(add_danger_zone(zone))++pressure_metrics.death_blasts;
 }
 if(pressure_active(PressureKind::Undying)&&!boss&&!elite&&enemy.kind!=EnemyKind::Minion&&enemy.abilities.shadow_class<0&&!enemy.abilities.revived&&reviving_enemies_.size()<12&&ability_rng_()%100<30){
  DangerZone zone;zone.kind=DangerKind::Revival;zone.pos=enemy.pos;zone.owner_id=enemy.id;zone.radius=std::max(24.f,enemy.radius);zone.warning=2.2f;zone.duration=2.5f;
  if(add_danger_zone(zone)){
   Enemy revived=enemy;revived.hp=revived.max_hp*.35f;revived.velocity={};
   revived.abilities.revived=revived.abilities.reward_claimed=true;
   revived.abilities.pending_death=false;revived.abilities.promise_remaining=0;revived.abilities.blast_used=true;
   revived.abilities.feedback=0;
   revived.abilities.field_cooldown=std::max(3.f,revived.abilities.field_cooldown);
   revived.windup=revived.action_remaining=revived.recovery_remaining=revived.attack_clock=0;
   revived.slow_remaining=revived.burn_remaining=revived.poison_remaining=revived.frost_remaining=revived.mark_remaining=revived.armor_break_remaining=revived.lure_remaining=0;
   revived.poison_layers=revived.frost_layers=0;revived.shield_remaining=revived.buff_remaining=0;
   reviving_enemies_.push_back({revived,2.2f});
  }
 }
}

void Game::update_danger_zones(float dt){
 for(auto& zone:danger_zones){
  if(zone.following){auto owner=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& enemy){return enemy.id==zone.owner_id&&enemy.hp>0;});if(owner==enemies.end()){if(zone.age<zone.duration&&denial_kind(zone.kind))last_output_denial_end_=total_elapsed();zone.age=zone.duration;continue;}zone.pos=owner->pos;}
  if(zone.age>=zone.duration)continue;
  zone.age+=dt;
  if(zone.age>=zone.duration&&denial_kind(zone.kind))last_output_denial_end_=total_elapsed();
  if(zone.kind==DangerKind::Explosion&&!zone.triggered&&zone.age>=zone.warning){
   zone.triggered=true;
   if(length(player.pos-zone.pos)<=zone.radius+13){float before=player.hp;hurt_player(zone.damage,zone.from_boss,true,-1,zone.from_boss?"首领死亡爆炸":"延迟死亡／事件爆炸");if(player.hp<before&&player.hp<=0)pressure_metrics.death_cause=zone.from_boss?"首领死亡爆炸":"延迟死亡／事件爆炸";}
  }
 }
 for(auto& link:soul_links)if(!link.broken){
  auto first=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& enemy){return enemy.id==link.first&&enemy.hp>0;});
  auto second=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& enemy){return enemy.id==link.second&&enemy.hp>0;});
  if(!pressure_active(PressureKind::SoulLink)||link.source_id!=pressure_event.id||first==enemies.end()||second==enemies.end()||length(first->pos-second->pos)>link.max_distance)link.broken=true;
 }
 for(auto pending=reviving_enemies_.begin();pending!=reviving_enemies_.end();){
  pending->second=std::max(0.f,pending->second-dt);
  if(pending->second>0){++pending;continue;}
  if(enemies.size()>=enemy_capacity()){
   // Normal spawning reserves these slots. If another bounded producer fills
   // one first, retain the visible marker until the body can actually return.
   for(auto& zone:danger_zones)if(zone.kind==DangerKind::Revival&&zone.owner_id==pending->first.id)zone.age=std::min(zone.age,zone.warning);
   ++pending;continue;
  }
  Enemy revived=pending->first;apply_dark_tide(revived);resolve_scene_collision(revived.pos,revived.radius);
  enemies.push_back(revived);++pressure_metrics.revivals;
  for(auto& zone:danger_zones)if(zone.kind==DangerKind::Revival&&zone.owner_id==revived.id)zone.triggered=true;
  pending=reviving_enemies_.erase(pending);
 }
 danger_zones.erase(std::remove_if(danger_zones.begin(),danger_zones.end(),[&](const DangerZone& zone){return !zone_alive(*this,zone);}),danger_zones.end());
}

void Game::damage_pressure_facilities(Vec2 center,float radius,float damage){
 if(damage<=0||!std::isfinite(damage))return;
 for(auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone_active(*this,zone)&&length(center-zone.pos)<=radius+24){
  zone.hp=std::max(0.f,zone.hp-damage);
  if(zone.hp<=0){zone.age=zone.duration;coins+=3;if(gems.size()<config.max_gems)gems.push_back({zone.pos,8,0});else if(!gems.empty())gems.front().value+=8;else gain_xp(8);notify("黑暗祭坛已摧毁：附近敌人失去祭坛减伤。");}
 }
}

bool Game::update_enemy_abilities(Enemy& enemy,float dt){
 auto& ability=enemy.abilities;ability.feedback=std::max(0.f,ability.feedback-dt);
 if(ability.promise_remaining>0){
  ability.promise_remaining=std::max(0.f,ability.promise_remaining-dt);
  if(ability.promise_remaining<.0001f)ability.promise_remaining=0;
  if(ability.promise_remaining<=0){if(ability.pending_death){enemy.hp=0;return true;}return false;}
  Vec2 direction=normalized(player.pos-enemy.pos);enemy.velocity={};
  enemy.pos=enemy_attack_endpoint(enemy.pos,enemy.pos+direction*(enemy.speed*1.5f*dt),enemy.radius);
  resolve_scene_collision(enemy.pos,enemy.radius);if(std::abs(direction.x)>.05f)enemy.facing_left=direction.x<0;
  if(length(enemy.pos-player.pos)<enemy.radius+13){float before=player.hp;hurt_player(enemy.damage*1.5f,enemy.kind==EnemyKind::Boss,false,-1,"虚妄之诺追击");if(player.hp<before&&player.hp<=0)pressure_metrics.death_cause="虚妄之诺追击";}
  return true;
 }
 const uint32_t fields=ability_bit(EnemyAbility::Smoke)|ability_bit(EnemyAbility::Distortion)|ability_bit(EnemyAbility::NaturalOrder)|ability_bit(EnemyAbility::Regression);
 if(ability.mask&fields){
  ability.field_cooldown=std::max(0.f,ability.field_cooldown-dt);
  if(ability.field_cooldown<=0&&length(enemy.pos-player.pos)<360&&enemy_attack_visible(enemy)){
   DangerZone zone;zone.kind=has_ability(enemy,EnemyAbility::Smoke)?DangerKind::Smoke:has_ability(enemy,EnemyAbility::Distortion)?DangerKind::Distortion:has_ability(enemy,EnemyAbility::NaturalOrder)?DangerKind::NaturalOrder:DangerKind::Regression;
   zone.owner_id=enemy.id;zone.from_boss=enemy.kind==EnemyKind::Boss;zone.following=zone.kind==DangerKind::NaturalOrder||zone.kind==DangerKind::Regression;
   zone.pos=zone.following?enemy.pos:zone.kind==DangerKind::Smoke?player.pos:(enemy.pos+player.pos)*.5f;
   zone.radius=enemy.kind==EnemyKind::Boss?125.f:100.f;zone.warning=1.3f;zone.duration=zone.warning+10;
   ability.field_cooldown=add_danger_zone(zone)?12.f+unit_random(ability_rng_)*3.f:2.f;
  }
 }
 if(ability.shadow_class<0)return false;

 // Shadow hunters reuse the ordinary movement/attack fields, never a player
 // build, and cannot spawn children. Previews reserve their outgoing slots.
 for(auto& shot:effects)if(shot.hostile&&shot.target_id==enemy.id&&shot.age<shot.duration){
  if(shot.kind==EffectKind::Warning&&shot.evolved&&!shot.triggered&&shot.age+dt>=shot.delay){shot.kind=EffectKind::EnemyShot;shot.damage=shot.energy;shot.duration=shot.secondary_tick;shot.age=shot.delay=0;shot.energy=0;shot.evolved=false;shot.pattern=shot.proc_generated?3:0;}
  else if(shot.kind==EffectKind::EnemyShot&&shot.pattern==3&&shot.age>.2f&&shot.age<1.1f){float speed=length(shot.velocity);Vec2 heading=normalized(shot.velocity),target=normalized(player.pos-shot.pos);float turn=std::atan2(heading.x*target.y-heading.y*target.x,dot(heading,target));shot.velocity=rotate(heading,std::clamp(turn,-.9f*dt,.9f*dt))*speed;}
 }
 Vec2 direction=normalized(player.pos-enemy.pos);float distance=length(player.pos-enemy.pos);
 float speed=enemy.speed*scene_slow(enemy.pos,enemy.radius)*(enemy.slow_remaining>0?enemy.slow_factor:1.f)*(enemy.buff_remaining>0?1.3f:1.f);
 bool fixed=false;
 if(enemy.windup>0||enemy.action_remaining>0||enemy.recovery_remaining>0)enemy.attack_animation_time+=dt;
 if(enemy.windup>0){
  enemy.windup=std::max(0.f,enemy.windup-dt);fixed=true;
  if(enemy.windup<=0){
   enemy.recovery_remaining=enemy.boss_attack==104?1.2f:.8f;
   if(enemy.boss_attack==104)enemy.action_remaining=enemy.action_duration;
   if(enemy.boss_attack==105){Vec2 delta=player.pos-enemy.attack_start;float range=length(delta);if(range<=125+13&&(range<13||dot(normalized(delta),enemy.attack_direction)>=std::cos(.65f)))hurt_player(enemy.damage*1.15f);}
   ++enemy.phase;
  }
 }else if(enemy.action_remaining>0){
  Vec2 from=enemy.pos;enemy.action_remaining=std::max(0.f,enemy.action_remaining-dt);fixed=true;
  enemy.pos=enemy.attack_start+(enemy.attack_end-enemy.attack_start)*(1-enemy.action_remaining/std::max(.001f,enemy.action_duration));
  if(segment_distance(player.pos,from,enemy.pos)<=enemy.radius+13)hurt_player(enemy.damage);
 }else if(enemy.recovery_remaining>0){enemy.recovery_remaining=std::max(0.f,enemy.recovery_remaining-dt);speed*=.1f;}
 else{
  if(ability.shadow_class<2){if(distance<145)direction=direction*(-1);else if(distance<245)direction={};}
  enemy.attack_clock+=dt;
  const float interval=(ability.shadow_class==0?3.8f:ability.shadow_class==1?4.4f:3.5f)*enemy_attack_multiplier();
  const float range=ability.shadow_class==0?360.f:ability.shadow_class==1?350.f:280.f;
  const int pattern=100+ability.shadow_class*2+(enemy.phase%2);
  const size_t needed=pattern==101?5:pattern==100?3:pattern==102||pattern==103?2:1;
  if(enemy.attack_clock>=interval&&distance<range&&enemy_attack_visible(enemy)&&effects.size()+needed<=config.max_effects&&can_start_enemy_attack(enemy)){
   enemy.attack_clock=0;enemy.boss_attack=pattern;enemy.attack_start=enemy.pos;enemy.attack_target=player.pos;enemy.attack_direction=normalized(player.pos-enemy.pos);enemy.attack_end=enemy.pos;
   enemy.windup=pattern==105?.9f:1.05f;enemy.attack_animation_time=0;fixed=true;
   auto preview=[&](Vec2 from,Vec2 aim,float reach,float velocity,float delay,float factor,bool tracking=false){Effect shot;shot.kind=EffectKind::Warning;shot.pos=from;shot.end=enemy_attack_endpoint(from,from+aim*reach,7);shot.radius=7;shot.velocity=aim*velocity;shot.delay=delay;shot.duration=delay+.25f;shot.energy=enemy.damage*factor;shot.secondary_tick=std::min(reach,length(shot.end-from)+2)/velocity;shot.pattern=1;shot.hostile=shot.evolved=true;shot.proc_generated=tracking;shot.target_id=enemy.id;shot.enemy_kind=enemy.kind;shot.enemy_visual=enemy.visual;shot.count=pattern;add_effect(shot);};
   if(pattern==100){for(int shot=0;shot<3;++shot)preview(enemy.pos,enemy.attack_direction,430,225,enemy.windup+shot*.18f,.65f);}
   else if(pattern==101){for(int shot=0;shot<5;++shot)preview(enemy.pos,rotate(enemy.attack_direction,(shot-2)*.22f),380,180,enemy.windup,.55f);}
   else if(pattern==102){Vec2 side={-enemy.attack_direction.y,enemy.attack_direction.x};for(int shot:{-1,1}){Vec2 from=enemy.pos+side*(shot*18.f);preview(from,normalized(enemy.attack_target-from),370,145,enemy.windup,.7f,true);}}
   else if(pattern==103){for(int pulse=0;pulse<2;++pulse){Effect field;field.kind=EffectKind::Warning;field.pos=enemy.attack_target+Vec2{-enemy.attack_direction.y,enemy.attack_direction.x}*(pulse*100.f);field.radius=58;field.delay=enemy.windup+pulse*.35f;field.duration=field.delay+.3f;field.damage=enemy.damage*.8f;field.hostile=true;field.target_id=enemy.id;field.enemy_kind=enemy.kind;field.enemy_visual=enemy.visual;field.count=pattern;add_effect(field);}}
   else{Effect warning;warning.kind=EffectKind::Warning;warning.pos=enemy.pos;warning.delay=enemy.windup;warning.duration=warning.delay+.3f;warning.target_id=enemy.id;warning.enemy_kind=enemy.kind;warning.enemy_visual=enemy.visual;warning.hostile=true;warning.count=pattern;
    if(pattern==104){enemy.action_duration=.68f;enemy.attack_end=enemy_attack_endpoint(enemy.pos,enemy.pos+enemy.attack_direction*250,enemy.radius);warning.end=enemy.attack_end;warning.pattern=1;warning.radius=enemy.radius+9;warning.duration+=enemy.action_duration;}
    else{warning.end=enemy.attack_direction;warning.radius=125;warning.angle=.65f;warning.pattern=4;}
    add_effect(warning);
   }
  }
 }
 if(!fixed)enemy.pos+=direction*(speed*dt)+enemy.velocity*dt;
 enemy.velocity=enemy.velocity*std::max(0.f,1-dt*9);resolve_scene_collision(enemy.pos,enemy.radius);
 Vec2 facing=enemy.windup>0||enemy.action_remaining>0?enemy.attack_direction:direction;if(std::abs(facing.x)>.05f)enemy.facing_left=facing.x<0;
 if(length(enemy.pos-player.pos)<=enemy.radius+13)hurt_player(enemy.damage);
 return true;
}
}

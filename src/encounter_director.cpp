#include "encounter_director.hpp"
#include <algorithm>
#include <cmath>

namespace maple {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr float tau=2*pi;
float finite_or(float value,float fallback){return std::isfinite(value)?value:fallback;}
float bounded(float value,float lo,float hi,float fallback){return std::clamp(finite_or(value,fallback),lo,hi);}
bool combat_kind(EncounterKind kind){return kind>=EncounterKind::Swarm&&kind<=EncounterKind::Crisis;}
float progression(const DirectorInput& input){return std::clamp(input.elapsed/input.chapter_duration+.08f*input.chapter+.04f*input.difficulty,0.f,1.f);}
}

std::array<EncounterRule,6> default_encounter_rules(){return {{
 {EncounterKind::Swarm,3.f,0.f,2.5f,16.f,8.f,1.8f,24,4,SpawnGeometry::Arc,true},
 {EncounterKind::Encirclement,2.4f,.10f,3.5f,18.f,11.f,2.5f,24,6,SpawnGeometry::OpenRing,true},
 {EncounterKind::Rush,2.1f,.16f,3.f,12.f,10.f,2.f,15,3,SpawnGeometry::Lane,true},
 {EncounterKind::EliteHunt,1.7f,.27f,4.f,24.f,12.f,4.f,9,2,SpawnGeometry::Arc,true},
 {EncounterKind::Crisis,1.5f,.38f,4.f,18.f,14.f,2.5f,20,4,SpawnGeometry::OpenRing,true},
 {EncounterKind::Breather,0.f,0.f,.6f,12.f,9.f,1.f,0,0,SpawnGeometry::Scatter,false}
}};}

EncounterDirector::EncounterDirector(DirectorConfig config):config_(config){
 config_.first_encounter=bounded(config_.first_encounter,0,600,12);
 config_.encounter_gap_min=bounded(config_.encounter_gap_min,0,120,8);
 config_.encounter_gap_max=bounded(config_.encounter_gap_max,config_.encounter_gap_min,120,14);
 config_.min_safe_distance=bounded(config_.min_safe_distance,240,2000,320);
 config_.escape_half_angle=bounded(config_.escape_half_angle,.35f,1.2f,.52f);
 config_.low_health=bounded(config_.low_health,.1f,.7f,.32f);
 config_.high_pressure=bounded(config_.high_pressure,.4f,1.f,.78f);
 config_.pressure_decay_seconds=bounded(config_.pressure_decay_seconds,.1f,60,7);
 config_.breather_duration=bounded(config_.breather_duration,2,60,12);
 config_.breather_cooldown=bounded(config_.breather_cooldown,2,120,18);
 config_.boss_telegraph=bounded(config_.boss_telegraph,.4f,15,4);
 config_.boss_recovery=bounded(config_.boss_recovery,1,60,10);
 config_.ambient_interval=bounded(config_.ambient_interval,.1f,10,.8f);
 config_.max_spawns_per_update=std::min<std::size_t>(64,config_.max_spawns_per_update);
 for(auto& rule:config_.rules){
  if(!combat_kind(rule.kind))rule.weight=0;
  rule.weight=bounded(rule.weight,0,100,0);
  rule.unlock_progress=bounded(rule.unlock_progress,0,1,0);
  rule.telegraph=bounded(rule.telegraph,.4f,15,3);
  rule.duration=bounded(rule.duration,1,120,16);
  rule.cooldown=bounded(rule.cooldown,1,120,9);
  rule.spawn_interval=bounded(rule.spawn_interval,.15f,30,2);
  rule.base_budget=std::clamp(rule.base_budget,0,256);
  rule.batch_size=std::clamp(rule.batch_size,0,32);
  if(rule.kind==EncounterKind::Breather){rule.weight=0;rule.base_budget=rule.batch_size=0;rule.important=false;}
  else rule.important=true; // Combat encounters always share the one major-encounter slot.
 }
 reset();
}
void EncounterDirector::reset(std::uint32_t seed,int chapter){
 seed_=seed;chapter_=std::clamp(chapter,0,1000000);
 random_state_=seed^(0x9e3779b9u*(std::uint32_t(chapter_)+1u));
 if(random_state_==0)random_state_=0xa341316cu;
 serial_=0;budget_=spawn_index_=elite_hunts_=0;kind_=last_kind_=EncounterKind::None;phase_=EncounterPhase::Idle;
 remaining_=spawn_clock_=ambient_clock_=pressure_=escape_angle_=0;
 next_encounter_=config_.first_encounter;boss_sent_=boss_seen_=boss_request_started_=false;rule_={};
}
std::uint32_t EncounterDirector::random_word(){
 random_state_^=random_state_<<13;random_state_^=random_state_>>17;random_state_^=random_state_<<5;return random_state_;
}
float EncounterDirector::random_unit(){return float(random_word()>>8)*(1.f/16777216.f);}

EncounterRule EncounterDirector::select_rule(const DirectorInput& input){
 std::array<float,6> weights{};float total=0;
 const float progress=progression(input);
 for(std::size_t i=0;i<config_.rules.size();++i){
  const auto& rule=config_.rules[i];if(rule.weight<=0||progress<rule.unlock_progress)continue;
  if(!combat_kind(rule.kind))continue;
  float weight=rule.weight;
  if(rule.kind==last_kind_)weight*=.15f;
  weights[i]=weight;total+=weight;
 }
 if(total<=0){EncounterRule none;none.kind=EncounterKind::None;none.weight=0;return none;}
 float roll=random_unit()*total;
 for(std::size_t i=0;i<weights.size();++i){if(weights[i]>0&&roll<weights[i])return config_.rules[i];roll-=weights[i];}
 for(std::size_t i=weights.size();i>0;--i)if(weights[i-1]>0)return config_.rules[i-1];
 return config_.rules.front();
}
void EncounterDirector::begin(const EncounterRule& rule,const DirectorInput& input,DirectorOutput& output){
 rule_=rule;kind_=rule.kind;phase_=EncounterPhase::Telegraph;remaining_=rule.telegraph;
 ++serial_;if(kind_==EncounterKind::EliteHunt)++elite_hunts_;spawn_index_=0;spawn_clock_=0;boss_sent_=boss_seen_=false;
 const float intensity=1+.16f*std::min(input.chapter,6)+.22f*input.difficulty+.45f*progression(input);
 budget_=std::clamp(int(std::round(rule.base_budget*intensity)),0,256);
 if(kind_==EncounterKind::Boss)budget_=1;
 const float move_length=std::hypot(input.move_x,input.move_y);
 escape_angle_=move_length>.1f?std::atan2(input.move_y,input.move_x):random_unit()*tau;
 output.announced=true;
}
void EncounterDirector::finish(bool aborted,DirectorOutput& output){
 output.ended=true;output.ended_kind=kind_;output.aborted=aborted;
 if(kind_!=EncounterKind::None)last_kind_=kind_;
 phase_=EncounterPhase::Cooldown;kind_=EncounterKind::None;budget_=0;
 remaining_=rule_.cooldown;next_encounter_=config_.encounter_gap_min+random_unit()*(config_.encounter_gap_max-config_.encounter_gap_min);
}
void EncounterDirector::spawn_batch(const DirectorInput& input,DirectorOutput& output,int count,bool ambient){
 const bool boss=!ambient&&kind_==EncounterKind::Boss;
 const int normal_count=std::max(0,ambient?count:std::min(count,budget_));
 const float multiplier=boss?1.f:input.spawn_budget_multiplier;
 const int requested=int(std::round(normal_count*multiplier));
 output.normal_spawn_budget+=normal_count;output.event_spawn_budget+=requested;output.requested_spawns+=requested;
 const std::size_t available=input.enemy_count>=input.max_enemies?0:input.max_enemies-input.enemy_count;
 const std::size_t per_update=std::size_t(std::round(config_.max_spawns_per_update*input.spawn_budget_multiplier));
 const std::size_t capacity=std::min(available,per_update);
 count=output.spawns.size()>=capacity?0:std::min(requested,int(capacity-output.spawns.size()));
 output.capacity_limited+=requested-count;
 // A capped batch is accounted for, never saved up into a later catch-up burst.
 if(!ambient&&!boss)budget_=std::max(0,budget_-normal_count);
 if(boss&&count>0)budget_=0;
 const float safe_distance=std::max(config_.min_safe_distance,input.spawn_radius);
 for(int i=0;i<count;++i){
  DirectorSpawn command;command.encounter_id=ambient?0:serial_;
  float angle=random_unit()*tau;
  float radius=safe_distance+30+random_unit()*90;
  if(ambient){
   if(input.reserved_escape_half_angle>0)angle=input.reserved_escape_angle+input.reserved_escape_half_angle+random_unit()*(tau-2*input.reserved_escape_half_angle);
   float roll=random_unit();float progress=progression(input);
   command.role=roll<.52f?DirectorEnemyRole::Common:DirectorEnemyRole::Swarm;
   if(progress>.12f&&roll>.82f)command.role=DirectorEnemyRole::Fast;
   if(progress>.28f&&roll>.90f)command.role=DirectorEnemyRole::Ranged;
   if(progress>.42f&&roll>.96f)command.role=DirectorEnemyRole::Support;
   if(progress>.32f&&roll>.94f&&roll<=.96f)command.role=DirectorEnemyRole::Ground;
   if(progress>.22f&&roll>.88f&&roll<=.90f)command.role=DirectorEnemyRole::Charger;
  }else{
   const int index=spawn_index_++;
   switch(rule_.geometry){
   case SpawnGeometry::OpenRing:
    // Sample the occupied arc only. The escape wedge is never filled by jitter.
    angle=escape_angle_+config_.escape_half_angle+(tau-2*config_.escape_half_angle)*((index%12+.5f)/12.f);break;
   case SpawnGeometry::Lane:angle=escape_angle_+pi+(index%2==0?-.2f:.2f)+(random_unit()-.5f)*.08f;radius+=float(index%3)*45;break;
   case SpawnGeometry::Arc:angle=escape_angle_+pi+(random_unit()-.5f)*1.7f;break;
   case SpawnGeometry::Scatter:break;
   }
   switch(kind_){
   case EncounterKind::Swarm:command.role=DirectorEnemyRole::Swarm;command.health_scale=.8f;break;
   case EncounterKind::Encirclement:command.role=index%4==0?DirectorEnemyRole::Armored:DirectorEnemyRole::Swarm;command.speed_scale=.9f;break;
   case EncounterKind::Rush:command.role=index%4==0?DirectorEnemyRole::Charger:DirectorEnemyRole::Fast;command.speed_scale=1.3f;command.health_scale=.8f;break;
   case EncounterKind::EliteHunt:command.role=(index==0||(elite_hunts_%2==0&&index==4))?DirectorEnemyRole::Elite:index%3==0?DirectorEnemyRole::Support:DirectorEnemyRole::Ranged;break;
   case EncounterKind::Crisis:command.role=index%7==0?DirectorEnemyRole::Ground:index%4==0?DirectorEnemyRole::Ranged:index%3==0?DirectorEnemyRole::Armored:DirectorEnemyRole::Swarm;break;
   case EncounterKind::Boss:command.role=DirectorEnemyRole::Boss;radius+=80;break;
   default:command.role=DirectorEnemyRole::Common;break;
   }
  }
  command.offset_x=std::cos(angle)*radius;command.offset_y=std::sin(angle)*radius;
  output.spawns.push_back(command);
 }
}
void EncounterDirector::snapshot(const DirectorInput& input,DirectorOutput& output) const {
 output.kind=kind_;output.phase=phase_;output.encounter_id=serial_;
 output.geometry=kind_==EncounterKind::None?SpawnGeometry::Scatter:rule_.geometry;
 output.remaining=std::max(0.f,remaining_);output.cooldown_remaining=phase_==EncounterPhase::Cooldown?output.remaining:0;
 output.pressure=pressure_;output.budget_remaining=int(std::round(budget_*(kind_==EncounterKind::Boss?1.f:input.spawn_budget_multiplier)));output.escape_angle=escape_angle_;
 output.escape_half_angle=(kind_==EncounterKind::Encirclement||kind_==EncounterKind::Crisis)?config_.escape_half_angle:0;
 output.min_spawn_distance=std::max(config_.min_safe_distance,input.spawn_radius);
 output.important_active=(phase_==EncounterPhase::Telegraph||phase_==EncounterPhase::Active)&&rule_.important;
 output.allow_special_events=phase_==EncounterPhase::Idle&&!input.boss_due&&!input.boss_alive&&!input.external_important_active;
 output.suppress_environment=phase_!=EncounterPhase::Idle||input.external_important_active;
}
DirectorOutput EncounterDirector::update(float dt,const DirectorInput& raw){
 DirectorOutput output;
 DirectorInput input=raw;
 input.elapsed=bounded(input.elapsed,0,1e8f,0);input.chapter_duration=bounded(input.chapter_duration,.05f,1e7f,600);
 input.chapter=std::clamp(input.chapter,0,1000000);input.difficulty=std::clamp(input.difficulty,0,2);
 input.hp_ratio=bounded(input.hp_ratio,0,1,1);input.recent_pressure=bounded(input.recent_pressure,0,1,0);
 input.spawn_radius=bounded(input.spawn_radius,0,5000,520);input.move_x=finite_or(input.move_x,0);input.move_y=finite_or(input.move_y,0);
 input.spawn_budget_multiplier=bounded(input.spawn_budget_multiplier,1,3,1);
 input.reserved_escape_angle=finite_or(input.reserved_escape_angle,0);input.reserved_escape_half_angle=bounded(input.reserved_escape_half_angle,0,1.2f,0);
 if(input.chapter!=chapter_)reset(seed_,input.chapter);
 if(!std::isfinite(dt)||dt<=0){snapshot(input,output);return output;}
 dt=std::min(dt,1.f); // No catch-up burst after a stalled frame.
 const float density=input.max_enemies==0?1.f:std::min(1.f,float(double(input.enemy_count)/double(input.max_enemies)));
 const float target=std::max({input.recent_pressure,density*.8f,(1-input.hp_ratio)*.55f});
 pressure_+=(target-pressure_)*std::min(1.f,dt/(target>pressure_?.8f:config_.pressure_decay_seconds));
 if(!input.boss_due)boss_request_started_=false;
 if(input.hp_ratio<=0){
  if(kind_!=EncounterKind::None)finish(true,output);
  snapshot(input,output);output.allow_special_events=false;output.suppress_environment=true;return output;
 }
 // Baseline pressure continues through telegraphs, encounters, cooldowns,
 // authored objectives and Boss fights. Health never lowers this cadence.
 ambient_clock_-=dt;
 if(ambient_clock_<=0){
  const float progress=progression(input);
  ambient_clock_+=config_.ambient_interval/(1+progress*.9f+input.difficulty*.15f);
  const int count=std::clamp(1+int(progress*2+input.difficulty*.5f),1,4);
  spawn_batch(input,output,count,true);
 }
 // Existing authored objectives own the major encounter slot until they finish.
 if(input.external_important_active&&!input.boss_alive){
  if(kind_!=EncounterKind::None)finish(true,output);
  snapshot(input,output);return output;
 }
 if(input.boss_due&&!boss_request_started_&&!input.external_important_active){
  if(kind_!=EncounterKind::None)finish(true,output);
  begin({EncounterKind::Boss,1,0,config_.boss_telegraph,0,config_.boss_recovery,1,1,1,SpawnGeometry::Arc,true},input,output);
  boss_request_started_=true;snapshot(input,output);return output;
 }
 if(input.boss_alive&&kind_!=EncounterKind::Boss){
  if(kind_!=EncounterKind::None)finish(true,output);
  EncounterRule boss{EncounterKind::Boss,1,0,config_.boss_telegraph,0,config_.boss_recovery,1,1,1,SpawnGeometry::Arc,true};
  begin(boss,input,output);phase_=EncounterPhase::Active;remaining_=0;boss_sent_=boss_seen_=true;budget_=0;output.announced=false;output.started=true;
 }
 if(kind_==EncounterKind::Boss){
  if(input.boss_alive)boss_seen_=true;
  if(boss_sent_&&!input.boss_alive&&(boss_seen_||input.boss_defeated)){
   finish(false,output);snapshot(input,output);return output;
  }
  if(phase_==EncounterPhase::Telegraph){remaining_=std::max(0.f,remaining_-dt);if(remaining_<=0){phase_=EncounterPhase::Active;output.started=true;}}
  if(phase_==EncounterPhase::Active&&!boss_sent_){spawn_batch(input,output,1,false);boss_sent_=std::any_of(output.spawns.begin(),output.spawns.end(),[](const DirectorSpawn& spawn){return spawn.role==DirectorEnemyRole::Boss;});}
  snapshot(input,output);return output;
 }
 if(phase_==EncounterPhase::Telegraph){
  remaining_=std::max(0.f,remaining_-dt);
  if(remaining_<=0){phase_=EncounterPhase::Active;remaining_=rule_.duration;output.started=true;}
 }else if(phase_==EncounterPhase::Active){
  remaining_=std::max(0.f,remaining_-dt);
  if(remaining_<=0){finish(false,output);snapshot(input,output);return output;}
 }else if(phase_==EncounterPhase::Cooldown){
  remaining_=std::max(0.f,remaining_-dt);if(remaining_<=0)phase_=EncounterPhase::Idle;
 }
 if(phase_==EncounterPhase::Active){
  spawn_clock_-=dt;
  if(spawn_clock_<=0){spawn_clock_+=rule_.spawn_interval;spawn_batch(input,output,rule_.batch_size,false);}
 }else if(phase_==EncounterPhase::Idle){
  next_encounter_-=dt;
  if(next_encounter_<=0){
   auto rule=select_rule(input);
   if(rule.kind!=EncounterKind::None){begin(rule,input,output);snapshot(input,output);return output;}
   next_encounter_=std::max(1.f,config_.encounter_gap_min);
  }
 }
 snapshot(input,output);return output;
}
const char* EncounterDirector::encounter_name(EncounterKind kind){
 switch(kind){
 case EncounterKind::Swarm:return "怪潮";
 case EncounterKind::Encirclement:return "留出口包围";
 case EncounterKind::Rush:return "高速突袭";
 case EncounterKind::EliteHunt:return "精英狩猎";
 case EncounterKind::Crisis:return "限时危机";
 case EncounterKind::Breather:return "已停用旧阶段";
 case EncounterKind::Boss:return "Boss 阶段";
 default:return "持续怪潮";
 }
}
const char* EncounterDirector::encounter_hint(EncounterKind kind){
 switch(kind){
 case EncounterKind::Swarm:return "怪潮即将从一侧涌入，保持移动清出道路。";
 case EncounterKind::Encirclement:return "包围即将形成，沿标记出口离开包围圈。";
 case EncounterKind::Rush:return "高速敌人即将成列突袭，向侧面移动避开冲锋。";
 case EncounterKind::EliteHunt:return "精英携护卫入场，优先处理远程和支援。";
 case EncounterKind::Crisis:return "限时危机即将开始，守住出口并避开地面预警。";
 case EncounterKind::Breather:return "旧喘息调度已停用，基础怪潮持续运行。";
 case EncounterKind::Boss:return "Boss 即将入场，清理附近敌人并留意攻击预警。";
 default:return "基础怪潮持续推进，移动并收集经验。";
 }
}
} // namespace maple


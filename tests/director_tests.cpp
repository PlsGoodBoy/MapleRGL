#ifdef NDEBUG
#undef NDEBUG
#endif
#include "encounter_director.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <vector>
using namespace maple;
namespace {
constexpr float pi=3.14159265358979323846f;
DirectorConfig one_kind(EncounterKind kind){
 DirectorConfig config;config.first_encounter=0;config.encounter_gap_min=config.encounter_gap_max=100;
 for(auto& rule:config.rules){rule.weight=rule.kind==kind?1.f:0.f;rule.unlock_progress=0;rule.telegraph=.4f;rule.duration=3;rule.cooldown=1;rule.spawn_interval=.2f;}
 return config;
}
void same_output(const DirectorOutput& a,const DirectorOutput& b){
 assert(a.kind==b.kind&&a.phase==b.phase&&a.ended_kind==b.ended_kind&&a.geometry==b.geometry);
 assert(a.encounter_id==b.encounter_id&&a.remaining==b.remaining&&a.pressure==b.pressure&&a.budget_remaining==b.budget_remaining);
 assert(a.announced==b.announced&&a.started==b.started&&a.ended==b.ended&&a.aborted==b.aborted);
 assert(a.important_active==b.important_active&&a.allow_special_events==b.allow_special_events&&a.suppress_environment==b.suppress_environment);
 assert(a.escape_angle==b.escape_angle&&a.escape_half_angle==b.escape_half_angle&&a.spawns.size()==b.spawns.size());
 for(std::size_t i=0;i<a.spawns.size();++i){
  const auto& x=a.spawns[i];const auto& y=b.spawns[i];
  assert(x.role==y.role&&x.offset_x==y.offset_x&&x.offset_y==y.offset_y&&x.speed_scale==y.speed_scale&&x.health_scale==y.health_scale&&x.encounter_id==y.encounter_id);
 }
}
void safety(const DirectorOutput& out,const DirectorInput& input,std::size_t frame_cap=16){
 const auto available=input.enemy_count>=input.max_enemies?0:input.max_enemies-input.enemy_count;
 assert(out.spawns.size()<=available&&out.spawns.size()<=frame_cap);
 for(const auto& spawn:out.spawns){
  const float distance=std::hypot(spawn.offset_x,spawn.offset_y);
  assert(std::isfinite(distance)&&distance+.01f>=out.min_spawn_distance);
  assert(spawn.speed_scale>0&&spawn.health_scale>0);
  if(out.escape_half_angle>0){
   float relative=std::remainder(std::atan2(spawn.offset_y,spawn.offset_x)-out.escape_angle,2*pi);
   assert(std::abs(relative)>out.escape_half_angle);
  }
 }
 if(out.phase==EncounterPhase::Telegraph||out.kind==EncounterKind::Breather||out.phase==EncounterPhase::Cooldown)assert(out.spawns.empty());
 if(out.important_active)assert(!out.allow_special_events);
}
void deterministic_sequences(){
 EncounterDirector a,b,c;a.reset(42);b.reset(42);c.reset(314);
 DirectorInput input;input.chapter_duration=180;
 bool differs=false;std::set<EncounterKind> kinds;
 for(int i=0;i<1800;++i){
  input.elapsed=i*.1f;input.hp_ratio=i>900&&i<1050?.5f:1.f;input.recent_pressure=i>900&&i<1050?.55f:0;
  auto x=a.update(.1f,input),y=b.update(.1f,input),z=c.update(.1f,input);same_output(x,y);safety(x,input);
  if(x.announced)kinds.insert(x.kind);
  if(x.kind!=z.kind||x.phase!=z.phase||x.spawns.size()!=z.spawns.size())differs=true;
  if(!x.spawns.empty()&&!z.spawns.empty()&&x.spawns[0].offset_x!=z.spawns[0].offset_x)differs=true;
 }
 assert(differs&&kinds.size()>=3);
 // Reset really restores the deterministic stream, including chapter-specific variation.
 a.reset(77,2);b.reset(77,2);input={};input.chapter=2;
 for(int i=0;i<300;++i){input.elapsed=i*.1f;same_output(a.update(.1f,input),b.update(.1f,input));}
}
void every_wave_has_real_commands(){
 const EncounterKind kinds[]={EncounterKind::Swarm,EncounterKind::Encirclement,EncounterKind::Rush,EncounterKind::EliteHunt,EncounterKind::Crisis,EncounterKind::Breather};
 for(auto kind:kinds){
  EncounterDirector director(one_kind(kind));director.reset(17);
  DirectorInput input;input.spawn_radius=450;input.move_x=1;
  if(kind==EncounterKind::Breather){input.hp_ratio=.7f;input.recent_pressure=.4f;}
  auto first=director.update(.1f,input);assert(first.announced&&first.phase==EncounterPhase::Telegraph&&first.kind==kind&&first.spawns.empty());
  int spawned=0,elites=0;bool started=false,ended=false;
  for(int i=0;i<60;++i){
   input.elapsed+=.1f;auto out=director.update(.1f,input);safety(out,input);
   started|=out.started;
   if(!out.spawns.empty())assert(started&&out.phase==EncounterPhase::Active);
   for(const auto& spawn:out.spawns){
    ++spawned;if(spawn.role==DirectorEnemyRole::Elite)++elites;
    if(kind==EncounterKind::Rush)assert(spawn.role==DirectorEnemyRole::Fast&&spawn.speed_scale>1);
    if(kind==EncounterKind::Swarm)assert(spawn.role==DirectorEnemyRole::Swarm);
    if(kind==EncounterKind::Encirclement)assert(out.geometry==SpawnGeometry::OpenRing&&out.escape_half_angle>=.35f);
   }
   if(out.ended){assert(out.ended_kind==kind&&!out.aborted&&out.phase==EncounterPhase::Cooldown&&out.cooldown_remaining>0);ended=true;break;}
  }
  assert(started&&ended);assert(kind==EncounterKind::Breather?spawned==0:spawned>0);
  if(kind==EncounterKind::EliteHunt)assert(elites==1);
 }
}
void capacity_and_safety(){
 auto config=one_kind(EncounterKind::Encirclement);config.max_spawns_per_update=3;config.min_safe_distance=600;
 for(auto& rule:config.rules){rule.batch_size=32;rule.base_budget=200;}
 EncounterDirector director(config);DirectorInput input;input.enemy_count=9;input.max_enemies=10;input.spawn_radius=30;
 bool spawned=false;
 for(int i=0;i<40;++i){auto out=director.update(.1f,input);safety(out,input,3);if(!out.spawns.empty()){spawned=true;assert(out.min_spawn_distance==600&&out.spawns.size()==1);}}
 assert(spawned);
 input.enemy_count=10;for(int i=0;i<20;++i){auto out=director.update(.1f,input);assert(out.spawns.empty());}
 input.max_enemies=0;input.enemy_count=0;for(int i=0;i<20;++i)assert(director.update(.1f,input).spawns.empty());
 // A stalled frame never creates an unbounded catch-up burst.
 input.max_enemies=500;input.enemy_count=0;director.reset(2);director.update(.1f,input);
 auto burst=director.update(500,input);safety(burst,input,3);
 auto before=director.update(0,input);same_output(before,director.update(-1,input));same_output(before,director.update(std::numeric_limits<float>::quiet_NaN(),input));
}
void relief_and_pressure(){
 EncounterDirector director(one_kind(EncounterKind::Swarm));DirectorInput input;
 assert(director.update(.1f,input).kind==EncounterKind::Swarm);
 input.hp_ratio=.2f;auto relief=director.update(.1f,input);
 assert(relief.kind==EncounterKind::Breather&&relief.announced&&relief.ended&&relief.ended_kind==EncounterKind::Swarm&&relief.aborted&&relief.spawns.empty());
 bool began=false,ended=false;
 for(int i=0;i<240;++i){auto out=director.update(.1f,input);assert(out.spawns.empty()&&!out.allow_special_events&&out.suppress_environment);began|=out.started;ended|=out.ended&&out.ended_kind==EncounterKind::Breather;}
 assert(began&&ended);
 EncounterDirector pressured(one_kind(EncounterKind::Swarm));input={};input.recent_pressure=1;
 assert(pressured.update(.1f,input).kind==EncounterKind::Breather);
 auto config=one_kind(EncounterKind::Swarm);EncounterDirector healthy(config),wounded(config),hard(config);
 input={};auto normal=healthy.update(.1f,input);input.hp_ratio=.5f;input.recent_pressure=.6f;auto reduced=wounded.update(.1f,input);
 assert(reduced.kind==EncounterKind::Swarm&&reduced.budget_remaining<normal.budget_remaining);
 input={};input.chapter=2;input.difficulty=2;auto increased=hard.update(.1f,input);assert(increased.budget_remaining>normal.budget_remaining);
 input={};input.hp_ratio=0;assert(healthy.update(.1f,input).spawns.empty());
}
void external_mutex_and_boss(){
 auto config=one_kind(EncounterKind::Rush);config.boss_telegraph=.4f;
 EncounterDirector director(config);DirectorInput input;
 assert(director.update(.1f,input).announced);
 input.external_important_active=true;auto interrupted=director.update(.1f,input);
 assert(interrupted.ended&&interrupted.aborted&&!interrupted.important_active&&interrupted.spawns.empty());
 input.boss_due=true;
 for(int i=0;i<30;++i){auto out=director.update(.1f,input);assert(!out.announced&&!out.important_active&&out.spawns.empty()&&!out.allow_special_events);}
 input.external_important_active=false;input.max_enemies=1;input.enemy_count=1;
 auto warning=director.update(.1f,input);assert(warning.kind==EncounterKind::Boss&&warning.announced&&warning.spawns.empty());
 for(int i=0;i<20;++i){auto out=director.update(.1f,input);assert(out.kind==EncounterKind::Boss&&out.spawns.empty());}
 input.enemy_count=0;auto spawn=director.update(.1f,input);assert(spawn.spawns.size()==1&&spawn.spawns[0].role==DirectorEnemyRole::Boss);safety(spawn,input);
 input.boss_due=false;input.boss_alive=true;input.enemy_count=1;input.hp_ratio=.1f;
 for(int i=0;i<60;++i){auto out=director.update(.1f,input);assert(out.kind==EncounterKind::Boss&&out.important_active&&out.spawns.empty()&&!out.allow_special_events);}
 input.boss_alive=false;input.boss_defeated=true;input.enemy_count=0;input.hp_ratio=1;
 auto defeated=director.update(.1f,input);assert(defeated.ended&&defeated.ended_kind==EncounterKind::Boss&&!defeated.aborted&&defeated.phase==EncounterPhase::Cooldown);
 // A future Endless boss may use the same director after boss_due fell to false.
 director.update(.1f,input);input.boss_due=true;
 assert(director.update(.1f,input).kind==EncounterKind::Boss);
 // An external objective that interrupts only a pending warning must not consume the boss.
 input.external_important_active=true;auto cancelled=director.update(.1f,input);assert(cancelled.ended&&cancelled.aborted);
 input.external_important_active=false;assert(director.update(.1f,input).kind==EncounterKind::Boss);
}
void default_seed_variety(){
 std::set<EncounterKind> seen;std::set<std::vector<int>> sequences;
 for(std::uint32_t seed=1;seed<=8;++seed){
  EncounterDirector director;director.reset(seed);DirectorInput input;input.chapter_duration=300;std::vector<int> sequence;
  for(int i=0;i<3000;++i){input.elapsed=i*.1f;auto out=director.update(.1f,input);safety(out,input);if(out.announced){seen.insert(out.kind);sequence.push_back(int(out.kind));}}
  sequences.insert(sequence);
 }
 for(auto kind:{EncounterKind::Swarm,EncounterKind::Encirclement,EncounterKind::Rush,EncounterKind::EliteHunt,EncounterKind::Crisis})assert(seen.count(kind)>0);
 assert(sequences.size()>=6);
}
}
int main(){
 deterministic_sequences();every_wave_has_real_commands();capacity_and_safety();relief_and_pressure();external_mutex_and_boss();default_seed_variety();
 std::cout<<"PASS director: deterministic seeds, six configurable wave recipes and Boss, warnings and endings, escape wedge, capacity, pressure relief and major encounter mutex.\n";
}

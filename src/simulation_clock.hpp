#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace maple {
// Combat always advances at wall-clock speed. Encounter pace belongs to Game.
class SimulationClock {
public:
 static constexpr double step_seconds=1.0/60.0;
 static constexpr size_t max_steps_per_frame=8;
 template<class Step> size_t advance(double real_seconds,Step step){
  if(!std::isfinite(real_seconds)||real_seconds<=0)return 0;
  accumulated_+=real_seconds;
  size_t count=0;
  while(accumulated_+1e-12>=step_seconds&&count<max_steps_per_frame){
   accumulated_-=step_seconds;++count;++steps_;
   if(!step(float(step_seconds))){suspend();break;}
  }
  accumulated_=std::max(0.0,accumulated_);
  peak_backlog_=std::max(peak_backlog_,accumulated_);
  if(accumulated_+1e-12>=step_seconds)++limited_frames_;
  return count;
 }
 // Older diagnostics may still supply a speed argument; it cannot accelerate actions.
 template<class Step> size_t advance(double real_seconds,int,Step step){return advance(real_seconds,step);}
 // A deliberate pause discards pending wall time; it never advances any game clock.
 void suspend(){accumulated_=0;}
 void reset(){*this={};}
 double backlog_seconds()const{return accumulated_;}
 double peak_backlog_seconds()const{return peak_backlog_;}
 uint64_t steps()const{return steps_;}
 uint64_t limited_frames()const{return limited_frames_;}
private:
 double accumulated_=0,peak_backlog_=0;
 uint64_t steps_=0,limited_frames_=0;
};
}

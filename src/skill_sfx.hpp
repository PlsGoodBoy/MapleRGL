#pragma once
#include "game.hpp"
#include <array>
#include <cmath>

namespace maple {
// Bounded, sample-clocked pulse/triangle/noise voices. Independent of combat RNG.
class SkillSfx {
 struct Voice {int left=0,total=0,tier=1,style=0,stage=0;float phase=0,second=0,frequency=440,gain=.3f;uint32_t noise=0x17539;};
 std::array<Voice,12> voices_{};std::array<uint64_t,5> next_{};uint64_t clock_=0;
public:
 std::array<uint64_t,14> triggered{};uint64_t limited=0;int peak_voices=0;
 void clear(){for(auto& v:voices_)v.left=0;}
 void play(FxStyle style,int tier,FxStage event,bool heavy=false){
  int stage=event==FxStage::Cast?0:event==FxStage::Impact?1:event==FxStage::Fusion?4:event==FxStage::End?3:2;
  if(stage==2)return;
  if(clock_<next_[stage]){++limited;return;}
  auto it=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.left==0;});
  if(it==voices_.end()){it=std::min_element(voices_.begin(),voices_.end(),[](const Voice& a,const Voice& b){return a.tier<b.tier;});if(it->tier>tier){++limited;return;}}
  static const float frequencies[]={590,820,220,135,310,110,330,1070,670,150,740,510,930,620};
  *it={};it->style=int(style);it->tier=tier;it->stage=stage;it->frequency=frequencies[int(style)]*(1+.045f*tier);it->gain=stage==3&&tier<3?.17f:stage==0?.24f:heavy?.4f:.3f;
  it->total=it->left=int(22050*(stage==4?.33f:stage==3?.2f:stage==0?.09f:.12f));
  next_[stage]=clock_+uint64_t(22050*(stage==0?.055f:stage==1?.07f:.12f));++triggered[int(style)];
 }
 float sample(){
  ++clock_;float sum=0;int active=0;
  for(auto& v:voices_)if(v.left>0){
   ++active;float t=1-float(v.left)/v.total,env=std::min(1.f,t/.04f)*(1-t)*(1-t);
   float sweep=v.stage==0?.75f+t*.65f:v.stage==4?1+t*.8f:1.7f-t*1.2f;
   float f=v.frequency*sweep;v.phase=std::fmod(v.phase+f/22050.f,1.f);v.second=std::fmod(v.second+f*1.5f/22050.f,1.f);
   v.noise^=v.noise<<13;v.noise^=v.noise>>17;v.noise^=v.noise<<5;float noise=float(v.noise&65535)/32768.f-1;
   const bool percussive=v.style==3||v.style==4||v.style==5||v.style==9;
   float duty=v.style==1||v.style==12?.125f:.25f,pulse=(v.phase<duty?1.f:0.f)-duty,triangle=1-4*std::abs(v.phase-.5f);
   float voice=percussive?triangle*.6f+noise*(v.stage==1?.55f:.3f)*(1-t):pulse*.9f+noise*.09f*(1-t);
   if(v.tier>=3&&t>.12f)voice+=((v.second<.25f?1.f:0.f)-.25f)*.32f;
   sum+=voice*env*v.gain;--v.left;
  }
  peak_voices=std::max(peak_voices,active);return sum/(1+.12f*std::max(0,active-3));
 }
};
}

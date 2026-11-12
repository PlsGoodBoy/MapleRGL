#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace maple {
enum class MusicScene { Menu, Camp, Battle, Pressure, Boss, Victory, Defeat };
enum class MusicCue { LevelUp, Choice, Scroll, Fusion, Boss, Victory, Defeat, Danger };
struct MusicTrack {
 const char* id; const char* title; MusicScene scene; int bpm,bars;
 int step_samples() const {return int(22050.0*30.0/bpm+.5);}
 std::uint64_t loop_samples() const {return std::uint64_t(step_samples())*bars*8;}
 double loop_seconds() const {return double(loop_samples())/22050.0;}
};
struct MusicSample {float music=0,cue=0;};

// Original, authored note scores. Each track has its own A/B melodies, bridge,
// chord itinerary and bass rhythm. No external samples, MIDI or game BGM.
class Chiptune {
 struct Score {
  MusicTrack info; int groove; float duty;
  std::array<int,64> a,b;std::array<int,32> bridge;
  std::array<int,8> roots_a,roots_b,minor,bass;
 };
 static const std::array<Score,8>& scores(){
  static const std::array<Score,8> values={{
  {{"maple_departure","枫叶启程",MusicScene::Menu,112,32},0,0.250f,
   {72,-1,76,79,0,76,74,72,67,-1,69,72,74,-1,71,0,76,79,81,-1,79,76,74,0,72,-1,67,69,72,-1,0,0,77,0,76,74,72,-1,69,72,74,77,79,-1,77,74,71,0,76,0,79,84,83,79,76,74,72,-1,0,67,72,-1,-1,0},
   {79,81,84,-1,83,79,76,0,77,-1,81,79,77,74,72,0,76,79,83,-1,81,79,74,71,72,-1,76,79,76,-1,0,0,81,-1,79,77,76,72,69,0,74,0,77,81,79,-1,77,74,76,79,84,83,81,79,76,74,72,-1,76,74,72,-1,-1,0},
   {69,-1,72,76,74,72,69,0,65,-1,69,72,74,-1,72,0,67,-1,71,74,77,74,71,0,79,-1,77,74,76,74,72,0},
   {48,53,55,48,53,50,55,48},{57,53,55,48,57,50,55,48},{0,0,0,0,0,1,0,0},{0,99,7,99,12,99,7,99}},
  {{"hearth_notebook","炉边手记",MusicScene::Camp,92,32},1,0.500f,
   {71,-1,-1,69,67,-1,0,0,74,-1,71,0,69,-1,67,0,76,-1,74,71,69,-1,-1,0,67,-1,66,0,67,-1,0,0,72,-1,76,74,71,-1,69,0,67,-1,71,0,74,-1,-1,0,69,-1,72,71,69,66,64,0,67,-1,-1,0,62,-1,67,0},
   {79,-1,78,76,74,-1,71,0,76,-1,74,72,71,-1,67,0,69,-1,71,74,76,-1,74,0,71,-1,-1,0,67,69,71,0,72,-1,74,76,79,-1,76,0,74,-1,72,69,71,-1,67,0,69,72,76,-1,74,71,69,66,67,-1,71,69,67,-1,-1,0},
   {64,-1,67,71,74,-1,0,0,60,-1,64,67,72,-1,71,0,62,-1,66,69,74,72,69,0,71,-1,69,66,67,-1,-1,0},
   {43,47,48,43,48,43,50,43},{40,48,50,43,48,47,50,43},{0,1,0,0,0,0,0,0},{0,99,99,7,0,99,12,99}},
  {{"canopy_steps","林梢跃步",MusicScene::Battle,124,48},2,0.250f,
   {74,0,77,76,74,81,0,79,77,74,72,0,74,-1,76,77,79,0,83,81,79,77,76,0,74,77,81,0,79,76,74,0,77,79,81,-1,79,77,74,72,76,0,79,77,76,74,72,0,74,76,77,81,83,81,79,76,74,-1,69,0,72,74,-1,0},
   {86,0,84,81,83,79,0,81,77,-1,79,81,84,81,79,77,76,79,83,86,84,-1,81,79,77,0,74,76,77,81,79,0,84,83,81,79,77,0,79,81,83,-1,79,77,76,79,81,0,86,84,83,81,79,77,76,72,74,-1,77,76,74,-1,-1,0},
   {69,72,74,0,77,0,76,74,67,71,74,0,79,0,77,76,65,69,72,0,77,74,72,69,69,72,76,79,81,-1,79,76},
   {38,41,43,38,41,43,45,38},{46,41,43,38,46,43,45,38},{1,0,0,1,0,0,1,1},{0,7,99,12,0,99,7,10}},
  {{"frontier_stride","荒原行进",MusicScene::Battle,132,48},3,0.500f,
   {64,64,0,67,71,-1,69,67,66,0,66,69,74,-1,71,69,67,67,0,71,74,76,74,71,64,-1,0,62,64,67,71,0,72,72,0,71,69,67,64,0,67,-1,69,71,74,0,71,67,66,69,72,74,71,69,66,62,64,-1,67,66,64,-1,0,0},
   {76,-1,79,78,76,74,71,0,74,74,0,76,78,-1,74,71,79,-1,78,76,74,71,67,0,71,0,74,76,79,-1,0,0,81,79,76,74,72,-1,71,69,79,0,76,74,71,69,67,0,74,78,81,-1,79,78,74,71,76,-1,74,71,67,66,64,0},
   {60,64,67,72,71,-1,67,64,62,66,69,74,72,-1,69,66,59,64,67,71,74,-1,71,67,63,66,71,75,74,71,66,63},
   {40,50,43,40,48,43,50,40},{40,50,43,47,48,43,50,40},{1,0,0,1,0,0,0,1},{0,0,7,0,12,0,7,10}},
  {{"astral_corridor","星纹回廊",MusicScene::Battle,116,48},4,0.125f,
   {81,-1,0,76,79,-1,77,76,72,76,79,-1,0,76,74,72,77,-1,81,84,83,-1,79,76,80,83,86,83,80,-1,76,0,81,-1,84,83,81,79,76,0,77,81,84,-1,0,81,79,77,76,-1,79,83,86,83,80,76,81,-1,76,72,69,-1,0,0},
   {88,-1,84,83,81,-1,79,76,84,88,91,-1,88,84,83,79,86,-1,83,81,79,77,74,0,83,86,89,-1,86,83,80,76,84,-1,81,79,77,76,72,0,81,84,88,86,84,-1,81,79,83,-1,80,76,74,71,68,0,69,-1,72,76,81,-1,-1,0},
   {65,-1,69,72,76,77,81,0,62,-1,65,69,72,74,77,0,64,68,71,76,80,83,86,0,83,80,76,71,68,71,76,80},
   {45,48,41,40,45,41,40,45},{48,48,43,40,41,45,40,45},{1,0,0,0,1,0,0,1},{0,99,12,7,0,12,99,7}},
  {{"undertow_drive","暗潮疾行",MusicScene::Pressure,144,48},5,0.250f,
   {74,74,0,77,74,81,0,77,73,0,76,73,81,76,73,0,74,77,81,0,84,81,77,74,76,76,0,73,69,73,76,0,77,0,81,77,86,81,77,0,79,79,0,82,79,86,82,79,81,85,88,85,81,76,73,69,74,-1,77,76,74,0,69,0},
   {86,0,86,84,81,82,81,77,85,0,88,85,81,76,73,0,86,84,81,77,74,77,81,84,85,88,85,81,76,-1,73,0,89,86,81,77,81,86,89,0,91,0,89,86,82,79,77,0,88,85,81,76,73,76,81,85,86,-1,84,81,77,76,74,0},
   {70,74,77,0,82,77,74,0,67,70,74,0,79,74,70,0,69,73,76,81,85,81,76,73,81,0,76,0,73,0,69,0},
   {38,45,38,45,46,43,45,38},{46,45,38,45,46,43,45,38},{1,0,1,0,0,1,0,1},{0,0,12,0,7,0,10,7}},
  {{"iron_bastion","铁城鼓点",MusicScene::Boss,128,48},6,0.500f,
   {60,-1,60,67,0,63,62,60,56,-1,0,60,63,-1,62,0,55,55,0,62,67,65,62,59,60,-1,67,0,63,62,60,0,65,-1,65,68,67,65,63,60,56,60,63,68,67,-1,63,0,55,-1,59,62,65,62,59,0,60,-1,0,55,60,-1,-1,0},
   {72,-1,75,74,72,-1,67,0,68,72,75,-1,74,72,68,0,71,-1,74,77,79,77,74,71,72,-1,79,75,74,72,67,0,77,77,0,80,79,77,75,72,80,-1,79,75,72,68,67,0,79,77,74,71,67,65,62,59,60,-1,63,67,72,-1,-1,0},
   {68,-1,67,63,60,63,67,0,65,-1,68,72,70,68,65,0,62,65,68,71,74,71,68,65,67,-1,65,62,59,-1,55,0},
   {36,44,43,36,41,44,43,36},{36,44,43,36,41,44,43,36},{1,0,0,1,1,0,0,1},{0,99,0,7,0,12,7,0}},
  {{"prism_tempest","棱镜风暴",MusicScene::Boss,160,56},7,0.125f,
   {78,81,85,81,78,0,80,81,76,80,83,80,76,0,73,76,81,85,88,85,81,78,0,80,80,84,87,84,80,76,73,0,85,81,78,81,85,90,88,85,83,80,76,80,83,88,87,83,84,87,92,87,84,80,76,73,78,-1,81,80,78,0,73,0},
   {90,88,85,81,85,88,90,0,88,87,83,80,83,87,88,0,93,90,88,85,81,85,88,90,92,87,84,80,76,80,84,87,90,93,97,93,90,88,85,81,88,92,95,92,88,87,83,80,87,84,80,76,73,76,80,84,90,-1,88,85,81,80,78,0},
   {74,78,81,85,90,85,81,78,71,75,78,83,87,83,78,75,73,77,80,85,89,85,80,77,80,84,87,92,87,84,80,76},
   {42,40,45,49,50,40,49,42},{50,40,45,49,50,40,49,42},{1,0,0,0,0,0,0,1},{0,12,7,0,12,7,10,7}}
  }};return values;
 }
 struct Playback {
  int track=0,last_step=-1,lead_note=72,bass_note=48,answer_note=72;
  std::uint64_t sample=0;
  double lead_phase=0,bass_phase=0,answer_phase=0,kick_phase=0;
  float lead_env=0,bass_env=0,answer_env=0;bool lead_gate=false,lead_tie=false,lead_hold=false,bass_gate=false,answer_gate=false;
  unsigned noise=0x5a17;int section=0;
 };
 Playback current_,outgoing_;bool crossfading_=false,auditioning_=false,softened_=false;
 float transition_=1,soft_gain_=1,duck_gain_=1,intensity_=0,intensity_target_=0;
 std::uint64_t clock_=0,entered_=0,request_since_=0;unsigned random_=0x34c7a51du;
 MusicScene scene_=MusicScene::Menu,requested_=MusicScene::Menu;
 std::array<int,7> last_track_{{0,-1,-1,-1,-1,-1,-1}};
 int transitions_=0;
 struct CueVoice {MusicCue kind=MusicCue::Choice;int sample=0,priority=0;bool active=false;double phase=0,low_phase=0;float env=0;};
 CueVoice cue_,outgoing_cue_;float cue_blend_=1;std::uint64_t last_cue_=0;bool has_cue_=false;
 static constexpr float rate=22050.f;
 static float frequency(int note){static const auto table=[](){std::array<float,128> values{};for(int i=0;i<128;++i)values[i]=440.f*std::pow(2.f,float(i-69)/12.f);return values;}();return table[note];}
 static float blep(double phase,double increment){
  if(phase<increment){double x=phase/increment;return float(x+x-x*x-1);}
  if(phase>1-increment){double x=(phase-1)/increment;return float(x*x+x+x+1);}return 0;
 }
 static float pulse(double& phase,int note,float duty){
  double increment=frequency(note)/rate;phase+=increment;if(phase>=1)phase-=1;
  float value=(phase<duty?1.f:-1.f)-float(2*duty-1);
  value+=blep(phase,increment);double shifted=phase-duty;if(shifted<0)shifted+=1;value-=blep(shifted,increment);
  return value*.5f;
 }
 static float triangle(double& phase,int note){phase+=frequency(note)/rate;if(phase>=1)phase-=1;return float(1-4*std::abs(phase-.5));}
 static int section_for(const Score& score,int bar){
  const int group=bar/8;
  if(score.info.bars==32){static constexpr int form[]={1,2,4,5};return form[group];}
  if(score.info.bars==56){static constexpr int form[]={0,1,2,3,4,6,5};return form[group];}
  return group;
 }
 static int melody(const Score& score,int step){
  int bar=step/8,section=section_for(score,bar),local=step%64;
  if(section==4)return score.bridge[local%32];
  if(section==2||section==6)return score.b[local];
  if(section==5&&local<32)return score.b[local+32];
  int note=score.a[local];
  // A variation uses a planned octave answer in the final two bars.
  if(section==3&&local>=48&&note>0)note+=12;
  // The introduction leaves space in alternating phrases.
  if(section==0&&bar%2==1&&step%8>=4)return 0;
  return note;
 }
 static float render(Playback& voice,float intensity){
  const auto& score=scores()[voice.track];const int step_length=score.info.step_samples();
  int step=int(voice.sample/step_length),within_samples=int(voice.sample%step_length);float within=float(within_samples)/step_length;
  int bar=step/8,beat=step%8;int section=section_for(score,bar);voice.section=section;
  if(step!=voice.last_step){
   voice.last_step=step;int note=melody(score,step);voice.lead_gate=note!=0;voice.lead_tie=note<0;
   if(note>0)voice.lead_note=note;
   voice.lead_hold=melody(score,(step+1)%(score.info.bars*8))<0;
   const auto& roots=(section==2||section==4||section==6)?score.roots_b:score.roots_a;
   int root=roots[bar%8],offset=score.bass[beat];
   if(section==0||section==4){offset=beat==0?0:beat==4?7:99;}
   voice.bass_gate=offset!=99;if(voice.bass_gate)voice.bass_note=root+offset;
   static constexpr int arp[]={0,7,12,7,3,7,12,15};int interval=arp[beat];
   if(!score.minor[bar%8]&&(interval==3||interval==15))++interval;
   voice.answer_note=root+24+interval;
   voice.answer_gate=section==2||section==3||section==5||section==6;
   if(section==1&&beat%2&&intensity>.55f)voice.answer_gate=true;
   if(score.groove==1)voice.answer_gate&=beat%2==1;
  }
  float lead_target=voice.lead_gate?voice.lead_tie?.66f:1.f-.36f*within:0;
  if(!voice.lead_hold&&within>.84f)lead_target=0;
  voice.lead_env+=(lead_target-voice.lead_env)*(lead_target>voice.lead_env?.018f:.008f);
  float bass_target=voice.bass_gate&&within<.82f?1.f-.25f*within:0;
  voice.bass_env+=(bass_target-voice.bass_env)*.012f;
  float answer_target=voice.answer_gate&&within<.42f?(1-within/.42f):0;
  voice.answer_env+=(answer_target-voice.answer_env)*.02f;
  float lead=pulse(voice.lead_phase,voice.lead_note,score.duty)*voice.lead_env;
  float bass=triangle(voice.bass_phase,voice.bass_note)*voice.bass_env;
  float answer=pulse(voice.answer_phase,voice.answer_note,.25f)*voice.answer_env;
  voice.noise^=voice.noise<<13;voice.noise^=voice.noise>>17;voice.noise^=voice.noise<<5;
  float hiss=(voice.noise&1)?1.f:-1.f;
  const bool quiet=score.info.scene==MusicScene::Menu||score.info.scene==MusicScene::Camp;
  bool kick=beat==0||beat==4||(score.groove==3&&beat==3)||(score.groove>=5&&beat==6);
  bool snare=beat==2||beat==6;
  float kick_env=kick?std::max(0.f,1-within*6):0;kick_env*=kick_env;
  voice.kick_phase+=(55+105*std::exp(-within*24))/rate;if(voice.kick_phase>=1)voice.kick_phase-=1;
  float thump=float(std::sin(voice.kick_phase*6.283185307))*kick_env;
  float snare_env=snare?std::max(0.f,1-within*9):0;snare_env*=snare_env;
  float hat_env=std::max(0.f,1-within*30);hat_env*=hat_env;
  float drums=thump*.15f+hiss*(snare_env*.09f+hat_env*.025f*(beat%2?1.3f:1.f));
  float density=section==0?.5f:section==4?.64f:1.f;
  float value=lead*.52f+bass*.22f+answer*(quiet?.085f:.14f)+drums*density*(quiet?.3f:1.f)*(1+intensity*.15f);
  ++voice.sample;if(voice.sample>=score.info.loop_samples()){voice.sample=0;voice.last_step=-1;}
  return value/(1+std::abs(value)*.25f);
 }
 unsigned random_word(){random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return random_;}
 int choose_track(MusicScene scene){
  int first=0,count=2;
  if(scene==MusicScene::Battle){first=2;count=3;}
  else if(scene==MusicScene::Pressure){first=5;count=1;}
  else if(scene==MusicScene::Boss){first=6;count=2;}
  else if(scene==MusicScene::Victory||scene==MusicScene::Defeat)return 1;
  int index=first+int(random_word()%unsigned(count));
  int pool=scene==MusicScene::Battle?2:scene==MusicScene::Pressure?3:scene==MusicScene::Boss?4:0;
  if(count>1&&index==last_track_[pool])index=first+(index-first+1)%count;
  return index;
 }
 void change_track(int track,MusicScene scene){
  outgoing_=current_;current_={};current_.track=track;current_.noise=0x5a17u+unsigned(track)*101u;
  crossfading_=true;transition_=0;scene_=scene;entered_=clock_;++transitions_;
  int pool=scene==MusicScene::Battle?2:scene==MusicScene::Pressure?3:scene==MusicScene::Boss?4:0;last_track_[pool]=track;
 }
 static bool idle(MusicScene scene){return scene==MusicScene::Menu||scene==MusicScene::Camp||scene==MusicScene::Victory||scene==MusicScene::Defeat;}
 void schedule(){
  if(auditioning_||requested_==scene_||crossfading_)return;
  const double resident=double(clock_-entered_)/rate,stable=double(clock_-request_since_)/rate;
  bool ending=requested_==MusicScene::Victory||requested_==MusicScene::Defeat;
  bool major=requested_==MusicScene::Boss||ending||idle(requested_)||idle(scene_);
  if(!major&&resident<24)return;
  if((scene_==MusicScene::Boss||scene_==MusicScene::Pressure)&&!major&&stable<6)return;
  if(requested_==MusicScene::Pressure&&stable<4)return;
  int step_length=scores()[current_.track].info.step_samples();
  // Major phase transitions wait at most one bar. Minor transitions use a two-bar phrase.
  int phrase=step_length*(major?8:16);
  if(ending||current_.sample%phrase==0)change_track(choose_track(requested_),requested_);
 }
 static int cue_priority(MusicCue cue){return cue==MusicCue::Victory||cue==MusicCue::Defeat?6:cue==MusicCue::Boss?4:cue==MusicCue::Danger?5:cue==MusicCue::Fusion?3:cue==MusicCue::Scroll?2:1;}
 static float cue_sample(CueVoice& voice){
  if(!voice.active)return 0;
  static constexpr std::array<std::array<int,8>,8> notes={{
   {{72,76,79,84,0,0,0,0}},{{79,84,0,0,0,0,0,0}},{{69,76,81,88,84,0,0,0}},
   {{60,67,72,79,84,91,88,96}},{{48,0,55,54,48,60,0,0}},{{72,76,79,84,79,84,88,96}},{{69,65,62,57,0,0,0,0}},{{86,0,74,0,0,0,0,0}}
  }};
  static constexpr int lengths[]={4,2,5,8,6,8,4,3};
  static constexpr int step_samples[]={2205,1654,2646,2205,3308,3308,4410,1323};
  int id=int(voice.kind),step_length=step_samples[id],step=voice.sample/step_length;
  if(step>=lengths[id]){voice.active=false;return 0;}
  int note=notes[id][step];float within=float(voice.sample%step_length)/step_length;
  float target=note&&within<.78f?1-within*.5f:0;voice.env+=(target-voice.env)*.012f;
  float sound=note?pulse(voice.phase,note,.5f)*voice.env*.37f+triangle(voice.low_phase,std::max(24,note-12))*voice.env*.085f:0;
  ++voice.sample;return sound;
 }
public:
 static constexpr int sample_rate=22050;
 static const std::array<MusicTrack,8>& tracks(){static const auto list=[](){std::array<MusicTrack,8> values{};for(size_t i=0;i<values.size();++i)values[i]=scores()[i].info;return values;}();return list;}
 static const char* scene_name(MusicScene scene){static constexpr const char* names[]={"菜单","营地","普通战斗","高压战斗","Boss战","胜利","失败"};return names[int(scene)];}
 void request_scene(MusicScene scene,float intensity=0){
  intensity_target_=std::clamp(intensity,0.f,1.f);
  if(scene!=requested_){requested_=scene;request_since_=clock_;}
 }
 void set_softened(bool value){softened_=value;}
 void cue(MusicCue kind){
  const int priority=cue_priority(kind);
  if(has_cue_&&clock_-last_cue_<std::uint64_t(rate*.18f)&&priority<=cue_.priority)return;
  if(cue_.active&&priority<cue_.priority)return;
  outgoing_cue_=cue_;cue_blend_=cue_.active?0.f:1.f;
  cue_={};cue_.kind=kind;cue_.priority=priority;cue_.active=true;last_cue_=clock_;has_cue_=true;
 }
 MusicSample next_sample(){
  schedule();intensity_+=(intensity_target_-intensity_)*.00004f;
  float value=render(current_,intensity_);
  if(crossfading_){float old=render(outgoing_,intensity_);transition_=std::min(1.f,transition_+1.f/(rate*1.4f));value=old*(1-transition_)+value*transition_;if(transition_>=1)crossfading_=false;}
  else if(clock_<std::uint64_t(rate*.25f))value*=float(clock_)/(rate*.25f);
  const bool ending=scene_==MusicScene::Victory||scene_==MusicScene::Defeat;
  float soft_target=softened_?.56f:ending?.65f:1.f;soft_gain_+=(soft_target-soft_gain_)*.0003f;
  float duck_target=cue_.active&&cue_.priority>=3&&cue_.kind!=MusicCue::Danger?.56f:1.f;duck_gain_+=(duck_target-duck_gain_)*.00022f;
  float jingle=cue_sample(cue_);
  if(cue_blend_<1){cue_blend_=std::min(1.f,cue_blend_+1.f/(rate*.012f));jingle=cue_sample(outgoing_cue_)*(1-cue_blend_)+jingle*cue_blend_;}
  ++clock_;return {value*soft_gain_*duck_gain_,jingle};
 }
 float next(){auto value=next_sample();return std::clamp(value.music+value.cue,-.98f,.98f);}
 float next(bool battle){request_scene(battle?MusicScene::Battle:MusicScene::Menu);return next();}
 void audition(int track,bool fade=true){
  if(track<0||track>=int(tracks().size()))return;
  auditioning_=true;change_track(track,tracks()[track].scene);cue_.active=outgoing_cue_.active=false;cue_blend_=1;
  if(!fade){crossfading_=false;transition_=1;}
 }
 void end_audition(){
  if(!auditioning_)return;auditioning_=false;
  // Restore the live scene immediately through the same fade, even if its normal dwell is longer.
  change_track(choose_track(requested_),requested_);request_since_=clock_;
 }
 bool auditioning() const{return auditioning_;}
 int track_index() const{return current_.track;}
 MusicScene scene() const{return scene_;}
 double playhead_seconds() const{return double(current_.sample)/rate;}
 std::uint64_t rendered_samples() const{return clock_;}
 int transition_count() const{return transitions_;}
 bool transitioning() const{return crossfading_;}
};
}

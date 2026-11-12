#include "chiptune.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using maple::Chiptune;
using maple::MusicScene;
using maple::MusicCue;
constexpr int rate=Chiptune::sample_rate;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void word(std::ofstream& out,std::uint32_t value,int bytes){for(int i=0;i<bytes;++i)out.put(char(value>>(i*8)));}
void write_wav(const std::filesystem::path& path,const std::vector<short>& pcm){
 std::ofstream out(path,std::ios::binary);require(bool(out),"Cannot open WAV output");
 out.write("RIFF",4);word(out,std::uint32_t(36+pcm.size()*2),4);out.write("WAVEfmt ",8);word(out,16,4);
 word(out,1,2);word(out,1,2);word(out,rate,4);word(out,rate*2,4);word(out,2,2);word(out,16,2);
 out.write("data",4);word(out,std::uint32_t(pcm.size()*2),4);
 out.write(reinterpret_cast<const char*>(pcm.data()),std::streamsize(pcm.size()*sizeof(short)));
 require(bool(out),"Cannot finish WAV output");
}
struct SampleStats {
 double energy=0,sum=0;float peak=0;std::uint64_t count=0,clipped=0;
 void add(float x){require(std::isfinite(x),"Non-finite audio sample");peak=std::max(peak,std::abs(x));energy+=double(x)*x;sum+=x;++count;if(std::abs(x)>=1)++clipped;}
 double rms()const{return std::sqrt(energy/std::max(std::uint64_t(1),count));}
 double dc()const{return sum/std::max(std::uint64_t(1),count);}
};
short pcm16(float x){return short(std::clamp(x,-1.f,1.f)*32767.f);}
struct Change {double seconds;int track;MusicScene scene;};
}

int main(int argc,char** argv){
 try {
  const std::filesystem::path output=argc>1?argv[1]:"artifacts/resonance/music";
  std::filesystem::create_directories(output);
  const auto start=std::chrono::steady_clock::now();
  std::ofstream report(output/"audio-report.json");report<<std::fixed<<std::setprecision(6);
  report<<"{\n  \"sample_rate\": "<<rate<<",\n  \"format\": \"mono PCM16\",\n  \"tracks\": [\n";
  double rendered_seconds=0;
  for(std::size_t index=0;index<Chiptune::tracks().size();++index){
   const auto& track=Chiptune::tracks()[index];Chiptune synth;synth.audition(int(index),false);
   std::vector<short> pcm;pcm.reserve(std::size_t(track.loop_samples()));SampleStats stats;
   for(std::uint64_t sample=0;sample<track.loop_samples();++sample){float value=synth.next_sample().music;stats.add(value);pcm.push_back(pcm16(value));}
   require(stats.rms()>.01&&stats.clipped==0,"Track must be audible and remain below full scale");
   write_wav(output/(std::string(track.id)+".wav"),pcm);rendered_seconds+=track.loop_seconds();
   if(index)report<<",\n";
   report<<"    {\"id\": \""<<track.id<<"\", \"title\": \""<<track.title<<"\", \"scene\": \""<<Chiptune::scene_name(track.scene)<<"\", \"bpm\": "<<track.bpm<<", \"bars\": "<<track.bars<<", \"loop_start_sample\": 0, \"loop_end_sample\": "<<track.loop_samples()<<", \"seconds\": "<<track.loop_seconds()<<", \"peak\": "<<stats.peak<<", \"rms\": "<<stats.rms()<<", \"dc\": "<<stats.dc()<<", \"clipped_samples\": "<<stats.clipped<<"}";
   std::cout<<track.id<<": "<<std::setprecision(3)<<track.loop_seconds()<<" s, peak="<<stats.peak<<", rms="<<stats.rms()<<"\n";
  }

  // One ordinary scene sequence: a brief pressure fluctuation, pause/restore,
  // a sustained high-pressure phase, Boss, victory, and return to camp.
  Chiptune scene;std::vector<short> sequence;sequence.reserve(rate*140);SampleStats scene_stats;std::vector<Change> changes;
  int previous_transitions=0,battle_track=-1,pause_track=-1;double pause_start=0;
  for(int sample=0;sample<rate*140;++sample){
   const double seconds=double(sample)/rate;
   if(sample%512==0){
    MusicScene target=seconds<8?MusicScene::Menu:seconds<40?MusicScene::Battle:seconds<43?MusicScene::Pressure:seconds<48?MusicScene::Battle:seconds<80?MusicScene::Pressure:seconds<126?MusicScene::Boss:seconds<132?MusicScene::Victory:MusicScene::Camp;
    scene.request_scene(target,seconds>=38&&seconds<80?.85f:.25f);scene.set_softened(seconds>=22&&seconds<27);
   }
   if(sample==rate*20)battle_track=scene.track_index();
   if(sample==rate*22){scene.cue(MusicCue::LevelUp);pause_track=scene.track_index();pause_start=scene.playhead_seconds();}
   if(sample==rate*27){require(scene.track_index()==pause_track,"Opening a paused upgrade must retain the song");require(std::abs(scene.playhead_seconds()-pause_start-5)<.001,"Pause softening must preserve musical time");scene.cue(MusicCue::Choice);}
   if(sample==rate*44)require(scene.track_index()==battle_track,"Brief pressure fluctuations should not replace the battle theme");
   if(sample==rate*55)require(scene.scene()==MusicScene::Pressure,"Sustained pressure should enter its theme");
   if(sample==rate*62)scene.cue(MusicCue::Scroll);
   if(sample==rate*72)scene.cue(MusicCue::Fusion);
   if(sample==rate*80)scene.cue(MusicCue::Boss);
   if(sample==rate*84)require(scene.scene()==MusicScene::Boss,"Boss entry should not wait for the minor-scene dwell timer");
   if(sample==rate*126)scene.cue(MusicCue::Victory);
   auto value=scene.next_sample();float mix=value.music+value.cue;scene_stats.add(mix);sequence.push_back(pcm16(mix));
   if(scene.transition_count()!=previous_transitions){previous_transitions=scene.transition_count();changes.push_back({seconds,scene.track_index(),scene.scene()});}
  }
  require(scene_stats.clipped==0,"Transition example should not clip");
  write_wav(output/"scene_transitions.wav",sequence);rendered_seconds+=140;

  Chiptune jingles;std::vector<short> cues;SampleStats cue_stats;
  for(int cue=0;cue<7;++cue){jingles.cue(MusicCue(cue));for(int sample=0;sample<rate*2;++sample){float value=jingles.next_sample().cue;cue_stats.add(value);cues.push_back(pcm16(value));}}
  write_wav(output/"seven_cues.wav",cues);rendered_seconds+=14;
  require(cue_stats.clipped==0&&cue_stats.rms()>.005,"Short cue export should be audible without clipping");

  Chiptune preview;preview.audition(7,false);preview.request_scene(MusicScene::Battle);
  for(int sample=0;sample<rate;++sample)preview.next_sample();
  require(preview.track_index()==7,"Live scene requests should not interrupt developer audition");
  preview.end_audition();require(preview.scene()==MusicScene::Battle&&!preview.auditioning(),"Leaving audition should restore the requested scene");
  // Re-enter the same scene pool once, with unrelated camp time between visits.
  int first_battle=preview.track_index();preview.request_scene(MusicScene::Camp);
  for(int sample=0;sample<rate*4;++sample)preview.next_sample();
  preview.request_scene(MusicScene::Battle);for(int sample=0;sample<rate*4;++sample)preview.next_sample();
  require(preview.track_index()!=first_battle,"An available battle pool should not repeat its previous track");

  const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  report<<"\n  ],\n  \"transitions\": [";
  for(std::size_t i=0;i<changes.size();++i){if(i)report<<",";report<<"\n    {\"seconds\": "<<changes[i].seconds<<", \"track\": \""<<Chiptune::tracks()[changes[i].track].id<<"\", \"scene\": \""<<Chiptune::scene_name(changes[i].scene)<<"\"}";}
  report<<"\n  ],\n  \"scene_peak\": "<<scene_stats.peak<<",\n  \"cue_peak\": "<<cue_stats.peak<<",\n  \"rendered_seconds\": "<<rendered_seconds<<",\n  \"export_wall_seconds\": "<<elapsed<<",\n  \"status\": \"passed\",\n  \"listening\": \"WAVs exported; no human listening claim.\"\n}\n";
  std::cout<<"PASS: eight authored loops, one scene sequence, seven cues, pause continuity and audition restore. Exported "<<rendered_seconds<<" audio seconds in "<<elapsed<<" seconds. No listening claim.\n";
  return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

#include "skill_sfx.hpp"
#include <fstream>
#include <iostream>
#include <vector>
using namespace maple;
int main(int argc,char** argv){
 if(argc<2)return 1;SkillSfx synth;std::vector<int16_t> pcm;float peak=0;
 for(int theme=0;theme<14;++theme)for(int sample=0;sample<33075;++sample){
  if(sample==0)synth.play(FxStyle(theme),1,FxStage::Cast);
  if(sample==6600)synth.play(FxStyle(theme),1,FxStage::Impact,true);
  if(sample==15400)synth.play(FxStyle(theme),4,FxStage::Cast);
  if(sample==19800)synth.play(FxStyle(theme),4,FxStage::Impact,true);
  if(sample==24200)synth.play(FxStyle(theme),4,FxStage::End,true);
  float v=synth.sample();peak=std::max(peak,std::abs(v));pcm.push_back(int16_t(std::clamp(v,-1.f,1.f)*25000));
 }
 std::ofstream out(argv[1],std::ios::binary);auto word=[&](uint32_t n,int bytes){for(int i=0;i<bytes;++i)out.put(char(n>>(8*i)));};out.write("RIFF",4);word(uint32_t(pcm.size()*2+36),4);out.write("WAVEfmt ",8);word(16,4);word(1,2);word(1,2);word(22050,4);word(44100,4);word(2,2);word(16,2);out.write("data",4);word(uint32_t(pcm.size()*2),4);out.write(reinterpret_cast<const char*>(pcm.data()),pcm.size()*2);
 std::cout<<"14 sound families / 21 seconds / peak "<<peak<<" / voices "<<synth.peak_voices<<'\n';return out.good()&&peak>0&&peak<1?0:1;
}

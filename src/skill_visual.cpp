#include "adventure_content.hpp"
namespace maple {
int Game::skill_visual_charge(Skill skill) const {
 return skill==Skill::EnergyBlast?energy_hits_:skill==Skill::ShadowBats?bat_hits_:skill==Skill::CoinBomb?combat_coins_:0;
}
FxStyle skill_visual_style(Skill s,uint32_t elements){
 const auto r=Game::skill_roles(s);const auto e=elements?elements:r;
 if(r&Rose)return FxStyle::Rose;if(r&Card)return FxStyle::Card;
 if(r&(Machine|Node))return FxStyle::Machine;
 if(r&Dragon)return FxStyle::Dragon;
 if(e&Poison)return FxStyle::Poison;if(r&Shadow)return FxStyle::Shadow;
 if(r&Gun)return FxStyle::Gun;if(r&Fist)return FxStyle::Fist;
 if(e&Cold)return FxStyle::Frost;if(e&Thunder)return FxStyle::Thunder;
 if(e&Fire)return FxStyle::Fire;if(r&(Holy|Shield))return FxStyle::Holy;
 if(r&(ArcaneField|Elemental))return FxStyle::Arcane;
 return Game::skill_owner(s)==CharacterClass::Mage?FxStyle::Arcane:FxStyle::Steel;
}
void Game::emit_skill_visual(FxStage stage,Skill source,Vec2 from,Vec2 to,float radius,float half,bool heavy,uint32_t elements){
 if(source==Skill::Count)return;
 const int tier=fusion_tier(source);
 if(tier==1&&skill_visuals.size()>=96&&(stage==FxStage::Cast||stage==FxStage::Impact||stage==FxStage::End)){++visual_dropped;return;}
 if(stage==FxStage::Cast)for(const auto& v:skill_visuals)if(v.stage==stage&&v.source==source&&v.age<.045f&&length(v.pos-from)<20){++visual_merged;return;}
 if(stage==FxStage::Impact){
  int impacts=0;for(auto& v:skill_visuals)if(v.stage==stage){++impacts;if(v.source==source&&v.age<.07f&&length(v.pos-from)<44){v.heavy|=heavy;++visual_merged;return;}}
  if(impacts>=40){++visual_dropped;return;}
 }
 if(skill_visuals.size()>=128){auto expendable=std::find_if(skill_visuals.begin(),skill_visuals.end(),[](const SkillVisual& v){return v.stage==FxStage::Impact||v.stage==FxStage::Cast||v.stage==FxStage::End;});if(expendable!=skill_visuals.end())skill_visuals.erase(expendable);else{++visual_dropped;return;}}
 SkillVisual v;v.source=source;v.stage=stage;v.pos=from;v.end=to;v.radius=radius;v.half_angle=half;v.heavy=heavy;v.elements=elements;v.tier=tier;v.serial=++visual_serial;
 v.duration=stage==FxStage::Fusion?.85f:stage==FxStage::End?.48f:stage==FxStage::Impact?.25f:stage==FxStage::Cast?.28f:heavy?.48f:.34f;
 skill_visuals.push_back(v);visual_peak=std::max(visual_peak,skill_visuals.size());
}
void Game::update_skill_visuals(float dt){for(auto& v:skill_visuals)v.age+=dt;skill_visuals.erase(std::remove_if(skill_visuals.begin(),skill_visuals.end(),[](const SkillVisual& v){return v.age>=v.duration;}),skill_visuals.end());}
}

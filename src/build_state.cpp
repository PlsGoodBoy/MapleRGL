#include "game.hpp"
#include "adventure_content.hpp"

namespace maple {
// Current authored base and fusion skills all have five ranks. One authority for growth and UI.
int Game::skill_level_cap(Skill skill){return int(skill)>=0&&int(skill)<skill_count?5:0;}
SkillStatus Game::skill_status(Skill skill) const {
 SkillStatus result;result.skill=skill;result.cap=skill_level_cap(skill);if(!result.cap)return result;
 result.level=skills[int(skill)];result.effective=effective_skill_level(skill);result.sealed=skill_sealed(skill);result.smoked=smoke_at(player.pos);result.cooldown=skill_cooldown_remaining(skill);return result;
}
FusionStatus Game::fusion_status(Skill result) const {
 FusionStatus view;int best=-1000;bool best_ready=false;
 for(const auto& recipe:fusion_recipes())if(recipe.released&&recipe.result==result){int score=0;bool ready=true;const auto inputs=fusion_inputs(recipe);for(Skill s:inputs){score+=skills[int(s)]?20+skills[int(s)]:-12;ready&=skills[int(s)]>=recipe.material_level;}score-=int(inputs.size());
  const bool priority=view.recipe&&(recipe.priority>view.recipe->priority||(recipe.priority==view.recipe->priority&&recipe.id<view.recipe->id));
  if(!view.recipe||(ready&&!best_ready)||(ready==best_ready&&(ready?priority:score>best||(score==best&&priority)))){best=score;best_ready=ready;view.recipe=&recipe;}
 }
 if(!view.recipe)return view;const auto& r=*view.recipe;view.first=skills[int(r.first)];view.second=skills[int(r.second)];
 view.known=true;view.equipped=skills[int(result)]>0;view.ready=fusion_ready(result);
 if(view.equipped)view.hint="已装备融合";
 else if(view.ready)view.hint="达标自动融合";
 else {view.hint="材料需各Lv.5";for(Skill input:fusion_inputs(r))if(!skills[int(input)]){view.hint=std::string("缺少 ")+skill_name(input);break;}}
 return view;
}
void Game::refresh_build_cache() const {
 if(cached_build_levels_==skills)return;
 cached_build_levels_=skills;
 for(auto& skill:ordered_skills_)if(skill!=Skill::Count&&!skills[int(skill)])skill=Skill::Count;
 for(int i=0;i<skill_count;++i)if(skills[i]&&std::find(ordered_skills_.begin(),ordered_skills_.end(),Skill(i))==ordered_skills_.end()){
  auto slot=std::find(ordered_skills_.begin(),ordered_skills_.end(),Skill::Count);if(slot!=ordered_skills_.end())*slot=Skill(i);
 }
 main_routes_.fill(Skill::Count);std::array<int,skill_count> scores{};scores.fill(-1);
 for(const auto& r:fusion_recipes())if(r.released&&!skills[int(r.result)]){int score=0;for(Skill s:fusion_inputs(r))score+=skills[int(s)]?10+skills[int(s)]:-8;score+=int(r.sources.size());
  for(Skill material:fusion_inputs(r))if(score>scores[int(material)]){scores[int(material)]=score;main_routes_[int(material)]=r.result;}
 }
 for(const auto& r:fusion_recipes())if(skills[int(r.result)]&&main_routes_[int(r.result)]==Skill::Count)main_routes_[int(r.result)]=r.result;
}
const std::array<Skill,8>& Game::skill_slots() const {refresh_build_cache();return ordered_skills_;}
Skill Game::main_fusion_route(Skill skill) const {refresh_build_cache();return int(skill)>=0&&int(skill)<skill_count?main_routes_[int(skill)]:Skill::Count;}
}

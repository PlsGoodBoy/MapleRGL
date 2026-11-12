#include "adventure_content.hpp"

namespace maple {
int Game::fusion_tier(Skill skill){static const auto tiers=[](){std::array<int,skill_count> values;values.fill(1);for(const auto& r:fusion_recipes())values[int(r.result)]=int(r.sources.size());return values;}();return skill==Skill::Count?1:tiers[int(skill)];}
bool Game::fusion_ready(Skill result) const {
 if(skills[int(result)])return false;
 for(const auto& r:fusion_recipes())if(r.result==result&&r.released){bool ready=true;for(Skill input:fusion_inputs(r))ready&=skills[int(input)]>=r.material_level;if(ready)return true;}return false;
}
void Game::auto_fuse_skills(){while(const auto* r=next_fusion(skills))apply_fusion(*r);}
int Game::fusion_result_level(Skill result) const {auto view=fusion_status(result);return view.recipe?fusion_rank(*view.recipe):1;}
std::string Game::fusion_preview(Skill learned,Skill removed) const {
 auto levels=skills;if(removed!=Skill::Count)levels[int(removed)]=0;
 if(learned!=Skill::Count)levels[int(learned)]=std::min(skill_level_cap(learned),levels[int(learned)]+1);
 const auto plan=fusion_plan(levels);std::string text;
 for(const auto* r:plan){if(!text.empty())text+="\n";text+="自动形成："+std::string(skill_name(r->result))+"（"+std::to_string(r->sources.size())+"源 · Lv."+std::to_string(fusion_rank(*r))+"）\n消耗：";
  bool first=true;for(Skill input:fusion_inputs(*r)){if(!first)text+=" + ";first=false;text+=skill_name(input);}text+="。共用材料的其他路线随之改变。";
 }return text;
}
std::string Game::fusion_requirement(Skill result) const {
 const auto view=fusion_status(result);if(!view.recipe)return {};const auto& r=*view.recipe;
 std::string text=std::to_string(r.sources.size())+"源融合 · 所有当前材料真实Lv.5\n";
 for(Skill input:fusion_inputs(r))text+=std::string(skill_name(input))+" Lv."+std::to_string(skills[int(input)])+"/5"+(skill_sealed(input)?"（封印会迁移）":"")+"\n";
 text+="基础来源：";for(Skill input:r.sources)text+=std::string(skill_name(input))+" / ";
 text+="\n形成Lv."+std::to_string(fusion_rank(r))+"，上限Lv.5。二项/三项/四项当前材料形成Lv.3/4/5；前置融合的历史投入保留记录，攻击倍率不连乘。";
 text+="\n同时达标优先四源→三源→二源，再按配方优先级和稳定ID裁决。";return text;
}
bool Game::apply_fusion(Skill result){
 const FusionRecipe* best=nullptr;
 for(const auto& r:fusion_recipes())if(r.released&&r.result==result){
  bool ready=!skills[int(result)];for(Skill input:fusion_inputs(r))ready&=skills[int(input)]>=r.material_level;
  if(ready&&(!best||r.priority>best->priority||(r.priority==best->priority&&r.id<best->id)))best=&r;
 }
 return best&&apply_fusion(*best);
}
bool Game::apply_fusion(const FusionRecipe& r){
 const Skill result=r.result;const auto inputs=fusion_inputs(r);auto replacement=skills;int investment=0;
 if(!r.released||skills[int(result)]||std::any_of(inputs.begin(),inputs.end(),[&](Skill input){return skills[int(input)]<r.material_level;}))return false;
 float remaining=.15f;for(Skill input:inputs)remaining=std::max(remaining,cooldown_[int(input)]);
 for(Skill input:inputs){
  investment+=skills[int(input)];
  for(const auto& old:scrolls.completed)if(old.result==input){
   const auto prior=std::find_if(fusion_recipes().begin(),fusion_recipes().end(),[&](const FusionRecipe& entry){return entry.id==old.recipe_id;});
   const int formed_rank=prior==fusion_recipes().end()?3:fusion_rank(*prior);
   investment+=std::max(0,old.investment-formed_rank);
  }
  replacement[int(input)]=0;
 }
 replacement[int(result)]=fusion_rank(r);
 int form=0;auto completed=std::find_if(scrolls.completed.begin(),scrolls.completed.end(),[&](const ScrollOffer& entry){return entry.result==result;});
 const bool first_creation=completed==scrolls.completed.end();
 if(!first_creation)form=completed->form;
 else{std::vector<int> forms;for(int i=0;i<form_count(result);++i)if(r.form_mask&(1u<<i))forms.push_back(i);if(!forms.empty())form=forms.size()==1?forms[0]:forms[rng_()%forms.size()];scrolls.completed.push_back({result,form});completed=std::prev(scrolls.completed.end());}
 completed->recipe_id=r.id;completed->investment=investment;
 refresh_build_cache();auto first_slot=ordered_skills_.end();
 for(auto slot=ordered_skills_.begin();slot!=ordered_skills_.end();++slot)if(std::find(inputs.begin(),inputs.end(),*slot)!=inputs.end()){if(first_slot==ordered_skills_.end())first_slot=slot;*slot=Skill::Count;}
 if(first_slot!=ordered_skills_.end())*first_slot=result;
 bool sealed=false,failed=false;for(Skill input:inputs){sealed|=skill_sealed(input);failed|=cast_failed_[int(input)];}
 skills=replacement;if(sealed)pressure_event.sealed=result;cast_failed_[int(result)]=failed;
 for(Skill base:r.sources)if(int(base)<64)consumed_skills|=uint64_t(1)<<int(base);
 for(Skill input:inputs){for(auto& object:scene_objects)if(object.source==input)object.source=result;remove_skill_runtime(input);cooldown_[int(input)]=0;}
 pending_resonance_effects_.erase(std::remove_if(pending_resonance_effects_.begin(),pending_resonance_effects_.end(),[&](const Effect& effect){return std::find(inputs.begin(),inputs.end(),effect.source)!=inputs.end();}),pending_resonance_effects_.end());
 cooldown_[int(result)]=remaining;cooldown_period_[int(result)]=skill_interval(result);
 if(result==Skill::DragonArmor&&first_creation){dragon_soul=40;if(form==0)shield_remaining=std::max(shield_remaining,1.f);}
 emit_skill_visual(FxStage::Fusion,result,player.pos,player.pos,40);
 ++metrics.fusions_created;if(metrics.first_fusion_time<0)metrics.first_fusion_time=total_elapsed();
 notify("自动融合："+std::string(skill_name(result))+" · "+std::to_string(r.sources.size())+"源 · Lv."+std::to_string(skills[int(result)]));add_event(EventKind::Fusion,player.pos,result);
 Effect pulse;pulse.kind=EffectKind::Frost;pulse.pos=player.pos;pulse.radius=45;pulse.duration=.45f;pulse.source=result;add_effect(pulse);return true;
}
}

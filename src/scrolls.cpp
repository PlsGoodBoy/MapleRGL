#include "game.hpp"
#include "battle_art.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace maple {
namespace {
const FusionRecipe* recipe_for(Skill result){for(const auto& recipe:fusion_recipes())if(recipe.result==result)return &recipe;return nullptr;}
bool form_allowed(Skill result,int form){const auto* recipe=recipe_for(result);return recipe&&form>=0&&form<Game::form_count(result)&&form<32&&(recipe->form_mask&(uint32_t(1)<<form));}
bool studied(const ScrollState& state,Skill result){return std::find(state.studied_results.begin(),state.studied_results.end(),result)!=state.studied_results.end();}
}
CharacterClass Game::skill_owner(Skill skill){
 if(skill>=Skill::BattleFocus&&skill<=Skill::EarthShatter)return CharacterClass::Warrior;
 if(skill>=Skill::PoisonMist&&skill<=Skill::MagicVortex)return CharacterClass::Mage;
 if(skill>=Skill::StormShot&&skill<=Skill::Decoy)return CharacterClass::Archer;
 switch(skill){
 case Skill::Arrow:case Skill::Pierce:case Skill::Summon:case Skill::Spread:case Skill::Rain:case Skill::Ricochet:case Skill::HunterMark:case Skill::ArrowTrap:return CharacterClass::Archer;
 case Skill::Homing:case Skill::Lightning:case Skill::Zone:case Skill::Frost:case Skill::Meteor:case Skill::Rune:case Skill::OrbitalOrb:case Skill::GravityWell:return CharacterClass::Mage;
 case Skill::Nova:case Skill::Cleave:case Skill::Orbit:case Skill::Charge:case Skill::BladeWave:case Skill::Retaliate:case Skill::HeavyStrike:case Skill::Bulwark:return CharacterClass::Warrior;
 default:if(auto recipe=recipe_for(skill))return recipe->character;return CharacterClass::Archer;
 }
}
uint64_t Game::material_skill_mask(){uint64_t mask=0;for(int id=0;id<std::min(skill_count,64);++id)if(!is_fusion(Skill(id)))mask|=uint64_t(1)<<id;return mask;}
bool Game::is_cross_fusion(Skill result){auto recipe=recipe_for(result);return recipe&&recipe->other_class>=0;}
int Game::fusion_count() const {int count=0;for(int i=0;i<skill_count;++i)if(skills[i]&&is_fusion(Skill(i)))++count;return count;}
int Game::cross_fusion_count() const {int count=0;for(const auto& recipe:fusion_recipes())if(recipe.other_class>=0&&skills[int(recipe.result)]>0)++count;return count;}
const FusionScroll* Game::held_scroll(Skill result) const {for(const auto& scroll:scrolls.held)if(scroll.result==result)return &scroll;return nullptr;}
bool Game::can_learn_material(Skill skill) const {
 const int id=int(skill);return id>=0&&id<skill_count&&!is_fusion(skill)&&skill_allowed(skill);
}
bool Game::scroll_reachable(Skill result) const {
 const auto* recipe=recipe_for(result);if(!recipe||!recipe->released||skills[int(result)]>0)return false;
 bool has_form=false;for(int form=0;form<form_count(result);++form)has_form|=form_allowed(result,form);
 return has_form&&can_learn_material(recipe->first)&&can_learn_material(recipe->second);
}
std::string Game::scroll_conflicts(Skill result) const {
 auto recipe=recipe_for(result);if(!recipe)return {};std::string text;
 for(const auto& scroll:scrolls.held){if(scroll.result==result)continue;auto other=recipe_for(scroll.result);if(other&&(recipe->first==other->first||recipe->first==other->second||recipe->second==other->first||recipe->second==other->second)){if(!text.empty())text+="、";text+=skill_name(scroll.result);}}
 return text.empty()?"":std::string("共用材料的路线：")+text+"。融合后可重新学习材料，路线仍然保留。";
}
std::string Game::scroll_status(uint32_t id) const {
 auto found=std::find_if(scrolls.held.begin(),scrolls.held.end(),[&](const FusionScroll& scroll){return scroll.id==id;});if(found==scrolls.held.end())return "尚未获得这份配方知识。";
 auto recipe=recipe_for(found->result);if(!recipe)return "未知配方，不能覆盖或替换。";
 std::string text;if(skill_sealed(recipe->first)||skill_sealed(recipe->second))text="遗言将转移到融合结果，剩余封印 "+std::to_string(int(std::ceil(pressure_event.remaining/encounter_pace())))+" 秒，融合不会解除。\n";if(!scroll_reachable(found->result))text="该融合结果已装备或配方尚不可用。\n";
 for(Skill material:{recipe->first,recipe->second})text+=std::string(skill_name(material))+" Lv."+std::to_string(skills[int(material)])+" / "+std::to_string(recipe->material_level)+(skills[int(material)]==0?"（尚未学习）":"")+"\n";
 text+=(found->study_used||studied(scrolls,found->result))?"本配方研习已使用；切换追踪不会重置。\n":"追踪后可在升级中研习一次缺少的材料，学习为Lv.1。\n";
 text+="超过材料门槛的投入每3级使初始等级+1，已有进化另+1，最高Lv.3。\n";
 text+="融合初始 Lv."+std::to_string(fusion_result_level(found->result))+" · "+scroll_form_name(found->result,found->form)+"\n"+scroll_form_description(found->result,found->form)+"\n"+scroll_conflicts(found->result);
 return text;
}
bool Game::generate_scroll_offer(){
 if(!scrolls.offers.empty())return false;
 std::vector<Skill> pool,related;for(const auto& recipe:fusion_recipes())if(scroll_reachable(recipe.result)&&!held_scroll(recipe.result)){pool.push_back(recipe.result);if(skills[int(recipe.first)]>0||skills[int(recipe.second)]>0)related.push_back(recipe.result);}
 if(pool.empty())return false;
 uint32_t theme=1;switch(BossProfile(encounter_plan.boss)){
 case BossProfile::ForestGuardian:case BossProfile::Faust:case BossProfile::Snowman:theme=2;break;
 case BossProfile::Balrog:theme=4;break;
 case BossProfile::Leviathan:theme=8;break;
 case BossProfile::Pirate:theme=16;break;
 default:break;
 }
 auto weight=[&](Skill result){const auto* recipe=recipe_for(result);return uint32_t(std::clamp(recipe->weight,1,10000))*((recipe->theme_tags&theme)?2u:1u);};
 auto weighted_choice=[&](const std::vector<Skill>& candidates){uint32_t total=0;for(Skill result:candidates)total+=weight(result);uint32_t draw=rng_()%total;for(Skill result:candidates){const uint32_t amount=weight(result);if(draw<amount)return result;draw-=amount;}return candidates.back();};
 auto rolled_offer=[&](Skill result){std::vector<int> forms;for(int form=0;form<form_count(result);++form)if(form_allowed(result,form))forms.push_back(form);return ScrollOffer{result,forms[forms.size()==1?0:rng_()%forms.size()]};};
 // Choose and persist each form once here. Display, selection and save/load do not reroll it.
 Skill first=weighted_choice(related.empty()?pool:related);scrolls.offers.push_back(rolled_offer(first));pool.erase(std::remove(pool.begin(),pool.end(),first),pool.end());
 while(scrolls.offers.size()<3&&!pool.empty()){Skill result=weighted_choice(pool);scrolls.offers.push_back(rolled_offer(result));pool.erase(std::remove(pool.begin(),pool.end(),result),pool.end());}
 // Related is guaranteed, but its display slot does not advertise a fixed recommendation.
 std::shuffle(scrolls.offers.begin(),scrolls.offers.end(),rng_);++scrolls.opportunities;if(scrolls.first_opportunity_time<0)scrolls.first_opportunity_time=total_elapsed();return true;
}
void Game::roll_scroll_reward(bool guaranteed){
 ++scrolls.eligible_rewards;
 if(!guaranteed&&scrolls.failed_rewards<3&&rng_()%100>=40){++scrolls.failed_rewards;return;}
 scrolls.failed_rewards=0;++scrolls.pending_opportunities;
 if(scrolls.offers.empty()){--scrolls.pending_opportunities;if(!generate_scroll_offer())coins+=8;}
}
bool Game::choose_scroll(size_t index){
 if(state!=State::ScrollChoice||index>=scrolls.offers.size())return false;
 ScrollOffer offer=scrolls.offers[index];if(!form_allowed(offer.result,offer.form)||!scroll_reachable(offer.result)||held_scroll(offer.result))return false;
 add_event(EventKind::Scroll,player.pos);scrolls.held.push_back({scrolls.next_id++,offer.result,offer.form,studied(scrolls,offer.result)});scrolls.offers.clear();notify(std::string("获得冒险秘卷：")+skill_name(offer.result)+" · "+scroll_form_name(offer.result,offer.form)+"。已收入配方书；可追踪材料，满足条件后融合。");continue_choices();return true;
}
bool Game::decline_scroll_offer(){if(state!=State::ScrollChoice||scrolls.offers.empty())return false;scrolls.offers.clear();coins+=8;notify("已放弃本次秘卷选择，获得8金币；本次机会仍计入保底记录。");continue_choices();return true;}
int Game::tracked_scroll_count() const {return int(std::count_if(scrolls.held.begin(),scrolls.held.end(),[](const FusionScroll& scroll){return scroll.tracked;}));}
bool Game::set_scroll_tracking(uint32_t id,bool tracked){
 if(state!=State::Playing&&state!=State::Paused)return false;
 auto found=std::find_if(scrolls.held.begin(),scrolls.held.end(),[&](const FusionScroll& scroll){return scroll.id==id;});
 if(found==scrolls.held.end()||(tracked&&!found->tracked&&tracked_scroll_count()>=2))return false;
 found->tracked=tracked;return true;
}
// Compatibility for earlier callers: removing a route now only stops tracking it.
bool Game::discard_scroll(uint32_t id){return set_scroll_tracking(id,false);}
bool Game::confirm_scroll_fusion(uint32_t id){
 if(state!=State::Playing&&state!=State::Paused)return false;
 auto found=std::find_if(scrolls.held.begin(),scrolls.held.end(),[&](const FusionScroll& scroll){return scroll.id==id;});if(found==scrolls.held.end())return false;
 return apply_fusion(found->result);
}
void Game::remove_skill_runtime(Skill source,bool keep_projectiles){
 skill_visuals.erase(std::remove_if(skill_visuals.begin(),skill_visuals.end(),[&](const SkillVisual& v){return v.source==source;}),skill_visuals.end());
 for(const auto& e:effects)if(e.source==source&&battle_actor(e))emit_skill_visual(FxStage::End,source,e.pos,e.pos+e.visual_aim,30);
 if(!keep_projectiles)projectiles.erase(std::remove_if(projectiles.begin(),projectiles.end(),[&](const Projectile& p){return p.skill==source;}),projectiles.end());
 effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect& e){return !e.hostile&&e.source==source;}),effects.end());
 for(auto& enemy:enemies)if(enemy.burn_source==source){enemy.burn_remaining=enemy.burn_tick=enemy.burn_damage=0;enemy.burn_source=Skill::Count;}
 for(auto& enemy:enemies){
  if(enemy.poison_source==source){enemy.poison_remaining=enemy.poison_tick=enemy.poison_damage=0;enemy.poison_layers=0;enemy.poison_source=Skill::Count;enemy.poison_spread=false;}
  if(enemy.mark_source==source||(source==Skill::HunterMark&&enemy.mark_source==Skill::Count)){enemy.mark_remaining=0;enemy.mark_source=Skill::Count;}
  if(enemy.frost_source==source){enemy.frost_remaining=enemy.slow_remaining=0;enemy.frost_layers=0;enemy.frost_source=Skill::Count;}
  if(enemy.armor_source==source){enemy.armor_break_remaining=0;enemy.armor_source=Skill::Count;}
  if(enemy.lure_source==source){enemy.lure_remaining=0;enemy.lure_source=Skill::Count;}
  if(enemy.thermal_source==source){enemy.thermal_side=0;enemy.thermal_lock=0;enemy.thermal_source=Skill::Count;}
 }
 if(source==Skill::BattleFocus||source==Skill::BattleBlades||source==Skill::SpiritJudgement){battle_focus=0;focus_hit_cooldown_=0;}
 if(fusion_dash_source_==source){fusion_dash_active_=false;fusion_dash_source_=Skill::Count;fusion_dash_hits_.clear();}
 cooldown_[int(source)]=cooldown_period_[int(source)]=0;announced_evolutions_[int(source)]=false;
 if(source==Skill::Bulwark||source==Skill::FrostArmor)guard_charge=0;if(source==Skill::DragonArmor)dragon_soul=0;if(source==Skill::Retaliate)retaliation_pending_=false;
}
bool Game::apply_scroll_study(uint32_t id,Skill learn,Skill replace){
 auto found=std::find_if(scrolls.held.begin(),scrolls.held.end(),[&](const FusionScroll& scroll){return scroll.id==id;});if(found==scrolls.held.end()||found->study_used||studied(scrolls,found->result)||!scroll_reachable(found->result))return false;
 auto recipe=recipe_for(found->result);if((learn!=recipe->first&&learn!=recipe->second)||!can_learn_material(learn)||skills[int(learn)]>0)return false;
 const bool full=active_skill_count()>=active_skill_limit;
 if(full){if(!apply_skill_replacement(learn,replace))return false;}
 else{if(replace!=Skill::Count)return false;skills[int(learn)]=1;}

 found->study_used=true;scrolls.studied_results.push_back(found->result);++scrolls.studies_used;cooldown_[int(learn)]=.15f;notify(std::string("研习完成：")+skill_name(learn)+" Lv.1。已使用本次升级选择。");return true;
}
bool Game::select_study_replacement(Skill skill){
 if(state!=State::Upgrade||!pending_study.active)return false;const int id=int(skill);if(id<0||id>=skill_count||skills[id]<=0||skill==pending_study.learn)return false;
 pending_study.replace=skill;return true;
}
bool Game::confirm_study_replacement(){
 if(state!=State::Upgrade||!pending_study.active||pending_study.replace==Skill::Count||pending_study.choice_index>=choices.size())return false;
 const auto pending=pending_study;const auto& choice=choices[pending.choice_index];if(!upgrade_choice_available(choice)||choice.skill!=pending.learn)return false;
 if(choice.kind==UpgradeKind::Study){if(!apply_scroll_study(choice.scroll_id,choice.skill,pending.replace))return false;}
 else if(choice.kind==UpgradeKind::Skill){if(!apply_skill_replacement(choice.skill,pending.replace))return false;}
 else return false;
 pending_study={};refresh_stats();complete_upgrade_choice(pending.choice_index);return true;
}
void Game::cancel_study_replacement(){pending_study={};}
bool Game::validate_scroll_state() const {
 if(scrolls.held.size()>fusion_recipes().size()||tracked_scroll_count()>2||scrolls.offers.size()>3||scrolls.foreign_class<-1||scrolls.foreign_class>2||scrolls.next_id==0||scrolls.failed_rewards<0||scrolls.failed_rewards>3||scrolls.eligible_rewards<0||scrolls.opportunities<0||scrolls.opportunities>scrolls.eligible_rewards||scrolls.studies_used<0||scrolls.replacements<0||scrolls.pending_opportunities<0||!std::isfinite(scrolls.first_opportunity_time)||scrolls.first_opportunity_time<-1)return false;
 std::set<int> held_results,studied_results,completed;std::set<uint32_t> ids;
 for(const auto& scroll:scrolls.held){if(!form_allowed(scroll.result,scroll.form)||scroll.id==0||scroll.id>=scrolls.next_id||!ids.insert(scroll.id).second||!held_results.insert(int(scroll.result)).second||scroll.study_used!=studied(scrolls,scroll.result))return false;}
 for(Skill result:scrolls.studied_results)if(!recipe_for(result)||!studied_results.insert(int(result)).second)return false;
 for(const auto& entry:scrolls.completed)if(!form_allowed(entry.result,entry.form)||!completed.insert(int(entry.result)).second)return false;
 for(const auto& offer:scrolls.offers)if(!form_allowed(offer.result,offer.form))return false;
 return true;
}
}

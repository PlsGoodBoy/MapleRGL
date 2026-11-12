#include "game.hpp"
#include <algorithm>
#include <sstream>

namespace maple {
namespace {
bool skill_card(const UpgradeChoice& choice){return choice.kind==UpgradeKind::Skill||choice.kind==UpgradeKind::Study||choice.kind==UpgradeKind::Fusion;}
bool same_card(const UpgradeChoice& a,const UpgradeChoice& b){
 if(skill_card(a)&&skill_card(b))return a.skill==b.skill;
 if(a.kind==UpgradeKind::Resonance&&b.kind==UpgradeKind::Resonance)return a.resonance==b.resonance;
 return a.kind==b.kind&&(a.kind!=UpgradeKind::Relic||a.relic==b.relic);
}
}
std::vector<UpgradeChoice> Game::available_upgrade_choices() const {
 std::vector<UpgradeChoice> pool;
 for(int id=0;id<skill_count;++id)if(can_gain_skill(Skill(id))){
  UpgradeChoice c{UpgradeKind::Skill,Skill(id),std::string(skill_name(Skill(id)))+(skills[id]?" +1":" 学习"),skill_upgrade_description(Skill(id))};c.expected_rank=skills[id];const auto preview=fusion_preview(Skill(id));if(!preview.empty())c.description+="\n"+preview;pool.push_back(c);
 }
 if(!bonus_selection_)for(const auto& upgrade:resonance_upgrades())if(!resonance_directions[int(upgrade.id)]&&resonance_status(upgrade.id).usable){UpgradeChoice c;c.kind=UpgradeKind::Resonance;c.skill=Skill::Count;c.resonance=upgrade.id;c.direction=upgrade.direction;c.title=std::string(resonance_definitions()[int(upgrade.id)].name)+" · "+upgrade.name;c.description=std::string(upgrade.description)+" 同一共鸣仅能选择一个方向。失去参与技能时暂时停用，重新满足条件后恢复。";pool.push_back(c);}
 static const char* increments[]={"","伤害 +15%","攻速 +12%","范围 +12%","投射物 +1","移速 +8%","生命 +20，恢复40","拾取距离 +22","冷却速度 +8%","冲刺、护盾、控制、持续领域时长 +18%","暴击率 +3.5%","暴击倍率 +18%"};
 for(int k=1;k<passive_count;++k)if(passives[k]<8){UpgradeChoice c{UpgradeKind(k),Skill::Count,passive_name(UpgradeKind(k)),std::string(increments[k])+"；当前 "+std::to_string(passives[k])+"/8。"};c.expected_rank=passives[k];pool.push_back(c);}
 if(pool.size()<5&&overcharge_levels<40)pool.push_back({UpgradeKind::Overcharge,Skill::Count,"强攻补给 +3.5%","增加本局基础伤害；最多40次。"});
 if(pool.size()<5&&vitality_levels<40)pool.push_back({UpgradeKind::Vitality,Skill::Count,"韧性补给 +8","增加8生命上限并恢复20生命；最多40次。"});
 pool.push_back({UpgradeKind::Recover,Skill::Count,"应急补给","恢复40生命并获得3秒护盾。"});
 return pool;
}
bool Game::upgrade_choice_available(const UpgradeChoice& c) const {
 if(c.selected)return false;
 if(c.kind==UpgradeKind::Resonance)return !resonance_directions[int(c.resonance)]&&resonance_status(c.resonance).usable;
 const int k=int(c.kind),id=int(c.skill);
 if(c.kind==UpgradeKind::Skill)return can_gain_skill(c.skill)&&(c.expected_rank<0||skills[id]==c.expected_rank);
 if(c.kind==UpgradeKind::Fusion)return fusion_ready(c.skill);
 if(c.kind==UpgradeKind::Study){
  const auto it=std::find_if(scrolls.held.begin(),scrolls.held.end(),[&](const FusionScroll& scroll){return scroll.id==c.scroll_id;});
  if(it==scrolls.held.end()||!it->tracked||it->study_used||!scroll_reachable(it->result)||!can_learn_material(c.skill)||skills[id]>0)return false;
  for(const auto& recipe:fusion_recipes())if(recipe.result==it->result)return c.skill==recipe.first||c.skill==recipe.second;
  return false;
 }
 if(c.kind==UpgradeKind::Relic){const int r=int(c.relic);return relic_selection_&&r>=0&&r<relic_count&&relic_allowed(c.relic,character)&&(unlocked_relic_mask_&(1u<<r))&&!owns_relic(c.relic);}
 if(k>0&&k<passive_count)return passives[k]<8&&(c.expected_rank<0||passives[k]==c.expected_rank);
 if(c.kind==UpgradeKind::Overcharge)return overcharge_levels<40;
 if(c.kind==UpgradeKind::Vitality)return vitality_levels<40;
 return c.kind==UpgradeKind::Recover;
}
int Game::upgrade_weight(const UpgradeChoice& c) const {
 if(c.kind==UpgradeKind::Skill){if(skills[int(c.skill)])return 5;for(const auto& r:fusion_recipes())if(r.released){const std::array<Skill,4> inputs={r.first,r.second,r.third,r.fourth};if(std::find(inputs.begin(),inputs.end(),c.skill)!=inputs.end()&&std::any_of(inputs.begin(),inputs.end(),[&](Skill s){return s!=Skill::Count&&s!=c.skill&&skills[int(s)]>0;}))return 4;}}
 if(c.kind==UpgradeKind::Resonance)return 3;
 if(c.kind==UpgradeKind::Fusion)return 3;
 if(c.kind==UpgradeKind::Study)return 2;

 return 1;
}
std::string Game::upgrade_pool_diagnostics() const {
 std::ostringstream out;out<<"skill_id\teligible\tweight\treason\n";
 for(int i=0;i<skill_count;++i){const Skill skill=Skill(i);const bool eligible=can_gain_skill(skill);
  const char* reason=eligible?(skills[i]?"owned_upgrade":active_skill_count()>=active_skill_limit?"new_skill_replace":"new_skill"):
   skills[i]>=skill_level_cap(skill)?"maximum_rank":!skill_allowed(skill)?"not_released_or_unavailable":is_fusion(skill)&&!skills[i]?"automatic_fusion_only":"material_unavailable";
  UpgradeChoice card;card.kind=UpgradeKind::Skill;card.skill=skill;
  out<<skill_key(skill)<<'\t'<<eligible<<'\t'<<(eligible?upgrade_weight(card):0)<<'\t'<<reason<<'\n';
 }
 out<<"Selection: owned upgrade, new skill, growth, second owned upgrade when available, then weighted free slots; weights are relative within each eligible category.\n";
 return out.str();
}
void Game::generate_choices(const std::vector<UpgradeChoice>* previous){
 choices.clear();size_t selected_slot=0;
 if(previous){for(size_t i=0;i<previous->size();++i)if((*previous)[i].selected){selected_slot=i;choices.push_back((*previous)[i]);}}
 else{upgrade_picks_taken=0;upgrade_resets_remaining=bonus_selection_?0:1;}
 auto pool=available_upgrade_choices();
 if(previous){
  auto fresh=pool;fresh.erase(std::remove_if(fresh.begin(),fresh.end(),[&](const UpgradeChoice& c){return std::any_of(previous->begin(),previous->end(),[&](const UpgradeChoice& old){return same_card(c,old);});}),fresh.end());
  if(fresh.size()>=5-choices.size())pool=std::move(fresh);
 }
 auto take=[&](auto predicate){
  std::vector<size_t> eligible;int total=0;
  for(size_t i=0;i<pool.size();++i)if(predicate(pool[i])&&(pool[i].kind!=UpgradeKind::Resonance||std::none_of(choices.begin(),choices.end(),[](const UpgradeChoice& c){return c.kind==UpgradeKind::Resonance;}))&&std::none_of(choices.begin(),choices.end(),[&](const UpgradeChoice& selected){return same_card(pool[i],selected);})){eligible.push_back(i);total+=upgrade_weight(pool[i]);}
  if(eligible.empty())return;
  int draw=int(rng_()%total);size_t chosen=eligible.back();for(auto i:eligible){draw-=upgrade_weight(pool[i]);if(draw<0){chosen=i;break;}}
  choices.push_back(pool[chosen]);
 };
 auto owned=[&](const UpgradeChoice& c){return c.kind==UpgradeKind::Skill&&skills[int(c.skill)]>0;};
 auto fresh=[&](const UpgradeChoice& c){return (c.kind==UpgradeKind::Skill||c.kind==UpgradeKind::Study)&&skills[int(c.skill)]==0;};
 auto passive=[](const UpgradeChoice& c){return !skill_card(c)&&c.kind!=UpgradeKind::Resonance;};
 for(int category=0;category<3;++category){auto match=[&](const UpgradeChoice& c){return category==0?owned(c):category==1?fresh(c):passive(c);};if(std::none_of(choices.begin(),choices.end(),match))take(match);}
 if(std::count_if(choices.begin(),choices.end(),owned)<2)take(owned);
 while(choices.size()<5){const auto count=choices.size();take([](const UpgradeChoice&){return true;});if(choices.size()==count)break;}
 // Only the first selection can be retained in an unfinished five-pick-two round.
 if(previous&&upgrade_picks_taken==1)std::swap(choices.front(),choices[selected_slot]);
 state=State::Upgrade;if(!previous)add_event(EventKind::LevelUp,player.pos);
}
void Game::refresh_upgrade_choices(){
 const auto pool=available_upgrade_choices();
 for(size_t slot=0;slot<choices.size();++slot){if(choices[slot].selected||upgrade_choice_available(choices[slot]))continue;
  std::vector<size_t> eligible;int total=0;
  for(size_t i=0;i<pool.size();++i){bool duplicate=false;for(size_t j=0;j<choices.size();++j)if(j!=slot&&same_card(pool[i],choices[j]))duplicate=true;if(pool[i].kind==UpgradeKind::Resonance)for(size_t j=0;j<choices.size();++j)if(j!=slot&&choices[j].kind==UpgradeKind::Resonance)duplicate=true;if(!duplicate){eligible.push_back(i);total+=upgrade_weight(pool[i]);}}
  if(eligible.empty())continue;
  int draw=int(rng_()%total);size_t replacement=eligible.back();for(auto i:eligible){draw-=upgrade_weight(pool[i]);if(draw<0){replacement=i;break;}}choices[slot]=pool[replacement];
 }
}
bool Game::reroll_upgrade(){
 if(state!=State::Upgrade||pending_study.active||relic_selection_||bonus_selection_||upgrade_resets_remaining+upgrade_rerolls_remaining<=0||choices.empty())return false;
 const auto old=choices;generate_choices(&old);
 if(upgrade_resets_remaining>0)--upgrade_resets_remaining;
 else{--upgrade_rerolls_remaining;++metrics.upgrade_rerolls_used;}
 notify(upgrade_picks_taken?"未选选项已重置，已选效果保留。":"本次升级的五个选项已重置。");return true;
}
bool Game::skip_upgrade(){
 if(state!=State::Upgrade||pending_study.active||relic_selection_||bonus_selection_)return false;
 const bool selected=upgrade_picks_taken>0;finish_upgrade_round();
 notify(selected?"已跳过剩余选择，已选效果保留。":"已跳过本次升级的全部选择，等级保留。");return true;
}
void Game::complete_upgrade_choice(size_t index){
 choices[index].selected=true;++upgrade_picks_taken;add_event(EventKind::Choice,player.pos);
 if(!relic_selection_&&!bonus_selection_)++metrics.upgrade_choices;else ++metrics.bonus_choices;
 auto_fuse_skills();
 if(upgrade_picks_taken<upgrade_picks_required()){refresh_upgrade_choices();return;}
 finish_upgrade_round();
}
void Game::finish_upgrade_round(){
 choices.clear();upgrade_resets_remaining=0;
 if(relic_selection_){pending_relic_rewards_=std::max(0,pending_relic_rewards_-1);relic_selection_=false;}
 else if(bonus_selection_){pending_skill_rewards_=std::max(0,pending_skill_rewards_-1);bonus_selection_=false;}
 else pending_levels_=std::max(0,pending_levels_-1);
 continue_choices();
}
bool Game::choose_upgrade(size_t index){
 if(state!=State::Upgrade||pending_study.active||index>=choices.size()||!upgrade_choice_available(choices[index]))return false;
 const auto c=choices[index];const int k=int(c.kind);
 if(c.kind==UpgradeKind::Relic){owned_relic_mask_|=1u<<int(c.relic);++metrics.relics_collected;notify(std::string("获得遗物：")+relic_definitions()[int(c.relic)].name);}
 else if(c.kind==UpgradeKind::Resonance){resonance_directions[int(c.resonance)]=c.direction;notify("共鸣方向已获得："+c.title);}
 else if(c.kind==UpgradeKind::Fusion){if(!apply_fusion(c.skill))return false;}
 else if((c.kind==UpgradeKind::Study||c.kind==UpgradeKind::Skill)&&skills[int(c.skill)]==0&&active_skill_count()>=active_skill_limit){pending_study={true,index,c.scroll_id,c.skill,Skill::Count};return true;}
 else if(c.kind==UpgradeKind::Study){if(!apply_scroll_study(c.scroll_id,c.skill))return false;}
 else if(c.kind==UpgradeKind::Skill){++skills[int(c.skill)];}
 else if(k>0&&k<passive_count)++passives[k];
 else if(c.kind==UpgradeKind::Overcharge)++overcharge_levels;
 else if(c.kind==UpgradeKind::Vitality)++vitality_levels;
 else if(c.kind==UpgradeKind::Recover){shield_remaining=std::max(shield_remaining,3.f);heal_player(40);}
 refresh_stats();if(c.kind==UpgradeKind::Health)heal_player(40);if(c.kind==UpgradeKind::Vitality)heal_player(20);
 for(int s=0;s<skill_count;++s)if(evolved(Skill(s))&&!announced_evolutions_[s]){announced_evolutions_[s]=true;notify(std::string("技能进化：")+evolution_name(Skill(s)));}
 complete_upgrade_choice(index);return true;
}
bool Game::apply_skill_replacement(Skill learn,Skill replace){
 const int old=int(replace),next=int(learn);
 if(!can_gain_skill(learn)||skills[next]>0||old<0||old>=skill_count||skills[old]<=0||old==next)return false;
 if(skill_sealed(replace))pressure_event.sealed=learn;
 cast_failed_[next]=cast_failed_[next]||cast_failed_[old];skills[old]=0;remove_skill_runtime(replace,true);skills[next]=1;
 cooldown_[next]=.15f;cooldown_period_[next]=skill_interval(learn);++scrolls.replacements;
 notify(std::string("已替换：")+skill_name(replace)+" → "+skill_name(learn)+" Lv.1。");return true;
}
} // namespace maple

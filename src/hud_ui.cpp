#include "app.hpp"
#include "adventure_content.hpp"
#include "boss_combat.hpp"
#include "build_info.hpp"
#include "ui_theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace maple {
namespace {
constexpr ImU32 ink=ui::panel_color,border=ui::border,text=ui::text,muted=ui::muted,gold=ui::warm,mint=ui::ready,red=ui::danger,fusion_color=ui::fusion;
float ui_scale(){return std::clamp(ImGui::GetFont()->FontSize/18.f,1.f,1.25f)*ImGui::GetIO().FontGlobalScale;}
void panel(ImDrawList* draw,ImVec2 at,ImVec2 size){ui::panel(draw,at,size);}
void label(ImDrawList* draw,ImVec2 at,ImU32 color,const std::string& value,float size=0){draw->AddText(ImGui::GetFont(),size>0?size:ImGui::GetFontSize(),at,color,value.c_str());}
void centered(ImDrawList* draw,ImVec2 at,ImU32 color,const std::string& value,float size){const float width=ImGui::GetFont()->CalcTextSizeA(size,10000,0,value.c_str()).x;label(draw,{at.x-width*.5f,at.y},color,value,size);}
std::string brief_description(const char* value){std::string result=value;auto end=result.find("。");if(end!=std::string::npos)result.resize(end+3);return result;}
std::string seconds(float value){char out[32];snprintf(out,sizeof(out),value<10?"%.1fs":"%.0fs",std::max(0.f,value));return out;}
std::string relic_icon(RelicId id){static const int keys[]={8,4,2,0,0,5,1,3,11,9,6,10};return "relic.icon."+std::to_string(keys[std::clamp(int(id),0,relic_count-1)]);}
std::string equipment_icon(EquipmentId id){static const int keys[]={0,0,3,8,1,2,0,6,3,7};return id==EquipmentId::MagnetPendant?"relic.icon.7":"equipment.icon."+std::to_string(keys[std::clamp(int(id),0,equipment_count-1)]);}
bool hover(ImVec2 at,ImVec2 size,State state){return state==State::Playing&&ImGui::IsMouseHoveringRect(at,{at.x+size.x,at.y+size.y},false);}
void tooltip(const std::string& title,const std::string& body){ImGui::BeginTooltip();ImGui::PushTextWrapPos(std::min(390.f,ImGui::GetIO().DisplaySize.x-48));ImGui::TextColored({.97f,.80f,.44f,1},"%s",title.c_str());ImGui::TextWrapped("%s",body.c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();}
bool recipe_for_class(const FusionRecipe& recipe,CharacterClass character){return recipe.released;}
std::string clock_time(double value){const auto total=static_cast<unsigned long long>(std::max(0.0,value));char out[48];snprintf(out,sizeof(out),"%02llu:%02llu",total/60,total%60);return out;}
void timing_view(const RunTiming& timing,float game_time){
 ImGui::Text("标准关卡进度 %s · 关卡节奏 %d× · 规则v%d",clock_time(game_time).c_str(),timing.speed,timing.rules_version);
 if(timing.combat_seconds>0)ImGui::Text("有效战斗时间 %s",clock_time(timing.combat_seconds).c_str());
 if(!timing.recorded){ImGui::TextDisabled("现实用时 / 使用倍率：未记录");return;}
 const char* speeds=timing.used_speeds==3?"混合 1× / 2×":timing.used_speeds==2?"2×":timing.used_speeds==1?"1×":"尚未进入战斗";
 ImGui::TextWrapped("有效战斗现实时间 %s · 总现实时间 %s",clock_time(timing.effective_real_seconds).c_str(),clock_time(timing.total_real_seconds).c_str());
 ImGui::TextDisabled("使用倍率：%s · 总时间包含选择与暂停",speeds);
}
bool dash_skill(Skill skill){return skill==Skill::Charge||skill==Skill::DashBreaker||skill==Skill::MountainBreaker||skill==Skill::PhoenixAssault;}
float frost_armor_energy(const Game& game){for(const auto& effect:game.effects)if(effect.kind==EffectKind::FrostArmor&&effect.pattern==0)return effect.energy;return 0;}
const PressureDefinition* pressure_definition(PressureKind kind){for(const auto& definition:pressure_definitions())if(definition.kind==kind)return &definition;return nullptr;}
std::string pressure_detail(const Game& game){
 char value[160];switch(game.pressure_event.kind){
 case PressureKind::Exhaustion:snprintf(value,sizeof(value),"普通移速 ×%.2f · 冲刺位移保留",game.config.exhaustion_factor);return value;
 case PressureKind::ThinAir:snprintf(value,sizeof(value),"技能恢复耗时 ×%.2f",game.cast_recovery_factor());return value;
 case PressureKind::Distraction:snprintf(value,sizeof(value),"合法施放失败率 %.0f%% · 单次失败后保护",game.config.distraction_chance*100);return value;
 case PressureKind::Shackles:snprintf(value,sizeof(value),"冲刺恢复耗时 ×%.2f · 位移保留",game.dash_recovery_factor());return value;
 case PressureKind::MapleDisorder:snprintf(value,sizeof(value),"常规吸取范围 ×%.2f",game.config.maple_disorder_factor);return value;
 case PressureKind::LastWords:return std::string("封印：")+Game::skill_name(game.pressure_event.sealed)+" · 冷却暂停";
 case PressureKind::Disarm:return "装备与正面 Buff 暂停贡献，原计时继续";
 default:if(const auto* definition=pressure_definition(game.pressure_event.kind))return definition->hint;return {};
 }
}
}

// Inventory/skill icons are drawn as full small bitmaps, independently of world-sprite origins.
void App::ui_icon(ImDrawList* draw,const std::string& key,ImVec2 at,float edge,ImU32 tint){
 auto found=sprites_.find(key);
 if(found==sprites_.end()&&key.rfind("skill.icon.",0)==0)for(int id=90;id<skill_count;++id){auto def=adventure_skill(Skill(id));if(key=="skill.icon."+std::string(def->key)){found=sprites_.find("skill.icon."+std::string(def->icon));break;}}
 if(found==sprites_.end()||found->second.empty()){draw->AddRect(at,{at.x+edge,at.y+edge},red,3);centered(draw,{at.x+edge*.5f,at.y+edge*.2f},red,"?",edge*.5f);return;}
 const auto& frame=found->second.front();auto& image=texture(frame.file);if(!image.view)return;
 const float scale=std::min(edge/std::max(1,image.w),edge/std::max(1,image.h));
 const ImVec2 origin{at.x+(edge-image.w*scale)*.5f,at.y+(edge-image.h*scale)*.5f};
 draw->AddImage((ImTextureID)(intptr_t)image.view,origin,{origin.x+image.w*scale,origin.y+image.h*scale},{0,0},{1,1},tint);
}

void App::fusion_recipe_view(const FusionRecipe& recipe,bool compact){
 const auto view=game_.fusion_status(recipe.result);const bool complete=view.equipped,ready=view.ready;
 const char* state=view.hint.c_str();
 if(!compact)ImGui::TextColored(complete||ready?ImVec4(.75f,.68f,1.f,1):ImVec4(.63f,.72f,.67f,1),"%s · %s",Game::skill_name(recipe.result),state);
 const auto at=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();const float edge=compact?26.f:30.f;
 const float stride=edge+29;auto parts=fusion_inputs(recipe);parts.push_back(recipe.result);
 for(size_t i=0;i<parts.size();++i){const auto skill=parts[i];const bool result=i+1==parts.size();const int rank=game_.skills[int(skill)];ImVec2 icon_at{at.x+i*stride,at.y};
  ui_icon(draw,"skill.icon."+std::string(Game::skill_key(skill)),icon_at,edge,rank||result?0xffffffff:IM_COL32(255,255,255,105));
  draw->AddRect({icon_at.x-2,icon_at.y-2},{icon_at.x+edge+2,icon_at.y+edge+2},result?fusion_color:rank>=5?mint:border,4);
  if(!result)label(draw,{icon_at.x+edge+9,icon_at.y+5},muted,i+2==parts.size()?"→":"+",14);
  centered(draw,{icon_at.x+edge*.5f,icon_at.y+edge+4},result?fusion_color:muted,result?std::to_string(recipe.sources.size())+"源":std::to_string(rank)+"/5",11);
 }
 ImGui::Dummy({parts.size()*stride,edge+20});
 if(compact)return;
 ImGui::TextWrapped("%s",game_.fusion_requirement(recipe.result).c_str());
 if(Game::form_count(recipe.result)>1&&ImGui::TreeNode("比较融合形态")){
  ImGui::TextWrapped("首次融合随机确定一种形态；本局再次合成沿用该形态。");
  for(int form=0;form<Game::form_count(recipe.result);++form){const bool selected=(complete&&game_.fusion_form(recipe.result)==form);ImGui::TextColored(selected?ImVec4(.75f,.68f,1.f,1):ImVec4(.94f,.93f,.84f,1),"%s%s",selected?"当前 · ":"",Game::scroll_form_name(recipe.result,form));ImGui::TextWrapped("%s",Game::scroll_form_description(recipe.result,form));}
  ImGui::TreePop();
 }
}

void App::run_gear_panel(){
 ImGui::Text("局内装备 %llu 件 · %zu 种效果记录",static_cast<unsigned long long>(game_.gear_count()),game_.run_gear.size());
 ImGui::TextWrapped("拾取即生效，不占技能槽；同定义同等级按数量合并。属性按 √数量 × 有效等级累计。机制装备共享各自冷却，增加份数提升效果质量。出发营地装备另外只应用一次。");
 if(game_.equipment_suppressed())ImGui::TextColored(ui::color(ui::danger),"装备受压制：新增拾取已保留，解除后恢复贡献。");
 if(game_.regression_active())ImGui::TextDisabled("退化中：效果等级暂时 -1，件数不变。");
 static int filter=0;ImGui::SetNextItemWidth(170);ImGui::Combo("筛选",&filter,"全部\0攻击\0防御\0机动成长\0风格\0机制\0");
 std::vector<const GearStack*> entries;for(const auto& stack:game_.run_gear){const auto* d=gear_definition(stack.id);if(d&&(!filter||(filter<=3&&d->group==filter-1)||(filter==4&&d->stat==12)||(filter==5&&d->stat>=20)))entries.push_back(&stack);}
 for(int row=0;row<int(entries.size());++row){const auto& stack=*entries[row];const auto* d=gear_definition(stack.id);
  ImGui::PushID(row);ImGui::TextColored(ui::color(ui::warm),"%s · Lv.%d ×%llu",d->name,stack.level,static_cast<unsigned long long>(stack.count));
  ImGui::TextWrapped("%s",game_.gear_description(stack).c_str());ImGui::Separator();ImGui::PopID();
 }
}

void App::skill_details(Skill skill){
 const auto status=game_.skill_status(skill);const int rank=status.level,effective=status.effective;const bool sealed=status.sealed,smoked=status.smoked;
 ImGui::TextColored(Game::is_fusion(skill)?ImVec4(.75f,.68f,1.f,1):ImVec4(.97f,.80f,.44f,1),"%s · Lv.%d%s",Game::skill_name(skill),rank,Game::is_fusion(skill)?(Game::fusion_tier(skill)==4?" · 四源":Game::fusion_tier(skill)==3?" · 三源":" · 二源"):"");
 if(effective!=rank)ImGui::TextColored({.98f,.57f,.51f,1},"真实 Lv.%d → 当前有效 Lv.%d",rank,effective);
 if(sealed)ImGui::TextColored({.98f,.57f,.51f,1},"遗言封印 · 新释放与新触发禁止，冷却进度暂停");
 if(smoked)ImGui::TextColored({.98f,.57f,.51f,1},"烟幕内 · 新攻击禁止，冷却继续；移动与冲刺可用");
 if(effective==0)ImGui::TextWrapped("有效等级为 0，暂不能攻击；离开退化区域后按最新真实等级恢复。");
 if(game_.skill_failure_feedback(skill)>0)ImGui::TextColored({.97f,.80f,.44f,1},"刚刚释放失败 · 本次正常冷却已消耗");
 ImGui::TextWrapped("%s",Game::skill_description(skill));
 if(effective>0)ImGui::TextWrapped("%s",game_.skill_parameters(skill,effective).c_str());
 if(Game::is_fusion(skill))for(const auto& completed:game_.scrolls.completed)if(completed.result==skill){ImGui::TextColored({.75f,.68f,1.f,1},"形态：%s",Game::scroll_form_name(skill,completed.form));ImGui::TextWrapped("%s",Game::scroll_form_description(skill,completed.form));break;}
 if(skill==Skill::BattleFocus||skill==Skill::BattleBlades||skill==Skill::SpiritJudgement){char resource[64];snprintf(resource,sizeof(resource),"斗气 %.0f / 100",std::clamp(game_.battle_focus,0.f,100.f));ImGui::ProgressBar(std::clamp(game_.battle_focus/100.f,0.f,1.f),{260,18},resource);}
 const float active=game_.skill_active_remaining(skill),remaining=game_.skill_cooldown_remaining(skill);
 if(active>0&&skill!=Skill::FrostArmor)ImGui::Text("当前效果剩余 %.1f 秒",active);
 if(skill==Skill::DragonArmor){char resource[64];snprintf(resource,sizeof(resource),"龙魂 %.0f / 100",std::clamp(game_.dragon_soul,0.f,100.f));ImGui::ProgressBar(std::clamp(game_.dragon_soul/100.f,0.f,1.f),{260,18},resource);ImGui::TextWrapped("龙魂积累至 100 后，可触发自动咆哮。");}
 else if(skill==Skill::Bulwark){ImGui::Text("当前蓄盾：%.1f 点",game_.guard_charge);ImGui::TextWrapped("蓄盾按点数吸收伤害，与限时免伤分开计算。");if(remaining>0)ImGui::Text("下次自动补盾：%.1f 秒",remaining);else ImGui::TextDisabled("补盾就绪");}
 else if(skill==Skill::FrostArmor){char resource[64];snprintf(resource,sizeof(resource),"寒甲积蓄 %.0f / 60",frost_armor_energy(game_));ImGui::ProgressBar(std::clamp(frost_armor_energy(game_)/60.f,0.f,1.f),{260,18},resource);ImGui::Text("当前蓄盾：%.1f 点",game_.guard_charge);ImGui::TextWrapped("合格受击或吸收时积蓄能量，达到 60 后发动寒霜反击；持续生效。");}
 else if(dash_skill(skill)){if(remaining>0)ImGui::Text("冲刺附加攻击冷却：%.1f 秒",remaining);else ImGui::TextColored({.75f,.68f,1.f,1},"就绪 · 等待玩家按空格冲刺");ImGui::TextWrapped("沿你主动冲刺的实际路径发动攻击。");}
 else if(remaining>0)ImGui::Text(sealed?"封印暂停中的冷却：%.1f 秒":"技能恢复剩余：%.1f 秒",remaining);
 else ImGui::TextDisabled("已就绪 · 满足目标或触发条件时自动释放");
 if(rank<Game::skill_level_cap(skill)){ImGui::Separator();ImGui::TextUnformatted("下一等级");ImGui::TextWrapped("%s",game_.skill_upgrade_description(skill).c_str());}
 const char* requirement=Game::evolution_requirement(skill);
 if(!Game::is_fusion(skill)&&requirement&&*requirement){ImGui::Separator();ImGui::TextColored({.60f,.90f,.76f,1},"%s",game_.evolved(skill)?Game::evolution_name(skill):"进化条件");ImGui::TextWrapped("%s",requirement);}
 ImGui::TextDisabled("以上均为正常战斗秒；紧凑2×不加速技能动作与冷却。");
 std::vector<const FusionRecipe*> related;for(const auto& recipe:fusion_recipes())if(recipe_for_class(recipe,game_.character)&&(recipe.first==skill||recipe.second==skill||recipe.result==skill))related.push_back(&recipe);
 std::stable_sort(related.begin(),related.end(),[&](const FusionRecipe* a,const FusionRecipe* b){return (a->result==skill||game_.main_fusion_route(skill)==a->result)>(b->result==skill||game_.main_fusion_route(skill)==b->result);});
 for(size_t i=0;i<std::min(size_t(3),related.size());++i){const auto& recipe=*related[i];ImGui::Separator();ImGui::TextWrapped("%s · %s",Game::skill_name(recipe.result),game_.skills[int(recipe.result)]>0?"已融合":"达标自动融合");fusion_recipe_view(recipe,true);}
 if(related.size()>3)ImGui::TextDisabled("另有 %zu 条路线，可在暂停配方表查看。",related.size()-3);
}

void App::fusion_panel(){
 ImGui::TextWrapped("当前材料真实等级各达到5级即自动融合；冲突优先四源→三源→二源，再按配方优先级、稳定ID裁决。");
 static int tier=0,style=0;static bool current=false;
 ImGui::Checkbox("仅当前构筑相关",&current);ImGui::SameLine();ImGui::SetNextItemWidth(130);ImGui::Combo("层级",&tier,"全部\0二源\0三源\0四源\0");ImGui::SameLine();ImGui::SetNextItemWidth(140);ImGui::Combo("风格",&style,"全部\0战士\0法师\0弓箭\0飞侠\0海盗\0战神\0龙神\0机械\0幻影\0");
 for(int id=0;id<skill_count;++id){const auto status=game_.fusion_status(Skill(id));if(!status.recipe)continue;const auto& recipe=*status.recipe;const auto inputs=fusion_inputs(recipe);
  if(tier&&int(recipe.sources.size())!=tier+1)continue;
  if(current&&std::none_of(inputs.begin(),inputs.end(),[&](Skill s){return game_.skills[int(s)]>0;})&&!game_.skills[id])continue;
  if(style&&std::none_of(recipe.sources.begin(),recipe.sources.end(),[&](Skill s){auto def=adventure_skill(s);int group=def?def->style:Game::skill_owner(s)==CharacterClass::Warrior?0:Game::skill_owner(s)==CharacterClass::Mage?1:2;return group==style-1;}))continue;
  ImGui::PushID(id);ImGui::Separator();ImGui::TextColored(ui::color(ui::fusion),"%s · %d源",Game::skill_name(Skill(id)),int(recipe.sources.size()));fusion_recipe_view(recipe,true);ImGui::TextWrapped("%s",status.hint.c_str());
  if(ImGui::TreeNode("详情 / 合法接续")){ImGui::TextWrapped("%s",game_.fusion_requirement(Skill(id)).c_str());ImGui::TextWrapped("%s",Game::skill_description(Skill(id)));for(const auto& route:fusion_recipes())if(route.result==Skill(id)){std::string text;for(Skill s:fusion_inputs(route)){if(!text.empty())text+=" + ";text+=Game::skill_name(s);}ImGui::TextWrapped("%s → %s",text.c_str(),Game::skill_name(Skill(id)));}ImGui::TreePop();}ImGui::PopID();
 }

}

void App::study_replacement_menu(){
 const auto pending=game_.pending_study;
 ImGui::TextColored(ui::color(ui::accent),"八槽已满 · 替换预览");
 ImGui::TextWrapped("学习 %s Lv.1。选择旧技能查看变化，确认后才使用一次升级选择。",Game::skill_name(pending.learn));
 const float height=ImGui::GetContentRegionAvail().y-92;const float half=(ImGui::GetContentRegionAvail().x-16)*.5f;
 ImGui::BeginChild("旧技能列表",{half,height},ImGuiChildFlags_Borders);
 for(Skill skill:game_.skill_slots())if(skill!=Skill::Count){ImGui::PushID(int(skill));
  const auto at=ImGui::GetCursorScreenPos();ui_icon(ImGui::GetWindowDrawList(),"skill.icon."+std::string(Game::skill_key(skill)),at,28);ImGui::Dummy({32,32});ImGui::SameLine();
  const std::string caption=std::string(Game::skill_name(skill))+" · Lv."+std::to_string(game_.skills[int(skill)])+(Game::is_fusion(skill)?(Game::fusion_tier(skill)==4?" · 四源":Game::fusion_tier(skill)==3?" · 三源":" · 二源"):"");
  if(ImGui::Selectable(caption.c_str(),game_.pending_study.replace==skill,0,{0,32}))game_.select_study_replacement(skill);if(skill==Skill::Homing)review_ui_flow_item(1);ImGui::PopID();
 }ImGui::EndChild();ImGui::SameLine(0,16);
 ImGui::BeginChild("构筑变化",{0,height},ImGuiChildFlags_Borders);
 const Skill replace=game_.pending_study.replace;
 if(replace!=Skill::Count){
  ImGui::TextColored(ui::color(ui::danger),"失去 %s Lv.%d",Game::skill_name(replace),game_.skills[int(replace)]);
  ImGui::TextDisabled("旧等级与投入不返还。");ImGui::Separator();
  ImGui::TextColored(ui::color(ui::ready),"获得 %s Lv.1",Game::skill_name(pending.learn));
  ImGui::TextWrapped("%s",game_.resonance_change_preview(replace,pending.learn).c_str());ImGui::TextWrapped("%s",game_.fusion_preview(pending.learn,replace).c_str());ImGui::Separator();
  const auto route=game_.fusion_status(game_.main_fusion_route(replace));
  if(route.recipe&&!route.equipped)ImGui::TextWrapped("融合路线：%s 需要重新学习 %s。",Game::skill_name(route.recipe->result),Game::skill_name(replace));
  if(game_.skill_sealed(replace))ImGui::TextWrapped("遗言转移到新技能，仍持续 %.1f 战斗秒。",game_.pressure_event.remaining/game_.encounter_pace());
  ImGui::TextDisabled("共鸣不会消耗技能。此处只替换选中的旧技能。");
 }else ImGui::TextWrapped("从左侧选择一项，查看共鸣与融合路线的变化。");
 ImGui::EndChild();ImGui::Separator();
 ImGui::BeginDisabled(replace==Skill::Count||upgrade_action_delay_>0);bool confirm=ui::primary_button("确认替换 · 使用一次选择",{-1,36});review_ui_flow_item(2);ImGui::EndDisabled();
 if(confirm&&game_.confirm_study_replacement()){++upgrade_selections_;upgrade_action_delay_=.18f;return;}
 if(ImGui::Button("取消 · Esc · 保留原候选与选择次数",{-1,32})){game_.cancel_study_replacement();upgrade_action_delay_=.18f;}
}

void App::upgrade_menu(){
 const bool relic=game_.choosing_relic();const int required_picks=game_.upgrade_picks_required();const auto size=ImGui::GetIO().DisplaySize;
 if(upgrade_detail_index_>=0&&!ImGui::IsPopupOpen("升级详情"))ImGui::OpenPopup("升级详情");
 const bool details_open=ImGui::IsPopupOpen("升级详情");
 if(relic)ImGui::TextColored({.97f,.80f,.44f,1},"遗物奖励 · 选择一项");
 else ImGui::TextColored({.97f,.80f,.44f,1},required_picks==2?"等级 %d · 五选二（可跳过）":"宝箱强化 · 五选一",game_.player.level);
 ImGui::SameLine();ImGui::Text("%d/%d · 数字键 1—%d",game_.upgrade_picks_taken,required_picks,int(game_.choices.size()));
 if(game_.pending_upgrade_levels()>1){ImGui::SameLine();ImGui::TextDisabled("待处理 %d 级",game_.pending_upgrade_levels());}
 if(required_picks==2){
  const int resets=game_.upgrade_resets_remaining+game_.upgrade_rerolls_remaining;ImGui::BeginDisabled(resets<=0);
  if(ImGui::Button(("重置选项 · 剩余 "+std::to_string(resets)).c_str())&&upgrade_action_delay_<=0){game_.reroll_upgrade();upgrade_action_delay_=.18f;}review_ui_flow_item(0);ImGui::EndDisabled();ImGui::SameLine();
  if(ImGui::Button(game_.upgrade_picks_taken?"跳过剩余选择 · Esc":"跳过本次升级 · Esc")&&upgrade_action_delay_<=0&&game_.skip_upgrade()){upgrade_action_delay_=.18f;return;}
  if(ImGui::GetContentRegionAvail().x>1100)ImGui::SameLine();ImGui::TextDisabled("每级免费1次 · 天赋额外%d次",game_.upgrade_rerolls_remaining);
 }
 ImGui::TextDisabled("可选零至两项。技能达标会自动融合；详情查看效果与配方。");
 const int count=int(game_.choices.size());const float gap=10;
 const int columns=std::min(count,ImGui::GetContentRegionAvail().x>=1320?5:3),rows=(count+columns-1)/columns;
 const float width=(ImGui::GetContentRegionAvail().x-gap*(columns-1))/columns,height=(ImGui::GetContentRegionAvail().y-gap*(rows-1)-4)/rows;
 int chosen=-1,detail=-1;ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{gap,10});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12,12});ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10,5});
 for(int i=0;i<count;++i){
  if(i%columns)ImGui::SameLine();ImGui::PushID(i);const auto& c=game_.choices[i];const bool skill=c.kind==UpgradeKind::Skill,study=c.kind==UpgradeKind::Study,fusion=c.kind==UpgradeKind::Fusion;
  const bool available=game_.upgrade_choice_available(c);ImGui::BeginChild("card",{width,height},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
  std::string category=fusion?"技能融合":study?"材料研习":skill?(c.expected_rank>0||game_.skills[int(c.skill)]?"技能升级":"新技能"):c.kind==UpgradeKind::Resonance?"共鸣方向":c.kind==UpgradeKind::Relic?"遗物奖励":"通用成长";
  if(c.selected)category+=" · 已选择";else if(skill)category+=" · Lv."+std::to_string(game_.skills[int(c.skill)])+" → "+std::to_string(game_.skills[int(c.skill)]+1);else if(fusion)category+=" · Lv."+std::to_string(game_.fusion_result_level(c.skill));else if(study)category+=" · Lv.1";
  if(!c.selected&&(skill||study)&&game_.skills[int(c.skill)]==0&&game_.active_skill_count()>=Game::active_skill_limit)category+=" · 需替换";
  ImGui::TextDisabled("%s",category.c_str());
  const auto key=c.kind==UpgradeKind::Resonance?"skill.icon."+std::string(Game::skill_key(resonance_definitions()[int(c.resonance)].first_example)):skill||study||fusion?"skill.icon."+std::string(Game::skill_key(c.skill)):c.kind==UpgradeKind::Relic?relic_icon(c.relic):"passive.icon."+std::to_string(int(c.kind)>0&&int(c.kind)<passive_count?int(c.kind):c.kind==UpgradeKind::Vitality?6:1);
  const auto at=ImGui::GetCursorScreenPos();ui_icon(ImGui::GetWindowDrawList(),key,at,32);ImGui::Dummy({38,34});ImGui::SameLine();ImGui::BeginGroup();ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x);
  ImGui::TextColored(c.selected?ImVec4(.60f,.90f,.76f,1):ImVec4(.97f,.80f,.44f,1),"%s",skill||study||fusion?Game::skill_name(c.skill):c.title.c_str());
  ImGui::PopTextWrapPos();ImGui::EndGroup();ImGui::Separator();
  ImGui::BeginChild("routes",{0,std::max(42.f,ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing())});
  if(skill||study||fusion){
   if(skill){if(c.selected&&game_.skills[int(c.skill)]==0)ImGui::TextUnformatted("已生效 · 材料已自动融合");else if(c.selected)ImGui::Text("已生效 · 基础伤害 %.1f",game_.skill_power(c.skill));else if(game_.skills[int(c.skill)]>0)ImGui::Text("基础伤害 %.1f → %.1f",game_.skill_power(c.skill,game_.skills[int(c.skill)]),game_.skill_power(c.skill,game_.skills[int(c.skill)]+1));else ImGui::Text("初始基础伤害 %.1f",game_.skill_power(c.skill,1));}
   const auto route=game_.fusion_status(game_.main_fusion_route(c.skill));const ResonanceDefinition* link=nullptr;
   if(!route.recipe||route.equipped||(!Game::is_fusion(c.skill)&&(!route.first||!route.second)))for(const auto& rule:resonance_definitions())if(Game::skill_roles(c.skill)&(rule.first_role|rule.second_role)){const auto current=game_.resonance_status(rule.id);if(!link||current.equipped){link=&rule;if(current.equipped)break;}}
   if(link){const auto current=game_.resonance_status(link->id);Skill partner=(Game::skill_roles(c.skill)&link->first_role)?current.second:current.first;if(partner==Skill::Count)partner=(Game::skill_roles(c.skill)&link->first_role)?link->second_example:link->first_example;
    const auto pos=ImGui::GetCursorScreenPos();ui_icon(ImGui::GetWindowDrawList(),"skill.icon."+std::string(Game::skill_key(partner)),pos,24);ImGui::Dummy({30,26});ImGui::SameLine();ImGui::TextWrapped("%s · %s",link->name,Game::skill_name(partner));if(ImGui::IsItemHovered())tooltip(link->name,link->description);
   }else if(route.recipe){Skill partner=route.recipe->first==c.skill?route.recipe->second:route.recipe->first;const auto pos=ImGui::GetCursorScreenPos();ui_icon(ImGui::GetWindowDrawList(),"skill.icon."+std::string(Game::skill_key(partner)),pos,24);ImGui::Dummy({30,26});ImGui::SameLine();ImGui::TextWrapped("融合 · %s",Game::skill_name(route.recipe->result));ImGui::TextDisabled("%s",route.hint.c_str());}
   else ImGui::TextDisabled("完整参数见详情");
  }else ImGui::TextWrapped("%s",brief_description(c.description.c_str()).c_str());

  if(skill&&!c.selected){const auto prediction=game_.fusion_preview(c.skill);if(!prediction.empty()){auto snapshot=game_.skills;++snapshot[int(c.skill)];const auto plan=fusion_plan(snapshot);if(!plan.empty())ImGui::TextColored(ui::color(ui::fusion),"选择后自动融合：%s",Game::skill_name(plan.front()->result));}}
  ImGui::EndChild();const float detail_width=(ImGui::GetContentRegionAvail().x-gap)*.4f;if(ImGui::Button("详情",{detail_width,0}))detail=i;ImGui::SameLine();
  ImGui::BeginDisabled(!available||upgrade_action_delay_>0);const auto caption=(c.selected?"已选择 ":fusion?"融合 ":study?"研习 ":"选择 ")+std::to_string(i+1);
  const bool pressed=ImGui::Button(caption.c_str(),{-1,0});if(upgrade_action_delay_<=0&&(pressed||(!details_open&&available&&ImGui::IsKeyPressed(ImGuiKey(ImGuiKey_1+i),false))))chosen=i;
  ImGui::EndDisabled();ImGui::EndChild();ImGui::PopID();
 }
 ImGui::PopStyleVar(3);
 if(detail>=0){upgrade_detail_index_=detail;ImGui::OpenPopup("升级详情");}
 else if(chosen>=0&&game_.choose_upgrade(size_t(chosen))){upgrade_action_delay_=.18f;if(!game_.pending_study.active)++upgrade_selections_;}
 ImGui::SetNextWindowSize({std::min(650.f,size.x-48),std::min(620.f,size.y-60)},ImGuiCond_Appearing);
 if(ImGui::BeginPopupModal("升级详情",nullptr,ImGuiWindowFlags_NoResize)){
  const auto& c=game_.choices[size_t(upgrade_detail_index_)];const bool skill=c.kind==UpgradeKind::Skill||c.kind==UpgradeKind::Study||c.kind==UpgradeKind::Fusion;
  ImGui::TextColored({.97f,.80f,.44f,1},"%s",skill?Game::skill_name(c.skill):c.title.c_str());
  ImGui::TextDisabled(c.selected?"已选择，效果已生效。":"查看说明不会选择卡片，也不会消耗重置次数。");ImGui::Separator();
  ImGui::BeginChild("完整说明",{0,-ImGui::GetFrameHeightWithSpacing()});ImGui::TextWrapped("%s",c.description.c_str());
  if(skill){ImGui::Separator();ImGui::TextWrapped("%s",game_.fusion_preview(c.skill).c_str());ImGui::TextUnformatted("融合路线");for(const auto& recipe:fusion_recipes()){auto inputs=fusion_inputs(recipe);if(!recipe.released||(std::find(inputs.begin(),inputs.end(),c.skill)==inputs.end()&&recipe.result!=c.skill))continue;
   std::string chain;for(Skill input:inputs){if(!chain.empty())chain+=" + ";chain+=Game::skill_name(input);}ImGui::TextWrapped("%s → %s（%d源）",chain.c_str(),Game::skill_name(recipe.result),int(recipe.sources.size()));ImGui::TextDisabled("当前材料真实等级各需 Lv.5 · 达标自动融合");
  }}
  ImGui::EndChild();if(ImGui::Button("关闭详情 · Esc",{-1,0})||ImGui::IsKeyPressed(ImGuiKey_Escape,false)){upgrade_detail_index_=-1;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
 }
}

// Full numbers and loadout text live in the voluntary pause view.
void App::passive_panel(){
 ImGui::BeginChild("构筑内容",{0,std::max(180.f,ImGui::GetContentRegionAvail().y-104)});
 if(ImGui::BeginTabBar("构筑分页")){
  if(ImGui::BeginTabItem("技能")){
   if(game_.regression_active())ImGui::TextColored(ui::color(ui::danger),"退化：真实等级保留，斜线块暂时失效。");
   for(Skill skill:game_.skill_slots())if(skill!=Skill::Count){ImGui::PushID(int(skill));const auto status=game_.skill_status(skill);const auto at=ImGui::GetCursorScreenPos();ui_icon(ImGui::GetWindowDrawList(),"skill.icon."+std::string(Game::skill_key(skill)),at,30);ImGui::Dummy({36,32});ImGui::SameLine();
    if(ImGui::TreeNode("skill","%s · Lv.%d / %d%s",Game::skill_name(skill),status.level,status.cap,status.usable()?"":" · 压制")){ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x);skill_details(skill);ImGui::PopTextWrapPos();ImGui::TreePop();}ImGui::PopID();
   }
   if(ImGui::CollapsingHeader("被动与装备")){for(int i=1;i<passive_count;++i)if(game_.passives[i]){ImGui::Text("%s · Lv.%d",Game::passive_name(UpgradeKind(i)),game_.passives[i]);ImGui::TextWrapped("%s",game_.passive_value(UpgradeKind(i)).c_str());}loadout_sidebar();}
   ImGui::EndTabItem();
  }
  for(int category=0;category<2;++category)if(ImGui::BeginTabItem(category==0?"已激活":"潜在")){
   ImGui::TextWrapped("同时装备不同技能即可共鸣，不消耗技能。");bool found=false;
   for(const auto& rule:resonance_definitions()){const auto status=game_.resonance_status(rule.id);if(status.equipped!=(category==0))continue;found=true;int id=int(rule.id);ImGui::Separator();ImGui::TextColored(ui::color(status.usable?ui::ready:ui::muted),"%s",rule.name);ImGui::TextWrapped("%s",status.hint.c_str());ImGui::TextWrapped("%s",rule.description);
    if(status.equipped)ImGui::TextWrapped("%s + %s · %d 次 · 伤害 %.0f · 有效护盾 %.0f · 辅助 %.0f",Game::skill_name(status.first),Game::skill_name(status.second),game_.metrics.resonance.triggers[id],game_.metrics.resonance.damage[id],game_.metrics.resonance.shield[id],game_.metrics.resonance.support[id]);
   }if(!found)ImGui::TextDisabled("当前没有符合此分类的共鸣。");ImGui::EndTabItem();
  }
  if(ImGui::BeginTabItem("融合",nullptr,preview_build_tab_==3?ImGuiTabItemFlags_SetSelected:0)){fusion_panel();ImGui::EndTabItem();}
  if(ImGui::BeginTabItem("局内装备",nullptr,preview_build_tab_==4?ImGuiTabItemFlags_SetSelected:0)){run_gear_panel();ImGui::EndTabItem();}
  if(ImGui::BeginTabItem("强化")){ImGui::TextWrapped("普通升级最多出现一项共鸣方向，每种共鸣只能选择一个方向，不占技能槽。");bool found=false;for(const auto& upgrade:resonance_upgrades())if(game_.resonance_directions[int(upgrade.id)]==upgrade.direction){found=true;const auto status=game_.resonance_status(upgrade.id);ImGui::Separator();ImGui::TextColored(ui::color(status.usable?ui::ready:ui::muted),"%s · %s",resonance_definitions()[int(upgrade.id)].name,upgrade.name);ImGui::TextWrapped("%s",status.usable?"强化生效中":"强化已获得，但当前未激活");ImGui::TextWrapped("%s",upgrade.description);}if(!found)ImGui::TextDisabled("尚未选择方向；基础共鸣照常生效。");ImGui::EndTabItem();}
  ImGui::EndTabBar();preview_build_tab_=-1;
 }ImGui::EndChild();
}

void App::settings_menu(){
 ImGui::TextColored(ui::color(ui::accent),"画面与声音");ImGui::TextDisabled("设置自动保存 · 音乐使用正常播放速度");
 ImGui::BeginChild("设置内容",{0,-46});bool changed=false;
 changed|=ImGui::Checkbox("背景音乐",&music_enabled_);changed|=ImGui::SliderFloat("音乐音量",&music_volume_,0,1);
 changed|=ImGui::Checkbox("战斗音效",&sound_);changed|=ImGui::SliderFloat("战斗音量",&volume_,0,1);
 changed|=ImGui::Checkbox("界面与关键提示",&ui_sound_);changed|=ImGui::SliderFloat("提示音量",&ui_volume_,0,1);
 if(ImGui::CollapsingHeader("原创音乐试听",ImGuiTreeNodeFlags_DefaultOpen)){
  const auto& tracks=Chiptune::tracks();if(ImGui::BeginCombo("曲目",tracks[audition_track_].title)){for(int i=0;i<int(tracks.size());++i)if(ImGui::Selectable(tracks[i].title,audition_track_==i))audition_track_=i;ImGui::EndCombo();}
  const auto& track=tracks[audition_track_];ImGui::Text("%s · %d BPM · %.0f 秒",Chiptune::scene_name(track.scene),track.bpm,track.loop_seconds());
  if(ImGui::Button("试听选中曲目"))music_.audition(audition_track_);ImGui::SameLine();if(ImGui::Button("返回场景音乐"))music_.end_audition();
  ImGui::TextWrapped("正在播放：%s · %.1f 秒%s",tracks[music_.track_index()].title,music_.playhead_seconds(),music_.auditioning()?" · 试听中":"");
  if(!audio_device_)ImGui::TextColored(ui::color(ui::danger),"音频设备未能打开。");else if(!music_enabled_||music_volume_==0)ImGui::TextDisabled("音乐当前静音；开启音乐并调整音量后试听。");
 }
 ImGui::Separator();changed|=ImGui::SliderFloat("友方特效不透明度",&friendly_effect_alpha_,.2f,1.f,"%.2f");
 changed|=ImGui::Checkbox("低特效模式",&low_effects_);ImGui::TextDisabled("减少装饰，保留危险提示和实际判定。");
 changed|=ImGui::Checkbox("屏幕震动",&shake_enabled_);changed|=ImGui::SliderFloat("震动强度",&shake_strength_,0,1,"%.2f");
 changed|=ImGui::SliderFloat("装饰闪光强度",&flash_strength_,0,1,"%.2f");ImGui::TextDisabled("降低闪光仍保留敌方判定与危险边界。");
 changed|=ImGui::SliderFloat("伤害数字密度",&damage_number_density_,0,1,"%.2f");changed|=ImGui::Checkbox("显示性能信息",&show_stats_);changed|=ImGui::SliderFloat("画面缩放",&zoom_,1,2.2f,"%.1f 倍");
 if(ImGui::SliderFloat("界面缩放",&ui_scale_,.85f,1.25f,"%.2f 倍")){changed=true;apply_ui_scale();}
 changed|=ImGui::Checkbox("首次玩法提示",&guidance_);
 if(ImGui::Button("重新显示首次提示")){guide_seen_=boss_hints_seen_=0;guide_remaining_=guide_gap_=0;guidance_=true;changed=true;}
 if(ImGui::CollapsingHeader("操作与玩法说明"))guide_reference();
 if(ImGui::CollapsingHeader("版本与运行信息")){
  ImGui::Text("程序 %s · 内容 %s",MAPLERGL_VERSION,MAPLERGL_CONTENT_VERSION);
  ImGui::Text("构建 %s · %s",MAPLERGL_BUILD_COMMIT,MAPLERGL_BUILD_TIME);
  ImGui::TextWrapped("素材目录：%s",std::filesystem::absolute("assets").u8string().c_str());
  if(progression_)ImGui::TextWrapped("存档错误日志：%s",(progression_->directory()/"logs"/"profile-errors.log").u8string().c_str());
  ImGui::Text("%s · %s",Game::mode_name(game_.run_mode()),game_speed()==2?"紧凑2×":"标准1×");
  if(!error_.empty())ImGui::TextColored(ui::color(ui::danger),"%s",error_.c_str());
  for(const auto& message:content_diagnostics_)ImGui::TextWrapped("%s",message.c_str());
 }
 if(changed){settings_dirty_=true;settings_save_delay_=1.f;}
 ImGui::EndChild();if(ui::primary_button("返回",{-1,36})){music_.end_audition();settings_=false;}
}

void App::build_sidebar(){
 auto* draw=ImGui::GetBackgroundDrawList();const auto size=ImGui::GetIO().DisplaySize;const float width=std::clamp(size.x*.20f,238.f,290.f),x=14,y=102;
 const bool compact=size.y<800;const float name_size=compact?16.f:19.f,aux_size=compact?14.f:16.f;const auto& slots=game_.skill_slots();const float row=std::clamp((size.y-y-112-28)/8.f,48.f,78.f);panel(draw,{x,y},{width,28+row*8});
 label(draw,{x+12,y+7},gold,"技能构筑 · "+std::to_string(game_.active_skill_count())+"/8",15);
 label(draw,{x+width-90,y+7},muted,"Esc 详情",14);
 if(hover({x,y},{width,28},game_.state)&&ImGui::IsMouseClicked(0))game_.toggle_pause();
 for(int i=0;i<8;++i){const Skill skill=slots[i];const float top=y+28+i*row;
  if(i)draw->AddLine({x+10,top},{x+width-10,top},IM_COL32(183,193,200,30));
  if(skill==Skill::Count){label(draw,{x+14,top+16},IM_COL32(183,193,200,95),"空技能槽",14);continue;}
  if(!legacy_visuals_&&fusion_highlight_[int(skill)]>clock_){float fade=std::min(1.f,(fusion_highlight_[int(skill)]-clock_)/1.4f);draw->AddRectFilled({x+5,top},{x+width-5,top+row-2},IM_COL32(172,143,222,int(35*fade)),5);draw->AddRectFilled({x+5,top},{x+8,top+row-2},IM_COL32(241,213,154,int(230*fade)),2);}
  const auto status=game_.skill_status(skill);const bool fused=Game::is_fusion(skill);const float edge=row<58?28.f:34.f;
  ui_icon(draw,"skill.icon."+std::string(Game::skill_key(skill)),{x+10,top+5},edge,status.usable()?0xffffffff:IM_COL32(185,185,185,180));
  const float sx=x+52;label(draw,{sx,top+3},status.sealed?red:text,Game::skill_name(skill),name_size);
  if(status.sealed)label(draw,{x+width-34,top+3},red,"锁",14);else if(fused)label(draw,{x+width-34,top+3},fusion_color,std::to_string(Game::fusion_tier(skill))+"源",12);
  ui::levels(draw,{sx,top+27},72,status.level,status.effective,status.cap);
  const auto levels=status.effective==status.level?"Lv."+std::to_string(status.level)+"/"+std::to_string(status.cap):"Lv."+std::to_string(status.level)+" → "+std::to_string(status.effective);
  label(draw,{sx+82,top+22},status.effective==status.level?muted:red,levels,aux_size);
  const auto route=game_.fusion_status(game_.main_fusion_route(skill));
  const ResonanceDefinition* highlighted=nullptr;ResonanceStatus resonance_view;
  if(!route.recipe||route.equipped||(!fused&&(!route.first||!route.second)))for(const auto& rule:resonance_definitions())if(Game::skill_roles(skill)&(rule.first_role|rule.second_role)){const auto current=game_.resonance_status(rule.id);if(!highlighted||(current.equipped&&!resonance_view.equipped)){highlighted=&rule;resonance_view=current;}}
  if(highlighted){const float hy=top+(row<58?39:44);Skill partner=resonance_view.first==skill?resonance_view.second:resonance_view.first;if(partner==Skill::Count)partner=(Game::skill_roles(skill)&highlighted->first_role)?highlighted->second_example:highlighted->first_example;
   ui_icon(draw,"skill.icon."+std::string(Game::skill_key(partner)),{sx,hy},16,game_.skills[int(partner)]?0xffffffff:IM_COL32(180,180,180,120));
   std::string hint=resonance_view.equipped?std::string(highlighted->name)+(resonance_view.usable?" · 共鸣":" · 停用"):resonance_view.hint;draw->PushClipRect({sx+21,hy},{x+width-10,top+row-1},true);label(draw,{sx+21,hy},resonance_view.usable?mint:muted,hint,13);draw->PopClipRect();
   if(hover({sx,hy},{width-62,19},game_.state))tooltip(std::string(highlighted->name)+" · "+game_.resonance_direction_name(highlighted->id)+" · 不消耗技能",resonance_view.hint+"\n"+highlighted->description);
   if(game_.resonance_flash(highlighted->id)>0)draw->AddCircle({x+width-12,hy+6},3,mint,8,1.5f);
  }
  else if(route.recipe){const Skill partner=route.recipe->first==skill?route.recipe->second:route.recipe->first;const auto partner_status=game_.skill_status(partner);const float hy=top+(row<58?39:44);
   ui_icon(draw,"skill.icon."+std::string(Game::skill_key(partner)),{sx,hy},16,partner_status.level?0xffffffff:IM_COL32(200,200,200,110));
   const std::string hint=route.hint;
   draw->PushClipRect({sx+21,hy},{x+width-10,top+row-1},true);label(draw,{sx+21,hy},route.ready?mint:muted,hint,13);draw->PopClipRect();
   if(hover({sx,hy},{width-62,19},game_.state)){ImGui::BeginTooltip();ImGui::TextUnformatted("技能融合 · 达标自动消耗两个材料技能");fusion_recipe_view(*route.recipe,false);ImGui::EndTooltip();}
  }
  if(hover({x+8,top},{width-16,37},game_.state)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(380);skill_details(skill);ImGui::PopTextWrapPos();ImGui::EndTooltip();}
 }
}

void App::content_hud(){
 auto* draw=ImGui::GetBackgroundDrawList();const auto size=ImGui::GetIO().DisplaySize;const float scale=ui_scale();
 const float side=std::clamp(size.x*.215f,190.f,248.f),middle=std::min(650.f,size.x-2*(side+28));
 const float gap=6,edge=26;
 struct Timed{const char* key;const char* title;const char* detail;float remaining;bool harmful;bool contact;};
 bool muddy=false;if(game_.config.enable_scene_objects)for(const auto& object:game_.scene_objects)if(object.kind==SceneObjectKind::Mud&&object.state==SceneObjectState::Active&&length(object.pos-game_.player.pos)<object.radius+13){muddy=true;break;}
 const Timed states[]={
  {"fury","狂热","伤害 ×1.40，攻速 ×1.25。再次获得时按实际规则刷新持续时间。",game_.fury_remaining,false,false},
  {"magnet","磁力","经验吸附范围扩大，持续时间结束后恢复正常拾取范围。",game_.magnet_remaining,false,false},
  {"shield","护盾","持续期间免疫伤害。这是限时保护，不是额外生命值。",game_.shield_remaining,false,false},
  {"double_xp","双倍经验","拾取经验 ×2；营地经验加成继续按实际规则生效。",game_.double_xp_remaining,false,false},
  {"power","力量","伤害 ×1.50。",game_.power_remaining,false,false},
  {"rift","时空裂隙","技能冷却间隔 ×0.70，同时敌人移速 ×1.25。",game_.rift_remaining,false,false},
  {"fury","战吼","战吼徽记触发：攻速 ×1.20。",game_.relic_haste_remaining(),false,false},
  {"curse","诅咒","移速 ×0.80。",game_.curse_remaining,true,false},
  {"mud","泥地减速","身处泥地时移速 ×0.55，离开区域即解除。",muddy?1.f:0.f,true,true},
  {"curse","烟幕封锁","当前区域禁止新攻击，冷却继续。移动、冲刺和拾取保留；离开后不补发攻击。",game_.smoke_at(game_.player.pos)?1.f:0.f,true,true},
  {"power","装备压制","当前装备属性和附加能力暂不贡献；装备实例保留，压制来源消失后恢复。",game_.equipment_suppressed()?1.f:0.f,true,true},
  {"rift","退化光环","技能与装备有效等级降低 1 级；真实等级保留，多个区域不重复降低。",game_.regression_active()?1.f:0.f,true,true}
 };
 // Each timed effect has a reserved position; expirations never shift adjacent icons.
 const int state_count=int(sizeof(states)/sizeof(*states));const float state_width=state_count*(edge+gap)+12*scale;
 const float start=size.x-268;bool any=false;
 for(int i=0;i<state_count;++i){const auto& buff=states[i];if(buff.remaining<=0)continue;any=true;const bool suppressed=!buff.harmful&&game_.buffs_suppressed();
  const ImVec2 at{start+(i>=7?i-7:i)*(edge+gap),i>=7?86.f:28.f};
  draw->AddRectFilled({at.x-3,at.y-3},{at.x+edge+3,at.y+edge+19*scale},ink,5);
  ui_icon(draw,"buff.icon."+std::string(buff.key),at,edge,suppressed?IM_COL32(165,165,165,150):0xffffffffu);
  if(suppressed)draw->AddLine({at.x+3,at.y+edge-3},{at.x+edge-3,at.y+3},red,2);
  draw->AddRect({at.x-3,at.y-3},{at.x+edge+3,at.y+edge+19*scale},buff.harmful?red:border,5,0,buff.harmful?2.f:1.f);
  if(buff.harmful){draw->AddTriangleFilled({at.x+edge-9,at.y-3},{at.x+edge+3,at.y-3},{at.x+edge+3,at.y+9},red);label(draw,{at.x+edge-4,at.y-4},ink,"!",12*scale);}
  centered(draw,{at.x+edge*.5f,at.y+edge+2*scale},buff.harmful||suppressed?red:mint,buff.contact?(i==9?"烟幕":i==10?"压制":i==11?"退化":"区域"):seconds(buff.remaining),12*scale);
  if(hover({at.x-3,at.y-3},{edge+6,edge+22*scale},game_.state))tooltip(std::string(buff.title)+(suppressed?" · 暂停贡献":""),std::string(buff.detail)+(suppressed?"\n卸甲压制中，正面效果暂不生效，原有倒计时继续。":"")+(buff.contact?"":"\n剩余 "+seconds(buff.remaining)+"（战斗秒，紧凑模式不加速）"));
 }
 label(draw,{start,10},muted,"增益",13);label(draw,{start,69},muted,"负面状态",13);

 struct Persistent{std::string key,name,description,badge;bool suppressed=false;};std::vector<Persistent> persistent;
 for(int i=1;i<passive_count;++i)if(game_.passives[i])persistent.push_back({"passive.icon."+std::to_string(i),Game::passive_name(UpgradeKind(i)),game_.passive_value(UpgradeKind(i)),std::to_string(game_.passives[i])});
 if(game_.overcharge_levels)persistent.push_back({"passive.icon.1","极限强攻","本次冒险基础伤害 +"+std::to_string(game_.overcharge_levels*3.5f)+"%。",std::to_string(game_.overcharge_levels)});
 if(game_.vitality_levels)persistent.push_back({"passive.icon.6","永久韧性","本次冒险生命上限 +"+std::to_string(game_.vitality_levels*8)+"。",std::to_string(game_.vitality_levels)});
 for(auto id:game_.run_setup().equipment)if(id!=EquipmentId::None){const auto& def=equipment_definitions()[int(id)];const bool suppressed=game_.equipment_suppressed(),degraded=game_.regression_active();persistent.push_back({equipment_icon(id),def.name,std::string(def.description)+(suppressed?"\n装备来源被压制，当前不贡献属性与触发。":degraded?"\n真实效果 Lv.1 → 有效 Lv.0；离开退化区域后恢复。":"\n装备效果 Lv.1"),suppressed?"停":degraded?"0":"装",suppressed||degraded});}
 for(const auto& def:relic_definitions())if(game_.owns_relic(def.id))persistent.push_back({relic_icon(def.id),def.name,def.description,"遗"});
 const float pe=23,pg=8;const int capacity=8;
 const int visible=std::min(int(persistent.size()),capacity);const bool overflow=int(persistent.size())>capacity;
 float px=start;
 for(int i=0;i<visible;++i){ImVec2 at{px+i*(pe+pg),144};draw->AddRectFilled({at.x-3,at.y-3},{at.x+pe+3,at.y+pe+3},ink,4);
  if(overflow&&i==visible-1){centered(draw,{at.x+pe*.5f,at.y+4},gold,"+"+std::to_string(persistent.size()-i),14*scale);if(hover(at,{pe,pe},game_.state)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(360);ImGui::TextUnformatted("其他持续增益");for(size_t j=i;j<persistent.size();++j){ImGui::Separator();ImGui::TextColored({.97f,.80f,.44f,1},"%s · %s",persistent[j].name.c_str(),persistent[j].badge.c_str());ImGui::TextWrapped("%s",persistent[j].description.c_str());}ImGui::PopTextWrapPos();ImGui::EndTooltip();}continue;}
  const auto& entry=persistent[i];ui_icon(draw,entry.key,at,pe,entry.suppressed?IM_COL32(160,160,160,155):0xffffffffu);if(entry.suppressed)draw->AddLine({at.x+2,at.y+pe-2},{at.x+pe-2,at.y+2},red,1.5f);const ImVec2 badge{at.x+pe-8,at.y+pe-10};draw->AddRectFilled({badge.x-2,badge.y-1},{at.x+pe+5,at.y+pe+7},IM_COL32(8,18,17,245),2);label(draw,badge,entry.suppressed?red:gold,entry.badge,12*scale);
  if(hover({at.x-3,at.y-3},{pe+6,pe+10},game_.state))tooltip(entry.name+" · "+entry.badge,entry.description+"\n持续增益 · 本次冒险生效");
 }

 int boss_row=0;for(const auto& e:game_.enemies)if(e.kind==EnemyKind::Boss&&e.hp>0){const float width=std::min(600.f,size.x-2*(side+25)),x=(size.x-width)*.5f,y=82+49*scale*boss_row++,height=43*scale;panel(draw,{x,y},{width,height});char value[160];
  snprintf(value,sizeof(value),"%s · 阶段 %d",Game::boss_profile_name(e.boss_profile),e.boss_stage);const std::string title=value;label(draw,{x+10,y+5*scale},e.boss_stage==3?red:gold,title,16*scale);
  if(e.windup>0)snprintf(value,sizeof(value),"%s %.1fs",Game::boss_attack_name(e.boss_attack),e.windup);
  else if(e.action_remaining>0)snprintf(value,sizeof(value),"%s",Game::boss_attack_name(e.boss_attack));
  else if(e.recovery_remaining>0)snprintf(value,sizeof(value),"收招 %.1fs",e.recovery_remaining);
  else snprintf(value,sizeof(value),"观察预警");
  std::string action=value;const float title_width=ImGui::GetFont()->CalcTextSizeA(16*scale,10000,0,title.c_str()).x;
  float action_width=ImGui::GetFont()->CalcTextSizeA(14*scale,10000,0,action.c_str()).x;
  if(title_width+action_width+36>width){action=e.windup>0?"预警 "+seconds(e.windup):e.action_remaining>0?"施招中":e.recovery_remaining>0?"收招":"待机";action_width=ImGui::GetFont()->CalcTextSizeA(14*scale,10000,0,action.c_str()).x;}
  label(draw,{x+width-10-action_width,y+6*scale},e.recovery_remaining>0?mint:muted,action,14*scale);
  const float bx=x+10,bw=width-20,by=y+27*scale,bh=9*scale;draw->AddRectFilled({bx,by},{bx+bw,by+bh},IM_COL32(55,24,28,230),4);draw->AddRectFilled({bx,by},{bx+bw*std::clamp(e.hp/std::max(1.f,e.max_hp),0.f,1.f),by+bh},IM_COL32(214,92,96,255),4);
  const auto& boss_rule=boss_combat_definition(e.boss_profile);for(float mark:{boss_rule.phase_two,boss_rule.phase_three})draw->AddLine({bx+bw*mark,by},{bx+bw*mark,by+bh},gold);
  if(hover({x,y},{width,height},game_.state)){snprintf(value,sizeof(value),"生命 %.0f / %.0f · 阶段 %d / 3\n%s",e.hp,e.max_hp,e.boss_stage,Game::boss_attack_name(e.boss_attack));tooltip(Game::boss_profile_name(e.boss_profile),value);}
 }

 float side_y=88+49*scale*boss_row;const float side_x=(size.x-middle)*.5f;
 auto event_panel=[&](const std::string& title,const std::string& body,ImU32 color){const float height=48;panel(draw,{side_x,side_y},{middle,height});label(draw,{side_x+10,side_y+5},color,title,15);draw->PushClipRect({side_x+10,side_y+26},{side_x+middle-10,side_y+45},true);label(draw,{side_x+10,side_y+26},muted,body,14);draw->PopClipRect();if(hover({side_x,side_y},{middle,height},game_.state))tooltip(title,body);side_y+=height+6;};
 const auto* pressure=game_.pressure_event.remaining>0?pressure_definition(game_.pressure_event.kind):nullptr;
 if(pressure)event_panel(std::string(pressure->name)+" · "+seconds(game_.pressure_event.remaining/game_.encounter_pace()),pressure_detail(game_),red);
 if(game_.special_event.kind!=SpecialEventKind::None){char title[128];snprintf(title,sizeof(title),"%s · %.0fs",Game::special_event_name(game_.special_event.kind),game_.special_event.remaining/game_.encounter_pace());event_panel(title,game_.special_event_objective(),gold);
  auto at=screen(game_.special_event.pos);if(at.x<side+40||at.x>size.x-32||at.y<195*scale||at.y>size.y-165*scale){at.x=std::clamp(at.x,side+40,std::max(side+40,size.x-32));at.y=std::clamp(at.y,195*scale,std::max(195*scale,size.y-165*scale));const auto direction=normalized(game_.special_event.pos-game_.player.pos);draw->AddTriangleFilled({at.x+direction.x*11,at.y+direction.y*11},{at.x-direction.x*7-direction.y*6,at.y-direction.y*7+direction.x*6},{at.x-direction.x*7+direction.y*6,at.y-direction.y*7-direction.x*6},gold);}
 }
 const auto& d=game_.director_state;if(!pressure&&game_.config.enable_director&&d.kind!=EncounterKind::None&&d.kind!=EncounterKind::Boss&&(d.phase==EncounterPhase::Telegraph||d.phase==EncounterPhase::Active)){
  char title[128];snprintf(title,sizeof(title),"%s · %s %.0fs",EncounterDirector::encounter_name(d.kind),d.phase==EncounterPhase::Telegraph?"预告":"",std::max(0.f,d.remaining)/game_.encounter_pace());event_panel(title,EncounterDirector::encounter_hint(d.kind),gold);
 }
 std::string synergy_notice;int shown=0;for(int i=0;i<synergy_count;++i)if(game_.synergy_flash[i]>0&&shown<3){if(shown++)synergy_notice+='\n';synergy_notice+=std::string(Game::synergy_name(i))+" ×"+std::to_string(game_.metrics.synergy_triggers[i]);}
 if(!synergy_notice.empty())event_panel("联动触发",synergy_notice,fusion_color);
 const float dock_top=size.y-112*scale;
 const SceneObject* nearest=nullptr;float distance=80;for(const auto& object:game_.scene_objects)if(object.state==SceneObjectState::Active&&object.kind!=SceneObjectKind::Mud){const float candidate=length(object.pos-game_.player.pos);if(candidate<distance){nearest=&object;distance=candidate;}}
 if(nearest){const char* hint=nearest->kind==SceneObjectKind::HealingWell?(game_.player.hp>=game_.player.max_hp?"恢复石灯 · 生命已满":"E  恢复石灯"):nearest->kind==SceneObjectKind::Barrel?"E  引爆爆裂花 · 触发后远离":"E  破坏木箱";centered(draw,{size.x*.5f,dock_top-41*scale},mint,hint,14*scale);}
 bool fusion_feedback=false;
 if(fusion_feedback_skill_!=Skill::Count&&game_.state==State::Playing){
  if(game_.skills[int(fusion_feedback_skill_)]==0)fusion_feedback_skill_=Skill::Count;
  else{if(fusion_feedback_started_<0)fusion_feedback_started_=clock_;const float age=clock_-fusion_feedback_started_;fusion_feedback=age<1.1f;
   if(fusion_feedback){
    std::string entry_id;for(const auto& completed:game_.scrolls.completed)if(completed.result==fusion_feedback_skill_){entry_id=completed.recipe_id;break;}
    for(const auto& recipe:fusion_recipes())if(recipe.result==fusion_feedback_skill_&&(entry_id.empty()||recipe.id==entry_id)){const float join=std::clamp(age/.55f,0.f,1.f);const ImVec2 at{size.x*.5f,dock_top-99*scale};
     const ImU32 fade=IM_COL32(255,255,255,int(255*(1-join)));const auto inputs=fusion_inputs(recipe);
     for(size_t i=0;i<inputs.size();++i)ui_icon(draw,"skill.icon."+std::string(Game::skill_key(inputs[i])),{at.x-14+(float(i)-float(inputs.size()-1)*.5f)*64*(1-join),at.y},28,fade);
     ui_icon(draw,"skill.icon."+std::string(Game::skill_key(recipe.result)),{at.x-16,at.y-2},32,IM_COL32(255,255,255,int(255*std::min(1.f,join*2))));std::string title=std::string("融合完成 · ")+Game::skill_name(recipe.result);if(Game::form_count(recipe.result)>1)title+=" · "+std::string(Game::scroll_form_name(recipe.result,game_.fusion_form(recipe.result)));centered(draw,{at.x,at.y+36},fusion_color,title,14*scale);break;}
   }else fusion_feedback_skill_=Skill::Count;
  }
 }
 if(game_.notice_remaining>0&&!fusion_feedback){const float width=std::min(540.f,size.x-80),font=14*scale;const auto extent=ImGui::GetFont()->CalcTextSizeA(font,10000,width-20,game_.notice_text.c_str());const float height=std::min(2*font,extent.y)+12;const ImVec2 at{(size.x-width)*.5f,dock_top-63*scale-height};panel(draw,at,{width,height});draw->PushClipRect({at.x+8,at.y+6},{at.x+width-8,at.y+height-4},true);draw->AddText(ImGui::GetFont(),font,{at.x+10,at.y+6},text,game_.notice_text.c_str(),nullptr,width-20);draw->PopClipRect();}
}

void App::hud(){
 auto* draw=ImGui::GetBackgroundDrawList();const auto size=ImGui::GetIO().DisplaySize;const float scale=ui_scale(),side=std::clamp(size.x*.20f,238.f,290.f);char value[192];
 panel(draw,{14,12},{side,80});ui_icon(draw,player_sprite(int(game_.character),"idle"),{23,20},31);
 snprintf(value,sizeof(value),"冒险家 Lv.%d · 装备%llu",game_.player.level,static_cast<unsigned long long>(game_.gear_count()));label(draw,{63,19},gold,value,17);
 snprintf(value,sizeof(value),"%.0f / %.0f",game_.player.hp,game_.player.max_hp);label(draw,{63,40},text,value,15);
 const float hp_width=side-24;draw->AddRectFilled({26,64},{26+hp_width,74},ui::secondary,3);draw->AddRectFilled({26,64},{26+hp_width*std::clamp(game_.player.hp/std::max(1.f,game_.player.max_hp),0.f,1.f),74},red,3);
 if(game_.guard_charge>0||game_.shield_remaining>0||game_.resonance_shield()>0){snprintf(value,sizeof(value),"盾 %.0f · %.1fs",game_.guard_charge+game_.resonance_shield(),game_.shield_remaining);label(draw,{14+side-122,42},mint,value,13);}
 const float timer_width=std::min(300.f,size.x-2*(side+28)),right=(size.x-timer_width)*.5f;
 panel(draw,{right,12},{timer_width,61});const int remaining=game_.run_mode()==RunMode::Endless?int(game_.combat_elapsed):int(std::ceil(game_.remaining_combat_seconds()));
 snprintf(value,sizeof(value),"%02d:%02d  ·  %s",remaining/60,remaining%60,game_.encounter_pace()==2?"紧凑2×":"标准1×");centered(draw,{size.x*.5f,19},text,value,21);
 snprintf(value,sizeof(value),"击杀 %d  ·  Boss %d/%d  ·  金币 %d",game_.kills,game_.bosses_spawned(),game_.scheduled_boss_count(),game_.coins);centered(draw,{size.x*.5f,48},muted,value,13);
 if(hover({right,12},{timer_width,61},game_.state))tooltip("关卡节奏","紧凑2×只压缩关卡与出怪安排。移动、攻击、冷却和音乐保持正常。选择与暂停不计入战斗时间。");
 build_sidebar();
 content_hud();
 const int slots=Game::active_skill_limit;const float fit=std::min(scale,(size.x-side-56)/(slots*54.f+(slots-1)*8.f));const float gap=8*fit,edge=54*fit,width=slots*edge+(slots-1)*gap,x=std::max(side+28,(size.x-width)*.5f),y=size.y-112*scale;
 centered(draw,{size.x*.5f,y-19*scale},muted,"技能构筑 · 达标自动融合 · Esc 查看",12*scale);
 const auto& equipped=game_.skill_slots();
 for(int slot=0;slot<slots;++slot){const ImVec2 at{x+slot*(edge+gap),y};panel(draw,at,{edge,edge+16*scale});
  if(equipped[slot]==Skill::Count){centered(draw,{at.x+edge*.5f,at.y+19*scale},IM_COL32(112,140,125,170),"空",14*scale);continue;}
  const Skill skill=equipped[slot];const float cooldown=game_.skill_cooldown_remaining(skill),total=game_.skill_cooldown_total(skill),active=game_.skill_active_remaining(skill);const bool evolved=game_.evolved(skill),fused=Game::is_fusion(skill),sealed=game_.skill_sealed(skill),smoked=game_.smoke_at(game_.player.pos),failed=game_.skill_failure_feedback(skill)>0;const int effective=game_.effective_skill_level(skill);const bool blocked=sealed||smoked||effective==0;
  const auto route=game_.fusion_status(game_.main_fusion_route(skill));const bool ready=route.ready;
  ui_icon(draw,"skill.icon."+std::string(Game::skill_key(skill)),{at.x+7*scale,at.y+5*scale},edge-14*scale,blocked?IM_COL32(150,155,155,155):0xffffffffu);
  if(cooldown>0&&skill!=Skill::DragonArmor){const float fraction=std::clamp(cooldown/std::max(.01f,total),0.f,1.f);draw->AddRectFilled({at.x+3,at.y+3+(edge-6)*(1-fraction)},{at.x+edge-3,at.y+edge-3},IM_COL32(7,15,17,155),4);}
  draw->AddRect(at,{at.x+edge,at.y+edge+16*scale},blocked?red:failed?gold:fused||ready?fusion_color:evolved?gold:active>0?mint:border,8,0,blocked||failed||fused||ready||evolved?2.f:1.f);
  if(sealed){draw->AddRect({at.x+edge*.34f,at.y+edge*.32f},{at.x+edge*.66f,at.y+edge*.59f},red,3,0,2);draw->PathArcTo({at.x+edge*.5f,at.y+edge*.32f},edge*.11f,3.14159265f,6.2831853f,12);draw->PathStroke(red,0,2);}
  else if(smoked)draw->AddLine({at.x+edge*.25f,at.y+edge*.62f},{at.x+edge*.75f,at.y+edge*.22f},red,2);
  if(fused&&!legacy_visuals_){const int tier=Game::fusion_tier(skill);const ImU32 tone=tier==4?gold:tier==3?mint:fusion_color;draw->AddRect(at,{at.x+edge,at.y+edge+16*scale},tone,8,0,tier>=3?2.6f:1.8f);for(int n=0;n<tier-1;++n)draw->AddLine({at.x+edge-5.f-n*4,at.y+edge-3},{at.x+edge-5.f-n*4,at.y+edge-10},tone,2);}
  if(fusion_highlight_[int(skill)]>clock_&&!legacy_visuals_){float glow=std::min(1.f,(fusion_highlight_[int(skill)]-clock_)/1.4f);draw->AddRect({at.x-3,at.y-3},{at.x+edge+3,at.y+edge+16*scale+3},IM_COL32(250,217,155,int(220*glow)),8,0,2);}
  if(fused)label(draw,{at.x+edge-17*scale,at.y+3},fusion_color,std::to_string(Game::fusion_tier(skill)),12*scale);else if(evolved)label(draw,{at.x+edge-17*scale,at.y+3},gold,"进",12*scale);
  if(fused){for(const auto& completed:game_.scrolls.completed)if(completed.result==skill){const std::string form=Game::scroll_form_name(skill,completed.form);draw->AddRectFilled({at.x+3,at.y+3},{at.x+33*scale,at.y+17*scale},ink,3);label(draw,{at.x+5,at.y+3},fusion_color,form,10*scale);break;}}
  if(effective!=game_.skills[int(skill)])snprintf(value,sizeof(value),"%d→%d",game_.skills[int(skill)],effective);else snprintf(value,sizeof(value),"Lv.%d",game_.skills[int(skill)]);label(draw,{at.x+5,at.y+edge-15*scale},effective!=game_.skills[int(skill)]?red:text,value,12*scale);
  std::string status=active>0?"续 "+seconds(active):cooldown>0?seconds(cooldown):"就绪";
  if(skill==Skill::DragonArmor){snprintf(value,sizeof(value),"%.0f/100",std::clamp(game_.dragon_soul,0.f,100.f));status=value;draw->AddRectFilled({at.x+4,at.y+edge-3},{at.x+4+(edge-8)*std::clamp(game_.dragon_soul/100.f,0.f,1.f),at.y+edge},fusion_color,2);}
  if(skill==Skill::Bulwark){snprintf(value,sizeof(value),"%.0f盾 ",game_.guard_charge);status=value+(cooldown>0?seconds(cooldown):"就绪");}
  if(dash_skill(skill))status=cooldown>0?seconds(cooldown):"待冲刺";
  if(skill==Skill::FrostArmor){snprintf(value,sizeof(value),"寒甲 %.0f/60",frost_armor_energy(game_));status=value;draw->AddRectFilled({at.x+4,at.y+edge-3},{at.x+4+(edge-8)*std::clamp(frost_armor_energy(game_)/60.f,0.f,1.f),at.y+edge},mint,2);}
  if(skill==Skill::BattleFocus||skill==Skill::BattleBlades||skill==Skill::SpiritJudgement){snprintf(value,sizeof(value),"气 %.0f/100",std::clamp(game_.battle_focus,0.f,100.f));status=value;}
  if(sealed)status="封印 · 暂停";else if(smoked)status="烟幕禁攻";else if(effective==0)status="有效 0 级";else if(failed)status="施放失败";
  centered(draw,{at.x+edge*.5f,at.y+edge+1*scale},blocked?red:failed?gold:ready||fused?fusion_color:active>0?mint:muted,status,11*scale);
  if(hover(at,{edge,edge+16*scale},game_.state)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(std::min(390.f,size.x-48));skill_details(skill);ImGui::Separator();ImGui::TextDisabled("%s",dash_skill(skill)?"玩家冲刺时触发 · Esc 暂停可详细阅读":skill==Skill::FrostArmor?"受击积蓄 · Esc 暂停后可详细阅读":"自动施放 · Esc 暂停后可详细阅读");ImGui::PopTextWrapPos();ImGui::EndTooltip();}
 }
 const float xp_y=y+edge+24*scale;draw->AddRectFilled({x,xp_y},{x+width,xp_y+5*scale},IM_COL32(22,49,40,235),3);draw->AddRectFilled({x,xp_y},{x+width*std::clamp(game_.player.xp/std::max(1.f,game_.player.xp_next),0.f,1.f),xp_y+5*scale},mint,3);
 snprintf(value,sizeof(value),"经验 %.0f / %.0f",game_.player.xp,game_.player.xp_next);label(draw,{x,xp_y+9*scale},muted,value,11*scale);
 std::string dash=game_.player.dash_cooldown>0?"空格 冲刺 · "+seconds(game_.player.dash_cooldown*game_.dash_recovery_factor()):"空格 冲刺就绪";const auto dash_width=ImGui::GetFont()->CalcTextSizeA(11*scale,10000,0,dash.c_str()).x;label(draw,{x+width-dash_width,xp_y+9*scale},game_.player.dash_cooldown>0?muted:gold,dash,11*scale);
 label(draw,{16,size.y-44*scale},muted,"WASD / 方向键移动 · E互动",12*scale);label(draw,{16,size.y-25*scale},muted,"Esc 暂停 · 查看构筑",12*scale);
 if(show_stats_){snprintf(value,sizeof(value),"%.1f FPS  %.2f ms",ImGui::GetIO().Framerate,1000/std::max(1.f,ImGui::GetIO().Framerate));label(draw,{size.x-185*scale,size.y-44*scale},muted,value,12*scale);snprintf(value,sizeof(value),"敌人 %zu · 弹道 %zu",game_.enemies.size(),game_.projectiles.size());label(draw,{size.x-185*scale,size.y-25*scale},muted,value,12*scale);}
}

void App::guide_reference(){
 ImGui::TextWrapped("WASD 或方向键移动，空格冲刺，E 互动。Esc 暂停；升级时 Esc 跳过，替换时 Esc 取消。失焦或最小化会暂停，不补算后台战斗。");
 static const char* tips[]={
  "八技能槽：起手只决定初始外观和技能，公共池自由学习。",
  "五选二：每次升级可选零至两项，每级一次免费重置，已经选择的效果保留。",
  "满槽替换：新技能仍会出现；先预览再确认，取消不消耗选择。",
  "自动融合：配方材料的真实等级均达 Lv.5 即自动融合，不需要秘卷或额外确认。",
  "真实与有效等级：退化只暂时降低效果；真实等级仍用于融合，离开区域恢复。",
  "装备：拾取即生效，无持有上限。同定义聚合数量，在构筑页查看累计与下一件收益。",
  "紧凑2×：经典关卡由约20分钟压缩到约10分钟，移动、技能冷却、预警和音乐保持正常速度。"
 };
 for(const auto* tip:tips){ImGui::Separator();ImGui::TextWrapped("%s",tip);}
}

void App::guidance_panel(){
 if(!guidance_||guide_remaining_<=0||game_.state!=State::Playing)return;
 const auto size=ImGui::GetIO().DisplaySize;const float width=std::min(440.f,size.x*.42f);
 ImGui::SetNextWindowPos({size.x-18,size.y-148*ui_scale()},ImGuiCond_Always,{1.f,1.f});
 ImGui::SetNextWindowSize({width,0});ImGui::SetNextWindowBgAlpha(.93f);
 ImGui::Begin("首次玩法提示",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing);
 ImGui::SetWindowFontScale(.8f);
 ImGui::TextColored(ui::color(ui::accent),"%s",guide_title_.c_str());
 ImGui::TextWrapped("%s",guide_text_.c_str());
 if(ImGui::SmallButton("知道了"))guide_remaining_=0;ImGui::SameLine();
 if(ImGui::SmallButton("关闭首次提示")){guidance_=false;guide_remaining_=0;settings_dirty_=true;settings_save_delay_=.2f;}
 ImGui::End();
}

void App::combat_recap(){
 const auto& records=game_.combat_records;if(records.empty()){ImGui::TextDisabled("没有可用的最近战斗记录。");return;}
 auto status=[](uint32_t bits){std::string value;const char* names[]={"烟幕禁攻","装备压制","有效等级降低","技能封印"};for(int i=0;i<4;++i)if(bits&(1u<<i)){if(!value.empty())value+="、";value+=names[i];}return value.empty()?std::string("无上述压制记录"):value;};
 const CombatRecord* last=nullptr;std::map<std::string,float> sums;float shield=0,cost=0;int overlapping=0;float previous=-100;
 for(const auto& record:records){
  if(record.kind==CombatRecordKind::Damage&&record.amount>0){last=&record;sums[record.source]+=record.amount;if(record.time-previous<=.75f)++overlapping;previous=record.time;}
  if(record.kind==CombatRecordKind::Shield)shield+=record.amount;
  if(record.kind==CombatRecordKind::HealthCost)cost+=record.amount;
 }
 ImGui::TextColored(ui::color(ui::danger),"最近8战斗秒 · 受击复盘");
 if(last){ImGui::TextWrapped("最后有效受伤：%s · 生命减少 %.0f",last->source.c_str(),last->amount);ImGui::TextWrapped("受击当时：%s",status(last->states).c_str());}
 std::vector<std::pair<std::string,float>> ranked(sums.begin(),sums.end());std::stable_sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){return a.second>b.second;});
 for(size_t i=0;i<std::min(size_t(3),ranked.size());++i)ImGui::TextWrapped("%s：实际受伤 %.0f",ranked[i].first.c_str(),ranked[i].second);
 if(shield>0||cost>0)ImGui::Text("实际吸收 %.0f · 自付生命 %.0f",shield,cost);
 if(overlapping>0)ImGui::TextWrapped("记录到 %d 组间隔不超过0.75秒的连续受伤。",overlapping);
 if(ImGui::TreeNode("展开最近事件")){
  static const char* kinds[]={"受伤","吸收","状态","施放失败","生命成本","阶段"};
  for(const auto& record:records){ImGui::TextWrapped("前 %.1fs · %s · %s",std::max(0.f,game_.combat_elapsed-record.time),kinds[int(record.kind)],record.source.c_str());
   if(record.kind==CombatRecordKind::Damage||record.kind==CombatRecordKind::Shield||record.kind==CombatRecordKind::HealthCost)ImGui::Text("实际量 %.0f · 生命 %.0f · 位置 (%.0f, %.0f)",record.amount,record.hp,record.position.x,record.position.y);
   ImGui::TextWrapped("%s",status(record.states).c_str());
  }ImGui::TreePop();
 }
 ImGui::Separator();
}

void App::menus(){
 const auto size=ImGui::GetIO().DisplaySize;const auto state=game_.state;if(state==State::Playing)return;
 ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},size,IM_COL32(4,13,9,170));
 const float width=state==State::Menu?std::min(960.f,size.x-32):state==State::Upgrade?std::min(game_.pending_study.active?940.f:1690.f,size.x-40):std::min(960.f,size.x-40);
 ImGui::SetNextWindowPos({size.x*.5f,size.y*.5f},ImGuiCond_Always,{.5f,.5f});
 const bool fixed=state!=State::ChapterComplete;
 ImGui::SetNextWindowSize({width,fixed?std::min(state==State::Menu?760.f:660.f,size.y-44):0.f});
 ImGui::SetNextWindowSizeConstraints({width,0},{width,size.y-36});
 ImGui::Begin("枫林幸存者",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|(fixed?0:ImGuiWindowFlags_AlwaysAutoResize));
 if(settings_)settings_menu();
 else if(state==State::Menu)camp_menu();
 else if(state==State::ChapterComplete)chapter_menu();
 else if(state==State::Upgrade&&game_.pending_study.active)study_replacement_menu();
 else if(state==State::Upgrade){upgrade_menu(); }else if(state==State::Paused){
  ImGui::TextColored(ui::color(ui::accent),"冒险暂歇");ImGui::SameLine();ImGui::TextDisabled("Esc继续 · 战斗已暂停");
  ImGui::BeginDisabled(pending_settlement_.has_value());if(ui::primary_button("继续冒险",{ImGui::GetContentRegionAvail().x*.55f,36}))game_.toggle_pause();ImGui::EndDisabled();ImGui::SameLine();
  if(ImGui::Button("画面与声音设置",{-1,36}))settings_=true;
  ImGui::TextUnformatted("关卡节奏");ImGui::SameLine();if(ImGui::RadioButton("标准1×",game_speed()==1))set_game_speed(1);ImGui::SameLine();if(ImGui::RadioButton("紧凑2×",game_speed()==2))set_game_speed(2);ImGui::SameLine();ImGui::TextDisabled("进度 %s",clock_time(game_.total_elapsed()).c_str());
  passive_panel();ImGui::Separator();
  ImGui::TextWrapped(game_.run_mode()==RunMode::Campaign?"远征仅在章节补给点保存。结束并结算会关闭本次远征。":"结束冒险会结算已完成的进度。直接关闭程序不保存当前战场。");
  if(ImGui::Button("结束冒险并返回营地",{-1,36})){if(settle_adventure(RunOutcome::Abandoned))return_to_camp();}
  if(pending_settlement_&&!settlement_ok_&&ImGui::Button("重新加载存档并重试结算",{-1,36})){if(settle_adventure(RunOutcome::Abandoned,true))return_to_camp();}
  if(!camp_notice_.empty())ImGui::TextWrapped("%s",camp_notice_.c_str());
 }else{
  const char* result=state!=State::Victory?"冒险结束":game_.run_mode()==RunMode::Campaign?"远征完成":game_.boss_defeated?"Boss 击破":"生存成功";
  ImGui::SetWindowFontScale(1.45f);ImGui::TextUnformatted(result);ImGui::SetWindowFontScale(1);
  ImGui::Text("Lv.%d · 击杀%d · Boss击败%d · %s",game_.player.level,game_.kills,game_.metrics.bosses_defeated,game_.timing.speed==2?"紧凑2×":"标准1×");
  ImGui::Text("战斗 %s · 枫叶结晶 +%d",clock_time(game_.timing.combat_seconds).c_str(),settlement_reward_);
  ImGui::Text("局内装备 %llu 件 · %zu 种效果记录",static_cast<unsigned long long>(game_.gear_count()),game_.run_gear.size());
  if(ImGui::CollapsingHeader("用时与结算说明")){timing_view(game_.timing,game_.total_elapsed());ImGui::Text("种子 %u",run_seed_);ImGui::TextWrapped("%s",camp_notice_.c_str());}
  if(game_.combat_records.empty()&&!game_.pressure_metrics.death_cause.empty()&&state==State::Defeat)ImGui::TextWrapped("最后受击：%s",game_.pressure_metrics.death_cause.c_str());
  ImGui::BeginChild("本局构筑回顾",{0,std::max(160.f,ImGui::GetContentRegionAvail().y-142)},ImGuiChildFlags_Borders);
  if(state==State::Defeat)combat_recap();
  std::string defeated;for(int i=0;i<boss_profile_count;++i)if(game_.metrics.content.boss_kills[i]>0){if(!defeated.empty())defeated+=" · ";defeated+=Game::boss_profile_name(BossProfile(i));}if(!defeated.empty())ImGui::TextWrapped("击败首领：%s",defeated.c_str());
  ImGui::TextColored(ui::color(ui::ready),"最终构筑");std::string build;for(Skill skill:game_.skill_slots())if(skill!=Skill::Count){if(!build.empty())build+=" · ";build+=std::string(Game::skill_name(skill))+" Lv."+std::to_string(game_.skills[int(skill)]);}ImGui::TextWrapped("%s",build.c_str());
  ImGui::Text("完成融合 %d 次",game_.metrics.fusions_created);for(const auto& completed:game_.scrolls.completed)ImGui::TextWrapped("%s · %s",Game::skill_name(completed.result),Game::scroll_form_name(completed.result,completed.form));
  if(!game_.run_gear.empty()&&ImGui::CollapsingHeader("本局装备明细"))for(const auto& stack:game_.run_gear)if(const auto* def=gear_definition(stack.id))ImGui::TextWrapped("%s · Lv.%d ×%llu · %s",def->name,stack.level,static_cast<unsigned long long>(stack.count),def->description);
  std::vector<int> damage_ranked;for(int i=0;i<skill_count;++i)if(game_.metrics.skill_damage[i]>0)damage_ranked.push_back(i);std::stable_sort(damage_ranked.begin(),damage_ranked.end(),[&](int a,int b){return game_.metrics.skill_damage[a]>game_.metrics.skill_damage[b];});
  ImGui::Separator();ImGui::TextUnformatted("主要技能贡献");for(size_t i=0;i<std::min(size_t(3),damage_ranked.size());++i)ImGui::TextWrapped("%s · 实际伤害 %.0f",Game::skill_name(Skill(damage_ranked[i])),game_.metrics.skill_damage[damage_ranked[i]]);
  if(damage_ranked.size()>3&&ImGui::TreeNode("其他技能贡献")){for(size_t i=3;i<damage_ranked.size();++i)ImGui::TextWrapped("%s · 实际伤害 %.0f",Game::skill_name(Skill(damage_ranked[i])),game_.metrics.skill_damage[damage_ranked[i]]);ImGui::TreePop();}
  std::vector<int> ranked;for(int i=0;i<resonance_count;++i)if(game_.metrics.resonance.triggers[i])ranked.push_back(i);std::stable_sort(ranked.begin(),ranked.end(),[&](int a,int b){return game_.metrics.resonance.damage[a]+game_.metrics.resonance.shield[a]>game_.metrics.resonance.damage[b]+game_.metrics.resonance.shield[b];});
  ImGui::Separator();ImGui::TextUnformatted("主要共鸣贡献");if(ranked.empty())ImGui::TextDisabled("本局尚未触发共鸣。");for(int id:ranked){ImGui::TextColored(ui::color(ui::ready),"%s · %s",resonance_definitions()[id].name,game_.resonance_direction_name(Resonance(id)));ImGui::TextWrapped("%d次 · 实际伤害 %.0f · 吸收 %.0f · 辅助 %.0f",game_.metrics.resonance.triggers[id],game_.metrics.resonance.damage[id],game_.metrics.resonance.shield[id],game_.metrics.resonance.support[id]);}
  ImGui::EndChild();

  if(!settlement_ok_){ImGui::TextWrapped("结算尚未保存，请重试后再开始下一局。");if(ImGui::Button("重新加载存档并重试结算",{-1,40}))settle_adventure(state==State::Victory?RunOutcome::Victory:RunOutcome::Defeat,true);}
  else{if(ui::primary_button("返回冒险营地",{-1,38}))return_to_camp();float button_width=(ImGui::GetContentRegionAvail().x-12)*.5f;if(ImGui::Button(game_.run_mode()==RunMode::Daily?"再次挑战今日固定种子":"新冒险 · 新种子",{button_width,34}))begin_adventure();if(game_.run_mode()!=RunMode::Daily){ImGui::SameLine();if(ImGui::Button("重试本局种子",{-1,34}))begin_adventure(true);}ImGui::TextDisabled("相同种子和操作可重现随机进程。");}
 }

 ImGui::End();
}
}

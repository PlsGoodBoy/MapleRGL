#include "app.hpp"
#include "boss_combat.hpp"
#include <algorithm>
namespace maple {
static const char* rarity_name(int value){return value>=3?"史诗":value==2?"稀有":"普通";}
static const char* class_name(int value){static const char* names[]={"弓术联动","法术联动","战技联动"};return value>=0&&value<3?names[value]:"通用";}
static std::string relic_icon(RelicId id){static const int keys[]={8,4,2,0,0,5,1,3,11,9,6,10};return "relic.icon."+std::to_string(keys[std::clamp(int(id),0,relic_count-1)]);}
static std::string equipment_icon(EquipmentId id){static const int keys[]={0,0,3,8,1,2,0,6,3,7};if(id==EquipmentId::MagnetPendant)return "relic.icon.7";return "equipment.icon."+std::to_string(keys[std::clamp(int(id),0,equipment_count-1)]);}
static std::string unlock_condition(EquipmentId equipment,RelicId relic){
 for(const auto& def:achievement_definitions())if((equipment!=EquipmentId::None&&def.equipment_reward==equipment)||(relic!=RelicId::Count&&(def.relic_reward_mask&(1u<<int(relic)))))return std::string(def.name)+"："+def.description;
 return "初始可用";
}
void App::collection_menu(){
 const auto& profile=progression_->profile();
 auto draw_icon=[&](const std::string& key){auto at=ImGui::GetCursorScreenPos();ImGui::Dummy({46,44});sprite(ImGui::GetWindowDrawList(),key,{at.x+22,at.y+22},1.2f,clock_);ImGui::SameLine();};
 if(camp_tab_==3){
  ImGui::TextWrapped("装备只在营地调整。出发时固定为本次冒险快照，已保存的远征不会被营地改装影响。");
  static const char* slots[]={"武器","防具","饰品"};static const char* classes[]={"弓起手","法杖起手","剑起手"};
  ImGui::Text("起始方案：%s · 装备均向冒险家开放",classes[selected_]);for(int i=0;i<3;++i){if(i)ImGui::SameLine();ImGui::RadioButton(classes[i],&selected_,i);}
  for(int slot=0;slot<equipment_slot_count;++slot){auto slot_id=EquipmentSlot(slot);auto current=progression_->equipped(slot_id);ImGui::Separator();ImGui::TextColored({.97f,.8f,.44f,1},"%s · 已装备：%s",slots[slot],equipment_definitions()[int(current)].name);
   ImGui::BeginDisabled(progression_->read_only()||profile.active_run.has_value());if(current!=EquipmentId::None){ImGui::PushID(slot);if(ImGui::SmallButton("卸下")){auto result=progression_->equip(slot_id,EquipmentId::None,CharacterClass(selected_),current);camp_notice_=result.ok?"装备配置已保存。":result.message;}ImGui::PopID();}ImGui::EndDisabled();
   for(const auto& def:equipment_definitions()){if(def.id==EquipmentId::None||def.slot!=slot_id)continue;ImGui::PushID(int(def.id));draw_icon(equipment_icon(def.id));ImGui::BeginGroup();bool owned=progression_->owns_equipment(def.id),allowed=equipment_allowed(def.id,slot_id,CharacterClass(selected_));ImGui::Text("%s · %s · %s",def.name,rarity_name(def.rarity),class_name(def.allowed_class));ImGui::TextWrapped("%s",def.description);
    if(!owned)ImGui::TextWrapped("解锁：%s",unlock_condition(def.id,RelicId::Count).c_str());
    ImGui::BeginDisabled(!owned||!allowed||def.id==current||progression_->read_only()||profile.active_run.has_value());if(ImGui::SmallButton(def.id==current?"已装备":owned?(allowed?"装备":"职业不符"):"未解锁")){auto result=progression_->equip(slot_id,def.id,CharacterClass(selected_),current);camp_notice_=result.ok?std::string("已装备：")+def.name:result.message;}ImGui::EndDisabled();ImGui::EndGroup();ImGui::PopID();
   }
  }
 }else if(camp_tab_==4){
  ImGui::TextWrapped("收藏表示可在下一局获得，不会开局自动携带。遗物仅保留在当前冒险/远征，按职业过滤，不能重复获得同一件。");
  for(const auto& def:relic_definitions()){ImGui::PushID(int(def.id));draw_icon(relic_icon(def.id));ImGui::BeginGroup();bool unlocked=progression_->relic_unlocked(def.id);ImGui::TextColored(unlocked?ImVec4(.97f,.8f,.44f,1):ImVec4(.6f,.65f,.6f,1),"%s · %s · %s",def.name,rarity_name(def.rarity),class_name(def.allowed_class));ImGui::TextWrapped("%s",def.description);ImGui::TextWrapped("%s",unlocked?"已解锁：可在新冒险奖励中出现。":("解锁："+unlock_condition(EquipmentId::None,def.id)).c_str());ImGui::EndGroup();ImGui::Separator();ImGui::PopID();}
 }else if(camp_tab_==5){
  ImGui::TextWrapped("目标根据真实冒险记录自动结算，奖励只发放一次。章节奖励会立即更新进度，下一局可使用新解锁内容。");
  for(const auto& status:progression_->achievement_status()){const auto& def=achievement_definitions()[int(status.id)];ImGui::PushID(int(status.id));ImGui::TextColored(status.completed?ImVec4(.6f,.92f,.7f,1):ImVec4(.97f,.8f,.44f,1),"%s%s",status.completed?"已完成 · ":"",def.name);ImGui::TextWrapped("%s",def.description);std::string value=std::to_string(std::min(status.progress,status.target))+" / "+std::to_string(status.target);ImGui::ProgressBar(status.target?std::clamp(float(status.progress)/float(status.target),0.f,1.f):1.f,{-1,18},value.c_str());ImGui::TextWrapped("奖励：%s",achievement_reward_text(status.id).c_str());ImGui::Separator();ImGui::PopID();}
 }
 if(camp_tab_==6){
  const auto& content=profile.lifetime_content;
  ImGui::TextColored({.97f,.8f,.44f,1},"首领图鉴 · 战斗观察");
  ImGui::Text("正式内容：%d 位首领 · %d 种怪物",boss_profile_count,enemy_visual_count);
  ImGui::TextWrapped("战绩来自实际结算，不限制技能或融合。旧版本只保留存档中仍有的战绩；未记录的数据不补算。");
  for(int id=0;id<boss_profile_count;++id){
   const auto boss=BossProfile(id);const auto& definition=boss_combat_definition(boss);const bool seen=(content.bosses_seen&(1u<<id))||content.boss_kills[id]>0;
   ImGui::PushID(id);const auto at=ImGui::GetCursorScreenPos();ui_icon(ImGui::GetWindowDrawList(),std::string(Game::boss_sprite_key(boss))+".idle",at,46,seen?0xffffffff:IM_COL32(190,190,190,175));ImGui::Dummy({54,48});ImGui::SameLine();
   ImGui::BeginGroup();const bool open=ImGui::TreeNode("boss","%s%s",Game::boss_profile_name(boss),seen?"":" · 尚未遇见");
   ImGui::Text("已记录遭遇 %d · 实际击败 %d",content.boss_encounters[id],content.boss_kills[id]);
   if(open){
    ImGui::TextWrapped("核心机制：%s",definition.mechanism);
    ImGui::TextColored({.60f,.90f,.76f,1},"应对提示");ImGui::TextWrapped("%s",definition.counterplay);
    for(int move=0;move<3;++move){const auto& attack=definition.moves[move];ImGui::TextWrapped("%s%s · 起手 %.1fs · 收招 %.1fs",(content.boss_moves_seen[id]&(1<<move))?"已观察 · ":"待观察 · ",attack.name,attack.windup,attack.recovery);}
    ImGui::Text("阶段变化：生命 %.0f%% / %.0f%%",definition.phase_two*100,definition.phase_three*100);
    if(seen&&content.boss_encounters[id]==0)ImGui::TextDisabled("旧记录留有战绩，尚未记录本版本的遭遇次数。");
    ImGui::TreePop();
   }
   ImGui::EndGroup();ImGui::Separator();ImGui::PopID();
  }
  if(ImGui::CollapsingHeader("怪物种类 · 已记录累计")){
   if(ImGui::BeginTable("enemy_collection",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp)){
    ImGui::TableSetupColumn("外观",ImGuiTableColumnFlags_WidthFixed,54);ImGui::TableSetupColumn("种类");ImGui::TableSetupColumn("战绩",ImGuiTableColumnFlags_WidthFixed,130);ImGui::TableHeadersRow();
    for(int id=0;id<enemy_visual_count;++id){const bool seen=(content.enemies_seen&(uint64_t(1)<<id))||content.enemy_kills[id]>0;ImGui::TableNextRow();ImGui::TableNextColumn();const auto at=ImGui::GetCursorScreenPos();
     ui_icon(ImGui::GetWindowDrawList(),std::string(Game::enemy_visual_key(id))+".idle",at,42,seen?0xffffffff:IM_COL32(190,190,190,175));ImGui::Dummy({48,46});
     ImGui::TableNextColumn();ImGui::TextWrapped("%s",Game::enemy_visual_name(id));ImGui::TableNextColumn();ImGui::Text("%s · %d",seen?"击杀":"尚未遇见",content.enemy_kills[id]);
    }ImGui::EndTable();
   }
  }
  if(ImGui::CollapsingHeader("行为分类 · 历史累计"))bestiary_menu();
 }

}
void App::loadout_sidebar(){
 ImGui::Separator();ImGui::TextUnformatted("本局装备");bool equipped=false;for(auto id:game_.run_setup().equipment)if(id!=EquipmentId::None){equipped=true;const auto& def=equipment_definitions()[int(id)];ImGui::TextWrapped("%s",def.name);if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::PushTextWrapPos(280);ImGui::TextUnformatted(def.description);ImGui::PopTextWrapPos();ImGui::EndTooltip();}}
 if(!equipped)ImGui::TextDisabled("未携带装备");ImGui::Separator();ImGui::TextUnformatted("本局遗物");bool any=false;for(const auto& def:relic_definitions())if(game_.owns_relic(def.id)){any=true;ImGui::TextColored({.97f,.8f,.44f,1},"%s · %s",def.name,rarity_name(def.rarity));ImGui::TextWrapped("%s",def.description);}
 if(!any)ImGui::TextWrapped("完成事件或击败精英，寻找遗物奖励。");
}
}

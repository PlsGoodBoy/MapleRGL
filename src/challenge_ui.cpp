#include "app.hpp"
#include <windows.h>
#include <algorithm>
namespace maple {
static std::string today(){SYSTEMTIME t;GetLocalTime(&t);char date[16];snprintf(date,sizeof(date),"%04d-%02d-%02d",t.wYear,t.wMonth,t.wDay);return date;}
void App::adventure_options(){
 ImGui::Separator();ImGui::TextUnformatted("关卡节奏");ImGui::SameLine();if(ImGui::RadioButton("标准1×",game_speed()==1))set_game_speed(1);ImGui::SameLine();if(ImGui::RadioButton("紧凑2×",game_speed()==2))set_game_speed(2);
 ImGui::TextWrapped("经典标准1×约20分钟，紧凑2×约10分钟有效战斗；远征每章和每日仍为10/5分钟。加快出怪与关卡进程，不加快移动、动作、弹道和冷却；暂停与选卡不计时。");
 ImGui::Separator();for(int i=0;i<4;++i){if(i%2)ImGui::SameLine();bool available=progression_->mode_unlocked(RunMode(i));ImGui::BeginDisabled(!available);ImGui::RadioButton(Game::mode_name(RunMode(i)),&selected_mode_,i);ImGui::EndDisabled();if(!available)ImGui::TextWrapped("%s",progression_->mode_unlock_requirement(RunMode(i)).c_str());}
 if(selected_mode_==int(RunMode::Daily)){
  auto spec=make_daily_spec(today());if(spec.valid){selected_=int(spec.character);ImGui::TextColored({.97f,.8f,.44f,1},"本地每日挑战 · %s",spec.date.c_str());ImGui::TextWrapped("%s",spec.rules.c_str());ImGui::TextWrapped("固定种子、起始方案、装备与遗物条件，不带营地天赋。首次当日胜利获得8结晶，重复挑战只更新本地最佳；普通冒险仍是主要资源来源。");auto record=progression_->daily_record(spec.date);if(record)ImGui::Text("今日挑战 %llu 次 / 胜利 %llu 次 / 最佳 %llu 分",static_cast<unsigned long long>(record->attempts),static_cast<unsigned long long>(record->wins),static_cast<unsigned long long>(record->best.best_score));}
  else ImGui::TextWrapped("日期不可用：%s",spec.error.c_str());
 }else{
  if(!progression_->difficulty_unlocked(Difficulty(selected_difficulty_)))selected_difficulty_=0;
  for(int i=0;i<3;++i){if(i)ImGui::SameLine();bool available=progression_->difficulty_unlocked(Difficulty(i));ImGui::BeginDisabled(!available);ImGui::RadioButton(Game::difficulty_name(Difficulty(i)),&selected_difficulty_,i);ImGui::EndDisabled();if(!available&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",progression_->difficulty_unlock_requirement(Difficulty(i)).c_str());}
  if(selected_mode_==int(RunMode::Campaign))ImGui::TextWrapped("射手村 → 魔法密林 → 勇士部落。每章保留构筑，可在章节边界保存退出。战斗中关闭后从上一已完成章补给点继续。");
  else if(selected_mode_==int(RunMode::Endless))ImGui::TextWrapped("完成首轮十分钟目标后继续生存，特殊怪、危险场地与周期首领持续变化。随时在暂停菜单结束结算；无尽不保存战斗中途进度。");
  else ImGui::TextWrapped("十分钟单章：击败首领或坚持生存即可胜利。");
  if(selected_difficulty_==1)ImGui::TextWrapped("困难：精英更早、攻击节奏更快、治疗减弱，首领获得额外攻击。");
  if(selected_difficulty_==2)ImGui::TextWrapped("噩梦：更密集特殊敌人与更紧凑预警，治疗进一步受限，首领交错招式考验走位。");
 }
}
void App::records_summary(){
 const auto& records=progression_->profile().records;ImGui::Text("累计游戏时间 %.1f 小时",records.total_elapsed/3600.0);ImGui::Text("起手场次（含旧职业记录）：弓 %llu / 法杖 %llu / 剑 %llu",static_cast<unsigned long long>(records.class_runs[0]),static_cast<unsigned long long>(records.class_runs[1]),static_cast<unsigned long long>(records.class_runs[2]));int owned=0,relics=0,achievements=0;for(const auto& d:equipment_definitions())if(d.id!=EquipmentId::None&&progression_->owns_equipment(d.id))++owned;for(const auto& d:relic_definitions())if(progression_->relic_unlocked(d.id))++relics;for(const auto& a:progression_->achievement_status())if(a.completed)++achievements;ImGui::Text("收藏进度：装备 %d/9 · 遗物 %d/12 · 目标 %d/20",owned,relics,achievements);
 ImGui::TextUnformatted("历史累计最佳（跨规则版本）");bool any=false;static const char* classes[]={"弓起手 / 旧射手","法杖起手 / 旧法师","剑起手 / 旧战士"};
 for(int mode=0;mode<4;++mode)for(int difficulty=0;difficulty<3;++difficulty)for(int cls=0;cls<3;++cls){auto best=progression_->run_best(CharacterClass(cls),Difficulty(difficulty),RunMode(mode));if(!best.recorded)continue;any=true;ImGui::TextColored({.97f,.8f,.44f,1},"%s / %s / %s",classes[cls],Game::mode_name(RunMode(mode)),Game::difficulty_name(Difficulty(difficulty)));ImGui::TextWrapped("最长 %.0fs · 最高 Lv.%d · 最多击杀 %llu · 首领 %llu · 最高伤害 %.0f",best.longest,best.level,static_cast<unsigned long long>(best.kills),static_cast<unsigned long long>(best.bosses),best.damage);}
 if(!any)ImGui::TextDisabled("完成冒险后记录真实成绩。不同指标可能来自不同场次。");ImGui::Separator();
}
void App::bestiary_menu(){
 static const std::array<const char*,enemy_kind_count> keys={"slime","mushroom","blue_mushroom","pig","elite","boss","blue_mushroom","slime","elite","slime","enemy.zombie","enemy.drake"};
 static const std::array<const char*,enemy_kind_count> regions={"射手村 / 魔法密林","射手村","勇士部落","射手村 / 勇士部落","各地精英","各地区首领","魔法密林","魔法密林","魔法密林","分裂怪召唤","魔法密林 / 勇士部落","魔法密林 / 勇士部落"};
 ImGui::TextWrapped("按敌人的战斗行为累计遇见和击杀，配图仅为代表外观；具体首领与怪物种类的 ID 明细见最近一次冒险。");
 for(const auto& entry:progression_->bestiary()){int index=int(entry.kind);ImGui::PushID(index);ImGui::BeginChild("enemy",{-1,135},ImGuiChildFlags_Borders);auto pos=ImGui::GetCursorScreenPos();ImGui::Dummy({115,100});if(entry.seen)sprite(ImGui::GetWindowDrawList(),std::string(keys[index])+".move",{pos.x+56,pos.y+90},index==5?.8f:1.1f,clock_);else ImGui::GetWindowDrawList()->AddText({pos.x+30,pos.y+40},IM_COL32(140,155,140,255),"未知");ImGui::SameLine();ImGui::BeginGroup();ImGui::TextColored({.97f,.8f,.44f,1},"%s",entry.seen?Game::enemy_name(entry.kind):"尚未遇见的怪物");if(entry.seen){ImGui::Text("地区：%s · 击杀 %llu",regions[index],static_cast<unsigned long long>(entry.kills));ImGui::TextWrapped("%s",Game::enemy_description(entry.kind));}else ImGui::TextWrapped("继续探索各地区、精英和特殊敌人。");ImGui::EndGroup();ImGui::EndChild();ImGui::PopID();}
}
}

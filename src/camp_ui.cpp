#include "app.hpp"
#include "ui_theme.hpp"
#include <algorithm>
#include <filesystem>
#include <windows.h>
namespace maple {
static std::string local_date(){SYSTEMTIME t;GetLocalTime(&t);char value[16];snprintf(value,sizeof(value),"%04d-%02d-%02d",t.wYear,t.wMonth,t.wDay);return value;}
static std::string recorded_duration(double value){const auto total=static_cast<unsigned long long>(std::max(0.0,value));char out[48];snprintf(out,sizeof(out),"%02llu:%02llu",total/60,total%60);return out;}
static void record_timing(const RunTiming& timing){
 ImGui::TextDisabled(timing.rules_version>=2?"当前规则（经典20分钟）":"历史规则（经典10分钟）");
 if(!timing.recorded){ImGui::TextDisabled("现实用时 / 倍率：未记录");return;}
 const char* speeds=timing.used_speeds==3?"混合 1× / 2×":timing.used_speeds==2?"2×":timing.used_speeds==1?"1×":"尚未进入战斗";
 ImGui::TextWrapped("有效战斗现实时间 %s · 总现实时间 %s · %s",recorded_duration(timing.effective_real_seconds).c_str(),recorded_duration(timing.total_real_seconds).c_str(),speeds);
}
void App::initialize_progression(){
 std::filesystem::path root=profile_root_override_;
 if(root.empty()&&diagnostics_)root=std::filesystem::temp_directory_path()/(L"MapleRGL-diagnostic-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
 progression_=std::make_unique<ProgressionStore>(root);
 const auto& settings=progression_->profile().settings;
 sound_=settings.sound;ui_sound_=settings.ui_sound;ui_volume_=settings.ui_volume;music_enabled_=settings.music;volume_=settings.sound_volume;music_volume_=settings.music_volume;zoom_=settings.zoom;shake_enabled_=settings.shake;show_stats_=settings.show_stats;friendly_effect_alpha_=settings.friendly_effect_alpha;shake_strength_=settings.shake_strength;damage_number_density_=settings.damage_number_density;low_effects_=settings.low_effects;selected_game_speed_=settings.game_speed==2?2:1;
 flash_strength_=settings.flash_strength;ui_scale_=settings.ui_scale;guidance_=settings.guidance;guide_seen_=settings.guide_seen;boss_hints_seen_=settings.boss_hints_seen;apply_ui_scale();
 if(progression_->load_state()==ProfileLoadState::RecoveredBackup)camp_notice_="主存档异常，已从备份恢复。永久成长已保留。";
 else if(progression_->read_only())camp_notice_="存档暂时只读："+progression_->last_error();
 else if(progression_->profile().active_run)camp_notice_="上次战斗未结算。开启新冒险会结束该次战斗，已有永久成长不受影响。";
}
void App::save_profile_settings(){
 if(!progression_||profiling){settings_dirty_=false;return;}
 ProfileSettings s;s.sound=sound_;s.ui_sound=ui_sound_;s.ui_volume=ui_volume_;s.music=music_enabled_;s.sound_volume=volume_;s.music_volume=music_volume_;s.zoom=zoom_;s.shake=shake_enabled_;s.show_stats=show_stats_;s.friendly_effect_alpha=friendly_effect_alpha_;s.shake_strength=shake_strength_;s.damage_number_density=damage_number_density_;s.low_effects=low_effects_;s.game_speed=selected_game_speed_;
 s.flash_strength=flash_strength_;s.ui_scale=ui_scale_;s.guidance=guidance_;s.guide_seen=guide_seen_;s.boss_hints_seen=boss_hints_seen_;
 auto r=progression_->save_settings(s);settings_dirty_=false;if(!r.ok)camp_notice_="设置保存失败："+r.message;
}
bool App::begin_adventure(bool replay){
 if(!progression_)return false;if(settings_dirty_)save_profile_settings();
 uint32_t seed=replay?run_seed_:uint32_t(GetTickCount64());if(!replay&&seed==run_seed_)++seed;
 auto r=selected_mode_==int(RunMode::Daily)?progression_->begin_daily(local_date()):progression_->begin_run(CharacterClass(selected_),seed,RunMode(selected_mode_),Difficulty(selected_difficulty_));
 if(!r.ok){camp_notice_="无法开始并保存冒险："+r.message;return false;}
 const auto active=*progression_->profile().active_run;selected_=int(active.character);pending_settlement_.reset();run_id_=r.run_id;run_seed_=active.seed;settlement_attempted_=false;settlement_ok_=false;settlement_reward_=0;camp_notice_.clear();settings_=false;
 music_.end_audition();chapter_save_attempted_=chapter_saved_=false;game_.start(active.character,active.seed,active.bonuses,active.setup);game_.timing.speed=selected_game_speed_;game_.timing.recorded=true;prepare_chapter_resources();return true;
}
bool App::settle_adventure(RunOutcome outcome,bool reload){
 if(profiling&&!complete_run_){settlement_attempted_=settlement_ok_=true;settlement_reward_=0;return true;}
 settlement_attempted_=true;
 if(!progression_||!run_id_){camp_notice_="没有可结算的冒险记录。";settlement_ok_=false;return false;}
 if(!pending_settlement_)pending_settlement_=make_run_report(game_,outcome,run_seed_);
 if(reload){progression_->load();if(progression_->read_only()){camp_notice_="重新加载失败，当前结算仍保留："+progression_->last_error();return false;}}
 // The store validates the original run ID and its closed receipt before issuing currency.
 auto r=progression_->settle(run_id_,*pending_settlement_);settlement_ok_=r.ok;
 if(r.ok){settlement_reward_=r.crystals_awarded;camp_notice_=r.already_claimed?"本次冒险已结算，不会重复发放奖励。":"奖励和冒险记录已保存。返回营地可购买永久天赋。";}
 else camp_notice_="保存结算失败："+r.message;
 return r.ok;
}
void App::return_to_camp(){
 if(progression_&&game_.state==State::ChapterComplete&&chapter_saved_&&progression_->profile().active_run&&progression_->profile().active_run->id==run_id_){auto saved=progression_->update_checkpoint_timing(run_id_,game_.timing);if(!saved.ok){camp_notice_="章间用时尚未保存，请重试："+saved.message;return;}}
 pending_settlement_.reset();if(settings_dirty_)save_profile_settings();game_.reset_to_menu();camp_tab_=0;settings_=false;run_id_=0;
}
void App::camp_menu(){
 const auto& p=progression_->profile();
 ImGui::TextColored({.97f,.8f,.44f,1},"枫林冒险营地");ImGui::SameLine();ImGui::Text("枫叶结晶 %lld",static_cast<long long>(p.crystals));
 ImGui::TextDisabled("营地成长会永久保留；每次冒险重新组合局内技能。");
 const char* tabs[]={"出发冒险","天赋成长","冒险记录","装备管理","遗物收藏","任务成就","怪物图鉴"};for(int i=0;i<7;++i){if(i%4)ImGui::SameLine();if(ImGui::Selectable(tabs[i],camp_tab_==i,0,{130,28}))camp_tab_=i;}
 ImGui::Separator();
 if(p.active_run&&p.chapter_checkpoint.is_null()&&run_id_==0){ImGui::TextWrapped("发现上次中断的战斗。可以开启新冒险，或关闭这条未结算记录后管理天赋。");ImGui::BeginDisabled(progression_->read_only());if(ImGui::Button("结束上次中断冒险（不发放奖励）")){auto active=*p.active_run;RunReport report;report.mode=active.mode;report.character=active.character;report.seed=active.seed;report.outcome=RunOutcome::Interrupted;auto result=progression_->settle(active.id,report);camp_notice_=result.ok?"已关闭中断记录，永久成长不受影响。":result.message;}ImGui::EndDisabled();}
 if(p.active_run&&!p.chapter_checkpoint.is_null()){
  ImGui::TextColored({.97f,.8f,.44f,1},"已有保存的远征。继续不会重复领取章节奖励。");
  ImGui::BeginDisabled(progression_->read_only());if(ImGui::Button("继续已保存远征",{-1,40}))resume_expedition();ImGui::EndDisabled();
 }
 ImGui::BeginChild("camp_content",{0,std::max(170.f,ImGui::GetContentRegionAvail().y-110.f)},ImGuiChildFlags_None);
 if(camp_tab_==0){
  if(ImGui::IsKeyPressed(ImGuiKey_1))selected_=0;if(ImGui::IsKeyPressed(ImGuiKey_2))selected_=1;if(ImGui::IsKeyPressed(ImGuiKey_3))selected_=2;
  static const char* names[]={"弓 · 精准射击","法杖 · 追踪魔弹","剑 · 劈空斩"};
  static const char* identity[]={"控制弹道、穿透与弹射，凤凰提供持续压制。","追踪、连锁、冰冻与延迟爆发，控制密集怪群。","挥砍、冲锋、剑阵和防御反击，在近战中制造空间。"};
  const float setup_width=ImGui::GetContentRegionAvail().x-220;ImGui::BeginGroup();ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+setup_width);ImGui::TextUnformatted("冒险家 · 选择起始方案");for(int i=0;i<3;++i){if(ImGui::Selectable(names[i],selected_==i,0,{280,30}))selected_=i;}ImGui::TextWrapped("%s",identity[selected_]);ImGui::TextWrapped("生命110 · 移速170。起手只决定初始技能与外观。");ImGui::TextWrapped("1 / 2 / 3 选择起手 · 全部技能均可学习");ImGui::PopTextWrapPos();ImGui::EndGroup();ImGui::SameLine();
  auto pos=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;ImGui::Dummy({width,190});auto*dl=ImGui::GetWindowDrawList();dl->AddRectFilled(pos,{pos.x+width,pos.y+190},IM_COL32(19,45,30,245),12);sprite(dl,player_sprite(selected_,"idle"),{pos.x+width*.5f,pos.y+170},1.7f,clock_);dl->AddText({pos.x+16,pos.y+15},ui::warm,"冒险家");
  adventure_options();
  ImGui::Spacing();
  auto bonus=selected_mode_==int(RunMode::Daily)?RunBonuses{}:progression_->run_bonuses();ImGui::Text("本次天赋：伤害 +%.0f%% / 生命 +%.0f / 经验 +%.0f%%",bonus.damage_bonus*100,bonus.health_bonus,bonus.xp_bonus*100);ImGui::Text("重抽 %d 次 / 濒死保护 %d 次",bonus.upgrade_rerolls,bonus.death_protections);

 }else if(camp_tab_==1){
  ImGui::TextWrapped("选择一条成长方向。数值强化有明确上限，机制天赋为每局提供新的选择。");
  for(int route=0;route<3;++route){if(route)ImGui::SameLine();float width=(ImGui::GetContentRegionAvail().x-float(2-route)*12)/float(3-route);ImGui::BeginChild(("route"+std::to_string(route)).c_str(),{width,350},ImGuiChildFlags_Borders);ImGui::TextColored({.97f,.8f,.44f,1},"%s",talent_route_name(route));
   for(const auto& def:talent_definitions())if(def.route==route){int rank=progression_->talent_rank(def.id);ImGui::PushID(int(def.id));ImGui::Text("%s %d/%d",def.name,rank,def.max_rank);ImGui::TextWrapped("%s",def.description);int cost=progression_->talent_cost(def.id);ImGui::BeginDisabled(cost<0||p.crystals<cost||progression_->read_only()||camp_action_cooldown_>0);std::string label=cost<0?"已达到上限":"成长 · "+std::to_string(cost)+" 结晶";if(ImGui::Button(label.c_str(),{-1,30})){auto r=progression_->purchase(def.id,rank);camp_notice_=r.ok?std::string("已保存天赋成长：")+def.name:"成长保存失败："+r.message;camp_action_cooldown_=.4f;}ImGui::EndDisabled();ImGui::Separator();ImGui::PopID();}ImGui::EndChild();}
 }else if(camp_tab_==2){
  ImGui::Text("累计冒险 %llu / 胜利 %llu / 击杀 %llu",static_cast<unsigned long long>(p.records.runs),static_cast<unsigned long long>(p.records.victories),static_cast<unsigned long long>(p.records.total_kills));
  ImGui::Text("最长生存 %.0f 秒 / 最高等级 %d",p.records.longest_run,p.records.best_level);
  records_summary();
  if(p.history.empty())ImGui::TextWrapped("完成第一场冒险后，这里会留下你的足迹。");
  static const char* classes[]={"弓箭手","魔法师","战士"};for(auto it=p.history.rbegin();it!=p.history.rend();++it){
   const auto&r=it->report;ImGui::Text("%s · %s · Lv.%d · %d 击杀 · +%d 结晶",(r.adventurer?(r.character==CharacterClass::Archer?"冒险家 · 弓起手":r.character==CharacterClass::Mage?"冒险家 · 法杖起手":"冒险家 · 剑起手"):classes[std::clamp(int(r.character),0,2)]),run_outcome_name(r.outcome),r.level,r.kills,it->crystals);
   ImGui::Text("游戏时间 %s · 种子 %u",recorded_duration(r.elapsed).c_str(),r.seed);record_timing(r.timing);
   std::string build="技能：";for(int i=0;i<skill_count;++i)if(r.skills[i])build+=std::string(Game::skill_name(Skill(i)))+" "+std::to_string(r.skills[i])+"  ";ImGui::TextWrapped("%s",build.c_str());
   if(!r.scrolls.completed.empty()){std::string forms="融合形态：";for(const auto& completed:r.scrolls.completed){if(forms!="融合形态：")forms+=" · ";forms+=std::string(Game::skill_name(completed.result))+" / "+Game::scroll_form_name(completed.result,completed.form);}ImGui::TextWrapped("%s",forms.c_str());}
   std::string relics="遗物：";for(const auto& def:relic_definitions())if(r.owned_relic_mask&(1u<<int(def.id)))relics+=std::string(def.name)+"  ";ImGui::TextWrapped("%s",relics.c_str());
   for(const auto& rule:resonance_definitions()){int id=int(rule.id);if(r.resonance.triggers[id])ImGui::TextWrapped("共鸣 %s · %d次 · 伤害%.0f · 吸收%.0f",rule.name,r.resonance.triggers[id],r.resonance.damage[id],r.resonance.shield[id]);}
   std::string synergies;for(int i=0;i<synergy_count;++i)if(r.synergy_triggers[i]>0){if(!synergies.empty())synergies+=" · ";synergies+=std::string(Game::synergy_name(i))+" "+std::to_string(r.synergy_triggers[i])+"次";}if(!synergies.empty())ImGui::TextWrapped("联动：%s",synergies.c_str());ImGui::Separator();
  }
 }
 if(camp_tab_>=3)collection_menu();
 ImGui::EndChild();
 if(camp_tab_==0){  ImGui::BeginDisabled(progression_->read_only()||!p.chapter_checkpoint.is_null());if(ui::primary_button(selected_mode_==int(RunMode::Daily)?"开始今日固定挑战":"开始新冒险 · 新种子",{-1,38})||(!progression_->read_only()&&p.chapter_checkpoint.is_null()&&ImGui::IsKeyPressed(ImGuiKey_Enter)))begin_adventure();ImGui::EndDisabled();}

 if(!camp_notice_.empty())ImGui::TextWrapped("%s",camp_notice_.c_str());if(!error_.empty())ImGui::TextWrapped("素材错误：%s",error_.c_str());
 if(ImGui::Button("设置",{120,32}))settings_=!settings_;ImGui::SameLine();if(ImGui::Button("重新载入存档",{150,32})){auto r=progression_->load();camp_notice_=r.ok?"已重新载入存档。":r.message;}ImGui::SameLine();if(ImGui::Button("退出游戏",{120,32})){if(settings_dirty_)save_profile_settings();quit=true;}
}
}

namespace maple {
bool App::save_chapter(bool reload){
 chapter_save_attempted_=true;RunBuild build;
 if(!progression_||!run_id_||!game_.export_build(build)){camp_notice_="无法读取有效章节边界，尚未保存。";return false;}
 if(reload){auto loaded=progression_->load();if(!loaded.ok||progression_->read_only()){camp_notice_="重新加载失败："+loaded.message;return false;}}
 auto report=make_run_report(game_,RunOutcome::Victory,run_seed_);
 auto result=progression_->claim_chapter(run_id_,game_.chapter_index(),report,build);
 chapter_saved_=result.ok;
 camp_notice_=result.ok?(result.already_claimed?"此章节已保存，不会重复发放奖励。":"章节已保存，新增枫叶结晶："+std::to_string(result.crystals_awarded)):"章节保存失败，构筑仍保留在当前窗口："+result.message;
 return result.ok;
}
bool App::resume_expedition(){
 RunBuild build;bool have_build=progression_->checkpoint_build(build);const auto& p=progression_->profile();
 if(progression_->read_only()||!have_build||!p.active_run||!game_.restore_build(build)){camp_notice_="远征存档不可恢复，原存档保持不变。";return false;}
 run_id_=p.active_run->id;run_seed_=build.seed;selected_=int(build.character);selected_mode_=1;
 pending_settlement_.reset();settlement_attempted_=settlement_ok_=false;chapter_save_attempted_=chapter_saved_=true;settings_=false;
 prepare_chapter_resources();camp_notice_="已恢复章节边界、原种子与原有构筑，天赋不会重复叠加。";return true;
}
void App::chapter_menu(){
 if(pending_settlement_&&!settlement_ok_){ImGui::TextWrapped("%s",camp_notice_.c_str());if(ImGui::Button("重新加载并重试远征结算",{-1,40})){if(settle_adventure(RunOutcome::Abandoned,true))return_to_camp();}return;}
 if(!chapter_save_attempted_)save_chapter();
 ImGui::TextColored({.97f,.8f,.44f,1},"第 %d 章完成 · %s",game_.chapter_index()+1,Game::chapter_name(game_.chapter_index()));
 ImGui::Text("累计游戏时间 %s / 等级 %d / 击杀 %d",recorded_duration(game_.total_elapsed()).c_str(),game_.player.level,game_.kills);record_timing(game_.timing);
 ImGui::TextWrapped("生命、经验、技能、被动与剩余天赋次数继承到下一章。选择一次补给后继续，也可保存远征回到营地。");
 ImGui::TextWrapped("%s",camp_notice_.c_str());
 if(!chapter_saved_){if(ImGui::Button("重新加载并保存章节",{-1,40}))save_chapter(true);return;}
 if(game_.intermission_choice()<0){
  for(int i=0;i<3;++i){ImGui::PushID(i);auto description=game_.chapter_reward_description(i);if(ImGui::Button(description.c_str(),{-1,42})){
    auto result=progression_->update_checkpoint_timing(run_id_,game_.timing);if(result.ok)result=progression_->choose_chapter_supply(run_id_,i);
    if(result.ok){RunBuild build;const auto timing=game_.timing;if(progression_->checkpoint_build(build)&&game_.restore_build(build)){game_.timing=timing;camp_notice_="补给已保存，下一章保留本次选择。";}else camp_notice_="已保存补给但恢复失败，请从营地重新加载。";}
    else camp_notice_="补给尚未保存："+result.message;
   }ImGui::PopID();}
 }else{ImGui::TextWrapped("本章补给已选择并保存。");if(ImGui::Button("带着当前构筑进入下一章",{-1,44})){
   auto saved=progression_->update_checkpoint_timing(run_id_,game_.timing);RunBuild build;const auto timing=game_.timing;if(saved.ok&&progression_->checkpoint_build(build)&&game_.restore_build(build)&&game_.advance_chapter()){game_.timing=timing;chapter_save_attempted_=chapter_saved_=false;camp_notice_.clear();prepare_chapter_resources();}else camp_notice_=saved.ok?"无法进入下一章，已保存的远征仍可恢复。":"章间用时尚未保存，请重试："+saved.message;
  }}
 if(ImGui::Button("重新读取已保存章节",{-1,30})){auto saved=progression_->update_checkpoint_timing(run_id_,game_.timing);if(saved.ok){auto loaded=progression_->load();if(loaded.ok)resume_expedition();else camp_notice_="重新加载失败："+loaded.message;}else camp_notice_="章间用时尚未保存，请重试："+saved.message;}
 if(ImGui::Button("保存远征并返回营地",{-1,36}))return_to_camp();
 if(ImGui::Button("结束远征并结算",{-1,36})){if(settle_adventure(RunOutcome::Abandoned))return_to_camp();}
}
}

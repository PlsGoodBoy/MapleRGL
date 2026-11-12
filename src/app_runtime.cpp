#include "app.hpp"
#include "encounter_pool.hpp"
#include "boss_combat.hpp"
#include "ui_theme.hpp"
#include "stb_image.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>

namespace maple {
void App::apply_ui_scale(){
 ui::apply();ImGui::GetStyle().ScaleAllSizes(ui_scale_);ImGui::GetIO().FontGlobalScale=ui_scale_;
}
void App::review_playthrough(uint32_t seed,int speed){
 ordinary_playthrough_=complete_run_=profiling=true;selected_=1;selected_mode_=0;selected_game_speed_=speed==2?2:1;run_seed_=seed;
 review_controller_={};autoplay_choice_delay_=0;begin_adventure(true);
}
Input App::autoplay_input(float dt){return review_controller_.input(game_,dt);}
void App::autoplay_choice(){
 if(game_.pending_study.active){const Skill replaced=review_controller_.replacement(game_);if(replaced!=Skill::Count){game_.select_study_replacement(replaced);game_.confirm_study_replacement();}else game_.cancel_study_replacement();autoplay_choice_delay_=.18f;}
 else if(game_.state==State::Upgrade){const int choice=review_controller_.choice_index(game_);if(choice>=0)game_.choose_upgrade(size_t(choice));else game_.skip_upgrade();autoplay_choice_delay_=.18f;}
}
void App::update_guidance(float dt){
 if(!guidance_){guide_remaining_=0;return;}
 if(game_.state!=State::Playing){if(game_.state==State::Menu||game_.state==State::Victory||game_.state==State::Defeat)guide_remaining_=0;return;}
 guide_remaining_=std::max(0.f,guide_remaining_-dt);guide_gap_=std::max(0.f,guide_gap_-dt);
 if(guide_remaining_>0)return;
 auto show=[&](const char* title,const char* detail){guide_title_=title;guide_text_=detail;guide_remaining_=7;guide_gap_=18;settings_dirty_=true;settings_save_delay_=1.f;};
 for(const auto& enemy:game_.enemies)if(enemy.kind==EnemyKind::Boss&&enemy.hp>0&&!(boss_hints_seen_&(1u<<int(enemy.boss_profile)))){
  boss_hints_seen_|=1u<<int(enemy.boss_profile);show(Game::boss_profile_name(enemy.boss_profile),boss_combat_definition(enemy.boss_profile).counterplay);return;
 }
 if(guide_gap_>0)return;
 const bool eligible[]={true,game_.metrics.upgrade_choices>0,game_.active_skill_count()==Game::active_skill_limit,game_.metrics.fusions_created>0,game_.regression_active(),game_.gear_count()>0,game_.timing.speed==2};
 static const char* titles[]={"自由构筑 · 八技能槽","升级可选零至两项","技能槽已满","真实等级达标自动融合","真实等级与有效等级","装备拾取即生效","紧凑2× · 关卡节奏"};
 static const char* details[]={"起手只决定初始技能。公共技能池均可学习，Esc 暂停查看构筑。","每次升级五选二；Esc 可全部跳过，每级还有一次免费重置。","仍可学习新技能。先预览替换，取消会保留原候选和选择次数。","配方材料真实等级都达 Lv.5 后自动融合，接续路线可在暂停中查看。","退化只降低当前效果；真实等级保留，离开区域即恢复。","局内装备无限持有，同定义聚合数量；暂停可查看下一件的收益。","刷怪和关卡进度加快，移动、冷却、预警、弹速和音乐保持正常。"};
 for(int i=0;i<7;++i)if(eligible[i]&&!(guide_seen_&(1u<<i))){guide_seen_|=1u<<i;show(titles[i],details[i]);break;}
}
void App::initialize_content_resources(){
 auto available=[&](const std::string& prefix){
  for(const char* action:{".idle",".move",".hit",".die"}){
   auto found=sprites_.find(prefix+action);if(found==sprites_.end()||found->second.empty())return false;
   for(const auto& frame:found->second){int width=0,height=0,channels=0;if(!stbi_info(("assets/"+frame.file).c_str(),&width,&height,&channels))return false;}
  }
  for(const auto& entry:sprites_)if(entry.first.rfind(prefix+".",0)==0)for(const auto& frame:entry.second){int width=0,height=0,channels=0;if(!stbi_info(("assets/"+frame.file).c_str(),&width,&height,&channels))return false;}
  return true;
 };
 game_.config.available_boss_mask=0;game_.config.available_enemy_mask=0;
 for(const auto& boss:boss_definitions())if(boss.released){if(available(boss.sprite))game_.config.available_boss_mask|=1u<<int(boss.id);else content_diagnostics_.push_back(std::string("Boss资源不可用，排除候选：")+boss.key);}
 for(const auto& enemy:enemy_definitions())if(enemy.released){if(available(enemy.key))game_.config.available_enemy_mask|=uint64_t(1)<<enemy.id;else content_diagnostics_.push_back(std::string("怪物资源不可用，排除候选：")+enemy.key);}
 if(!game_.config.available_boss_mask||!game_.config.available_enemy_mask)error_="没有可用的敌人候选资源，请检查 assets 文件夹。";
 for(const auto& entry:sprites_){
  if(entry.first.rfind("enemy.",0)==0||entry.first.rfind("boss",0)==0||entry.first.rfind("battle.hostile.",0)==0)continue;
  for(const auto& frame:entry.second)if(!texture(frame.file).view)error_="素材纹理加载失败："+frame.file;
 }
}
bool App::preload_sprite_family(const std::string& prefix){
 bool okay=true,found=false;
 for(const auto& entry:sprites_)if(entry.first.rfind(prefix+".",0)==0){found=true;for(const auto& frame:entry.second)if(!texture(frame.file).view)okay=false;}
 return found&&okay;
}
void App::prepare_chapter_resources(){
 // A decode/GPU failure excludes that family before play and draws from the remaining legal pool.
 // Each pass removes at least one candidate, so this cannot become an unbounded reroll.
 for(int attempt=0;attempt<=boss_profile_count+enemy_visual_count;++attempt){
  std::set<std::string> needed;
  for(int id:game_.encounter_plan.roster)needed.insert(Game::enemy_visual_key(id));
  if(game_.encounter_plan.boss>=0)for(int encounter=0;encounter<game_.scheduled_boss_count();++encounter)needed.insert(Game::boss_sprite_key(game_.planned_boss_profile(encounter)));
  for(int id:{0,1,2,6,8,13,14})if(game_.config.available_enemy_mask&(uint64_t(1)<<id))needed.insert(Game::enemy_visual_key(id));
  for(const auto& enemy:game_.enemies)needed.insert(enemy.kind==EnemyKind::Boss?Game::boss_sprite_key(enemy.boss_profile):Game::enemy_visual_key(enemy.visual));
  for(const auto& prefix:needed){auto art=enemy_art_.find(prefix);if(art!=enemy_art_.end())for(const auto& key:{art->second.projectile,art->second.impact}){auto seq=sprites_.find(key);if(seq!=sprites_.end())for(const auto& frame:seq->second)texture(frame.file);}}
  bool failed=false;
  for(const auto& prefix:needed)if(!preload_sprite_family(prefix)){
   failed=true;content_diagnostics_.push_back("素材解码或纹理加载失败，排除并重选："+prefix);
   for(const auto& boss:boss_definitions())if(prefix==boss.sprite)game_.config.available_boss_mask&=~(1u<<int(boss.id));
   for(const auto& enemy:enemy_definitions())if(prefix==enemy.key)game_.config.available_enemy_mask&=~(uint64_t(1)<<enemy.id);
  }
  if(!failed)break;
  game_.encounter_plan.epoch=-1;game_.prepare_encounter_plan();
  if(game_.encounter_plan.boss<0||game_.encounter_plan.roster.empty()){error_="当前资源池不足以开始冒险，请检查 assets 文件夹。";break;}
  for(auto& enemy:game_.enemies){
   if(enemy.kind==EnemyKind::Boss&&!(game_.config.available_boss_mask&(1u<<int(enemy.boss_profile))))enemy.boss_profile=BossProfile(game_.encounter_plan.boss);
   else if(enemy.kind!=EnemyKind::Boss&&!(game_.config.available_enemy_mask&(uint64_t(1)<<enemy.visual))){
    int replacement=-1;for(int id:game_.encounter_plan.roster)if(enemy_definition(id).roles&(1u<<int(enemy.kind))){replacement=id;break;}
    if(replacement<0){error_="缺少当前遭遇角色的可用怪物资源。";break;}
    enemy.visual=replacement;enemy.radius=enemy_definition(replacement).radius;
   }
  }
  if(!error_.empty())break;
 }
 simulation_clock_.reset();buffered_dash_=buffered_interact_=false;reset_real_baseline_=true;rate_real_=rate_game_=0;
 actual_game_speed_=double(game_.timing.speed);
}
void App::set_game_speed(int speed){
 speed=speed==2?2:1;selected_game_speed_=speed;
 if(game_.state!=State::Menu)game_.timing.speed=speed;
 rate_real_=rate_game_=0;actual_game_speed_=double(speed);
 settings_dirty_=true;settings_save_delay_=.3f;
}
void App::suspend_for_background(float real_seconds){
 if(game_.state!=State::Menu&&game_.state!=State::Victory&&game_.state!=State::Defeat&&std::isfinite(real_seconds)&&real_seconds>0)game_.timing.total_real_seconds+=real_seconds;
 if(game_.state==State::Playing)game_.toggle_pause();
 simulation_clock_.suspend();buffered_dash_=buffered_interact_=false;combat_actions_armed_=false;reset_real_baseline_=true;
}
void App::frame(float real_dt){
 using Clock=std::chrono::steady_clock;auto begin=Clock::now();auto milliseconds=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
 if(!std::isfinite(real_dt)||real_dt<0)real_dt=0;
 if(reset_real_baseline_){real_dt=0;reset_real_baseline_=false;}
 const float display_dt=std::min(real_dt,.1f);
 upgrade_action_delay_=std::max(0.f,upgrade_action_delay_-real_dt);
 camp_action_cooldown_=std::max(0.f,camp_action_cooldown_-real_dt);if(settings_dirty_){settings_save_delay_-=real_dt;if(settings_save_delay_<=0)save_profile_settings();}
 game_.set_combat_view(ImGui::GetIO().DisplaySize.x/(2*zoom_),ImGui::GetIO().DisplaySize.y/(2*zoom_));
 clock_+=display_dt;audio_cooldown_=std::max(0.f,audio_cooldown_-real_dt);hitstop_cooldown_=std::max(0.f,hitstop_cooldown_-real_dt);hitstop_remaining_=std::max(0.f,hitstop_remaining_-real_dt);
 if(game_.state==State::Playing){hurt_timer_=std::max(0.f,hurt_timer_-display_dt);hurt_age_+=display_dt;}shake_=std::max(0.f,shake_-display_dt*4);
 const bool in_adventure=game_.state!=State::Menu&&game_.state!=State::Victory&&game_.state!=State::Defeat;
 if(in_adventure)game_.timing.total_real_seconds+=real_dt;
 if(!pending_settlement_&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)&&ImGui::IsKeyPressed(ImGuiKey_Escape,false)){
  if(settings_){music_.end_audition();settings_=false;}
  else if(game_.state==State::Upgrade){if(game_.pending_study.active)game_.cancel_study_replacement();else game_.skip_upgrade();upgrade_action_delay_=.18f;}
  else game_.toggle_pause();
 }
 if(!profiling&&ImGui::GetIO().AppFocusLost){if(game_.state==State::Playing)game_.toggle_pause();buffered_dash_=buffered_interact_=false;combat_actions_armed_=false;}
 if(input_state_!=game_.state){buffered_dash_=buffered_interact_=false;combat_actions_armed_=false;input_state_=game_.state;}
 auto feedback=[&](){
  if(game_.visual_serial<last_visual_sound_)last_visual_sound_=0;
  for(const auto& cue:game_.skill_visuals)if(cue.serial>last_visual_sound_&&!legacy_visuals_&&sound_)skill_sfx_.play(skill_visual_style(cue.source,cue.elements),cue.tier,cue.stage,cue.heavy);
  last_visual_sound_=game_.visual_serial;
  for(const auto& event:game_.events){if(event.kind==EventKind::Fusion){fusion_feedback_skill_=event.source;fusion_feedback_started_=-1.f;fusion_highlight_[int(event.source)]=clock_+1.8f;}if(event.kind==EventKind::Hurt){shake_=1;hurt_timer_=.35f;hurt_age_=0;}if(event.kind==EventKind::Hit&&event.critical&&event.actual_damage>0&&!event.secondary&&hitstop_cooldown_<=0){hitstop_remaining_=.025f;hitstop_cooldown_=.15f;shake_=std::max(shake_,.25f);}if(event.kind==EventKind::BossAttack){music_.cue(MusicCue::Danger);skill_sfx_.clear();}else play_sound(event.kind);}game_.events.clear();
 };
 feedback();size_t steps=0;
 if(ordinary_playthrough_){autoplay_choice_delay_=std::max(0.f,autoplay_choice_delay_-real_dt);if(autoplay_choice_delay_<=0)autoplay_choice();}
 else if(complete_run_){
  if(game_.pending_study.active){for(int i=0;i<skill_count;++i)if(game_.skills[i]>0&&!Game::is_fusion(Skill(i))){game_.select_study_replacement(Skill(i));game_.confirm_study_replacement();break;}}
  else if(game_.state==State::Upgrade){for(size_t i=0;i<game_.choices.size();++i)if(game_.upgrade_choice_available(game_.choices[i])){game_.choose_upgrade(i);break;}}
 }
 if(game_.state==State::Playing){
  Input input;const bool keyboard_free=!ImGui::GetIO().WantCaptureKeyboard&&!ImGui::GetIO().AppFocusLost&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);
  if(!ImGui::IsKeyDown(ImGuiKey_Space)&&!ImGui::IsKeyDown(ImGuiKey_E))combat_actions_armed_=true;
  if(keyboard_free){input.move={float(ImGui::IsKeyDown(ImGuiKey_D)||ImGui::IsKeyDown(ImGuiKey_RightArrow))-float(ImGui::IsKeyDown(ImGuiKey_A)||ImGui::IsKeyDown(ImGuiKey_LeftArrow)),float(ImGui::IsKeyDown(ImGuiKey_S)||ImGui::IsKeyDown(ImGuiKey_DownArrow))-float(ImGui::IsKeyDown(ImGuiKey_W)||ImGui::IsKeyDown(ImGuiKey_UpArrow))};
   if(combat_actions_armed_){buffered_dash_|=ImGui::IsKeyPressed(ImGuiKey_Space,false);buffered_interact_|=ImGui::IsKeyPressed(ImGuiKey_E,false);}}
  else buffered_dash_=buffered_interact_=false;
  if(input.move.x!=0||input.move.y!=0)++move_input_frames_;if(ImGui::IsKeyPressed(ImGuiKey_Space,false))++dash_presses_;
  if(hitstop_remaining_<=0||profiling){
   game_.timing.effective_real_seconds+=real_dt;rate_real_+=real_dt;
   steps=simulation_clock_.advance(real_dt,[&](float dt){
    input.dash=buffered_dash_;input.interact=buffered_interact_;
    if(ordinary_playthrough_)input=autoplay_input(dt);
    else if(profiling){input={};if(preview_domain_exit_){if(game_.combat_elapsed>=3.f&&game_.combat_elapsed<5.f)input.move={1,0};}else if(!preview_static_){input.move={std::cos(game_.combat_elapsed),std::sin(game_.combat_elapsed)};input.dash=game_.simulation_steps%120==0;}else if(preview_dash_){input.move={1,0};input.dash=game_.simulation_steps%120==0;}}
    game_.update(dt,input);buffered_dash_=buffered_interact_=false;feedback();
    return game_.state==State::Playing&&(profiling||hitstop_remaining_<=0);
   });
   if(steps){game_.timing.used_speeds|=1u<<(game_.timing.speed-1);rate_game_+=steps*SimulationClock::step_seconds*game_.encounter_pace();}
   if(rate_real_>=1){actual_game_speed_=rate_game_/rate_real_;rate_real_=rate_game_=0;}
  }else simulation_clock_.suspend();
 }else{simulation_clock_.suspend();buffered_dash_=buffered_interact_=false;}
 if((!profiling||complete_run_)&&!settlement_attempted_&&(game_.state==State::Victory||game_.state==State::Defeat))settle_adventure(game_.state==State::Victory?RunOutcome::Victory:RunOutcome::Defeat);
 update_guidance(display_dt);
 auto update_end=Clock::now();pump_audio();auto audio_end=Clock::now();scene();auto scene_end=Clock::now();if(game_.state!=State::Menu)hud();if(review_ui_flow_)review_ui_targets_.fill(false);menus();guidance_panel();auto ui_end=Clock::now();
 measurements_={milliseconds(begin,update_end),milliseconds(update_end,audio_end),milliseconds(audio_end,scene_end),milliseconds(scene_end,ui_end),game_.enemies.size(),game_.projectiles.size(),game_.effects.size(),game_.scene_objects.size(),int(game_.state),game_.elapsed};
 measurements_.simulation_steps=steps;measurements_.backlog_seconds=simulation_clock_.backlog_seconds();measurements_.actual_speed=actual_game_speed_;measurements_.game_speed=game_.timing.speed;measurements_.effective_real_seconds=game_.timing.effective_real_seconds;measurements_.total_real_seconds=game_.timing.total_real_seconds;
 if(tracing){trace_clock_+=real_dt;if(trace_clock_>=.25f){trace_clock_=0;write_playtest_state();}}
}
}

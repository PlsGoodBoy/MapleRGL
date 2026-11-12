#pragma once
#include "game.hpp"
#include "chiptune.hpp"
#include "skill_sfx.hpp"
#include "progression.hpp"
#include "simulation_clock.hpp"
#include "review_controller.hpp"
#include <memory>
#include <filesystem>
#include "imgui.h"
#include <d3d11.h>
#include <mmsystem.h>
#include <map>
#include <string>
#include <vector>
namespace maple {
struct Texture {ID3D11ShaderResourceView* view=nullptr;int w=0,h=0;};
struct Frame {std::string file;float duration=.1f;float ox=0,oy=0,cx=0,cy=0,alpha_start=1,alpha_end=1;};
struct FrameMeasurements { double update_ms=0,audio_ms=0,scene_ms=0,ui_ms=0;size_t enemies=0,projectiles=0,effects=0,objects=0;int state=0;double elapsed=0;size_t simulation_steps=0;double backlog_seconds=0,actual_speed=0,effective_real_seconds=0,total_real_seconds=0;int game_speed=1; };
class App {
public:
 void preview_battle_art(int scene);size_t texture_count()const{return textures_.size();}
 void review_quality(bool low){low_effects_=low;}void review_pause(){game_.toggle_pause();}
 void preview_boss(int profile);void preview_fusion(int character,int variant=0,int panel=0,int form=0,int boss=-1);
 void preview_pressure(int event);void preview_ability(int ability);void preview_domain(int domain);
 void preview_visuals(int scene,bool legacy=false);bool fixed_visual_review=false;
 void preview_adventure(int panel=0);void preview_build_panels(int panel=0);void profile_dark_tide();void preview_upgrade_choices(int panel=0);void profile_complete_run();
 void review_playthrough(uint32_t seed,int speed);bool automated_playthrough()const{return ordinary_playthrough_;}
 void review_boss_fight(int profile,int build);
 void review_ui_flow_scene(int scene);bool review_ui_flow_target(int target,ImVec2& point)const;nlohmann::json review_ui_flow_snapshot()const;
 void apply_ui_scale();
 nlohmann::json audio_diagnostics() const;
 const Game& diagnostic_game()const{return game_;}
 const PressureDiagnostics& pressure_diagnostics()const{return game_.pressure_metrics;}const PressureEvent& active_pressure()const{return game_.pressure_event;}size_t danger_zone_count()const{return game_.danger_zones.size();}
 void set_game_speed(int speed);int game_speed()const{return game_.state==State::Menu?selected_game_speed_:game_.timing.speed;}
 double simulation_backlog()const{return simulation_clock_.backlog_seconds();}double actual_game_speed()const{return actual_game_speed_;}
 void suspend_for_background(float real_seconds);
 const std::vector<std::string>& content_diagnostics()const{return content_diagnostics_;}
 App(ID3D11Device* device,bool diagnostics=false,std::filesystem::path profile_root={}); ~App(); void frame(float dt); bool quit=false; void profile_start(bool stress=true,bool fused=false);void mute_diagnostics(){sound_=music_enabled_=ui_sound_=false;}const FrameMeasurements& measurements()const{return measurements_;} bool profiling=false,tracing=false; bool assets_ready() const{return error_.empty();}
private:
 bool legacy_visuals_=false;uint64_t last_visual_sound_=0;SkillSfx skill_sfx_;std::array<float,skill_count> fusion_highlight_{};std::map<std::string,ImVec2> visual_sprite_metrics_;
 struct EnemyArt {std::string projectile,impact,action;};std::map<std::string,EnemyArt> enemy_art_;
 void battle_sprite(ImDrawList*,const std::string&,Vec2,float,float,ImU32=0xffffffffu,float=0,bool=true,bool=true,bool=false);
 void draw_battle_summon(ImDrawList*,const Effect&);void draw_battle_field(ImDrawList*,const DangerZone&);void draw_hostile_shot(ImDrawList*,const Effect&);void draw_hostile_impacts(ImDrawList*);void draw_hostile_release(ImDrawList*,const Effect&);
 void visual_sprite(ImDrawList*,const char*,Vec2,float,float,ImU32,float rotation=0);void draw_skill_visuals(ImDrawList*,bool);void draw_adventure_loop(ImDrawList*,const Effect&);bool draw_skill_projectile(ImDrawList*,const Projectile&);
 int selected_game_speed_=1;SimulationClock simulation_clock_;bool reset_real_baseline_=false;
 double rate_real_=0,rate_game_=0,actual_game_speed_=1;std::vector<std::string> content_diagnostics_;
 void initialize_content_resources();void prepare_chapter_resources();bool preload_sprite_family(const std::string& prefix);
 void settings_menu();
 void guidance_panel();void guide_reference();void combat_recap();void update_guidance(float dt);
 float ui_scale_=1.f,flash_strength_=1.f,guide_remaining_=0,guide_gap_=0,upgrade_action_delay_=0;
 bool guidance_=true,combat_actions_armed_=false;uint32_t guide_seen_=0,boss_hints_seen_=0;
 std::string guide_title_,guide_text_;State input_state_=State::Menu;
 bool ordinary_playthrough_=false;float autoplay_choice_delay_=0;ReviewController review_controller_;
 Input autoplay_input(float dt);void autoplay_choice();
 bool review_ui_flow_=false;std::array<ImVec2,3> review_ui_points_{};std::array<bool,3> review_ui_targets_{};
 void review_ui_flow_item(int target);
 void run_gear_panel();void build_sidebar();void fusion_panel();void study_replacement_menu();void upgrade_menu();
 int upgrade_detail_index_=-1,preview_build_tab_=-1;
 void setup_pressure_preview();void pressure_ground(ImDrawList* draw);void pressure_foreground(ImDrawList* draw);void pressure_altar(ImDrawList* draw,const DangerZone& zone);
 bool complete_run_=false;
 bool preview_static_=false,preview_dash_=false,preview_domain_exit_=false;FrameMeasurements measurements_;std::filesystem::path profile_root_override_;float hitstop_remaining_=0,hitstop_cooldown_=0;bool buffered_dash_=false,buffered_interact_=false;
 std::unique_ptr<ProgressionStore> progression_;bool diagnostics_=false,settlement_attempted_=false,settlement_ok_=false,settings_dirty_=false;uint64_t run_id_=0;uint32_t run_seed_=0;int camp_tab_=0,settlement_reward_=0,selected_mode_=0,selected_difficulty_=0;bool chapter_save_attempted_=false,chapter_saved_=false;float settings_save_delay_=0,camp_action_cooldown_=0;std::string camp_notice_;std::optional<RunReport> pending_settlement_;
 void adventure_options();void bestiary_menu();void records_summary();void collection_menu();void loadout_sidebar();void chapter_menu();bool save_chapter(bool reload=false);bool resume_expedition();void camp_menu();void initialize_progression();bool begin_adventure(bool replay=false);bool settle_adventure(RunOutcome outcome,bool reload=false);void return_to_camp();void save_profile_settings();
 ID3D11Device* device_; std::map<std::string,Texture> textures_; std::map<std::string,std::vector<Frame>> sprites_; std::map<std::string,float> forward_angles_; Game game_; float clock_=0,shake_=0,hurt_timer_=0,hurt_age_=0; bool sound_=true,shake_enabled_=true,show_stats_=false,settings_=false; int selected_=0; float zoom_=1.6f,volume_=.35f; std::string error_; struct Voice {float phase=0,frequency=0;int left=0,total=0;}; std::array<Voice,12> voices_{}; HWAVEOUT audio_device_=nullptr; std::array<WAVEHDR,8> audio_headers_{}; std::array<std::array<short,512>,8> audio_buffers_{}; float audio_cooldown_=0; Chiptune music_; bool music_enabled_=true; float music_volume_=.35f,ui_volume_=.5f;bool ui_sound_=true;int audition_track_=0;State audio_state_=State::Menu;uint64_t audio_buffers_written_=0;int audio_errors_=0,audio_queue_gaps_=0;float audio_peak_=0;
 float trace_clock_=0;int move_input_frames_=0,dash_presses_=0,upgrade_selections_=0;void write_playtest_state();
 void initialize_audio();void pump_audio();std::string player_sprite(int cls,const char* state)const;
 void ui_icon(ImDrawList* draw,const std::string& key,ImVec2 at,float size,ImU32 tint=0xffffffff);void skill_details(Skill skill);void fusion_recipe_view(const FusionRecipe& recipe,bool compact=false);
 Skill fusion_feedback_skill_=Skill::Count;float fusion_feedback_started_=-1.f;
 float friendly_effect_alpha_=.70f,shake_strength_=1.f,damage_number_density_=.70f;bool low_effects_=false;
 Texture& texture(const std::string& file); void sprite(ImDrawList* dl,const std::string& key,ImVec2 p,float scale,float time,bool flip=false,ImU32 tint=0xffffffff,float rotation=0,bool loop=true); void scene();void hud();void passive_panel();void content_hud();void menus();void play_sound(EventKind kind); ImVec2 screen(Vec2 p) const; ImVec2 center_{};
};
}

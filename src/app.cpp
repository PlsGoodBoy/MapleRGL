#include "app.hpp"
#include "adventure_content.hpp"
#include "battle_art.hpp"
#include "boss_combat.hpp"
#include "boss_art_timing.hpp"
#include "build_info.hpp"
#include "encounter_pool.hpp"
#include "ui_theme.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <windows.h>
#include <mmsystem.h>
namespace maple {
static ImU32 gold=IM_COL32(247,203,111,255), ivory=IM_COL32(239,236,213,255), muted=IM_COL32(157,181,164,255);
App::App(ID3D11Device* d,bool diagnostics,std::filesystem::path profile_root):device_(d),diagnostics_(diagnostics),profile_root_override_(std::move(profile_root)){
 game_.config.enable_director=true;game_.config.enable_scene_objects=true;game_.config.enable_boss_phases=true;
 ui::apply();
 try{std::ifstream in("assets/manifest.json");if(!in)throw std::runtime_error("找不到素材清单 assets/manifest.json");nlohmann::json j;in>>j;for(auto it=j["sprites"].begin();it!=j["sprites"].end();++it){forward_angles_[it.key()]=it.value().value("forward_angle_degrees",180.f)*.0174532925f;for(auto& f:it.value()["frames"]){Frame fr;fr.file=f["file"].get<std::string>();fr.duration=std::max(.016f,f.value("duration_ms",100)/1000.f);fr.alpha_start=f.value("alpha_start",255.f)/255.f;fr.alpha_end=f.value("alpha_end",255.f)/255.f;if(f.contains("origin")){fr.ox=f["origin"][0];fr.oy=f["origin"][1];}if(f.contains("visual_center")){fr.cx=f["visual_center"][0];fr.cy=f["visual_center"][1];}else if(f.contains("alpha_nonzero_bounds")&&f["alpha_nonzero_bounds"].is_array()){fr.cx=(f["alpha_nonzero_bounds"][0].get<float>()+f["alpha_nonzero_bounds"][2].get<float>())*.5f;fr.cy=(f["alpha_nonzero_bounds"][1].get<float>()+f["alpha_nonzero_bounds"][3].get<float>())*.5f;}else{fr.cx=fr.ox;fr.cy=fr.oy;}sprites_[it.key()].push_back(fr);}}if(j.contains("battle_art"))for(auto it=j["battle_art"]["enemies"].begin();it!=j["battle_art"]["enemies"].end();++it)enemy_art_[it.key()]={it.value()["projectile"],it.value()["impact"],it.value()["body_action"]};}catch(const std::exception&e){error_=e.what();}
 initialize_content_resources();
 preload_sprite_family("vfx.skills");

 try {std::ifstream rules_file("assets/encounter_rules.json");if(rules_file){nlohmann::json rules;rules_file>>rules;if(rules.value("schema",0)!=1)throw std::runtime_error("unsupported director rules schema");auto cfg=game_.config.director_config;
#define LOAD_DIRECTOR_FLOAT(name) cfg.name=rules.value(#name,cfg.name)
 LOAD_DIRECTOR_FLOAT(first_encounter);LOAD_DIRECTOR_FLOAT(encounter_gap_min);LOAD_DIRECTOR_FLOAT(encounter_gap_max);LOAD_DIRECTOR_FLOAT(min_safe_distance);LOAD_DIRECTOR_FLOAT(escape_half_angle);LOAD_DIRECTOR_FLOAT(low_health);LOAD_DIRECTOR_FLOAT(high_pressure);LOAD_DIRECTOR_FLOAT(breather_duration);LOAD_DIRECTOR_FLOAT(boss_telegraph);
#undef LOAD_DIRECTOR_FLOAT
 cfg.max_spawns_per_update=rules.value("max_spawns_per_update",cfg.max_spawns_per_update);
 static const char* keys[]={"swarm","encirclement","rush","elite_hunt","crisis","breather"};
 for(const auto& entry:rules.at("rules")){const auto key=entry.at("kind").get<std::string>();int index=-1;for(int n=0;n<6;++n)if(key==keys[n])index=n;if(index<0)throw std::runtime_error("unknown director encounter kind");auto& rule=cfg.rules[index];
#define LOAD_RULE(name) rule.name=entry.value(#name,rule.name)
 LOAD_RULE(weight);LOAD_RULE(unlock_progress);LOAD_RULE(telegraph);LOAD_RULE(duration);LOAD_RULE(cooldown);LOAD_RULE(spawn_interval);LOAD_RULE(base_budget);LOAD_RULE(batch_size);
#undef LOAD_RULE
 }game_.config.director_config=cfg;}}
 catch(const std::exception& ex){error_="遭遇配置无法读取，请检查 assets/encounter_rules.json";content_diagnostics_.push_back(std::string("encounter_rules: ")+ex.what());}
 initialize_progression();
 initialize_audio();
 if(progression_&&(!error_.empty()||!content_diagnostics_.empty())){
  auto directory=progression_->directory()/"logs";std::error_code ec;std::filesystem::create_directories(directory,ec);
  std::ofstream log(directory/"runtime-errors.log",std::ios::app);
  if(log){log<<MAPLERGL_VERSION<<" "<<MAPLERGL_BUILD_COMMIT<<" seed="<<run_seed_<<" mode="<<int(game_.run_mode())<<" "<<error_<<'\n';for(const auto& reason:content_diagnostics_)log<<reason.substr(0,600)<<'\n';}
  else camp_notice_+=" 无法写入运行错误日志。";
 }
}
App::~App(){if(progression_&&game_.state==State::ChapterComplete&&chapter_saved_&&progression_->profile().active_run&&progression_->profile().active_run->id==run_id_)progression_->update_checkpoint_timing(run_id_,game_.timing);if(settings_dirty_)save_profile_settings();if(audio_device_){waveOutReset(audio_device_);for(auto& h:audio_headers_)waveOutUnprepareHeader(audio_device_,&h,sizeof(h));waveOutClose(audio_device_);}for(auto& p:textures_)if(p.second.view)p.second.view->Release();}
Texture& App::texture(const std::string& file){auto found=textures_.find(file);if(found!=textures_.end())return found->second;Texture t;int n=0;auto path="assets/"+file;unsigned char* pixels=stbi_load(path.c_str(),&t.w,&t.h,&n,4);if(pixels){D3D11_TEXTURE2D_DESC desc{};desc.Width=t.w;desc.Height=t.h;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels;data.SysMemPitch=t.w*4;ID3D11Texture2D* tex=nullptr;if(SUCCEEDED(device_->CreateTexture2D(&desc,&data,&tex))){device_->CreateShaderResourceView(tex,nullptr,&t.view);tex->Release();}stbi_image_free(pixels);}return textures_.emplace(file,t).first->second;}
void App::sprite(ImDrawList* dl,const std::string& key,ImVec2 p,float scale,float time,bool flip,ImU32 tint,float rotation,bool loop){auto it=sprites_.find(key);if(it==sprites_.end()||it->second.empty()){auto dot=key.find('.');if(dot!=std::string::npos&&key.substr(dot)!=".idle")sprite(dl,key.substr(0,dot)+".idle",p,scale,time,flip,tint,rotation,loop);return;}auto& frames=it->second;float total=0;for(auto&f:frames)total+=f.duration;float at=loop?std::fmod(std::max(0.f,time),total):std::min(std::max(0.f,time),std::max(0.f,total-.0001f));auto* fr=&frames.back();for(auto& f:frames){if(at<f.duration){fr=&f;break;}at-=f.duration;}float frame_alpha=std::clamp(fr->alpha_start+(fr->alpha_end-fr->alpha_start)*std::clamp(at/fr->duration,0.f,1.f),0.f,1.f);tint=(tint&0x00ffffffu)|(ImU32(((tint>>24)&255)*frame_alpha)<<24);auto&t=texture(fr->file);if(!t.view)return;float ox=flip?t.w-fr->ox:fr->ox;ImVec2 a{std::floor(p.x-ox*scale),std::floor(p.y-fr->oy*scale)},b{a.x+t.w*scale,a.y+t.h*scale};if(std::abs(rotation)<.0001f){dl->AddImage((ImTextureID)(intptr_t)t.view,a,b,flip?ImVec2(1,0):ImVec2(0,0),flip?ImVec2(0,1):ImVec2(1,1),tint);}else{float c=std::cos(rotation),sn=std::sin(rotation);auto rotate=[&](ImVec2 v){float x=v.x-p.x,y=v.y-p.y;return ImVec2(p.x+x*c-y*sn,p.y+x*sn+y*c);};float u0=flip?1.f:0.f,u1=1.f-u0;dl->AddImageQuad((ImTextureID)(intptr_t)t.view,rotate(a),rotate({b.x,a.y}),rotate(b),rotate({a.x,b.y}),{u0,0},{u1,0},{u1,1},{u0,1},tint);}}
std::string App::player_sprite(int cls,const char* state)const{static const char* names[]={"player_archer","player_mage","player_warrior"};std::string key=std::string(names[std::clamp(cls,0,2)])+"."+state;return sprites_.count(key)?key:std::string("player.")+state;}
ImVec2 App::screen(Vec2 p)const{return {center_.x+(p.x-game_.player.pos.x)*zoom_,center_.y+(p.y-game_.player.pos.y)*zoom_};}
namespace {
ImU32 danger_color(DangerKind kind,int alpha){
 switch(kind){case DangerKind::Explosion:return IM_COL32(255,133,91,alpha);case DangerKind::Smoke:return IM_COL32(145,169,189,alpha);case DangerKind::Distortion:return IM_COL32(137,194,248,alpha);case DangerKind::NaturalOrder:return IM_COL32(184,221,121,alpha);case DangerKind::Regression:return IM_COL32(177,151,200,alpha);case DangerKind::Altar:return IM_COL32(216,150,221,alpha);case DangerKind::Revival:return IM_COL32(234,205,132,alpha);}return IM_COL32(240,160,130,alpha);
}
const char* ability_icon(EnemyAbility ability){static const char* keys[]={"buff.icon.shield","skill.icon.meteor","buff.icon.curse","passive.icon.6","skill.icon.gravity_well","equipment.icon.3","passive.icon.1"};return keys[std::clamp(int(ability),0,enemy_ability_count-1)];}
const char* danger_hint(DangerKind kind){switch(kind){case DangerKind::Explosion:return "延迟爆炸 · 离开边界";case DangerKind::Smoke:return "烟幕 · 禁止新攻击";case DangerKind::Distortion:return "扭曲 · 敌我弹速 ×0.5";case DangerKind::NaturalOrder:return "自然秩序 · 装备压制";case DangerKind::Regression:return "退化 · 有效等级 -1";case DangerKind::Altar:return "黑暗祭坛 · 可破坏";case DangerKind::Revival:return "复苏预告 · 无爆炸伤害";}return "危险区域";}
}
void App::pressure_ground(ImDrawList* draw){
 constexpr float pi=3.14159265f;const auto size=ImGui::GetIO().DisplaySize;
 for(const auto& zone:game_.danger_zones){if(zone.age>=zone.duration)continue;const auto p=screen(zone.pos);const float radius=zone.radius*zoom_;if(p.x+radius<0||p.y+radius<0||p.x-radius>size.x||p.y-radius>size.y)continue;
  const bool warning=zone.age<zone.warning;const float progress=std::clamp(zone.age/std::max(.01f,zone.warning),0.f,1.f);
  if(warning)draw->AddCircle(p,radius*progress,danger_color(zone.kind,90),40,1.5f);
  draw_battle_field(draw,zone);
 }
 // Broken chain links are deliberately separate from hostile continuous beams.
 for(const auto& link:game_.soul_links){if(link.broken)continue;const Enemy *first=nullptr,*second=nullptr;for(const auto& enemy:game_.enemies){if(enemy.id==link.first&&enemy.hp>0)first=&enemy;if(enemy.id==link.second&&enemy.hp>0)second=&enemy;}if(!first||!second)continue;
  const auto a=screen(first->pos),b=screen(second->pos);const float distance=length(second->pos-first->pos);if(distance>link.max_distance)continue;const Vec2 axis=normalized(second->pos-first->pos);const int pieces=std::clamp(int(distance/22),3,16);
  for(int part=0;part<pieces;++part){const float t=(part+.5f)/pieces,bend=std::sin(t*pi)*9*zoom_;const ImVec2 p{a.x+(b.x-a.x)*t-axis.y*bend,a.y+(b.y-a.y)*t+axis.x*bend};draw->AddEllipse(p,{3.5f*zoom_,2*zoom_},IM_COL32(177,173,210,115),std::atan2(axis.y,axis.x),8,1);}
 }
}
void App::pressure_altar(ImDrawList* draw,const DangerZone& zone){
 const auto p=screen(zone.pos);const float r=24*zoom_;const bool warning=zone.age<zone.warning;const int alpha=warning?90:230;
 draw->AddEllipseFilled({p.x,p.y+3*zoom_},{r,8*zoom_},IM_COL32(20,13,24,130));
 draw->AddQuadFilled({p.x-r*.8f,p.y},{p.x+r*.8f,p.y},{p.x+r*.55f,p.y-12*zoom_},{p.x-r*.55f,p.y-12*zoom_},IM_COL32(89,69,99,alpha));
 draw->AddQuadFilled({p.x,p.y-47*zoom_},{p.x+12*zoom_,p.y-27*zoom_},{p.x,p.y-10*zoom_},{p.x-12*zoom_,p.y-27*zoom_},IM_COL32(193,125,207,alpha));
 draw->AddLine({p.x,p.y-44*zoom_},{p.x,p.y-13*zoom_},IM_COL32(239,205,249,alpha),2);
 if(zone.max_hp>0){const float width=48*zoom_,top=p.y-57*zoom_;draw->AddRectFilled({p.x-width*.5f,top},{p.x+width*.5f,top+5*zoom_},IM_COL32(38,20,40,220),2);draw->AddRectFilled({p.x-width*.5f,top},{p.x-width*.5f+width*std::clamp(zone.hp/zone.max_hp,0.f,1.f),top+5*zoom_},IM_COL32(221,147,224,230),2);}
}
void App::pressure_foreground(ImDrawList* draw){
 constexpr float pi=3.14159265f;const auto size=ImGui::GetIO().DisplaySize;const float scale=std::clamp(ImGui::GetFontSize()/18.f,1.f,1.5f);
 for(const auto& zone:game_.danger_zones){if(zone.age>=zone.duration)continue;const auto p=screen(zone.pos);const float radius=zone.radius*zoom_;if(p.x+radius<0||p.y+radius<0||p.x-radius>size.x||p.y-radius>size.y)continue;const bool warning=zone.age<zone.warning;const ImU32 color=danger_color(zone.kind,warning?230:180);
  const int segments=zone.kind==DangerKind::Smoke?16:12;for(int arc=0;arc<segments;++arc){const float a=arc*2*pi/segments;draw->PathArcTo(p,radius,a,a+2*pi/segments*(warning?.68f:zone.kind==DangerKind::Smoke?.8f:.93f),5);draw->PathStroke(color,0,warning?2.6f:1.7f);}
  if(zone.kind==DangerKind::Explosion&&warning){const float r=9*scale;draw->AddTriangle({p.x,p.y-r},{p.x+r,p.y+r},{p.x-r,p.y+r},color,2);draw->AddLine({p.x,p.y-r*.35f},{p.x,p.y+r*.35f},color,2);}
  char value[144];const float remaining=warning?zone.warning-zone.age:zone.duration-zone.age;snprintf(value,sizeof(value),"%s%s · %.1fs",warning?"预告 ":"",danger_hint(zone.kind),std::max(0.f,remaining));const float font=12*scale;const auto extent=ImGui::GetFont()->CalcTextSizeA(font,10000,0,value);const ImVec2 at{p.x-extent.x*.5f,p.y-radius-3-font};
  draw->AddRectFilled({at.x-4,at.y-2},{at.x+extent.x+4,at.y+font+3},IM_COL32(12,20,19,200),3);draw->AddText(ImGui::GetFont(),font,at,color,value);
 }
 int feedback_count=0;
 for(const auto& enemy:game_.enemies){if(enemy.hp<=0)continue;const auto p=screen(enemy.pos);if(p.x< -80||p.y< -180||p.x>size.x+80||p.y>size.y+150)continue;
  const bool prominent=enemy.kind==EnemyKind::Boss||enemy.kind==EnemyKind::Elite||enemy.abilities.elite;const float y=p.y-(enemy.radius+27)*zoom_,edge=prominent?16*scale:10*scale;int count=0;for(int i=0;i<enemy_ability_count;++i)if(enemy.abilities.mask&(1u<<i))++count;
  int index=0;for(int i=0;i<enemy_ability_count;++i)if(enemy.abilities.mask&(1u<<i)){const auto ability=EnemyAbility(i);const ImVec2 at{p.x-count*(edge+3)*.5f+index++*(edge+3),y-edge};
   if(prominent){draw->AddRectFilled({at.x-2,at.y-2},{at.x+edge+2,at.y+edge+2},IM_COL32(13,23,23,205),3);ui_icon(draw,ability_icon(ability),at,edge);}
   else{const ImVec2 center{at.x+edge*.5f,at.y+edge*.5f};const ImU32 colors[]={IM_COL32(245,215,132,200),IM_COL32(251,158,108,205),IM_COL32(190,173,212,195),IM_COL32(156,222,242,210),IM_COL32(146,187,242,205),IM_COL32(189,216,124,205),IM_COL32(119,222,198,205)};if(i==int(EnemyAbility::DeathBlast))draw->AddTriangle({center.x,at.y},{at.x+edge,at.y+edge},{at.x,at.y+edge},colors[i],1.4f);else if(i==int(EnemyAbility::Refraction))draw->AddQuad({center.x,at.y},{at.x+edge,center.y},{center.x,at.y+edge},{at.x,center.y},colors[i],1.4f);else draw->AddCircle(center,edge*.42f,colors[i],8,1.4f);}
   const bool spent=(ability==EnemyAbility::Refraction&&enemy.abilities.refraction<=0)||(ability==EnemyAbility::Promise&&enemy.abilities.promise_used&&enemy.abilities.promise_remaining<=0);
   if(spent)draw->AddRectFilled(at,{at.x+edge,at.y+edge},IM_COL32(20,28,30,150),2);
   if(ability==EnemyAbility::Refraction){char layers[12];snprintf(layers,sizeof(layers),"%d",enemy.abilities.refraction);draw->AddText(ImGui::GetFont(),11*scale,{at.x+edge-3,at.y+edge-6},spent?IM_COL32(155,165,170,215):IM_COL32(221,247,255,245),layers);}
   if(game_.state==State::Playing&&ImGui::IsMouseHoveringRect(at,{at.x+edge,at.y+edge},false)){ImGui::BeginTooltip();ImGui::TextUnformatted(enemy_ability_name(ability));if(ability==EnemyAbility::Refraction)ImGui::Text("剩余抵挡 %d 次",enemy.abilities.refraction);if(ability==EnemyAbility::Promise)ImGui::Text("%s",enemy.abilities.promise_used?"本身份已触发一次":"濒死触发一次，持续 3 游戏秒");ImGui::EndTooltip();}
  }
  if(game_.displacement_immune(enemy)){const float x=p.x-(enemy.radius+6)*zoom_;draw->AddLine({x,y-2},{x,y+7*scale},IM_COL32(234,192,130,185),2);draw->AddLine({x-3*scale,y+4*scale},{x+3*scale,y+4*scale},IM_COL32(234,192,130,185),2);}
  std::string feedback;if(enemy.abilities.promise_remaining>0){char remaining[48];snprintf(remaining,sizeof(remaining),"虚妄 · %.1fs",enemy.abilities.promise_remaining);feedback=remaining;}
  else if(enemy.abilities.feedback>0){switch(enemy.abilities.feedback_kind){case 1:feedback="完全免伤";break;case 2:feedback="折光抵挡";break;case 3:feedback="虚妄启动";break;case 4:feedback="位移免疫";break;}}
  else if(enemy.abilities.shadow_class>=0){static const char* names[]={"暗影弓手","暗影法师","暗影战士"};feedback=names[std::clamp(enemy.abilities.shadow_class,0,2)];}
  if(!feedback.empty()&&(prominent||feedback_count++<8)){const float font=12*scale;const auto extent=ImGui::GetFont()->CalcTextSizeA(font,10000,0,feedback.c_str());const ImVec2 at{p.x-extent.x*.5f,y-edge-font-5};draw->AddRectFilled({at.x-4,at.y-2},{at.x+extent.x+4,at.y+font+2},IM_COL32(13,21,23,210),3);draw->AddText(ImGui::GetFont(),font,at,IM_COL32(244,214,159,240),feedback.c_str());}
 }
}
void App::scene(){
 auto* dl=ImGui::GetBackgroundDrawList();
 const auto size=ImGui::GetIO().DisplaySize;
 center_={size.x*.5f,size.y*.53f};
 if(shake_enabled_&&shake_>0){center_.x+=std::sin(clock_*93)*shake_*shake_strength_*7;center_.y+=std::cos(clock_*101)*shake_*shake_strength_*5;}
 const int region=std::clamp(game_.region_index(),0,2);
 const float pi=3.14159265f;
 auto fade_vertices=[&](int start,float alpha){for(int i=start;i<dl->VtxBuffer.Size;++i){auto& color=dl->VtxBuffer[i].col;const unsigned a=(color>>24)&255;color=(color&0x00ffffffu)|(unsigned(a*alpha)<<24);}};
 auto visible=[&](ImVec2 p,float padding=240.f){return p.x>=-padding&&p.y>=-padding&&p.x<=size.x+padding&&p.y<=size.y+padding;};
 auto centered_text=[&](ImVec2 p,ImU32 color,const char* text){
  auto extent=ImGui::CalcTextSize(text);p.x-=extent.x*.5f;
  dl->AddRectFilled({p.x-5,p.y-3},{p.x+extent.x+5,p.y+extent.y+3},IM_COL32(9,19,15,200),4);
  dl->AddText(p,color,text);
 };
 auto sector=[&](ImVec2 p,float radius,float axis,float half,ImU32 fill,ImU32 outline,float thickness){
  if(fill){dl->PathLineTo(p);dl->PathArcTo(p,radius,axis-half,axis+half,24);dl->PathFillConvex(fill);}
  dl->PathArcTo(p,radius,axis-half,axis+half,24);dl->PathStroke(outline,0,thickness);
  for(float side:{-1.f,1.f})dl->AddLine(p,{p.x+std::cos(axis+side*half)*radius,p.y+std::sin(axis+side*half)*radius},outline,1.f);
 };
 auto capsule=[&](Vec2 from,Vec2 to,float radius,ImU32 fill,ImU32 outline,float thickness=1.5f){
  const auto p=screen(from),q=screen(to);const float axis=std::atan2(to.y-from.y,to.x-from.x),r=std::max(1.f,radius*zoom_);
  auto path=[&](){dl->PathArcTo(p,r,axis+pi*.5f,axis+pi*1.5f,12);dl->PathArcTo(q,r,axis-pi*.5f,axis+pi*.5f,12);};
  if(fill){path();dl->PathFillConvex(fill);}if(outline){path();dl->PathStroke(outline,ImDrawFlags_Closed,thickness);}
 };
 auto animation_phase=[&](const std::string& key,float fraction){float duration=0;const auto found=sprites_.find(key);if(found!=sprites_.end())for(const auto& frame:found->second)duration+=frame.duration;return std::max(0.f,duration-.0001f)*std::clamp(fraction,0.f,1.f);};
 auto impact_phase=[](float age,float delay,float duration){return age<delay?.65f*std::clamp(age/std::max(.01f,delay),0.f,1.f):.65f+.35f*std::clamp((age-delay)/std::max(.01f,duration-delay),0.f,1.f);};
 const ImU32 floor_base[]={IM_COL32(31,55,39,255),IM_COL32(24,47,47,255),IM_COL32(91,65,44,255)};
 const ImU32 floor_alt[]={IM_COL32(35,59,41,255),IM_COL32(28,51,51,255),IM_COL32(95,69,47,255)};
 const char* decor[3][3]={{"map.henesys.flowers","map.henesys.clover","map.henesys.mushrooms"},{"map.ellinia.leaf_patch","map.ellinia.vine","map.ellinia.magic_orb"},{"map.perion.pebbles","map.perion.dry_grass","map.perion.thorn_bush"}};
 dl->AddRectFilled({0,0},size,floor_base[region]);
 for(int x=-9;x<=9;++x)for(int y=-7;y<=7;++y){
  const int tx=int(std::floor(game_.player.pos.x/96))+x,ty=int(std::floor(game_.player.pos.y/96))+y;
  auto p=screen({float(tx*96),float(ty*96)});
  unsigned h=unsigned(tx)*73856093u^unsigned(ty)*19349663u;
  dl->AddRectFilled(p,{p.x+96*zoom_,p.y+96*zoom_},h%3?floor_base[region]:floor_alt[region]);
  if(h%13==0)sprite(dl,decor[region][(h/13)%3],{p.x+40*zoom_,p.y+40*zoom_},zoom_*.65f,clock_,false,IM_COL32(255,255,255,160));
 }
 if(game_.state==State::Menu){sprite(dl,"map.camp.fire",{size.x*.12f,size.y*.78f},1.4f,clock_);sprite(dl,"map.camp.tree",{size.x*.88f,size.y*.60f},1.8f,clock_);}

 // Source-origin offsets are invariant across animation frames. Never bottom-center the PNGs.
 struct PropVisual {Vec2 source_to_ground;float scale;};
 static const auto prop_visuals=[](){
  std::array<PropVisual,4> result{{{{0,17},1.15f},{{0,34},.9f},{{-73,105},.185f},{{0,1},.5f}}};
  try{
   std::ifstream input("assets/scene_props.json");nlohmann::json data;
   if(input){input>>data;for(int i=0;i<4;++i){
    const auto& v=data.at("prop_visuals").at(Game::scene_object_key(SceneObjectKind(i)));
    const float x=v.at("source_origin_to_ground_px").at(0).get<float>(),y=v.at("source_origin_to_ground_px").at(1).get<float>();
    const float scale=v.at("suggested_render_scale").get<float>();
    if(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(scale)&&scale>0&&scale<4)result[i]={{x,y},scale};
   }}
  }catch(const std::exception&){/* Exact exported offsets above remain a safe fallback. */}
  return result;
 }();
 auto draw_prop=[&](const SceneObject& object){
  const auto& visual=prop_visuals[std::clamp(int(object.kind),0,3)];
  const ImVec2 origin=screen(object.pos-visual.source_to_ground*visual.scale);
  const bool broken=object.state==SceneObjectState::Broken;
  const bool spent=object.state==SceneObjectState::Spent;
  const char* animation=spent?"idle":Game::scene_animation_key(object);
  float time=object.age;
  if(broken)time=std::max(0.f,(object.kind==SceneObjectKind::Barrel?1.6f:.75f)-object.countdown);
  else if(object.state==SceneObjectState::Armed)time=std::max(0.f,1.1f-object.countdown);
  else if(object.hit_flash>0)time=std::max(0.f,.16f-object.hit_flash);
  const ImU32 tint=spent?IM_COL32(110,120,119,150):object.hit_flash>0?IM_COL32(255,214,180,255):0xffffffffu;
  sprite(dl,std::string(Game::scene_object_key(object.kind))+"."+animation,origin,zoom_*visual.scale,time,false,tint,0,!broken&&!spent&&object.state!=SceneObjectState::Armed&&object.hit_flash<=0);
 };

 // All gameplay footprints and large AOE telegraphs are below actors and their shadows.
 for(const auto& object:game_.scene_objects){
  const auto p=screen(object.pos);if(!visible(p,object.radius*zoom_+160))continue;
  if(object.kind==SceneObjectKind::Mud){
   dl->AddCircleFilled(p,object.radius*zoom_,IM_COL32(52,35,27,135),48);
   dl->AddCircle(p,object.radius*zoom_,IM_COL32(186,146,92,155),48,2);
  }else if(object.kind==SceneObjectKind::HealingWell&&object.state==SceneObjectState::Active){
   dl->AddCircleFilled(p,34*zoom_,IM_COL32(87,225,150,30),32);
   dl->AddCircle(p,34*zoom_,IM_COL32(116,235,170,135),32,1.5f);
  }else if(object.kind==SceneObjectKind::Barrel&&object.state==SceneObjectState::Armed){
   const float progress=std::clamp(1-object.countdown/1.1f,0.f,1.f);
   const float radius=118*zoom_;
   dl->AddCircleFilled(p,radius,IM_COL32(246,67,59,38+int(progress*28)),48);
   dl->AddCircleFilled(p,radius*progress,IM_COL32(255,150,72,40),48);
   dl->AddCircle(p,radius,IM_COL32(255,132,83,230),48,3);
  }
 }
 for(const auto& object:game_.scene_objects)if(object.kind==SceneObjectKind::Mud&&visible(screen(object.pos)))draw_prop(object);
 pressure_ground(dl);
 for(const auto& e:game_.effects){
  const auto p=screen(e.pos);const float progress=e.delay>0?std::clamp(e.age/e.delay,0.f,1.f):1.f;
  const float fade=std::max(0.f,1-e.age/std::max(.01f,e.duration));
  if((low_effects_||simulation_clock_.backlog_seconds()>.25)&&e.kind==EffectKind::Hit&&!e.hostile)continue;
  int effect_vertex_start=dl->VtxBuffer.Size;
  switch(e.kind){
  case EffectKind::Adventure:{
   if(!legacy_visuals_){draw_adventure_loop(dl,e);break;}
   const auto roles=Game::skill_roles(e.source);const auto end=screen(e.end);const ImU32 color=(roles&Shadow)?IM_COL32(186,161,232,170):(roles&(Gun|Machine))?IM_COL32(244,201,122,170):IM_COL32(126,220,219,170);
   const float pulse=e.count?std::clamp(e.tick/std::max(.08f,e.secondary_tick),0.f,1.f):.2f;
   if(roles&(Turret|Node)){for(int n=0;n<(Game::fusion_tier(e.source)>=3?3:1);++n){float a=e.angle+n*2*pi/3;ImVec2 node{p.x+std::cos(a)*60*zoom_,p.y+std::sin(a)*60*zoom_};dl->AddRect({node.x-12*zoom_,node.y-12*zoom_},{node.x+12*zoom_,node.y+12*zoom_},color,3,0,2);dl->AddLine(node,end,color,1.5f);}}
   if(roles&(Landing|Rose|Cold)){dl->AddCircle(end,e.radius*zoom_,color,32,e.age<e.delay?1.f:2.f);if(e.count)dl->AddCircle(end,e.radius*zoom_*(1-pulse*.5f),color,32,1.5f);}
   if(roles&(Blade|Fist)){const float a=e.angle+(e.count%2?.6f:-.6f);dl->PathArcTo(p,std::min(180.f,e.radius)*zoom_,a-.7f,a+.7f,18);dl->PathStroke(color,0,3.f);}
   if((roles&(Dragon|Thunder|Heavy))||e.pattern==1){dl->AddLine(p,end,color,2+2*pulse);dl->AddCircle(end,12*zoom_,color,16,2);}
   break;}
  case EffectKind::Resonance:{const auto color=e.resonance_id==int(Resonance::PoisonIgnition)||e.resonance_id==int(Resonance::DashFlame)?IM_COL32(232,154,74,int(190*fade)):IM_COL32(108,197,210,int(210*fade));const auto end=screen(e.end);dl->AddLine(p,end,color,e.pattern==1?8.f:2.f);dl->AddCircle(end,e.radius*zoom_*(e.pattern==1?.8f:1.f-fade*.35f),color,24,2.f);if(e.pattern==1)dl->AddCircleFilled(p,e.radius*zoom_*.65f,IM_COL32(232,154,74,int(24*fade)),20);break;}
  case EffectKind::ThunderPrison:
   if(e.pattern==1){dl->AddCircle(p,e.radius*zoom_,IM_COL32(181,214,252,int(180*std::min(1.f,fade*3))),24,1.5f);for(int mark=0;mark<3;++mark){float a=e.age*1.6f+mark*2*pi/3;dl->AddCircleFilled({p.x+std::cos(a)*e.radius*zoom_,p.y+std::sin(a)*e.radius*zoom_},2*zoom_,IM_COL32(208,237,255,200),8);}}break;
  case EffectKind::FrostArmor:
   if(e.pattern==0&&e.energy>0){dl->PathArcTo(p,33*zoom_,-pi*.5f,-pi*.5f+2*pi*std::clamp(e.energy/60.f,0.f,1.f),32);dl->PathStroke(IM_COL32(166,225,248,160),0,2);}break;
  case EffectKind::MiasmaDragon:
   if(e.pattern==0&&e.age<e.delay){sector(p,e.radius*zoom_,e.angle,.58f,IM_COL32(112,163,72,16),IM_COL32(170,213,114,110),1.5f);if(!low_effects_)for(int stream=0;stream<3;++stream){const float a=e.angle+(stream-1)*.28f;const Vec2 at=e.pos+Vec2{std::cos(a),std::sin(a)}*(e.radius*.58f);sprite(dl,"skill.poison_mist.zone",screen(at),zoom_*e.radius/220.f,e.age,false,IM_COL32(255,255,255,60));}}
   else if(e.pattern==1)dl->AddCircle(p,e.radius*zoom_,IM_COL32(181,218,117,int(190*fade)),28,2);break;
  case EffectKind::DivineVerdict:
   if(e.pattern==0){dl->AddCircle(p,e.radius*zoom_,IM_COL32(231,222,167,e.triggered?45:80),40,1);for(int mark=0;mark<std::clamp(e.count,0,3);++mark){const float a=-pi*.5f+mark*2*pi/3;const ImVec2 at{p.x+std::cos(a)*22*zoom_,p.y+std::sin(a)*22*zoom_};dl->AddQuadFilled({at.x,at.y-3*zoom_},{at.x+3*zoom_,at.y},{at.x,at.y+3*zoom_},{at.x-3*zoom_,at.y},IM_COL32(249,231,155,185));}}break;
  case EffectKind::PhoenixAssault:{
   const Vec2 direction=normalized(e.end-e.pos);if(length(direction)<.01f)break;const Vec2 side{-direction.y*35,direction.x*35};const float travel=std::clamp((e.age-.08f)/.42f,0.f,1.f);
   const Vec2 first=e.pos+(e.end-e.pos)*travel+side,second=e.end+(e.pos-e.end)*travel-side;
   if(!low_effects_){dl->AddLine(screen(e.pos+side),screen(e.end+side),IM_COL32(237,173,107,55),1);dl->AddLine(screen(e.end-side),screen(e.pos-side),IM_COL32(237,173,107,55),1);}
   for(Vec2 point:{first,second})dl->AddCircle(screen(point),e.radius*zoom_,IM_COL32(248,193,111,int(150*std::min(1.f,fade*3))),28,1.5f);break;}
  case EffectKind::DragonBreaker:
   if(e.count>0&&!e.triggered){const auto q=screen(e.end);dl->AddCircle(q,17*zoom_,IM_COL32(242,207,137,160),24,1.5f);if(e.age<.5f)dl->AddCircle(q,e.radius*zoom_,IM_COL32(238,202,132,int(120*std::clamp((.5f-e.age)/.25f,0.f,1.f))),40,2);}break;
  case EffectKind::FissureArrows:
   if(e.pattern==0)capsule(e.pos,e.end,e.radius,IM_COL32(178,147,98,e.triggered?13:5),IM_COL32(223,191,132,e.triggered?95:60),1);
   else{dl->AddCircle(p,e.radius*zoom_,IM_COL32(239,209,149,e.triggered?180:100),32,e.triggered?2.5f:1.2f);if(!e.triggered)dl->AddCircle(p,e.radius*zoom_*progress,IM_COL32(238,216,171,75),28,1);}break;
  case EffectKind::PoisonHuntingGround:
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(160,207,111,e.pattern?int(190*fade):90),40,e.pattern?2.5f:1.2f);if(e.pattern==0&&!e.triggered)sprite(dl,"skill.poison_mist.zone",p,zoom_*e.radius/75.f,e.age,false,IM_COL32(255,255,255,85));break;
  case EffectKind::DragonPhoenix:
   if(e.pattern>0){capsule(e.pos,e.end,e.radius,e.pattern%2?IM_COL32(232,229,170,e.triggered?22:6):IM_COL32(237,168,97,e.triggered?22:6),e.pattern%2?IM_COL32(243,232,187,e.triggered?185:90):IM_COL32(245,188,121,e.triggered?185:90),e.triggered?2.5f:1.2f);if(e.pattern>=3)dl->AddCircle(screen(e.end),e.radius*2.4f*zoom_,IM_COL32(239,212,147,e.triggered?145:70),32,1.5f);}break;
  case EffectKind::GlacialSniper:
   if(e.pattern==0&&!e.triggered){const auto q=screen(e.end);dl->AddLine(screen(game_.player.pos),q,IM_COL32(173,222,249,105),1.2f);dl->PathArcTo(q,13*zoom_,-pi*.5f,-pi*.5f+2*pi*progress,24);dl->PathStroke(IM_COL32(209,240,252,190),0,2);}
   else if(e.pattern==1){dl->AddCircle(p,e.radius*zoom_,IM_COL32(178,226,252,e.triggered?185:90),40,e.triggered?2.5f:1.2f);if(!e.triggered)dl->AddCircle(p,e.radius*zoom_*progress,IM_COL32(203,236,250,75),32,1);}break;
  case EffectKind::FlowingFire:
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(245,172,101,e.pattern?e.triggered?180:85:75),40,e.pattern&&e.triggered?2.5f:1.2f);
   if(e.pattern==0)sprite(dl,"skill.zone",p,zoom_*e.radius/135.f,e.age,false,IM_COL32(255,216,177,90));break;
  case EffectKind::BattleBlades:
   if(e.pattern>=10)sector(p,e.radius*zoom_,std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x),e.angle,IM_COL32(238,208,142,int((e.pattern==11?35:18)*fade)),IM_COL32(252,228,170,int(205*fade)),e.pattern==11?3.f:1.5f);break;
  case EffectKind::DragonFormation:
   if(e.pattern<2)capsule(e.pos,e.end,e.radius,IM_COL32(241,210,138,e.pattern==1?22:7),IM_COL32(250,226,166,e.pattern==1?180:85),e.pattern==1?2.5f:1.f);
   else dl->AddCircle(screen(e.end),e.radius*zoom_,IM_COL32(255,225,160,int(220*std::clamp((e.duration-e.age)/.4f,0.f,1.f))),48,3);break;
  case EffectKind::EchoShield:
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(191,226,245,int(210*fade)),48,3);break;
  case EffectKind::MountainBreaker:
   if(e.pattern<2)capsule(e.pos,e.end,e.radius,IM_COL32(168,124,74,e.pattern==1&&e.triggered?25:9),IM_COL32(237,203,146,e.pattern==1?(e.triggered?155:80):int(85*fade)),e.triggered?2.f:1.f);
   else dl->AddCircle(p,e.radius*zoom_,IM_COL32(246,216,151,int(220*fade)),48,3);break;
  case EffectKind::SpiritJudgement:
   if(e.pattern==0){for(int mark=0;mark<std::clamp(e.count,0,4);++mark){const float a=-pi*.5f+mark*pi*.5f;const ImVec2 at{p.x+std::cos(a)*18*zoom_,p.y+std::sin(a)*18*zoom_};dl->AddQuadFilled({at.x,at.y-3*zoom_},{at.x+3*zoom_,at.y},{at.x,at.y+3*zoom_},{at.x-3*zoom_,at.y},IM_COL32(244,224,150,180));}}
   else{dl->AddCircle(p,e.radius*zoom_,IM_COL32(244,222,160,e.pattern==2?205:100),48,e.pattern==2?3.f:1.5f);if(e.pattern==1){const float charge=std::clamp(1-(e.delay-e.age)/.6f,0.f,1.f);dl->PathArcTo(p,e.radius*zoom_,-pi*.5f,-pi*.5f+charge*2*pi,40);dl->PathStroke(IM_COL32(255,239,185,180),0,2);}}break;
  case EffectKind::AzureRift:case EffectKind::SiegeBolt:
   dl->AddCircle(p,e.radius*zoom_,e.kind==EffectKind::AzureRift?IM_COL32(193,222,248,e.triggered?195:90):IM_COL32(244,190,112,e.triggered?200:95),32,e.triggered?2.5f:1.2f);if(!e.triggered)dl->AddCircle(p,e.radius*zoom_*progress,IM_COL32(224,218,184,65),24,1);break;
  case EffectKind::Sanctuary:
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(226,225,176,e.pattern==2?(e.triggered?195:95):95),48,e.pattern==2&&e.triggered?2.5f:1.5f);
   if(e.pattern==1&&!low_effects_)sprite(dl,"skill.magic_vortex.cast",p,zoom_*e.radius/180.f,e.age,false,IM_COL32(255,255,255,75));break;
  case EffectKind::PoisonFlare:
   dl->AddCircle(p,e.radius*zoom_,e.pattern>=10?IM_COL32(243,183,102,e.triggered?195:100):IM_COL32(166,208,109,90),40,e.pattern>=10&&e.triggered?2.5f:1.2f);
   if(e.pattern<10)sprite(dl,"skill.poison_mist.zone",p,zoom_*e.radius/75.f,e.age,false,IM_COL32(255,255,255,e.pattern==0?85:40));break;
  case EffectKind::ThunderLance:
   capsule(e.pos,e.end,e.radius,IM_COL32(165,218,248,e.triggered?18:5),IM_COL32(186,227,251,e.triggered?190:90),e.triggered?2.5f:1.f);
   if(e.triggered)dl->AddCircle(screen(e.end),e.radius*zoom_*2.5f,IM_COL32(186,229,251,int(160*std::clamp((e.duration-e.age)/.4f,0.f,1.f))),32,2);break;
  case EffectKind::HeavenDragon:
   if(e.pattern==1)capsule(e.pos,e.end,e.radius,IM_COL32(230,229,188,e.triggered?18:5),IM_COL32(244,237,202,e.triggered?125:75),1.5f);break;
  case EffectKind::InfernoArray:
   if(e.pattern>0){dl->AddCircle(p,e.radius*zoom_,IM_COL32(244,178,107,e.pattern==1?140:100),40,1.5f);if(e.pattern==1)dl->AddCircle(p,e.radius*zoom_*progress,IM_COL32(250,210,150,85),32,1);else sprite(dl,"skill.zone",p,zoom_*e.radius/120.f,e.age-e.delay,false,IM_COL32(255,202,159,115));}break;
  case EffectKind::VenomVortex:case EffectKind::PurityWell:{
   const bool poison=e.kind==EffectKind::VenomVortex;const ImU32 color=poison?IM_COL32(174,214,119,e.pattern?190:100):IM_COL32(222,229,191,e.pattern?155:100);
   dl->AddCircle(p,e.radius*zoom_,color,48,e.pattern?2.5f:1.5f);
   if(!low_effects_&&(e.pattern==0||!poison))for(int arm=0;arm<3;++arm){for(int node=0;node<10;++node){const float t=node/9.f,a=-e.age*1.8f+arm*2*pi/3+t*1.4f,r=e.radius*zoom_*(.9f-.7f*t);dl->PathLineTo({p.x+std::cos(a)*r,p.y+std::sin(a)*r});}dl->PathStroke(color,0,1);}
   if(poison&&e.pattern==0)sprite(dl,"skill.poison_mist.zone",p,zoom_*e.radius/100.f,e.age,false,IM_COL32(255,255,255,75));break;}
  case EffectKind::FrostFire:
   if(e.pattern==2)dl->AddCircle(p,e.radius*zoom_,IM_COL32(233,216,185,int(200*fade)),32,2.5f);
   else for(int side=0;side<2;++side){const auto q=side?screen(e.end):p;const bool cold=(e.pattern==0?side==0:side==1);dl->AddCircle(q,e.radius*zoom_,cold?IM_COL32(160,219,248,115):IM_COL32(243,176,114,115),40,1.5f);sprite(dl,cold?"skill.frost":"skill.zone",q,zoom_*e.radius/(cold?155.f:135.f),e.age,false,cold?IM_COL32(213,240,255,100):IM_COL32(255,209,173,100));}break;
  case EffectKind::ThousandArrows:case EffectKind::PrismShot:
   if(!e.triggered){const auto q=screen(e.end);dl->AddLine(p,q,IM_COL32(214,222,189,e.kind==EffectKind::PrismShot?100:55),1);dl->AddCircle(q,8*zoom_,IM_COL32(232,231,189,120),20,1.2f);}break;
  case EffectKind::EagleExecution:
   if(e.pattern>=1){const auto q=screen(e.end);dl->AddCircle(q,16*zoom_,IM_COL32(245,222,138,185),24,1.8f);for(int arm=0;arm<4;++arm){float a=arm*pi*.5f;dl->AddLine({q.x+std::cos(a)*12*zoom_,q.y+std::sin(a)*12*zoom_},{q.x+std::cos(a)*21*zoom_,q.y+std::sin(a)*21*zoom_},IM_COL32(245,222,138,185),1.5f);}}break;
  case EffectKind::FrozenBarrage:
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(174,224,249,e.pattern>=10?int(200*fade):100),48,e.pattern>=10?2.5f:1.2f);break;
  case EffectKind::TrickDecoy:
   dl->AddCircle(p,e.radius*zoom_,e.pattern==3?IM_COL32(246,189,112,int(195*std::clamp((e.duration-e.age)/.3f,0.f,1.f))):IM_COL32(192,204,144,e.pattern==2?105:65),40,e.pattern==3?2.5f:1.f);
   if(e.pattern==2){const float r=8*zoom_;dl->AddQuadFilled({p.x,p.y-r},{p.x+r,p.y},{p.x,p.y+r},{p.x-r,p.y},IM_COL32(102,79,42,180));dl->AddLine({p.x-r,p.y},{p.x+r,p.y},IM_COL32(235,198,122,e.age>=e.delay?190:90),2);}break;
  case EffectKind::BattleFocus:{const float axis=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x);sector(p,e.radius*zoom_,axis,e.angle,IM_COL32(216,195,116,e.triggered?32:13),IM_COL32(243,223,157,e.triggered?205:100),e.triggered?3.f:1.f);break;}
  case EffectKind::SpearCombo:case EffectKind::DragonThrust:
   capsule(e.pos,e.end,e.radius,IM_COL32(154,201,241,e.pattern?18:5),IM_COL32(185,222,250,e.pattern?155:65),e.pattern?2.f:1.f);break;
  case EffectKind::Judgement:case EffectKind::HolyLight:{
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(245,225,157,e.triggered?190:95),48,e.triggered?3.f:1.2f);
   if(e.age<e.delay){dl->PathArcTo(p,e.radius*zoom_,-pi*.5f,-pi*.5f+progress*2*pi,40);dl->PathStroke(IM_COL32(255,238,186,160),0,2);}break;}
  case EffectKind::EarthShatter:
   if(e.pattern==0)capsule(e.pos,e.end,e.radius,0,IM_COL32(220,185,135,65),1);
   else{dl->AddCircle(p,e.radius*zoom_,IM_COL32(231,195,133,e.age<e.delay?80:135),32,1.5f);if(e.age>=e.delay)dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(136,99,60,28));}break;
  case EffectKind::PoisonMist:
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(158,206,105,110),48,1.2f);sprite(dl,"skill.poison_mist.zone",p,zoom_*e.radius/70.f,e.age,false,IM_COL32(255,255,255,low_effects_?60:95));break;
  case EffectKind::IceLance:
   if(e.pattern>0)dl->AddCircle(p,e.radius*zoom_,IM_COL32(165,222,247,int(140*std::min(1.f,fade*4))),24,1.5f);break;
  case EffectKind::HolyDragon:
   if(e.pattern>0)capsule(e.pos,e.end,e.radius,IM_COL32(243,220,152,e.triggered?22:8),IM_COL32(255,235,179,e.triggered?185:90),e.triggered?2.5f:1.f);break;
  case EffectKind::FireBeast:
   if(e.pattern>0)sector(p,e.radius*zoom_,std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x),e.angle,IM_COL32(241,148,75,e.triggered?25:9),IM_COL32(249,191,119,e.triggered?180:80),e.triggered?2.5f:1.f);break;
  case EffectKind::MagicVortex:{
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(192,186,239,e.pattern?190:100),48,e.pattern?3.f:1.5f);
   if(!low_effects_&&e.pattern==0)for(int arm=0;arm<3;++arm){for(int point=0;point<12;++point){float t=point/11.f,a=-e.age*2.2f+arm*2*pi/3+t*1.4f,r=e.radius*zoom_*(.92f-.72f*t);dl->PathLineTo({p.x+std::cos(a)*r,p.y+std::sin(a)*r});}dl->PathStroke(IM_COL32(191,184,233,95),0,1.2f);}break;}
  case EffectKind::StormShot:case EffectKind::Sniper:{
   const auto q=screen(e.end);const bool locked=e.kind==EffectKind::Sniper&&e.age>=.5f;dl->AddLine(p,q,IM_COL32(200,224,235,locked?140:55),locked?1.8f:1.f);dl->AddCircle(q,(locked?12.f:8.f)*zoom_,IM_COL32(223,231,181,locked?180:100),20,1.5f);break;}
  case EffectKind::IceArrow:case EffectKind::BlastArrow:
   if(e.pattern>0){const bool frost=e.kind==EffectKind::IceArrow;dl->AddCircle(p,e.radius*zoom_,frost?IM_COL32(177,226,252,int(180*fade)):IM_COL32(250,180,105,int(190*fade)),32,2);}
   break;
  case EffectKind::Decoy:
   dl->AddCircle(p,e.radius*zoom_,e.pattern?IM_COL32(241,192,117,int(180*fade)):IM_COL32(182,198,133,75),48,e.pattern?2.5f:1.f);break;
  case EffectKind::FrostStorm:{
   dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(93,177,211,24));dl->AddCircle(p,e.radius*zoom_,IM_COL32(153,227,248,185),56,2);
   for(int j=0;j<6;++j){float a=j*pi/3+e.age*.2f;dl->AddLine({p.x+std::cos(a)*e.radius*zoom_*.78f,p.y+std::sin(a)*e.radius*zoom_*.78f},{p.x+std::cos(a)*e.radius*zoom_,p.y+std::sin(a)*e.radius*zoom_},IM_COL32(198,235,255,170),2);}
   sprite(dl,"skill.rune",p,zoom_*.45f,e.age,false,IM_COL32(161,216,255,100));break;}
  case EffectKind::HunterMark:{
   const float r=e.radius*zoom_;for(int quadrant=0;quadrant<4;++quadrant){float a=quadrant*pi*.5f;dl->PathArcTo(p,r,a-.25f,a+.25f,6);dl->PathStroke(IM_COL32(255,208,105,210),0,2);}
   break;}
  case EffectKind::ArrowTrap:{
   const bool armed=e.age>=e.delay;const float life=e.triggered?std::clamp((e.duration-e.age)/.28f,0.f,1.f):1.f;
   const ImU32 color=armed?IM_COL32(241,199,101,int(195*life)):IM_COL32(144,170,154,140);
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(224,191,103,int((armed?100:45)*life)),32,1.2f);
   if(!armed){dl->PathArcTo(p,e.radius*zoom_,-pi*.5f,-pi*.5f+progress*2*pi,32);dl->PathStroke(color,0,2);}
   const float r=10*zoom_;dl->AddQuadFilled({p.x,p.y-r},{p.x+r,p.y},{p.x,p.y+r},{p.x-r,p.y},IM_COL32(85,70,40,int(180*life)));
   dl->AddLine({p.x-r,p.y},{p.x+r,p.y},color,2);dl->AddLine({p.x,p.y-r},{p.x,p.y+r},color,2);
   for(float side:{-1.f,1.f}){dl->AddLine({p.x-r,p.y+side*6*zoom_},{p.x-r*.5f,p.y},color,1.5f);dl->AddLine({p.x+r,p.y+side*6*zoom_},{p.x+r*.5f,p.y},color,1.5f);}
   if(e.triggered)dl->AddCircle(p,e.radius*zoom_,IM_COL32(255,222,147,int(220*life)),32,3);break;}
  case EffectKind::OrbitalOrb:case EffectKind::ArcaneRing:case EffectKind::SwordCyclone:
   dl->AddCircle(p,e.radius*zoom_,e.kind==EffectKind::SwordCyclone?IM_COL32(223,204,159,65):IM_COL32(167,191,248,70),48,1);break;
  case EffectKind::FlameDance:{
   if(e.pattern==0){dl->AddCircle(p,e.radius*zoom_,IM_COL32(239,182,108,70),48,1);break;}
   // Every short capsule is one surviving core fire-path segment, including its endpoint radius.
   const auto q=screen(e.end);const float axis=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x),radius=e.radius*zoom_;
   auto path=[&](){dl->PathArcTo(p,radius,axis+pi*.5f,axis+pi*1.5f,12);dl->PathArcTo(q,radius,axis-pi*.5f,axis+pi*.5f,12);};
   path();dl->PathFillConvex(IM_COL32(226,134,63,int(30*fade)));path();dl->PathStroke(IM_COL32(242,181,105,int(105*fade)),ImDrawFlags_Closed,1.2f);
   if(!(low_effects_||simulation_clock_.backlog_seconds()>.25)){const int flames=std::clamp(int(length(e.end-e.pos)/36)+1,1,3);for(int flame=0;flame<flames;++flame){const auto at=screen(e.pos+(e.end-e.pos)*((flame+.5f)/flames));sprite(dl,"skill.flame_dance.cast",at,zoom_*std::clamp(e.radius/60.f,.2f,.5f),e.age,false,IM_COL32(255,255,255,int(125*fade)),0,false);}}
   break;}
  case EffectKind::FeatherStorm:{
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(219,219,161,75),48,1);
   const int feathers=std::clamp(int(e.energy),0,6);for(int feather=0;feather<feathers;++feather){const float angle=-pi*.5f+feather*pi/3;const ImVec2 at{p.x+std::cos(angle)*e.radius*zoom_*.36f,p.y+std::sin(angle)*e.radius*zoom_*.36f};dl->AddCircleFilled(at,2.5f*zoom_,IM_COL32(249,225,150,190),8);}break;}
  case EffectKind::StarBow:{
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(168,214,246,110),48,1.5f);
   if(e.age<e.delay){dl->PathArcTo(p,e.radius*zoom_,-pi*.5f,-pi*.5f+progress*2*pi,40);dl->PathStroke(IM_COL32(189,230,255,165),0,2);}
   if(!(low_effects_||simulation_clock_.backlog_seconds()>.25))sprite(dl,"skill.rune",p,zoom_*e.radius/170.f,e.age,false,IM_COL32(186,220,252,85));break;}
  case EffectKind::GravityWell:{
   const float radius=e.radius*zoom_;dl->AddCircleFilled(p,radius,IM_COL32(114,106,182,17));dl->AddCircle(p,radius,IM_COL32(170,157,235,130),48,1.5f);
   if(!(low_effects_||simulation_clock_.backlog_seconds()>.25))for(int arm=0;arm<3;++arm){for(int point=0;point<10;++point){float t=point/9.f,angle=-e.age*1.7f+arm*2*pi/3+t*1.25f,r=radius*(.92f-.75f*t);dl->PathLineTo({p.x+std::cos(angle)*r,p.y+std::sin(angle)*r});}dl->PathStroke(IM_COL32(186,173,244,125),0,1.2f);}
   sprite(dl,"skill.rune",p,zoom_*.32f,e.age,false,IM_COL32(192,183,239,90));break;}
  case EffectKind::HeavyStrike:{
   const float axis=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x),impact=e.triggered?std::clamp((e.duration-e.age)/std::max(.01f,e.duration-e.delay),0.f,1.f):1.f;
   sector(p,e.radius*zoom_,axis,e.angle,IM_COL32(221,193,122,int((e.triggered?27:12+13*progress)*impact)),IM_COL32(242,215,147,int(165*impact)),e.triggered?3.f:1.5f);
   if(!e.triggered){dl->PathArcTo(p,e.radius*zoom_*.28f,axis-e.angle,axis-e.angle+2*e.angle*progress,18);dl->PathStroke(IM_COL32(255,221,143,220),0,3);}
   break;}
  case EffectKind::Bulwark:
   dl->PathArcTo(p,e.radius*zoom_,-pi*.85f,pi*.85f,32);dl->PathStroke(IM_COL32(133,206,236,int(180*fade)),0,3);break;
  case EffectKind::PhoenixRain:{
   const bool raining=e.age>=e.delay&&e.age<e.secondary_tick,burning=e.age>=e.secondary_tick;
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(242,178,92,burning?100:145),48,1.5f);
   if(e.age<e.delay){dl->PathArcTo(p,e.radius*zoom_,-pi*.5f,-pi*.5f+progress*2*pi,40);dl->PathStroke(IM_COL32(255,216,133,195),0,2);}
   if(raining){
    dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(245,146,70,18));const int arrows=(low_effects_||simulation_clock_.backlog_seconds()>.25)?3:7;
    for(int arrow=0;arrow<arrows;++arrow){const float phase=std::fmod((e.age-e.delay)*4+arrow*.143f,1.f),angle=arrow*2.399963f,distance=e.radius*std::sqrt((arrow+.5f)/arrows)*.82f;const Vec2 hit=e.pos+Vec2{std::cos(angle)*distance,std::sin(angle)*distance};auto q=screen(hit);q.y-=(1-phase)*65*zoom_;
     sprite(dl,"skill.rain",q,zoom_*.55f,e.age,false,IM_COL32(255,221,172,175),pi*.5f-forward_angles_["skill.rain"]);
     if(!(low_effects_||simulation_clock_.backlog_seconds()>.25)&&phase>.78f)sprite(dl,"skill.rain_hit",screen(hit),zoom_*.32f,(phase-.78f)*1.4f,false,IM_COL32(255,209,145,125),0,false);
    }
   }else if(burning){dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(220,118,52,23));const int patches=(low_effects_||simulation_clock_.backlog_seconds()>.25)?1:3;for(int patch=0;patch<patches;++patch){float angle=patch*2*pi/3;auto q=screen(e.pos+Vec2{std::cos(angle),std::sin(angle)}*(patches>1?e.radius*.43f:0));sprite(dl,"skill.zone",q,zoom_*.5f,e.age-e.secondary_tick,false,IM_COL32(255,171,108,125));}}
   break;}
  case EffectKind::MoltenStar:{
   if(e.pattern==0){
    dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(227,127,57,22));dl->AddCircle(p,e.radius*zoom_,IM_COL32(235,164,97,110),48,1.5f);
    sprite(dl,"skill.zone",p,zoom_*.75f,e.age,false,IM_COL32(255,169,98,110));
    const float heat=std::clamp(e.energy/4.f,0.f,1.f);for(int spark=0;spark<4;++spark){const float a=-pi*.5f+spark*pi*.5f;const ImVec2 q{p.x+std::cos(a)*17*zoom_,p.y+std::sin(a)*17*zoom_};dl->AddCircleFilled(q,2.8f*zoom_,spark<heat*4?IM_COL32(255,211,117,220):IM_COL32(115,87,57,125),8);}
   }else if(!e.triggered){dl->AddCircle(p,e.radius*zoom_,IM_COL32(255,194,110,190),48,2);dl->AddCircle(p,e.radius*zoom_*progress,IM_COL32(255,211,154,100),40,1.5f);}
   else{const float impact=std::clamp((e.duration-e.age)/std::max(.01f,e.duration-e.delay),0.f,1.f);dl->AddCircle(p,e.radius*zoom_,IM_COL32(255,202,117,int(205*impact)),48,3);sprite(dl,"skill.meteor",p,zoom_*.7f,e.age-e.delay,false,IM_COL32(255,224,189,int(170*impact)),0,false);}
   break;}
  case EffectKind::Charge:
   if(e.source==Skill::DashBreaker){auto q=screen(e.end);const auto direction=normalized(e.end-e.pos);const ImVec2 side{-direction.y*e.radius*zoom_,direction.x*e.radius*zoom_};dl->AddQuadFilled({p.x+side.x,p.y+side.y},{q.x+side.x,q.y+side.y},{q.x-side.x,q.y-side.y},{p.x-side.x,p.y-side.y},IM_COL32(255,207,117,int(18*fade)));dl->AddLine({p.x+side.x,p.y+side.y},{q.x+side.x,q.y+side.y},IM_COL32(250,222,163,int(110*fade)),1.5f);dl->AddLine({p.x-side.x,p.y-side.y},{q.x-side.x,q.y-side.y},IM_COL32(250,222,163,int(110*fade)),1.5f);}break;
  case EffectKind::Zone:
   for(int n=0;n<(low_effects_?3:5);++n){float a=n*2.39996f;Vec2 at=e.pos+Vec2{std::cos(a),std::sin(a)}*(n?e.radius*.52f:0);battle_sprite(dl,"battle.field.fire",at,e.radius*.9f,e.age+n*.19f,IM_COL32(255,224,196,120));}
   dl->AddCircle(p,e.radius*zoom_,IM_COL32(224,172,116,65),40,1);break;
  case EffectKind::Rain:
   dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(205,180,85,30));
   for(int arrow=0;arrow<7;++arrow){
    float phase=std::fmod(e.age*2.8f+arrow*.143f,1.f),angle=arrow*2.399963f;
    float distance=e.radius*std::sqrt((arrow+.5f)/7.f)*.85f;
    auto q=screen(e.pos+Vec2{std::cos(angle)*distance,std::sin(angle)*distance});q.y-=(1-phase)*65*zoom_;
    sprite(dl,"skill.rain",q,zoom_*.7f,e.age,false,IM_COL32(255,255,255,175),pi*.5f-forward_angles_["skill.rain"]);
   }break;
  case EffectKind::Frost:dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(100,190,255,35));sprite(dl,"skill.frost",p,zoom_,e.age,false,IM_COL32(255,255,255,175),0,false);break;
  case EffectKind::Meteor:
   if(e.age<e.delay){dl->AddCircle(p,e.radius*zoom_,IM_COL32(255,164,72,155),40,2);dl->AddCircle(p,e.radius*zoom_*progress,IM_COL32(255,198,106,125),40,2);}
   else sprite(dl,"skill.meteor",p,zoom_,e.age-e.delay,false,IM_COL32(255,255,255,175),0,false);break;
  case EffectKind::Rune:dl->AddCircleFilled(p,e.radius*zoom_,IM_COL32(167,124,238,25));sprite(dl,"skill.rune",p,zoom_,e.age,false,IM_COL32(255,255,255,140));break;
  case EffectKind::Nova:
   if(e.environment){dl->AddCircleFilled(p,e.radius*zoom_*(1-.2f*fade),IM_COL32(255,137,67,int(75*fade)),48);dl->AddCircle(p,e.radius*zoom_*(1-.25f*fade),IM_COL32(255,196,118,int(240*fade)),48,4);}
   else{sprite(dl,"skill.nova",p,zoom_,e.age,false,IM_COL32(255,255,255,185),0,false);dl->AddCircle(p,e.radius*zoom_*(1-.25f*fade),IM_COL32(135,208,255,int(180*fade)),48,3);}break;
  case EffectKind::Warning:{
   const ImU32 color=e.hostile?IM_COL32(255,111,83,230):IM_COL32(230,190,110,170);
   const ImU32 fill=e.hostile?IM_COL32(255,65,55,40+int(35*progress)):IM_COL32(230,190,110,25);
   if(e.pattern==4){const float axis=std::atan2(e.end.y,e.end.x);sector(p,e.radius*zoom_,axis,e.angle,fill,color,2.5f);}
   else if(e.pattern==1){
    const auto q=screen(e.end);const Vec2 direction=normalized(e.end-e.pos);
    const ImVec2 offset{-direction.y*e.radius*zoom_,direction.x*e.radius*zoom_};
    dl->AddQuadFilled({p.x+offset.x,p.y+offset.y},{q.x+offset.x,q.y+offset.y},{q.x-offset.x,q.y-offset.y},{p.x-offset.x,p.y-offset.y},fill);
    dl->AddLine({p.x+offset.x,p.y+offset.y},{q.x+offset.x,q.y+offset.y},color,2);
    dl->AddLine({p.x-offset.x,p.y-offset.y},{q.x-offset.x,q.y-offset.y},color,2);
    dl->AddTriangleFilled(q,{q.x-direction.x*18-direction.y*10,q.y-direction.y*18+direction.x*10},{q.x-direction.x*18+direction.y*10,q.y-direction.y*18-direction.x*10},color);
   }else{
    dl->AddCircleFilled(p,e.radius*zoom_*progress,fill,48);dl->AddCircle(p,e.radius*zoom_,color,48,3);
    if(e.pattern==2)for(int ray=0;ray<10;++ray){float angle=ray*2*pi/10;dl->AddLine({p.x+std::cos(angle)*e.radius*zoom_,p.y+std::sin(angle)*e.radius*zoom_},{p.x+std::cos(angle)*(e.radius+30)*zoom_,p.y+std::sin(angle)*(e.radius+30)*zoom_},color,2);}
   }break;
  }
  case EffectKind::Orbit:dl->AddCircle(p,e.radius*zoom_,IM_COL32(185,219,255,65),40,1);break;
  case EffectKind::Retaliate:dl->AddCircle(p,e.radius*zoom_*(1-.2f*fade),IM_COL32(255,215,125,int(180*fade)),32,3);break;
  default:break;
  }
  if(!e.hostile&&e.source!=Skill::Count)fade_vertices(effect_vertex_start,friendly_effect_alpha_*(.35f+.65f*flash_strength_)*((low_effects_||simulation_clock_.backlog_seconds()>.25)?.65f:1.f));
 }
 const auto& encounter=game_.director_state;
 const bool pressure_escape=game_.pressure_active(PressureKind::Encirclement);const bool escape_open=pressure_escape||(game_.config.enable_director&&encounter.kind==EncounterKind::Encirclement&&(encounter.phase==EncounterPhase::Telegraph||encounter.phase==EncounterPhase::Active));
 if(escape_open){
  const auto p=screen(game_.player.pos);const float radius=126*zoom_;
  const float half=pressure_escape?std::max(.65f,game_.config.director_config.escape_half_angle):std::max(.2f,encounter.escape_half_angle),angle=pressure_escape?game_.pressure_event.escape_angle:encounter.escape_angle;
  dl->PathArcTo(p,radius,angle+half,angle+2*pi-half,56);dl->PathStroke(IM_COL32(255,145,82,125),0,2);
  dl->PathArcTo(p,radius,angle-half,angle+half,16);dl->PathStroke(IM_COL32(128,244,183,230),0,5);
  for(float sign:{-1.f,1.f}){float a=angle+sign*half;dl->AddLine({p.x+std::cos(a)*74*zoom_,p.y+std::sin(a)*74*zoom_},{p.x+std::cos(a)*156*zoom_,p.y+std::sin(a)*156*zoom_},IM_COL32(128,244,183,155),2);}
  const Vec2 direction{std::cos(angle),std::sin(angle)};const auto q=screen(game_.player.pos+direction*151);
  dl->AddTriangleFilled(q,{q.x-direction.x*20-direction.y*10,q.y-direction.y*20+direction.x*10},{q.x-direction.x*20+direction.y*10,q.y-direction.y*20-direction.x*10},IM_COL32(128,244,183,235));
 }
 for(const auto& item:game_.items){
  auto p=screen(item.pos);if(!visible(p,70))continue;
  static const char* keys[]={"item.heal","item.magnet","item.fury","item.shield","item.bomb","item.double_xp","item.power","item.reset","item.chest"};
  ImU32 color=item.kind==ItemKind::Health?IM_COL32(247,111,116,90):item.kind==ItemKind::Magnet?IM_COL32(100,220,255,90):IM_COL32(255,206,91,90);
  dl->AddCircleFilled(p,17*zoom_,color,20);sprite(dl,keys[std::clamp(int(item.kind),0,8)],{p.x,p.y+std::sin(clock_*5+item.age)*3},zoom_*.75f,clock_);
 }
 if(game_.special_event.kind!=SpecialEventKind::None){
  const auto& e=game_.special_event;auto p=screen(e.pos);
  ImU32 color=e.kind==SpecialEventKind::Blessing?IM_COL32(118,236,162,220):e.kind==SpecialEventKind::BountyElite?IM_COL32(255,134,105,220):gold;
  float radius=e.kind==SpecialEventKind::Rift?95.f:38.f;
  dl->AddCircle(p,radius*zoom_,color,40,3);
  if(e.kind==SpecialEventKind::Rift){dl->AddCircleFilled(p,radius*zoom_,IM_COL32(148,103,230,25));dl->PathArcTo(p,(radius+5)*zoom_,-pi*.5f,-pi*.5f+2*pi*std::clamp(e.progress/8.f,0.f,1.f),40);dl->PathStroke(IM_COL32(210,175,255,210),0,4);}
  if(e.kind==SpecialEventKind::GuardedChest)sprite(dl,"event.chest",p,zoom_,clock_);
  else{dl->AddCircleFilled(p,11*zoom_,color);dl->AddLine({p.x-14*zoom_,p.y},{p.x+14*zoom_,p.y},ivory,2);dl->AddLine({p.x,p.y-14*zoom_},{p.x,p.y+14*zoom_},ivory,2);}
 }
 for(const auto& g:game_.gems){auto p=screen(g.pos);if(!visible(p,20))continue;float r=3.5f*zoom_;dl->AddCircleFilled(p,r*2,IM_COL32(96,225,215,25));dl->AddQuadFilled({p.x,p.y-r},{p.x+r,p.y},{p.x,p.y+r},{p.x-r,p.y},IM_COL32(124,236,215,255));}

 draw_skill_visuals(dl,false);
 // One primitive pass for all shadows avoids a font-texture switch for every monster.
 for(const auto& e:game_.enemies)if(visible(screen(e.pos))){auto p=screen(e.pos);dl->AddEllipseFilled({p.x,p.y+3*zoom_},{e.radius*zoom_,(e.kind==EnemyKind::Boss?8.f:5.f)*zoom_},IM_COL32(0,0,0,70));}
 for(const auto& object:game_.scene_objects)if(object.kind!=SceneObjectKind::Mud&&object.state!=SceneObjectState::Broken&&visible(screen(object.pos))){auto p=screen(object.pos);dl->AddEllipseFilled({p.x,p.y+3*zoom_},{object.radius*zoom_,5*zoom_},IM_COL32(0,0,0,70));}
 auto player_pos=screen(game_.player.pos);
 dl->AddEllipseFilled({player_pos.x,player_pos.y+4*zoom_},{18*zoom_,6*zoom_},IM_COL32(0,0,0,95));
 if(game_.player.invulnerable>0||(game_.shield_remaining>0&&!game_.buffs_suppressed()))dl->AddCircle(player_pos,24*zoom_,IM_COL32(179,231,255,150),24,2);
 if(game_.shield_remaining>0&&!game_.buffs_suppressed())dl->AddCircle(player_pos,30*zoom_,IM_COL32(120,212,255,200),32,3);
 if(game_.skills[int(Skill::DragonArmor)]>0){dl->PathArcTo(player_pos,35*zoom_,-pi*.5f,-pi*.5f+2*pi*std::clamp(game_.dragon_soul/100,0.f,1.f),40);dl->PathStroke(IM_COL32(255,194,102,210),0,2.5f);}
 if(game_.guard_charge>0&&!game_.buffs_suppressed()){const float full=game_.skills[int(Skill::Bulwark)]>0?20.f+4*game_.skills[int(Skill::Bulwark)]:game_.skills[int(Skill::FrostArmor)]>0?8.f+2*game_.skills[int(Skill::FrostArmor)]:20.f,fraction=std::clamp(game_.guard_charge/std::max(1.f,full),0.f,1.f);for(int segment=0;segment<5;++segment){float fill=std::clamp(fraction*5-segment,0.f,1.f);if(fill<=0)continue;float start=-pi*.5f+segment*2*pi/5;dl->PathArcTo(player_pos,39*zoom_,start+.05f,start+.05f+(2*pi/5-.12f)*fill,10);dl->PathStroke(IM_COL32(135,222,230,200),0,2.5f);}}

 struct Actor {float y;const Enemy* enemy=nullptr;const SceneObject* object=nullptr;const Effect* death=nullptr;const DangerZone* altar=nullptr;const Effect* summon=nullptr;};
 std::vector<Actor> order;order.reserve(game_.enemies.size()+game_.scene_objects.size()+game_.effects.size()+1);
 for(const auto& e:game_.enemies)if(visible(screen(e.pos)))order.push_back({e.pos.y,&e});
 for(const auto& object:game_.scene_objects)if(object.kind!=SceneObjectKind::Mud&&visible(screen(object.pos)))order.push_back({object.pos.y,nullptr,&object});
 for(const auto& e:game_.effects)if(e.kind==EffectKind::Death&&visible(screen(e.pos)))order.push_back({e.pos.y,nullptr,nullptr,&e});
 for(const auto& zone:game_.danger_zones)if(zone.kind==DangerKind::Altar&&zone.hp>0&&zone.age<zone.duration&&visible(screen(zone.pos)))order.push_back({zone.pos.y,nullptr,nullptr,nullptr,&zone});
 for(const auto& e:game_.effects)if(battle_actor(e))order.push_back({e.pos.y,nullptr,nullptr,nullptr,nullptr,&e});
 order.push_back({game_.player.pos.y});
 const bool crowded=!legacy_visuals_&&game_.enemies.size()>120;
 auto actor_priority=[&](const Actor& a){return !crowded?0:a.enemy&&a.enemy->kind==EnemyKind::Boss?1:!a.enemy&&!a.object&&!a.death&&!a.altar&&!a.summon?2:0;};
 std::stable_sort(order.begin(),order.end(),[&](const Actor& a,const Actor& b){int pa=actor_priority(a),pb=actor_priority(b);return pa!=pb?pa<pb:a.y<b.y;});
 struct EnemyVisualSettings{float scale=1;Vec2 offset;};
 static const auto enemy_visuals=[](){std::map<std::string,EnemyVisualSettings> result;for(const char* file:{"assets/enemy_appearances.json","assets/expansion/enemies.json"})try{std::ifstream input(file);nlohmann::json data;if(input){input>>data;for(auto it=data.at("visuals").begin();it!=data.at("visuals").end();++it){const auto& v=it.value();EnemyVisualSettings settings;settings.scale=v.at("suggested_render_scale").get<float>();settings.offset={v.at("source_origin_to_ground_px").at(0).get<float>(),v.at("source_origin_to_ground_px").at(1).get<float>()};result[it.key()]=settings;}}}catch(const std::exception&){}return result;}();
 auto enemy_key=[&](EnemyKind kind,BossProfile profile,int visual){return kind==EnemyKind::Boss?Game::boss_sprite_key(profile):Game::enemy_visual_key(visual);};
 auto enemy_scale=[&](EnemyKind kind,BossProfile profile,int visual){auto found=enemy_visuals.find(enemy_key(kind,profile,visual));return zoom_*(kind==EnemyKind::Minion?.65f:1.f)*(found!=enemy_visuals.end()?found->second.scale:kind==EnemyKind::Boss?Game::boss_render_scale(profile):Game::enemy_visual_scale(visual));};
 auto enemy_origin=[&](Vec2 position,EnemyKind kind,BossProfile profile,int visual){auto found=enemy_visuals.find(enemy_key(kind,profile,visual));return screen(found==enemy_visuals.end()?position:position-found->second.offset*found->second.scale);};
 for(const auto& actor:order){
  if(actor.summon){draw_battle_summon(dl,*actor.summon);continue;}
  if(actor.altar){pressure_altar(dl,*actor.altar);continue;}
  if(actor.object){draw_prop(*actor.object);continue;}
  if(actor.death){const auto& e=*actor.death;sprite(dl,std::string(enemy_key(e.enemy_kind,e.boss_profile,e.enemy_visual))+".die",enemy_origin(e.pos,e.enemy_kind,e.boss_profile,e.enemy_visual),enemy_scale(e.enemy_kind,e.boss_profile,e.enemy_visual),e.age,false,0xffffffffu,0,false);continue;}
  if(actor.enemy){
   const auto& e=*actor.enemy;
   if(e.abilities.shadow_class>=0){const char* action=e.hit_flash>0?"hit":e.windup>0?"idle":"move";sprite(dl,player_sprite(e.abilities.shadow_class,action),screen(e.pos),zoom_,e.age,!e.facing_left,e.hit_flash>0?IM_COL32(255,175,169,245):IM_COL32(157,125,194,235));continue;}
   const char* animation=e.hit_flash>0?"hit":e.kind==EnemyKind::Boss&&(e.windup>0||e.recovery_remaining>0)?"idle":"move";
   bool attacking=(e.kind==EnemyKind::Boss&&(e.windup>0||e.action_remaining>0))||(e.kind==EnemyKind::Ranger&&e.windup>0);
   if(attacking){const int variant=e.kind==EnemyKind::Boss&&e.boss_attack>=200?boss_art_actions[int(e.boss_profile)][(e.boss_attack-200)%3]:1;const char* action=variant==3?"attack3":variant==2?"attack2":variant==1?"attack1":"idle";std::string prefix=enemy_key(e.kind,e.boss_profile,e.visual);if(sprites_.count(prefix+"."+action))animation=action;else if(sprites_.count(prefix+".attack1"))animation="attack1";}
   const ImU32 tint=e.hit_flash>0?IM_COL32(255,190,170,255):e.slow_remaining>0?IM_COL32(150,210,255,255):e.buff_remaining>0?IM_COL32(255,205,160,255):0xffffffffu;
   float pose_time=e.attack_animation_time;
   if(attacking&&e.kind==EnemyKind::Boss&&e.boss_attack>=200){int move=(e.boss_attack-200)%3;float release=boss_art_releases[int(e.boss_profile)][move],warning=boss_combat_definition(e.boss_profile).moves[move].windup;if(release>0)pose_time=pose_time<warning?pose_time*release/warning:release+pose_time-warning;}
   Vec2 leap=e.kind==EnemyKind::Boss&&e.action_step==2&&e.action_remaining>0?Vec2{0,-55.f*std::sin(pi*std::clamp((e.action_duration-e.action_remaining)/std::max(.01f,e.action_tick),0.f,1.f))}:Vec2{};
   sprite(dl,std::string(enemy_key(e.kind,e.boss_profile,e.visual))+"."+animation,enemy_origin(e.pos+leap,e.kind,e.boss_profile,e.visual),enemy_scale(e.kind,e.boss_profile,e.visual),attacking?pose_time:e.age,!e.facing_left,tint,0,!attacking);
  }else{
   const std::string animation=game_.state==State::Defeat?"die":hurt_timer_>0?"hit":game_.player.moving?"move":"idle";
   sprite(dl,player_sprite(int(game_.character),animation.c_str()),player_pos,zoom_,hurt_timer_>0?hurt_age_:game_.player.animation_time,!game_.player.facing_left);
  }
 }
 for(const auto& drop:game_.gear_drops){const auto at=screen(drop.pos);sprite(dl,"equipment.icon.3",at,zoom_*.65f,0,false);dl->AddCircle(at,13*zoom_,IM_COL32(244,201,122,160),16,1);}
 for(const auto& p:game_.projectiles){
  if(draw_skill_projectile(dl,p))continue;
  int projectile_vertex_start=dl->VtxBuffer.Size;auto q=screen(p.pos);auto tail=screen(p.pos-p.velocity*.045f);dl->AddLine(tail,q,p.skill==Skill::StarBow?IM_COL32(182,225,255,120):IM_COL32(255,211,117,130),3);
  std::string key=p.skill==Skill::FeatherStorm?"skill.feather_storm.projectile":p.skill==Skill::StarBow?"skill.star_bow.projectile":(p.skill==Skill::Pierce||p.skill==Skill::StarReturn)?"skill.pierce":(p.skill==Skill::Arrow||p.skill==Skill::Spread||p.skill==Skill::Ricochet||p.skill==Skill::HunterVolley||p.skill==Skill::ArrowTrap)?"skill.arrow":(p.skill==Skill::BladeWave||p.skill==Skill::SwordCyclone)?"skill.blade_wave":"skill.projectile";
  float scale=p.skill==Skill::SwordCyclone?zoom_*std::clamp(2*p.radius/138.f,.18f,.5f):p.skill==Skill::FeatherStorm?zoom_*std::clamp(2.6f*p.radius/72.f,.25f,.45f):p.skill==Skill::StarBow?zoom_*std::clamp(2.4f*p.radius/73.f,.25f,.45f):zoom_*.8f;
  if(p.skill>=Skill::TripleStar){const auto roles=Game::skill_roles(p.skill);key=(roles&Shadow)?"skill.feather_storm.projectile":(roles&Gun)?"skill.sniper.projectile":(roles&Dragon)?"skill.dragon_thrust.cast":"skill.projectile";scale=zoom_*(roles&Dragon?.08f:.45f);}
  float time=p.age;ImU32 tint=p.skill==Skill::StarReturn?IM_COL32(178,242,255,255):0xffffffffu;
  const bool shield=p.skill==Skill::HolyShield||p.skill==Skill::EchoShield||p.skill==Skill::Sanctuary||(p.skill==Skill::MirrorShield&&p.blocks>=0);
  if(shield){key="skill.holy_shield.cast";scale=zoom_*p.radius/70.f;dl->AddCircle(q,p.radius*zoom_,p.returned?IM_COL32(190,233,248,210):IM_COL32(248,225,153,210),24,2);
   for(int block=0;block<std::clamp(p.blocks,0,3);++block){const float a=-pi*.5f+block*2*pi/3;dl->AddCircleFilled({q.x+std::cos(a)*p.radius*zoom_*.65f,q.y+std::sin(a)*p.radius*zoom_*.65f},2*zoom_,IM_COL32(224,244,252,230),8);}}
  else if(p.skill==Skill::DragonThrust||p.skill==Skill::AzureRift){key="skill.dragon_thrust.cast";scale=zoom_*p.radius/170.f;time=animation_phase(key,std::clamp(p.age/.7f,.12f,.42f));}
  else if(p.skill==Skill::IceLance||p.skill==Skill::ThunderLance){key="skill.ice_lance.hit";scale=zoom_*p.radius/24.f;time=0;}
  else if(p.skill==Skill::Sniper||p.skill==Skill::EagleExecution||p.skill==Skill::PrismShot||p.skill==Skill::SiegeBolt){key="skill.sniper.projectile";scale=zoom_*std::clamp(p.radius/16.f,.5f,1.f);}
  else if(p.skill==Skill::SilverEagle||p.skill==Skill::TwinHunt){key="skill.silver_eagle.hit";scale=zoom_*std::clamp(p.radius/20.f,.35f,.6f);time=0;}
  else if(p.skill==Skill::StormShot||p.skill==Skill::ThousandArrows||p.skill==Skill::IceArrow||p.skill==Skill::FrozenBarrage||p.skill==Skill::BlastArrow){key="skill.arrow";scale=zoom_*.65f;if(p.skill==Skill::IceArrow||p.skill==Skill::FrozenBarrage)tint=IM_COL32(174,226,255,255);}
  else if(p.skill==Skill::ThunderPrison){key="skill.blade_wave";scale=zoom_*p.radius/38.f;tint=IM_COL32(206,229,255,255);}
  else if(p.skill==Skill::FrozenDragonSpear){key="skill.ice_lance.hit";scale=zoom_*p.radius/24.f;time=0;if(p.evolved)dl->AddCircle(q,p.radius*zoom_*1.4f,IM_COL32(202,236,252,160),20,1.4f);}
  else if(p.skill==Skill::DragonBreaker||p.skill==Skill::SpearBolt||p.skill==Skill::GlacialSniper){key="skill.sniper.projectile";scale=zoom_*std::clamp(p.radius/16.f,.55f,1.f);if(p.skill==Skill::GlacialSniper)tint=IM_COL32(183,231,255,255);}
  else if(p.skill==Skill::StormArrowStream){key="skill.arrow";scale=zoom_*.65f;if(p.evolved)dl->AddCircle(q,p.radius*zoom_,IM_COL32(166,220,252,170),16,1.4f);}
  else if(p.skill==Skill::FlowingFire){key="skill.blast_arrow.hit";scale=zoom_*p.radius/25.f;time=.12f;}
  else if(p.skill==Skill::MirrorShield){key="skill.projectile";scale=zoom_*p.radius/20.f;tint=IM_COL32(235,241,255,255);}
  float rotation=std::atan2(p.velocity.y,p.velocity.x)-forward_angles_[key];if(shield||p.skill==Skill::FlowingFire)rotation=0;else if(p.skill==Skill::DragonThrust||p.skill==Skill::AzureRift)rotation=std::atan2(p.velocity.y,p.velocity.x)+pi*.5f;else if(p.skill==Skill::IceLance||p.skill==Skill::ThunderLance||p.skill==Skill::FrozenDragonSpear)rotation=std::atan2(p.velocity.y,p.velocity.x)-2.2f;
  sprite(dl,key,q,scale,time,false,tint,rotation);fade_vertices(projectile_vertex_start,friendly_effect_alpha_);
 }
 for(const auto& e:game_.effects){
  auto p=screen(e.pos);float fade=std::max(0.f,1-e.age/std::max(.01f,e.duration));
  if((low_effects_||simulation_clock_.backlog_seconds()>.25)&&e.kind==EffectKind::Hit&&!e.hostile)continue;
  int effect_vertex_start=dl->VtxBuffer.Size;
  switch(e.kind){
  case EffectKind::ThunderPrison:
   if(e.pattern==1&&!low_effects_)sprite(dl,"skill.rune",p,zoom_*e.radius/100.f,e.age,false,IM_COL32(210,231,255,130));break;
  case EffectKind::FrostArmor:
   if(e.pattern==0&&game_.guard_charge>0&&!game_.buffs_suppressed()&&!low_effects_)sprite(dl,"skill.holy_shield.cast",p,zoom_*.32f,.48f,false,IM_COL32(176,224,255,110),0,false);break;
  case EffectKind::MiasmaDragon:
   if(e.pattern==0&&e.age<e.delay)sprite(dl,"skill.holy_dragon.attack",p,zoom_*.45f,animation_phase("skill.holy_dragon.attack",(.45f-std::clamp(e.tick,0.f,.45f))/.45f),std::cos(e.angle)>0,IM_COL32(226,242,206,170),0,false);
   else if(e.pattern==1)sprite(dl,"skill.blast_arrow.hit",p,zoom_*e.radius/45.f,animation_phase("skill.blast_arrow.hit",e.age/std::max(.01f,e.duration)),false,IM_COL32(191,233,141,190),0,false);break;
  case EffectKind::PhoenixAssault:{
   const Vec2 direction=normalized(e.end-e.pos);if(length(direction)<.01f)break;const Vec2 side{-direction.y*35,direction.x*35};const float travel=std::clamp((e.age-.08f)/.42f,0.f,1.f);
   const Vec2 first=e.pos+(e.end-e.pos)*travel+side,second=e.end+(e.pos-e.end)*travel-side;const ImU32 tint=IM_COL32(255,235,198,int(225*std::min(1.f,fade*3)));
   sprite(dl,"skill.summon",screen(first),zoom_*.75f,e.age,direction.x>0,tint);sprite(dl,"skill.summon",screen(second),zoom_*.75f,e.age,direction.x<0,tint);break;}
  case EffectKind::MirrorShield:
   if(e.pattern==1)sprite(dl,"skill.holy_dragon.hit",p,zoom_*.35f,animation_phase("skill.holy_dragon.hit",e.age/std::max(.01f,e.duration)),false,IM_COL32(220,237,255,180),0,false);break;
  case EffectKind::DragonBreaker:
   if(e.count>0&&e.age<.55f)sprite(dl,"skill.earth_shatter.hit",screen(e.end),zoom_*e.radius/155.f,animation_phase("skill.earth_shatter.hit",(e.age-.25f)/.3f),false,IM_COL32(255,235,194,175),0,false);break;
  case EffectKind::FissureArrows:
   if(e.pattern==0&&e.triggered&&e.age-e.delay<.3f)sprite(dl,"skill.earth_shatter.impact",screen((e.pos+e.end)*.5f),zoom_*.45f,animation_phase("skill.earth_shatter.impact",(e.age-e.delay)/.3f),false,IM_COL32(255,255,255,160),0,false);
   else if(e.pattern==1){if(!e.triggered){const auto at=screen(e.pos+Vec2{0,-120*std::clamp(1-e.age/std::max(.01f,e.delay),0.f,1.f)});sprite(dl,"skill.arrow",at,zoom_*.7f,e.age,false,IM_COL32(255,234,190,180),pi*.5f-forward_angles_["skill.arrow"]);}
    else sprite(dl,"skill.rain_hit",p,zoom_*e.radius/70.f,animation_phase("skill.rain_hit",(e.age-e.delay)/std::max(.01f,e.duration-e.delay)),false,IM_COL32(255,234,191,200),0,false);}break;
  case EffectKind::StormArrowStream:
   if(e.energy>0&&e.tick>.07f&&!low_effects_)sprite(dl,"skill.storm_shot.cast",screen(game_.player.pos),zoom_*.35f,animation_phase("skill.storm_shot.cast",(.13f-std::clamp(e.tick,0.f,.13f))/.13f),e.end.x>game_.player.pos.x,IM_COL32(215,237,255,110),0,false);break;
  case EffectKind::PoisonHuntingGround:
   if(e.pattern==0&&!e.triggered)sprite(dl,"skill.decoy.idle",p,zoom_*.7f,e.age,false,IM_COL32(255,255,255,215));
   else if(e.pattern==1)sprite(dl,"skill.blast_arrow.hit",p,zoom_*e.radius/85.f,animation_phase("skill.blast_arrow.hit",e.age/std::max(.01f,e.duration)),false,IM_COL32(194,231,146,185),0,false);break;
  case EffectKind::DragonPhoenix:
   if(e.pattern==0){const Vec2 orbit{std::cos(e.age*2)*e.radius*.7f,std::sin(e.age*2)*e.radius*.45f};sprite(dl,"skill.holy_dragon.move",screen(e.pos+orbit),zoom_*.42f,e.age,false,IM_COL32(255,244,218,175));sprite(dl,"skill.summon",screen(e.pos-orbit),zoom_*.7f,e.age,true,IM_COL32(255,227,190,175));}
   if(e.pattern>0){const bool dragon=e.pattern%2==1;const float travel=std::clamp(e.age/std::max(.01f,e.delay),0.f,1.f);const auto at=screen(e.pos+(e.end-e.pos)*travel);const float opacity=e.triggered?std::clamp((e.duration-e.age)/std::max(.01f,e.duration-e.delay),0.f,1.f):1.f;
    sprite(dl,dragon?"skill.holy_dragon.move":"skill.summon",at,zoom_*(dragon?.45f:.75f),e.age,e.end.x>=e.pos.x,IM_COL32(255,244,218,int(215*opacity)));
    if(e.triggered&&!low_effects_)dl->AddLine(screen(e.pos),screen(e.end),dragon?IM_COL32(249,232,182,int(125*opacity)):IM_COL32(245,175,99,int(125*opacity)),e.radius*zoom_*.4f);
   }break;
  case EffectKind::GlacialSniper:
   if(e.pattern==1&&e.triggered)sprite(dl,"skill.ice_arrow.hit",p,zoom_*e.radius/105.f,animation_phase("skill.ice_arrow.hit",(e.age-e.delay)/std::max(.01f,e.duration-e.delay)),false,IM_COL32(219,245,255,210),0,false);break;
  case EffectKind::FlowingFire:
   if(e.pattern>0&&e.triggered)sprite(dl,"skill.blast_arrow.hit",p,zoom_*e.radius/55.f,animation_phase("skill.blast_arrow.hit",(e.age-e.delay)/std::max(.01f,e.duration-e.delay)),false,IM_COL32(255,255,255,210),0,false);break;
  case EffectKind::BattleBlades:
   if(e.pattern>=10)sprite(dl,"skill.slash",p,zoom_*e.radius/140.f,e.age,false,e.pattern==11?IM_COL32(255,229,165,int(245*fade)):IM_COL32(255,244,213,int(200*fade)),std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x)-pi);
   else if(e.pattern==1&&!low_effects_)sprite(dl,"skill.battle_focus.state",p,zoom_*.8f,e.age,false,IM_COL32(255,255,255,145));break;
  case EffectKind::DragonFormation:
   if(e.pattern<2){const float travel=std::clamp((e.age-e.delay)/.35f,0.f,1.f);const float axis=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x);for(int lane=-1;lane<=1;++lane){const float angle=axis+lane*.46f;const auto at=screen(e.pos+Vec2{std::cos(angle),std::sin(angle)}*320*game_.area_multiplier*travel);sprite(dl,"skill.dragon_thrust.cast",at,zoom_*.55f,animation_phase("skill.dragon_thrust.cast",std::clamp(e.age/.55f,0.f,1.f)),std::cos(angle)>0,IM_COL32(255,255,255,205),angle, false);}}
   else sprite(dl,"skill.dragon_thrust.hit",screen(e.end),zoom_*e.radius/140.f,animation_phase("skill.dragon_thrust.hit",(e.age-.55f)/.5f),false,IM_COL32(255,255,255,215),0,false);break;
  case EffectKind::EchoShield:
   sprite(dl,"skill.holy_shield.cast",p,zoom_*e.radius/160.f,animation_phase("skill.holy_shield.cast",e.age/std::max(.01f,e.duration)),false,IM_COL32(255,255,255,175),0,false);break;
  case EffectKind::MountainBreaker:
   if(e.pattern==1&&e.triggered)sprite(dl,"skill.earth_shatter.impact",screen((e.pos+e.end)*.5f),zoom_*e.radius/65.f,animation_phase("skill.earth_shatter.impact",(.4f-std::clamp(e.tick,0.f,.4f))/.4f),false,IM_COL32(255,255,255,175),0,false);
   else if(e.pattern==2)sprite(dl,"skill.earth_shatter.hit",p,zoom_*e.radius/110.f,animation_phase("skill.earth_shatter.hit",e.age/std::max(.01f,e.duration)),false,IM_COL32(255,255,255,210),0,false);break;
  case EffectKind::SpiritJudgement:
   if(e.pattern>0)sprite(dl,"skill.judgement.cast",p,zoom_*e.radius/160.f,animation_phase("skill.judgement.cast",impact_phase(e.age-(e.delay-.6f),.6f,1.f)),false,IM_COL32(255,255,255,205),0,false);break;
  case EffectKind::AzureRift:case EffectKind::SiegeBolt:
   if(e.triggered){const std::string key=e.kind==EffectKind::AzureRift?"skill.dragon_thrust.hit":"skill.blast_arrow.hit";sprite(dl,key,p,zoom_*e.radius/(e.kind==EffectKind::AzureRift?95.f:43.f),animation_phase(key,(e.age-e.delay)/std::max(.01f,e.duration-e.delay)),false,IM_COL32(255,255,255,205),0,false);}break;
  case EffectKind::Sanctuary:
   if(e.pattern==2)sprite(dl,"skill.judgement.cast",p,zoom_*e.radius/140.f,animation_phase("skill.judgement.cast",impact_phase(e.age,e.delay,e.duration)),false,IM_COL32(255,255,255,180),0,false);break;
  case EffectKind::PoisonFlare:
   if(e.pattern>=10&&e.triggered)sprite(dl,"skill.blast_arrow.hit",p,zoom_*e.radius/55.f,animation_phase("skill.blast_arrow.hit",(e.age-e.delay)/std::max(.01f,e.duration-e.delay)),false,IM_COL32(255,255,255,195),0,false);break;
  case EffectKind::ThunderLance:
   if(e.triggered){const auto q=screen(e.end);const Vec2 direction=normalized(e.end-e.pos);const float phase=std::clamp((e.duration-e.age)/.4f,0.f,1.f);dl->PathLineTo(p);for(int node=1;node<6;++node){const Vec2 at=e.pos+(e.end-e.pos)*(node/6.f)+Vec2{-direction.y,direction.x}*((node%2?1.f:-1.f)*e.radius*.45f);dl->PathLineTo(screen(at));}dl->PathLineTo(q);dl->PathStroke(IM_COL32(184,230,255,int(225*phase)),0,2.5f*zoom_);sprite(dl,"skill.ice_lance.hit",q,zoom_,e.age-e.delay,false,IM_COL32(255,255,255,205),0,false);}break;
  case EffectKind::HeavenDragon:
   if(e.pattern==0){} // Body participates in actor sorting.
   else if(e.triggered){const float pulse=.6f-std::clamp(e.tick,0.f,.6f);if(pulse<.25f)dl->AddLine(p,screen(e.end),IM_COL32(249,238,196,int(170*(1-pulse/.25f))),e.radius*zoom_*.45f);if(!low_effects_)sprite(dl,"skill.holy_light.cast",screen((e.pos+e.end)*.5f),zoom_*.32f,animation_phase("skill.holy_light.cast",pulse/.6f),false,IM_COL32(255,255,255,145),0,false);}break;
  case EffectKind::InfernoArray:
   if(e.pattern==0)sprite(dl,"skill.fire_beast.move",p,zoom_*.5f,e.age,e.end.x>e.pos.x,IM_COL32(255,255,255,220));
   else if(e.pattern==1){auto at=p;at.y-=std::max(0.f,e.delay-e.age)*130*zoom_;sprite(dl,"skill.meteor",at,zoom_*e.radius/120.f,e.age,false,IM_COL32(255,255,255,145),0,false);}
   else if(e.age-e.delay<.35f)sprite(dl,"skill.earth_shatter.hit",p,zoom_*e.radius/85.f,animation_phase("skill.earth_shatter.hit",(e.age-e.delay)/.35f),false,IM_COL32(255,231,199,205),0,false);break;
  case EffectKind::VenomVortex:
   if(e.pattern==1)sprite(dl,"skill.blast_arrow.hit",p,zoom_*e.radius/90.f,animation_phase("skill.blast_arrow.hit",(e.age-(e.duration-.3f))/.3f),false,IM_COL32(198,237,148,190),0,false);break;
  case EffectKind::FrostFire:
   if(e.pattern==2)sprite(dl,"skill.ice_arrow.hit",p,zoom_*e.radius/110.f,animation_phase("skill.ice_arrow.hit",e.age/std::max(.01f,e.duration)),false,IM_COL32(255,230,205,190),0,false);break;
  case EffectKind::PurityWell:
   if(e.pattern==1)sprite(dl,"skill.holy_light.cast",p,zoom_*e.radius/250.f,animation_phase("skill.holy_light.cast",(.75f-std::clamp(e.tick,0.f,.75f))/.75f),false,IM_COL32(255,255,255,155),0,false);break;
  case EffectKind::ThousandArrows:
   if(!low_effects_&&e.tick>.08f)sprite(dl,"skill.storm_shot.cast",p,zoom_*.45f,animation_phase("skill.storm_shot.cast",(.22f-std::clamp(e.tick,0.f,.22f))/.22f),e.end.x>e.pos.x,IM_COL32(255,255,255,e.pattern?155:100),0,false);break;
  case EffectKind::EagleExecution:
   sprite(dl,"skill.silver_eagle.move",p,zoom_*.65f,e.age,e.end.x>e.pos.x,IM_COL32(255,255,255,220));break;
  case EffectKind::FrozenBarrage:
   if(e.pattern>=10)sprite(dl,"skill.ice_arrow.hit",p,zoom_*e.radius/90.f,animation_phase("skill.ice_arrow.hit",e.age/std::max(.01f,e.duration)),false,IM_COL32(255,255,255,210),0,false);break;
  case EffectKind::TrickDecoy:
   if(e.pattern<2)sprite(dl,"skill.decoy.idle",p,zoom_*.7f,e.age,false,IM_COL32(255,255,255,e.pattern?100:225));
   else if(e.pattern==3)sprite(dl,"skill.blast_arrow.hit",p,zoom_*e.radius/60.f,animation_phase("skill.blast_arrow.hit",(e.age-(e.duration-.3f))/.3f),false,IM_COL32(255,255,255,205),0,false);break;
  case EffectKind::TwinHunt:{
   const bool headhunt=game_.fusion_form(e.source)==1;float fired_age=10;for(const auto& shot:game_.projectiles)if(shot.skill==e.source)fired_age=std::min(fired_age,shot.age);
   const bool first_fired=fired_age<.24f&&(headhunt||e.pattern==1),second_fired=fired_age<.1f&&(headhunt||e.pattern==0);const Enemy* target=nullptr;for(const auto& enemy:game_.enemies)if(enemy.id==e.target_id&&enemy.hp>0){target=&enemy;break;}
   const std::string phoenix=first_fired&&sprites_.count("skill.phoenix.attack")?"skill.phoenix.attack":"skill.summon";
   sprite(dl,phoenix,p,zoom_*.75f,first_fired?(phoenix=="skill.phoenix.attack"?std::min(.36f,fired_age*1.5f):animation_phase(phoenix,fired_age/.24f)):e.age,target?target->pos.x>e.pos.x:-std::sin(e.angle)>0,IM_COL32(255,255,255,220),0,!first_fired);
   sprite(dl,second_fired?"skill.silver_eagle.attack":"skill.silver_eagle.move",screen(e.end),zoom_*.65f,second_fired?fired_age:e.age,target?target->pos.x>e.end.x:std::sin(-e.angle-pi)>0,IM_COL32(255,255,255,220),0,!second_fired);break;}
  case EffectKind::PrismShot:
   if(!e.triggered){const auto q=screen(e.end);const float r=12*zoom_;dl->AddQuad({q.x-r,q.y-r},{q.x+r,q.y-r},{q.x+r,q.y+r},{q.x-r,q.y+r},IM_COL32(223,231,245,165),1.5f);}break;
  case EffectKind::BattleFocus:
   if(e.age<e.delay)sprite(dl,"skill.battle_focus.state",p,zoom_*.85f,e.age,false,IM_COL32(255,255,255,180));
   else sprite(dl,"skill.slash",p,zoom_*e.radius/150.f,e.age-e.delay,false,IM_COL32(255,236,185,int(230*fade)),std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x)-pi);break;
  case EffectKind::SpearCombo:
   if(e.pattern>0)sprite(dl,"skill.spear_combo.thrust",p,zoom_*std::clamp(length(e.end-e.pos)/135.f,.7f,2.f),e.age,false,0xffffffffu,std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x)-pi,false);break;
  case EffectKind::HolyShield:
   sprite(dl,"skill.holy_shield.cast",p,zoom_*.45f,animation_phase("skill.holy_shield.cast",e.age/std::max(.01f,e.duration)),false,IM_COL32(255,255,255,175),0,false);break;
  case EffectKind::Judgement:
   sprite(dl,"skill.judgement.cast",p,zoom_*std::clamp(e.radius/160.f,.35f,.9f),animation_phase("skill.judgement.cast",impact_phase(e.age,e.delay,e.duration)),false,IM_COL32(255,255,255,205),0,false);break;
  case EffectKind::DragonThrust:
   sprite(dl,"skill.dragon_thrust.cast",p,zoom_*.55f,animation_phase("skill.dragon_thrust.cast",e.age/std::max(.01f,e.duration)),e.end.x>e.pos.x,IM_COL32(255,255,255,165),0,false);break;
  case EffectKind::EarthShatter:
   if(e.pattern>0&&e.age>=e.delay)sprite(dl,"skill.earth_shatter.impact",p,zoom_*e.radius/80.f,animation_phase("skill.earth_shatter.impact",(.45f-std::clamp(e.tick,0.f,.45f))/.45f),false,IM_COL32(255,255,255,200),0,false);break;
  case EffectKind::IceLance:
   if(e.pattern>0)sprite(dl,"skill.ice_lance.hit",p,zoom_*.75f,std::min(e.age,.3f),false,IM_COL32(255,255,255,int(220*std::min(1.f,fade*4))),0,false);break;
  case EffectKind::HolyLight:
   sprite(dl,"skill.holy_light.cast",p,zoom_*std::clamp(e.radius/220.f,.35f,.85f),animation_phase("skill.holy_light.cast",e.age/std::max(.01f,e.duration)),false,IM_COL32(255,255,255,190),0,false);break;
  case EffectKind::HolyDragon:case EffectKind::FireBeast:case EffectKind::SilverEagle:{
   if(e.pattern==0){} else if(e.kind==EffectKind::HolyDragon&&e.triggered){dl->AddLine(p,screen(e.end),IM_COL32(255,232,179,int(190*fade)),e.radius*zoom_*.45f);sprite(dl,"skill.holy_dragon.hit",screen(e.end),zoom_*.6f,e.age-e.delay,false,IM_COL32(255,255,255,190),0,false);}
   else if(e.kind==EffectKind::FireBeast&&e.triggered)sprite(dl,"skill.fire_beast.hit",screen(e.pos+(e.end-e.pos)*.55f),zoom_*.7f,e.age-e.delay,false,IM_COL32(255,255,255,185),0,false);break;}
  case EffectKind::MagicVortex:
   sprite(dl,"skill.magic_vortex.cast",p,zoom_*e.radius/190.f,e.pattern?animation_phase("skill.magic_vortex.cast",(e.age-(e.duration-.3f))/.3f):std::fmod(e.age,.6f),false,IM_COL32(255,255,255,e.pattern?180:120),0,e.pattern==0);break;
  case EffectKind::StormShot:
   if(e.age<.72f)sprite(dl,"skill.storm_shot.cast",p,zoom_*.45f,e.age,e.end.x>e.pos.x,IM_COL32(255,255,255,125),0,false);break;
  case EffectKind::Sniper:
   if(!e.triggered){const auto q=screen(e.end);const float r=(7+4*std::clamp(e.age/std::max(.01f,e.delay),0.f,1.f))*zoom_;for(int mark=0;mark<4;++mark){float a=mark*pi*.5f;dl->AddLine({q.x+std::cos(a)*r,q.y+std::sin(a)*r},{q.x+std::cos(a)*(r+5*zoom_),q.y+std::sin(a)*(r+5*zoom_)},IM_COL32(242,233,169,180),1.5f);}}break;
  case EffectKind::IceArrow:case EffectKind::BlastArrow:
   if(e.pattern>0){const std::string key=e.kind==EffectKind::IceArrow?"skill.ice_arrow.hit":"skill.blast_arrow.hit";sprite(dl,key,p,zoom_*std::clamp(e.radius/(e.kind==EffectKind::IceArrow?100.f:42.f),.35f,1.3f),animation_phase(key,e.age/std::max(.01f,e.duration)),false,IM_COL32(255,255,255,200),0,false);}break;
  case EffectKind::Decoy:
   if(e.pattern==0){sprite(dl,"skill.decoy.idle",p,zoom_*.7f,e.age,false,IM_COL32(255,255,255,225));if(e.age<.5f&&!low_effects_)sprite(dl,"skill.decoy.spawn",p,zoom_*.7f,animation_phase("skill.decoy.spawn",e.age/.5f),false,IM_COL32(255,255,255,155),0,false);}
   else sprite(dl,"skill.earth_shatter.hit",p,zoom_*e.radius/90.f,animation_phase("skill.earth_shatter.hit",(e.age-(e.duration-.25f))/.25f),false,IM_COL32(255,255,255,190),0,false);break;
  case EffectKind::OrbitalOrb:{
   for(int orb=0;orb<e.count;++orb){const float angle=game_.combat_elapsed*1.65f+orb*2*pi/std::max(1,e.count);const auto q=screen(e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius);
    dl->AddCircle(q,12*zoom_,IM_COL32(160,211,246,100),16,1);sprite(dl,"skill.projectile",q,zoom_*.6f,e.age,false,0xffffffffu,angle-forward_angles_["skill.projectile"]);
   }break;}
  case EffectKind::ArcaneRing:{
   const int total=std::max(1,int(e.energy));const float charge=e.delay>0?std::clamp(e.age/e.delay,0.f,1.f):1;
   for(int orb=0;orb<e.count;++orb){const float angle=e.angle+(total-e.count+orb)*2*pi/total;const auto q=screen(e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius);
    if(!(low_effects_||simulation_clock_.backlog_seconds()>.25))dl->AddCircle(q,10*zoom_,IM_COL32(198,182,255,110),16,1.2f);sprite(dl,"skill.projectile",q,zoom_*(.4f+.25f*charge),e.age,false,IM_COL32(227,213,255,220),angle-forward_angles_["skill.projectile"]);
   }
   if(e.age<e.delay){dl->PathArcTo(p,e.radius*zoom_,-pi*.5f,-pi*.5f+charge*2*pi,40);dl->PathStroke(IM_COL32(196,179,243,100),0,1.5f);}break;}
  case EffectKind::SwordCyclone:{
   for(int blade=0;blade<e.count;++blade){const float angle=e.angle+blade*2*pi/std::max(1,e.count);const auto q=screen(e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius);
    if(!(low_effects_||simulation_clock_.backlog_seconds()>.25)){dl->PathArcTo(p,e.radius*zoom_,angle-.25f,angle,7);dl->PathStroke(IM_COL32(243,222,165,110),0,2);}
    sprite(dl,"skill.orbit",q,zoom_*.9f,e.age,false,IM_COL32(255,240,205,230),angle+pi*.5f-forward_angles_["skill.orbit"]);
   }break;}
  case EffectKind::FlameDance:case EffectKind::FeatherStorm:{
   if(e.pattern!=0)break;const bool flame=e.kind==EffectKind::FlameDance;
   for(int blade=0;blade<e.count;++blade){const float angle=e.angle+blade*2*pi/std::max(1,e.count);const auto q=screen(e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius);
    if(!(low_effects_||simulation_clock_.backlog_seconds()>.25)){dl->PathArcTo(p,e.radius*zoom_,angle-.24f,angle,7);dl->PathStroke(flame?IM_COL32(249,174,88,130):IM_COL32(226,224,162,125),0,2);}
    const std::string key=flame?"skill.orbit":"skill.feather_storm.projectile";sprite(dl,key,q,zoom_*(flame?.9f:.46f),e.age,false,flame?IM_COL32(255,216,164,240):IM_COL32(255,255,255,230),angle+pi*.5f-forward_angles_[key]);
   }
   if(!flame&&!(low_effects_||simulation_clock_.backlog_seconds()>.25)&&e.age<.84f)sprite(dl,"skill.feather_storm.cast",p,zoom_*.5f,e.age,false,IM_COL32(255,255,255,135),0,false);break;}
  case EffectKind::StarBow:{
   const int total=std::max(1,int(e.energy));const float charge=e.delay>0?std::clamp(e.age/e.delay,0.f,1.f):1.f;
   // The fixed node loses exactly one queued arrow whenever the core launches one.
   for(int arrow=0;arrow<e.count;++arrow){const float angle=e.angle+(total-e.count+arrow)*2*pi/total;const auto q=screen(e.pos+Vec2{std::cos(angle),std::sin(angle)}*e.radius);
    if(!(low_effects_||simulation_clock_.backlog_seconds()>.25))dl->AddCircle(q,9*zoom_,IM_COL32(185,225,255,85),16,1);
    sprite(dl,"skill.star_bow.projectile",q,zoom_*(.18f+.12f*charge),e.age,false,IM_COL32(255,255,255,int(140+80*charge)),angle-forward_angles_["skill.star_bow.projectile"]);
   }break;}
  case EffectKind::HunterVolley:{
   const auto target=screen(e.end);const float r=16*zoom_;dl->AddCircle(target,r,IM_COL32(255,213,134,155),24,1.5f);
   for(int corner=0;corner<4;++corner){float angle=corner*pi*.5f;dl->AddLine({target.x+std::cos(angle)*r*.75f,target.y+std::sin(angle)*r*.75f},{target.x+std::cos(angle)*r*1.3f,target.y+std::sin(angle)*r*1.3f},IM_COL32(255,213,134,185),1.5f);}
   const auto direction=normalized(e.end-e.pos);for(float side:{-1.f,1.f}){auto q=screen(e.pos+Vec2{-direction.y,direction.x}*(side*16));const float ready=std::clamp(float(e.count)/std::max(1.f,e.energy),0.f,1.f);dl->AddLine({q.x-direction.x*10*zoom_,q.y-direction.y*10*zoom_},q,IM_COL32(255,226,163,int(190*ready)),2);}
   if(!(low_effects_||simulation_clock_.backlog_seconds()>.25)&&e.age<e.delay)dl->AddLine(p,target,IM_COL32(244,211,146,55),1);break;}
  case EffectKind::PhoenixRain:
   if(e.age<e.delay){const float t=std::clamp(e.age/std::max(.01f,e.delay),0.f,1.f);auto q=screen(e.end+(e.pos-e.end)*t+Vec2{0,-20.f*std::sin(pi*t)});sprite(dl,"skill.summon",q,zoom_*.55f,e.age,e.pos.x>e.end.x,IM_COL32(255,230,190,205));}break;
  case EffectKind::MoltenStar:
   if(e.pattern==1&&!e.triggered){const float t=std::clamp(e.age/std::max(.01f,e.delay),0.f,1.f);const ImVec2 q{p.x-25*zoom_*(1-t),p.y-150*zoom_*(1-t)};dl->AddLine({q.x-8*zoom_,q.y-33*zoom_},q,IM_COL32(255,178,91,145),3*zoom_);dl->AddCircleFilled(q,7*zoom_,IM_COL32(255,214,143,200),16);}break;
  case EffectKind::HeavyStrike:{
   const float axis=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x);
   if(!e.triggered){const auto q=screen(e.pos+normalized(e.end-e.pos)*28);sprite(dl,"skill.orbit",q,zoom_,e.age,false,IM_COL32(248,224,163,175),axis+pi*.5f-forward_angles_["skill.orbit"]);}
   else{const float impact=std::clamp((e.duration-e.age)/std::max(.01f,e.duration-e.delay),0.f,1.f);dl->PathArcTo(p,e.radius*zoom_,axis-e.angle,axis+e.angle,24);dl->PathStroke(IM_COL32(255,225,160,int(220*impact)),0,4*zoom_);sprite(dl,"skill.slash",p,zoom_*1.1f,e.age-e.delay,false,IM_COL32(255,235,192,int(215*impact)),axis-pi,false);}
   break;}
  case EffectKind::Lightning:dl->AddLine(p,screen(e.end),IM_COL32(176,224,255,int(255*fade)),3*zoom_);sprite(dl,"skill.lightning",screen(e.end),zoom_,e.age);break;
  case EffectKind::Summon:break;
  case EffectKind::Cleave:{float a=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x);dl->PathArcTo(p,e.radius*zoom_,a-(e.angle>0?e.angle:game_.config.cleave_half_angle),a+(e.angle>0?e.angle:game_.config.cleave_half_angle),e.source==Skill::DashBreaker?48:20);dl->PathStroke(IM_COL32(255,221,128,int(210*fade)),0,4*zoom_);sprite(dl,"skill.slash",p,zoom_,e.age,false,IM_COL32(255,245,214,int(255*fade)),a-pi);if(e.source==Skill::DashBreaker)sprite(dl,"skill.slash",p,zoom_*.8f,e.age,false,IM_COL32(255,225,172,int(185*fade)),a-pi*.5f);break;}
  case EffectKind::Orbit:{float a=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x)+pi*.5f-forward_angles_["skill.orbit"];sprite(dl,"skill.orbit",screen(e.end),zoom_,e.age,false,0xffffffffu,a);break;}
  case EffectKind::Dash:{bool left=e.end.x<e.pos.x;for(int ghost=0;ghost<3;++ghost){auto at=screen(e.pos+(e.end-e.pos)*(float(ghost)*.25f));sprite(dl,player_sprite(int(game_.character),"move"),at,zoom_,e.age,!left,IM_COL32(170,224,255,int(105*fade)));}sprite(dl,"skill.dash",p,zoom_,e.age,!left,IM_COL32(180,235,255,int(220*fade)));break;}
  case EffectKind::Charge:{float a=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x);if(e.source==Skill::DashBreaker){const auto q=screen(e.end);dl->AddLine(p,q,IM_COL32(255,226,163,int(150*fade)),2*zoom_);}else sprite(dl,"skill.charge",p,zoom_,e.age,false,IM_COL32(255,255,255,int(210*fade)),a-forward_angles_["skill.charge"],false);break;}
  case EffectKind::Retaliate:sprite(dl,"skill.retaliate",p,zoom_,e.age,false,IM_COL32(255,255,255,int(220*fade)),0,false);break;
  case EffectKind::EnemyShot:break; // Hostile projectiles are drawn above friendly bursts.
  case EffectKind::Warning:if(e.hostile)draw_hostile_release(dl,e);break;
  case EffectKind::Hit:{
   if(!legacy_visuals_&&e.source!=Skill::Count)break;
   std::string key="skill.hit";float scale=.7f;
   switch(e.source){
   case Skill::StarBow:key="skill.star_bow.hit";scale=.4f;break;
   case Skill::SpearCombo:case Skill::BattleFocus:case Skill::BattleBlades:case Skill::FrozenDragonSpear:case Skill::SpearBolt:key="skill.spear_combo.hit";scale=.55f;break;
   case Skill::DragonThrust:case Skill::DragonFormation:case Skill::AzureRift:key="skill.dragon_thrust.hit";scale=.35f;break;
   case Skill::EarthShatter:case Skill::MountainBreaker:key="skill.earth_shatter.hit";scale=.55f;break;
   case Skill::IceLance:case Skill::ThunderLance:key="skill.ice_lance.hit";scale=.65f;break;
   case Skill::IceArrow:case Skill::FrozenBarrage:key="skill.ice_arrow.hit";scale=.35f;break;
   case Skill::BlastArrow:case Skill::SiegeBolt:case Skill::TrickDecoy:key="skill.blast_arrow.hit";scale=.65f;break;
   case Skill::Sniper:case Skill::EagleExecution:case Skill::PrismShot:key="skill.sniper.hit";scale=.45f;break;
   case Skill::HolyDragon:case Skill::HeavenDragon:case Skill::Judgement:case Skill::SpiritJudgement:key="skill.holy_dragon.hit";scale=.4f;break;
   case Skill::FireBeast:case Skill::InfernoArray:key="skill.fire_beast.hit";scale=.45f;break;
   case Skill::SilverEagle:case Skill::TwinHunt:key="skill.silver_eagle.hit";scale=.6f;break;
   case Skill::ThunderPrison:case Skill::StormArrowStream:key="skill.lightning";scale=.45f;break;
   case Skill::FrostArmor:case Skill::GlacialSniper:key="skill.ice_arrow.hit";scale=.55f;break;
   case Skill::MiasmaDragon:case Skill::PoisonHuntingGround:key="skill.poison_mist.zone";scale=.35f;break;
   case Skill::FlowingFire:key="skill.blast_arrow.hit";scale=.5f;break;
   case Skill::DivineVerdict:case Skill::MirrorShield:case Skill::DragonPhoenix:key="skill.holy_dragon.hit";scale=.45f;break;
   case Skill::PhoenixAssault:key="skill.rain_hit";scale=.65f;break;
   case Skill::DragonBreaker:case Skill::FissureArrows:key="skill.earth_shatter.hit";scale=.55f;break;
   default:break;
   }
   sprite(dl,key,p,zoom_*scale,animation_phase(key,e.age/std::max(.01f,e.duration)),false,0xffffffffu,0,false);if(e.age<.055f)dl->AddCircleFilled(p,6*zoom_,IM_COL32(255,242,177,210));break;}
  default:break;
  }
  if(!e.hostile&&e.source!=Skill::Count)fade_vertices(effect_vertex_start,friendly_effect_alpha_*(.35f+.65f*flash_strength_)*((low_effects_||simulation_clock_.backlog_seconds()>.25)?.65f:1.f));
 }

 draw_skill_visuals(dl,true);
 draw_hostile_impacts(dl);
 for(const auto& e:game_.effects)if(e.hostile&&e.kind==EffectKind::EnemyShot)draw_hostile_shot(dl,e);
 pressure_foreground(dl);
 // Danger boundaries stay above friendly art at every quality setting.
 for(const auto& e:game_.effects)if(e.hostile&&e.kind==EffectKind::Warning&&e.age<e.delay){
  auto p=screen(e.pos);ImU32 color=IM_COL32(255,115,84,250);
  if(e.pattern==4)sector(p,e.radius*zoom_,std::atan2(e.end.y,e.end.x),e.angle,0,color,2.5f);
  else if(e.pattern==1){auto q=screen(e.end);auto d=normalized(e.end-e.pos);ImVec2 o{-d.y*e.radius*zoom_,d.x*e.radius*zoom_};dl->AddLine({p.x+o.x,p.y+o.y},{q.x+o.x,q.y+o.y},color,2.5f);dl->AddLine({p.x-o.x,p.y-o.y},{q.x-o.x,q.y-o.y},color,2.5f);dl->AddCircle(p,e.radius*zoom_,color,24,1.5f);dl->AddCircle(q,e.radius*zoom_,color,24,1.5f);}
  else dl->AddCircle(p,e.radius*zoom_,color,48,2.5f);
 }
 // Health bars, status rings and labels are batched after sprite drawing.
 // Keep dense crowds readable; important targets keep their status overlays.
 std::vector<ImVec2> overlay_positions;overlay_positions.reserve(40);
 for(const auto& e:game_.enemies){
  auto p=screen(e.pos);if(!visible(p))continue;
  const bool priority=e.boss_owner||e.kind==EnemyKind::Boss||e.kind==EnemyKind::Elite||e.mark_remaining>0||std::find(game_.special_event.guardians.begin(),game_.special_event.guardians.end(),e.id)!=game_.special_event.guardians.end();
  if(!priority&&(overlay_positions.size()>=40||std::any_of(overlay_positions.begin(),overlay_positions.end(),[&](ImVec2 other){return std::abs(p.x-other.x)<52&&std::abs(p.y-other.y)<32;})))continue;
  if(overlay_positions.size()<40)overlay_positions.push_back(p);
  if(e.boss_owner)centered_text({p.x,p.y-(e.radius+30)*zoom_},IM_COL32(248,210,138,245),boss_combat_definition(e.boss_profile).node_name);
  if(e.kind==EnemyKind::Boss&&e.boss_exposure>0&&e.windup<=0&&e.action_remaining<=0)centered_text({p.x,p.y-(e.radius+18)*zoom_},IM_COL32(170,239,207,245),"破绽 · 承伤 +30%");
  if(e.shield_remaining>0)dl->AddCircle(p,(e.radius+8)*zoom_,IM_COL32(130,208,255,190),24,2);
  if(e.buff_remaining>0)dl->AddCircle(p,(e.radius+12)*zoom_,IM_COL32(255,194,92,160),24,2);
  const float status_y=p.y-(e.radius+12)*zoom_,mark_size=4*zoom_;
  if(e.mark_remaining>0){const ImVec2 at{p.x-12*zoom_,status_y};dl->AddQuad({at.x,at.y-mark_size},{at.x+mark_size,at.y},{at.x,at.y+mark_size},{at.x-mark_size,at.y},IM_COL32(255,213,116,235),1.6f);dl->AddCircleFilled(at,1.1f*zoom_,IM_COL32(255,234,175,230),8);}
  if(e.armor_break_remaining>0){const ImVec2 at{p.x,status_y};const ImU32 color=IM_COL32(247,173,133,230);dl->PathLineTo({at.x-mark_size,at.y-mark_size});dl->PathLineTo({at.x+mark_size,at.y-mark_size});dl->PathLineTo({at.x+mark_size,at.y+mark_size*.4f});dl->PathLineTo({at.x,at.y+mark_size*1.3f});dl->PathLineTo({at.x-mark_size,at.y+mark_size*.4f});dl->PathStroke(color,ImDrawFlags_Closed,1.3f);dl->AddLine({at.x+1,at.y-mark_size},{at.x-1,at.y},color,1.5f);dl->AddLine({at.x-1,at.y},{at.x+1,at.y+mark_size},color,1.5f);}
  if(e.frost_remaining>0){const ImVec2 at{p.x+12*zoom_,status_y};for(int arm=0;arm<3;++arm){float angle=arm*pi/3;ImVec2 offset{std::cos(angle)*mark_size,std::sin(angle)*mark_size};dl->AddLine({at.x-offset.x,at.y-offset.y},{at.x+offset.x,at.y+offset.y},IM_COL32(171,231,252,235),1.4f);}}
  if(e.poison_remaining>0){const ImVec2 at{p.x+24*zoom_,status_y};dl->AddTriangle({at.x,at.y-mark_size},{at.x+mark_size,at.y+mark_size},{at.x-mark_size,at.y+mark_size},IM_COL32(176,229,116,235),1.4f);for(int layer=0;layer<std::clamp(e.poison_layers,0,3);++layer)dl->AddCircleFilled({at.x+(layer-1)*2.7f*zoom_,at.y+7*zoom_},.9f*zoom_,IM_COL32(186,238,129,240),6);}
  if(e.kind!=EnemyKind::Boss&&(e.boss_owner||e.hp<e.max_hp||e.kind==EnemyKind::Elite)){
   float w=36*zoom_,y=p.y-65*enemy_scale(e.kind,e.boss_profile,e.visual);
   dl->AddRectFilled({p.x-w/2,y},{p.x+w/2,y+3*zoom_},IM_COL32(20,25,20,230));
   dl->AddRectFilled({p.x-w/2,y},{p.x-w/2+w*std::clamp(e.hp/std::max(1.f,e.max_hp),0.f,1.f),y+3*zoom_},gold);
  }
  if(std::find(game_.special_event.guardians.begin(),game_.special_event.guardians.end(),e.id)!=game_.special_event.guardians.end())centered_text({p.x,p.y-80*enemy_scale(e.kind,e.boss_profile,e.visual)},gold,"目标");
 }
 for(const auto& object:game_.scene_objects){
  auto p=screen(object.pos);if(!visible(p))continue;
  if(object.kind==SceneObjectKind::Crate&&object.state==SceneObjectState::Active&&object.hp<object.max_hp){
   float w=32*zoom_,y=p.y-57*zoom_;
   dl->AddRectFilled({p.x-w/2,y},{p.x+w/2,y+3*zoom_},IM_COL32(20,25,20,230));
   dl->AddRectFilled({p.x-w/2,y},{p.x-w/2+w*std::clamp(object.hp/std::max(1.f,object.max_hp),0.f,1.f),y+3*zoom_},gold);
  }
  if(object.kind==SceneObjectKind::Barrel&&object.state==SceneObjectState::Armed){char text[96];snprintf(text,sizeof(text),"爆裂花 · %.1f 秒后爆炸",std::max(0.f,object.countdown));centered_text({p.x,p.y+22*zoom_},IM_COL32(255,184,112,255),text);}
  else if(object.kind==SceneObjectKind::HealingWell&&length(object.pos-game_.player.pos)<170)centered_text({p.x,p.y+23*zoom_},object.state==SceneObjectState::Spent?muted:IM_COL32(154,244,184,255),object.state==SceneObjectState::Spent?"恢复石灯 · 已耗尽":"恢复石灯 · 靠近按 E");
  else if(object.kind==SceneObjectKind::Mud&&length(object.pos-game_.player.pos)<object.radius+45)centered_text({p.x,p.y+object.radius*zoom_+6},IM_COL32(222,193,144,255),"泥地 · 移速降低");
 }
 if(escape_open){const float angle=pressure_escape?game_.pressure_event.escape_angle:encounter.escape_angle;const Vec2 direction{std::cos(angle),std::sin(angle)};auto p=screen(game_.player.pos+direction*170);centered_text(p,IM_COL32(160,250,199,255),"包围出口");}
 for(const auto& e:game_.effects)if(e.kind==EffectKind::Warning&&e.hostile&&e.age<e.delay&&e.enemy_kind==EnemyKind::Boss){
  auto p=screen(e.pattern==1?e.pos+(e.end-e.pos)*.5f:e.pos);char text[64];snprintf(text,sizeof(text),"%s · %.1f 秒",e.count>=200?Game::boss_attack_name(e.count):"闪避",std::max(0.f,e.delay-e.age));centered_text({p.x,p.y+18},IM_COL32(255,192,147,255),text);
 }
 for(const auto& item:game_.items)if(length(item.pos-game_.player.pos)<220){auto p=screen(item.pos);centered_text({p.x,p.y+25},ivory,Game::item_name(item.kind));}
 if(game_.special_event.kind!=SpecialEventKind::None){auto p=screen(game_.special_event.pos);centered_text({p.x,p.y+45*zoom_},gold,Game::special_event_name(game_.special_event.kind));}
 if(game_.shield_remaining>0||(game_.guard_charge>0&&game_.buffs_suppressed()))centered_text({player_pos.x,player_pos.y-78*zoom_},game_.buffs_suppressed()?IM_COL32(151,163,170,185):IM_COL32(175,230,255,255),game_.buffs_suppressed()?"护盾受压":"护盾");
 int number_index=0;for(const auto& n:game_.damage_numbers){if(damage_number_density_<=0)continue;const int stride=std::max(1,int(std::ceil(1.f/damage_number_density_)));if((number_index++%stride)!=0&&!n.critical)continue;auto p=screen(n.pos);p.y-=n.age*40;std::string text=std::to_string(n.value);dl->AddText({p.x+1,p.y+1},IM_COL32(0,0,0,200),text.c_str());dl->AddText(p,n.critical?gold:ivory,text.c_str());}
 dl->AddRectFilledMultiColor({0,0},{size.x,115},IM_COL32(9,20,15,240),IM_COL32(9,20,15,240),IM_COL32(9,20,15,0),IM_COL32(9,20,15,0));
}

void App::initialize_audio(){WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=22050;fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=44100;if(waveOutOpen(&audio_device_,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){audio_device_=nullptr;return;}for(size_t i=0;i<audio_headers_.size();++i){auto&h=audio_headers_[i];h.lpData=reinterpret_cast<LPSTR>(audio_buffers_[i].data());h.dwBufferLength=DWORD(audio_buffers_[i].size()*sizeof(short));waveOutPrepareHeader(audio_device_,&h,sizeof(h));waveOutWrite(audio_device_,&h,sizeof(h));}}
void App::play_sound(EventKind kind){
 switch(kind){
 case EventKind::LevelUp:music_.cue(MusicCue::LevelUp);return;
 case EventKind::Choice:music_.cue(MusicCue::Choice);return;
 case EventKind::Scroll:music_.cue(MusicCue::Scroll);return;
 case EventKind::Fusion:music_.cue(MusicCue::Fusion);return;
 case EventKind::Boss:music_.cue(MusicCue::Boss);return;
 case EventKind::Victory:music_.cue(MusicCue::Victory);return;
 case EventKind::Defeat:music_.cue(MusicCue::Defeat);return;
 default:break;
 }
 if(!audio_device_||!sound_||volume_<=0)return;
 if(!legacy_visuals_&&(kind==EventKind::Shot||kind==EventKind::Hit))return;
 bool important=kind==EventKind::Hurt||kind==EventKind::Dash||kind==EventKind::SpecialEvent;
 if(audio_cooldown_>0&&!important)return;audio_cooldown_=.065f;
 auto voice=std::find_if(voices_.begin(),voices_.end(),[](const auto& v){return v.left<=0;});if(voice==voices_.end())return;
 voice->phase=0;voice->frequency=kind==EventKind::Hurt?110.f:kind==EventKind::Dash?350.f:kind==EventKind::SpecialEvent?220.f:kind==EventKind::Kill?440.f:780.f;voice->total=voice->left=important?2200:1800;
}
void App::pump_audio(){
 if(!audio_device_)return;
 const State state=game_.state;const bool paused=state==State::Paused||state==State::Upgrade||state==State::ScrollChoice;
 if(state!=audio_state_){if(state!=State::Playing){for(auto& voice:voices_)voice.left=0;skill_sfx_.clear();}audio_state_=state;}
 music_.set_softened(paused||state==State::ChapterComplete);
 if(!paused){
  MusicScene scene=state==State::Menu?(camp_tab_==0?MusicScene::Menu:MusicScene::Camp):state==State::ChapterComplete?MusicScene::Camp:state==State::Victory?MusicScene::Victory:state==State::Defeat?MusicScene::Defeat:MusicScene::Battle;
  bool boss=std::any_of(game_.enemies.begin(),game_.enemies.end(),[](const Enemy& e){return e.hp>0&&e.kind==EnemyKind::Boss;});
  float pressure=game_.pressure_event.remaining>0?.85f:std::min(1.f,float(game_.enemies.size())/150.f);
  if(state==State::Playing){if(boss)scene=MusicScene::Boss;else if(game_.pressure_event.remaining>0)scene=MusicScene::Pressure;}
  music_.request_scene(scene,pressure);
 }
 if(audio_buffers_written_>0&&std::all_of(audio_headers_.begin(),audio_headers_.end(),[](const WAVEHDR& h){return (h.dwFlags&WHDR_DONE)!=0;}))++audio_queue_gaps_;
 for(size_t i=0;i<audio_headers_.size();++i){auto& header=audio_headers_[i];if(!(header.dwFlags&WHDR_DONE))continue;
  for(auto& sample:audio_buffers_[i]){float combat=0;for(auto& voice:voices_)if(voice.left>0){float env=float(voice.left)/voice.total;combat+=std::sin(voice.phase)*env*env;voice.phase+=6.2831853f*voice.frequency/22050.f;--voice.left;}
   const auto value=music_.next_sample();combat+=skill_sfx_.sample();float mix=combat*(sound_?volume_*.21f:0)+value.music*(music_enabled_?music_volume_*.65f:0)+value.cue*(ui_sound_?ui_volume_*.8f:0);
   float output=std::tanh(mix);audio_peak_=std::max(audio_peak_,std::abs(output));sample=short(output*32760.f);
  }if(waveOutWrite(audio_device_,&header,sizeof(header))!=MMSYSERR_NOERROR)++audio_errors_;else ++audio_buffers_written_;
 }
}
nlohmann::json App::audio_diagnostics() const{return {{"skill_voices_peak",skill_sfx_.peak_voices},{"skill_sounds",skill_sfx_.triggered},{"skill_sounds_limited",skill_sfx_.limited},{"device_open",audio_device_!=nullptr},{"buffers_written",audio_buffers_written_},{"write_errors",audio_errors_},{"empty_queue_frames",audio_queue_gaps_},{"queue_capacity_ms",audio_buffers_.size()*512.0/22.05},{"output_peak",audio_peak_},{"rendered_samples",music_.rendered_samples()},{"track_id",Chiptune::tracks()[music_.track_index()].id},{"playhead_seconds",music_.playhead_seconds()},{"transitions",music_.transition_count()},{"music_enabled",music_enabled_},{"combat_enabled",sound_},{"ui_enabled",ui_sound_}};}

void App::preview_build_panels(int panel){
 if(panel==10){preview_upgrade_choices(1);return;}
 if(panel==2||panel==3){preview_upgrade_choices(panel==3?2:0);return;}
 if(panel==5){profiling=true;preview_static_=true;return;}
 profiling=true;preview_static_=true;show_stats_=true;game_.config.enable_director=false;game_.config.enable_scene_objects=false;game_.config.first_event_time=10000;game_.config.pressure_first_min=game_.config.pressure_first_max=10000;game_.config.boss_time=panel==8?10000.f:0.f;
 game_.start(CharacterClass::Mage,20261112);game_.encounter_plan.boss=int(BossProfile::Golem);game_.skills.fill(0);
 for(Skill skill:{Skill::Frost,Skill::Lightning,Skill::PoisonMist,Skill::Zone,Skill::Charge,Skill::Bulwark,Skill::Retaliate,Skill::Homing})game_.skills[int(skill)]=skill==Skill::Frost?1:3;
 game_.player.invulnerable=10000;game_.player.xp_next=1e9f;game_.enemies.clear();
 for(int i=0;i<16;++i){Enemy e;e.id=93001+i;e.visual=i;e.pos={float(i%4)*38-70,float(i/4)*38-70};e.hp=e.max_hp=20000;e.speed=8;game_.enemies.push_back(e);}
 game_.update(1.f/60);for(auto& e:game_.enemies)if(e.kind==EnemyKind::Boss){e.pos={170,-75};e.hp=e.max_hp=100000;}
 game_.fury_remaining=game_.shield_remaining=20;
 if(panel==0){game_.begin_pressure_event(PressureKind::LastWords);game_.pressure_event.sealed=Skill::Homing;DangerZone zone;zone.kind=DangerKind::Regression;zone.pos=game_.player.pos;zone.radius=80;zone.warning=0;zone.duration=120;game_.add_danger_zone(zone);game_.update(1.f/60);}
 else if(panel==8)game_.begin_pressure_event(PressureKind::LastWords);else game_.begin_pressure_event(PressureKind::Frenzy);
 if(panel==7){for(int i=0;i<300;++i)game_.update(1.f/60);game_.state=State::Victory;settlement_ok_=true;camp_notice_="诊断预览：构筑与贡献来自本场战斗，未写入正式结算。";}
 if(panel==9){game_.skills.fill(0);for(Skill skill:{Skill::Homing,Skill::Rune,Skill::Arrow,Skill::BattleFocus,Skill::HolyLight,Skill::Bulwark,Skill::EarthShatter,Skill::Meteor})game_.skills[int(skill)]=3;game_.auto_fuse_skills();}
 if(panel==4){preview_build_tab_=3;}if(panel==4||panel==6)game_.toggle_pause();if(panel==6)settings_=true;prepare_chapter_resources();
}
// Opt-in acceptance fixtures use only the explicit diagnostic save directory.
void App::review_ui_flow_scene(int scene){
 // Only fixture preparation happens here. The review driver sends key/mouse
 // events to the rendered UI; it never calls the choice/replacement methods.
 review_ui_flow_=profiling=preview_static_=fixed_visual_review=true;ordinary_playthrough_=complete_run_=false;guidance_=false;guide_remaining_=0;
 game_.config.enable_pressure=game_.config.enable_director=game_.config.enable_scene_objects=false;game_.config.first_event_time=game_.config.boss_time=10000;
 game_.start(CharacterClass::Mage,20261120+scene);game_.skills.fill(0);
 if(scene==0){game_.skills[int(Skill::Homing)]=4;game_.skills[int(Skill::Rune)]=5;}
 else for(Skill skill:{Skill::Homing,Skill::Rune,Skill::Frost,Skill::Lightning,Skill::Cleave,Skill::BattleFocus,Skill::Zone,Skill::Meteor})game_.skills[int(skill)]=1;
 // A real XP pickup creates the ordinary two-pick round and its free reset.
 game_.gems.push_back({game_.player.pos,game_.player.xp_next});game_.update(1.f/60);
 const std::array<Skill,5> candidates=scene==0?std::array<Skill,5>{{Skill::Homing,Skill::Arrow,Skill::Frost,Skill::Zone,Skill::Meteor}}:std::array<Skill,5>{{Skill::PoisonDart,Skill::Homing,Skill::Rune,Skill::Frost,Skill::Lightning}};
 game_.choices.clear();for(Skill skill:candidates){UpgradeChoice card{UpgradeKind::Skill,skill,Game::skill_name(skill),game_.skill_upgrade_description(skill)};card.expected_rank=game_.skills[int(skill)];card.description+="\n"+game_.fusion_preview(skill);game_.choices.push_back(card);}
 upgrade_detail_index_=-1;upgrade_action_delay_=0;settings_=false;review_ui_targets_.fill(false);prepare_chapter_resources();
}
bool App::review_ui_flow_target(int target,ImVec2& point)const{
 if(target<0||target>=int(review_ui_targets_.size())||!review_ui_targets_[target])return false;point=review_ui_points_[target];return true;
}
void App::review_ui_flow_item(int target){
 if(!review_ui_flow_||!ImGui::IsItemVisible())return;
 const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();review_ui_points_[target]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};review_ui_targets_[target]=true;
}
nlohmann::json App::review_ui_flow_snapshot()const{
 nlohmann::json result={{"state",int(game_.state)},{"picks_taken",game_.upgrade_picks_taken},{"picks_required",game_.upgrade_picks_required()},{"free_resets",game_.upgrade_resets_remaining},{"commits",game_.metrics.upgrade_choices},{"fusions",game_.metrics.fusions_created},{"replacements",game_.scrolls.replacements},{"active_slots",game_.active_skill_count()},{"skills",nlohmann::json::object()},{"choices",nlohmann::json::array()}};
 for(int i=0;i<skill_count;++i)if(game_.skills[i])result["skills"][Game::skill_key(Skill(i))]=game_.skills[i];
 for(const auto& choice:game_.choices)result["choices"].push_back({{"kind",int(choice.kind)},{"skill",Game::skill_key(choice.skill)},{"selected",choice.selected},{"expected_rank",choice.expected_rank},{"available",game_.upgrade_choice_available(choice)}});
 result["pending_replacement"]={{"active",game_.pending_study.active},{"learn",game_.pending_study.learn==Skill::Count?"":Game::skill_key(game_.pending_study.learn)},{"replace",game_.pending_study.replace==Skill::Count?"":Game::skill_key(game_.pending_study.replace)}};
 return result;
}
void App::preview_upgrade_choices(int panel){
 profiling=true;preview_static_=true;show_stats_=true;game_.config.enable_pressure=false;game_.config.first_event_time=game_.config.boss_time=10000;
 game_.start(CharacterClass::Mage,20261112);game_.skills.fill(0);for(Skill skill:{Skill::Homing,Skill::Rune,Skill::Frost,Skill::Lightning,Skill::Cleave,Skill::BattleFocus,Skill::ThunderLance,Skill::SwordCyclone})game_.skills[int(skill)]=Game::is_fusion(skill)?2:1;game_.skills[int(Skill::Rune)]=2;
 game_.scrolls.completed={{Skill::ThunderLance,0},{Skill::SwordCyclone,0}};game_.player.invulnerable=10000;game_.gems.push_back({game_.player.pos,8});game_.update(1.f/60);
 if(panel==1){UpgradeChoice choice{UpgradeKind::Skill,Skill::Homing,"追踪魔弹 +1",game_.skill_upgrade_description(Skill::Homing)};choice.expected_rank=1;game_.choices[0]=choice;game_.choose_upgrade(0);}
 if(panel==3)upgrade_detail_index_=0;
 if(panel==2)for(size_t i=0;i<game_.choices.size();++i){const auto& choice=game_.choices[i];if(choice.kind==UpgradeKind::Skill&&game_.skills[int(choice.skill)]==0){game_.choose_upgrade(i);game_.select_study_replacement(Skill::SwordCyclone);break;}}
 prepare_chapter_resources();
}
void App::preview_adventure(int panel){
 preview_boss(int(BossProfile::GiantCrab));game_.skills.fill(0);game_.effects.clear();game_.projectiles.clear();game_.scrolls={};game_.consumed_skills=0;game_.metrics.fusions_created=0;game_.metrics.first_fusion_time=-1;
 game_.elapsed=720;game_.timing.speed=2;game_.config.duration=1200;game_.config.boss_time=1110;game_.config.enable_pressure=false;
 game_.skills[int(Skill::TripleStar)]=game_.skills[int(Skill::ShadowClone)]=5;game_.auto_fuse_skills();
 game_.skills[int(Skill::ThousandStars)]=5;game_.skills[int(Skill::Lightning)]=5;game_.auto_fuse_skills();
 if(panel==1){game_.skills[int(Skill::ThunderShadow)]=game_.skills[int(Skill::PoisonDart)]=5;game_.auto_fuse_skills();}
 if(panel==2){
  game_.skills.fill(0);game_.scrolls={};game_.consumed_skills=0;game_.metrics.fusions_created=0;
  game_.skills[int(Skill::Missiles)]=game_.skills[int(Skill::Engineer)]=5;game_.auto_fuse_skills();
  game_.skills[int(Skill::Cannon)]=game_.skills[int(Skill::OctopusTurret)]=5;game_.auto_fuse_skills();
  game_.skills[int(Skill::SkySwarm)]=game_.skills[int(Skill::DeepFleet)]=5;game_.auto_fuse_skills();
 }
 for(Skill skill:{Skill::RoseFinale,Skill::Uppercut,Skill::DragonConcert})game_.skills[int(skill)]=3;
 if(panel==4)for(const auto& gear:gear_definitions())game_.gain_gear(gear.id,2,10);
 if(panel==3||panel==4){game_.state=State::Paused;preview_build_tab_=panel==3?3:4;}
 if(panel==5){game_.skills.fill(0);game_.skills[int(Skill::ThousandStars)]=5;game_.skills[int(Skill::Lightning)]=4;game_.state=State::Upgrade;game_.upgrade_picks_taken=0;game_.upgrade_resets_remaining=1;game_.choices.clear();
  for(Skill skill:{Skill::Lightning,Skill::PoisonDart,Skill::DualGuns,Skill::PhantomCards,Skill::GlacierSpear}){UpgradeChoice card{UpgradeKind::Skill,skill,Game::skill_name(skill),game_.skill_upgrade_description(skill)};card.expected_rank=game_.skills[int(skill)];card.description+="\n"+game_.fusion_preview(skill);game_.choices.push_back(card);}
 }
 preview_static_=true;
}
void App::profile_complete_run(){
 // Immortality makes this a timer/settlement fixture, never evidence of normal balance.
 complete_run_=true;profiling=true;selected_=1;selected_mode_=0;selected_game_speed_=2;run_seed_=20261113;
 if(begin_adventure(true)){game_.player.invulnerable=10000;game_.timing.speed=2;}
}
void App::profile_start(bool stress,bool fused){
 // Synthetic render/load fixture: invulnerability and dense enemies are not a natural run.
 game_.config.duration=game_.config.boss_time=86400;profiling=true;sound_=true;show_stats_=true;game_.start(CharacterClass::Mage,123);game_.skills.fill(0);
 if(!fused)for(Skill skill:{Skill::Homing,Skill::Lightning,Skill::Zone,Skill::Frost,Skill::Meteor,Skill::Rune})game_.skills[int(skill)]=stress?5:3;
 else{
  for(Skill result:{Skill::PoisonFlare,Skill::DragonPhoenix})for(const auto& recipe:fusion_recipes())if(recipe.result==result){
   game_.skills[int(recipe.first)]=game_.skills[int(recipe.second)]=recipe.material_level;
   game_.scrolls.completed.push_back({recipe.result,1});game_.auto_fuse_skills();
   for(Skill material:{recipe.first,recipe.second})if(Game::skill_owner(material)!=game_.character)game_.scrolls.learned_foreign|=uint64_t(1)<<int(material);
   game_.skills[int(recipe.result)]=5;
  }
  for(int id=0;id<skill_count&&game_.active_skill_count()<Game::active_skill_limit;++id)if(!Game::is_fusion(Skill(id))&&Game::skill_owner(Skill(id))==game_.character&&game_.can_gain_skill(Skill(id)))game_.skills[id]=5;
 }
 game_.timing.recorded=true;game_.player.hp=game_.player.max_hp;game_.player.invulnerable=86400;game_.player.xp_next=1e9f;game_.enemies.clear();
 const int count=stress?500:150;for(int i=0;i<count;++i){Enemy enemy;enemy.id=i+1;enemy.kind=EnemyKind(i%4);enemy.visual=i%enemy_visual_count;enemy.pos={float(i%25*20-250),float(i/25*20-200)};enemy.hp=enemy.max_hp=1e9f;enemy.speed=stress?0.f:20.f;game_.enemies.push_back(enemy);}
 prepare_chapter_resources();
}
// These opt-in render fixtures never read or write the live profile.
void App::profile_dark_tide(){
 profile_start(true,true);game_.config.max_enemies=500;
 for(int i=int(game_.enemies.size());i<1500;++i){Enemy enemy=game_.enemies[size_t(i%500)];enemy.id=uint32_t(i+1);enemy.visual=i%enemy_visual_count;game_.enemies.push_back(enemy);}
 for(size_t i=0;i<game_.enemies.size();++i){auto& enemy=game_.enemies[i];enemy.pos={float(int(i%50)*12-294),float(int(i/50)*12-174)};enemy.speed=20;enemy.damage=8;}
 if(!game_.begin_pressure_event(PressureKind::DarkTide))content_diagnostics_.push_back("合成容量场景的黑暗狂潮未启动。");
}
void App::setup_pressure_preview(){
 preview_static_=true;preview_dash_=false;preview_domain_exit_=false;profiling=true;show_stats_=true;
 game_.config.duration=game_.config.boss_time=86400;game_.config.enable_pressure=true;game_.config.enable_director=false;game_.config.enable_scene_objects=false;
 RunSetup setup;setup.equipment={EquipmentId::ApprenticeWand,EquipmentId::ScholarRobe,EquipmentId::AmberPendant};game_.start(CharacterClass::Mage,20261112,{},setup);game_.skills.fill(0);
 game_.skills[int(Skill::Frost)]=game_.skills[int(Skill::Lightning)]=2;game_.auto_fuse_skills();
 for(Skill skill:{Skill::Homing,Skill::Zone,Skill::Rune,Skill::HolyLight,Skill::MagicVortex})game_.skills[int(skill)]=skill==Skill::Homing?1:3;
 game_.player.invulnerable=600;game_.player.xp_next=1e9f;game_.timing.recorded=true;game_.enemies.clear();game_.scene_objects.clear();game_.effects.clear();game_.projectiles.clear();game_.gems.clear();
 game_.fury_remaining=game_.magnet_remaining=game_.power_remaining=15;game_.shield_remaining=12;
 for(int i=0;i<14;++i){const float angle=float(i)*6.2831853f/14.f;Enemy enemy;enemy.id=92001+i;enemy.kind=i%4==0?EnemyKind::Ranger:EnemyKind::Slime;enemy.visual=i%enemy_visual_count;enemy.pos={std::cos(angle)*175,std::sin(angle)*175};enemy.hp=enemy.max_hp=20000;enemy.speed=8;enemy.damage=2;game_.enemies.push_back(enemy);}
}
void App::preview_pressure(int event){
 setup_pressure_preview();if(!game_.begin_pressure_event(PressureKind(std::clamp(event,1,pressure_kind_count-1))))content_diagnostics_.push_back("所选压力事件在当前合成构筑中不合法，未启动。");prepare_chapter_resources();
}
void App::preview_ability(int ability){
 setup_pressure_preview();const auto selected=EnemyAbility(std::clamp(ability-1,0,enemy_ability_count-1));auto& enemy=game_.enemies.front();enemy.kind=EnemyKind::Elite;enemy.visual=17;enemy.pos={105,-15};enemy.radius=26;enemy.speed=8;enemy.abilities.elite=true;enemy.abilities.mask=1u<<int(selected);enemy.abilities.field_cooldown=.15f;
 enemy.abilities.refraction=selected==EnemyAbility::Refraction?4:0;if(selected==EnemyAbility::Promise||selected==EnemyAbility::DeathBlast){enemy.hp=1;enemy.max_hp=100;}++game_.pressure_metrics.abilities[int(selected)];prepare_chapter_resources();
}
void App::preview_domain(int domain){
 setup_pressure_preview();preview_domain_exit_=true;DangerZone zone;zone.kind=DangerKind(std::clamp(domain-1,0,4));zone.pos={0,0};zone.radius=125;zone.warning=1.2f;zone.duration=zone.kind==DangerKind::Explosion?1.55f:12.f;zone.damage=8;if(!game_.add_danger_zone(zone))content_diagnostics_.push_back("所选领域未通过当前区域预算，未生成。");prepare_chapter_resources();
}
void App::review_boss_fight(int profile,int build){
 // Prescribed isolated combat: actual damage/death, no in-fight HP writes.
 // Acquisition and population are fixtures, not a natural run balance sample.
 ordinary_playthrough_=profiling=true;complete_run_=false;preview_static_=false;
 game_.config.enable_pressure=false;game_.config.first_event_time=game_.config.boss_time=1e6f;game_.config.max_enemies=40;
 game_.config.director_config.ambient_interval=4.f;
 game_.start(CharacterClass::Mage,20261112+uint32_t(profile));game_.skills.fill(0);game_.enemies.clear();
 const Skill builds[5][2]={{Skill::HeavyStrike,Skill::BladeWave},{Skill::Homing,Skill::Arrow},{Skill::Zone,Skill::Rune},{Skill::OctopusTurret,Skill::SilverEagle},{Skill::SeaFortress,Skill::PhantomFinale}};
 for(Skill skill:builds[std::clamp(build,0,4)])game_.skills[int(skill)]=3;
 Enemy boss;boss.id=90001;boss.kind=EnemyKind::Boss;boss.boss_profile=BossProfile(std::clamp(profile,0,boss_profile_count-1));boss.pos={220,-40};boss.hp=boss.max_hp=3200;boss.radius=boss_definition(boss.boss_profile).radius;boss.speed=boss_definition(boss.boss_profile).speed;boss.damage=12;boss.attack_cooldown=.6f;
 game_.enemies.push_back(boss);game_.metrics.content.boss_encounters[profile]=1;game_.metrics.content.bosses_seen|=1u<<profile;
 review_controller_={};autoplay_choice_delay_=0;game_.timing.recorded=true;prepare_chapter_resources();
}
void App::preview_boss(int profile){
 preview_static_=true;profiling=true;show_stats_=true;game_.start(CharacterClass::Archer,20261111);game_.skills.fill(0);game_.skills[int(Skill::Arrow)]=1;game_.player.invulnerable=100000;
 game_.enemies.clear();Enemy boss;boss.id=90001;boss.kind=EnemyKind::Boss;boss.boss_profile=BossProfile(std::clamp(profile,0,boss_profile_count-1));boss.pos={115,-45};boss.hp=boss.max_hp=100000;boss.radius=45;boss.speed=26;boss.damage=10;boss.attack_cooldown=.25f;game_.enemies.push_back(boss);game_.timing.recorded=true;prepare_chapter_resources();
}
void App::preview_fusion(int character,int variant,int panel,int form,int boss){
 if(boss>=0)panel=0;
 preview_static_=true;profiling=true;show_stats_=true;game_.start(CharacterClass(std::clamp(character,0,2)),20261111);game_.skills.fill(0);
 std::vector<const FusionRecipe*> recipes;for(const auto& recipe:fusion_recipes())if(recipe.character==game_.character||recipe.other_class==int(game_.character))recipes.push_back(&recipe);
 if(recipes.empty())return;const auto& recipe=*recipes[size_t(std::clamp(variant,0,int(recipes.size())-1))];

 if(panel==1){
  game_.skills[int(Game::skill_owner(recipe.first)==game_.character?recipe.first:recipe.second)]=2;
  preview_build_tab_=3;game_.toggle_pause();
 }else{
  game_.skills[int(recipe.first)]=recipe.material_level;
  if(Game::skill_owner(recipe.first)!=game_.character)game_.scrolls.learned_foreign|=uint64_t(1)<<int(recipe.first);
  if(panel<3){game_.skills[int(recipe.second)]=recipe.material_level;if(Game::skill_owner(recipe.second)!=game_.character)game_.scrolls.learned_foreign|=uint64_t(1)<<int(recipe.second);}
  if(panel<3){game_.scrolls.completed.push_back({recipe.result,std::clamp(form,0,Game::form_count(recipe.result)-1)});game_.auto_fuse_skills();}
  if(panel==0){for(int id=0;id<skill_count&&game_.active_skill_count()<Game::active_skill_limit;++id)if(game_.can_gain_skill(Skill(id)))game_.skills[id]=2;}
  else if(panel==2)game_.toggle_pause();
  else{
   if(panel==4)for(int id=0;id<skill_count&&game_.active_skill_count()<Game::active_skill_limit;++id)if(Skill(id)!=recipe.second&&!Game::is_fusion(Skill(id))&&game_.can_gain_skill(Skill(id)))game_.skills[id]=2;
   game_.gems.push_back({game_.player.pos,game_.player.xp_next,0});game_.update(1.f/60);
   if(panel==4){for(size_t i=0;i<game_.choices.size();++i)if(game_.choices[i].kind==UpgradeKind::Skill&&game_.skills[int(game_.choices[i].skill)]==0){game_.choose_upgrade(i);break;}}
  }
 }
 preview_dash_=recipe.result==Skill::DashBreaker||recipe.result==Skill::MountainBreaker||recipe.result==Skill::PhoenixAssault;game_.timing.recorded=true;
 game_.player.invulnerable=100000;game_.fury_remaining=game_.magnet_remaining=game_.double_xp_remaining=game_.power_remaining=20;game_.shield_remaining=10;
 game_.enemies.clear();for(int i=0;i<18;++i){float a=float(i)*6.2831853f/18.f;Enemy enemy;enemy.id=90001+i;enemy.visual=i%enemy_visual_count;enemy.kind=EnemyKind::Slime;enemy.pos={std::cos(a)*155,std::sin(a)*155};enemy.hp=enemy.max_hp=10000;enemy.speed=4;enemy.damage=0;game_.enemies.push_back(enemy);}
 if(boss>=0){Enemy enemy;enemy.id=91000;enemy.kind=EnemyKind::Boss;enemy.boss_profile=BossProfile(std::clamp(boss,0,boss_profile_count-1));enemy.pos={115,-45};enemy.hp=enemy.max_hp=100000;enemy.radius=45;enemy.speed=26;enemy.damage=10;enemy.attack_cooldown=.25f;game_.enemies.push_back(enemy);}
 if(recipe.result==Skill::FrostArmor&&panel==0){game_.player.invulnerable=0;game_.enemies.front().pos={18,0};game_.enemies.front().damage=1;}
 prepare_chapter_resources();
}
void App::write_playtest_state(){nlohmann::json j={{"version",MAPLERGL_VERSION},{"mode",Game::mode_name(game_.run_mode())},{"chapter",game_.chapter_index()},{"asset_root",std::filesystem::current_path().u8string()+"/assets"},{"state",int(game_.state)},{"class",int(game_.character)},{"elapsed",game_.elapsed},{"position",{game_.player.pos.x,game_.player.pos.y}},{"hp",game_.player.hp},{"level",game_.player.level},{"kills",game_.kills},{"move_input_frames",move_input_frames_},{"dash_presses",dash_presses_},{"upgrade_selections",upgrade_selections_},{"skills",game_.skills},{"passives",game_.passives},{"event",int(game_.special_event.kind)},{"event_remaining",game_.special_event.remaining},{"items",game_.items.size()},{"notice",game_.notice_text},{"automated_input",profiling},{"fps",ImGui::GetIO().Framerate},{"choices",nlohmann::json::array()}};j["combat_seconds"]=game_.timing.combat_seconds;j["upgrade_picks_taken"]=game_.upgrade_picks_taken;j["upgrade_commits"]=game_.metrics.upgrade_choices;j["replacements"]=game_.scrolls.replacements;j["pending_replacement"]={{"active",game_.pending_study.active},{"learn",int(game_.pending_study.learn)},{"replace",int(game_.pending_study.replace)}};j["director"]={{"kind",int(game_.director_state.kind)},{"phase",int(game_.director_state.phase)},{"remaining",game_.director_state.remaining}};j["scene_objects"]=game_.scene_objects.size();j["selected_boss"]=game_.encounter_plan.boss;j["chapter_roster"]=game_.encounter_plan.roster;j["game_speed"]=game_.timing.speed;j["used_speeds"]=game_.timing.used_speeds;j["effective_real_seconds"]=game_.timing.effective_real_seconds;j["total_real_seconds"]=game_.timing.total_real_seconds;j["simulation_backlog_seconds"]=simulation_clock_.backlog_seconds();j["content_diagnostics"]=content_diagnostics_;j["pressure_event"]={{"kind",int(game_.pressure_event.kind)},{"remaining",game_.pressure_event.remaining},{"sealed_skill",int(game_.pressure_event.sealed)}};j["danger_zones"]=game_.danger_zones.size();j["pressure_metrics"]={{"normal_spawn_budget",game_.pressure_metrics.normal_spawn_budget},{"event_spawn_budget",game_.pressure_metrics.event_spawn_budget},{"requested_spawns",game_.pressure_metrics.requested_spawns},{"actual_spawns",game_.pressure_metrics.actual_spawns},{"capacity_limited",game_.pressure_metrics.capacity_limited},{"ability_assignments",game_.pressure_metrics.abilities},{"max_enemies",game_.pressure_metrics.max_enemies},{"max_zones",game_.pressure_metrics.max_zones}};j["bosses"]=nlohmann::json::array();for(const auto& enemy:game_.enemies)if(enemy.kind==EnemyKind::Boss)j["bosses"].push_back({{"profile",int(enemy.boss_profile)},{"name",Game::boss_profile_name(enemy.boss_profile)},{"sprite",Game::boss_sprite_key(enemy.boss_profile)},{"stage",enemy.boss_stage},{"hp",enemy.hp}});for(auto&c:game_.choices)j["choices"].push_back({{"kind",int(c.kind)},{"skill",int(c.skill)},{"title",c.title},{"selected",c.selected},{"available",game_.upgrade_choice_available(c)}});std::ofstream("playtest-state.json")<<j.dump(2);}
}

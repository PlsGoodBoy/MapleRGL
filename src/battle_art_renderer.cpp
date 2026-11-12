#include "app.hpp"
#include "battle_art.hpp"
#include <cmath>

namespace maple {
void App::preview_battle_art(int scene){
 preview_visuals(3,false);game_.skills.fill(0);game_.effects.clear();game_.projectiles.clear();game_.skill_visuals.clear();game_.hostile_impacts.clear();game_.danger_zones.clear();
 game_.config.max_enemies=0;game_.config.enable_pressure=false;game_.elapsed=120;game_.combat_elapsed=0;game_.player.pos={0,0};
 auto boss=game_.enemies.front();boss.pos={240,-45};boss.hp=boss.max_hp=10000000;boss.boss_profile=BossProfile::Snowman;boss.attack_cooldown=.5f;boss.speed=0;game_.enemies={boss};
 std::vector<Skill> build;
 if(scene==0){build={Skill::OctopusTurret,Skill::Engineer,Skill::Missiles};}
 else if(scene==1){build={Skill::Summon,Skill::HolyDragon,Skill::FireBeast,Skill::SilverEagle,Skill::ShadowBats,Skill::DualGuns};game_.enemies[0].pos={-230,-35};}
 else if(scene==2||scene==3){
  build={Skill::OctopusTurret,Skill::Homing,Skill::TripleStar};game_.config.enable_pressure=true;preview_domain_exit_=true;
  game_.gain_gear("maple_badge",1,3);
  DangerZone a;a.kind=scene==2?DangerKind::Smoke:DangerKind::NaturalOrder;a.pos={-55,15};a.radius=110;a.warning=.6f;a.duration=8;game_.add_danger_zone(a);
  DangerZone b=a;b.kind=scene==2?DangerKind::Distortion:DangerKind::Regression;b.pos={175,65};b.radius=110;game_.add_danger_zone(b);
 }else{build={Skill::SeaFortress,Skill::PhantomFinale,Skill::ElementSanctum};}
 for(Skill skill:build)game_.skills[int(skill)]=3;
 for(int n=0;n<3;++n){Enemy e;e.id=7000+n;e.kind=n==0?EnemyKind::Ranger:EnemyKind::Slime;e.visual=n==0?20:n==1?7:18;e.pos={scene==1?-240.f:240.f,45.f+n*65};e.radius=16;e.hp=e.max_hp=10000000;e.speed=0;e.damage=3;game_.enemies.push_back(e);}
 prepare_chapter_resources();
}
namespace {
constexpr float pi=3.14159265f;
ImU32 opacity(ImU32 c,float a){return (c&0xffffffu)|(ImU32(std::clamp(a,0.f,1.f)*255)<<24);}
Vec2 radial(float r,float angle){return {std::cos(angle)*r,std::sin(angle)*r};}
ImVec2 radial(ImVec2 p,float r,float angle){return {p.x+std::cos(angle)*r,p.y+std::sin(angle)*r};}
}

// Centered patches/projectiles align their visible centers and fit canvas size;
// actors use a fixed native scale and the original per-frame ground origin.
void App::battle_sprite(ImDrawList* d,const std::string& key,Vec2 pos,float size,float time,ImU32 tint,float rotation,bool loop,bool centered,bool flip){
 auto found=sprites_.find(key);if(found==sprites_.end()||found->second.empty())return;
 const auto& frames=found->second;auto metrics=visual_sprite_metrics_.find(key);
 if(metrics==visual_sprite_metrics_.end()){
  float span=1,duration=0;for(const auto& f:frames){auto& tex=texture(f.file);span=std::max(span,float(std::max(tex.w,tex.h)));duration+=f.duration;}
  metrics=visual_sprite_metrics_.emplace(key,ImVec2{span,duration}).first;
 }
 float at=loop?std::fmod(std::max(0.f,time),metrics->second.y):std::clamp(time,0.f,metrics->second.y-.0001f);
 const Frame* frame=&frames.back();for(const auto& f:frames){if(at<f.duration){frame=&f;break;}at-=f.duration;}
 const float scale=centered?size/metrics->second.x:size;
 if(centered){Vec2 delta{(frame->ox-frame->cx)*(flip?-1.f:1.f)*scale,(frame->oy-frame->cy)*scale};pos+=Vec2{delta.x*std::cos(rotation)-delta.y*std::sin(rotation),delta.x*std::sin(rotation)+delta.y*std::cos(rotation)};}
 sprite(d,key,screen(pos),scale*zoom_,time,flip,tint,rotation,loop);
}

void App::draw_battle_summon(ImDrawList* d,const Effect& e){
 if(e.kind!=EffectKind::Adventure){
  const bool phoenix=e.kind==EffectKind::Summon,eagle=e.kind==EffectKind::SilverEagle,fire=e.kind==EffectKind::FireBeast;
  const std::string prefix=phoenix?"battle.phoenix":eagle?"skill.silver_eagle":fire?"skill.fire_beast":"skill.holy_dragon";
  const bool enabled=game_.skill_attack_available(e.source,e.pos)&&!game_.smoke_at(game_.player.pos);
  const bool firing=enabled&&e.visual_attack_age<.48f;std::string action=firing?"attack":"move";
  const bool flip=(firing?e.visual_aim:e.end-e.pos).x>0;
  battle_sprite(d,prefix+"."+action,e.pos,phoenix?.75f:eagle?.65f:fire?.5f:.55f,firing?e.visual_attack_age+(phoenix?.36f:eagle?.1f:fire?.88f:.84f):e.age,opacity(enabled?0xffffffffu:IM_COL32(172,182,197,255),.85f+.15f*friendly_effect_alpha_),0,!firing,false,flip);
  return;
 }
 const auto mount=battle_mount(e.source);if(mount==BattleMount::None||e.pattern!=0)return;
 const bool octopus=mount==BattleMount::Octopus,engineer=mount==BattleMount::Engineer;
 const std::string prefix=octopus?"battle.octopus":engineer?"battle.engineer":"battle.launcher";
 const float scale=octopus?.9f:engineer?.85f:.6f;
 const Vec2 axis{std::cos(e.angle),std::sin(e.angle)},side{-axis.y,axis.x};
 std::vector<Vec2> nodes{e.pos};
 if(e.source==Skill::SeaFortress)nodes={e.pos-side*130,e.pos+side*130,e.pos+axis*100};
 else if(e.source==Skill::SkySwarm)nodes={e.pos-side*65,e.pos+side*65};
 else if(e.source==Skill::MechanicalSiege)nodes={e.pos-axis*90,e.pos+axis*90};
 else if(e.source==Skill::BreachMines)nodes={e.pos-side*90,e.pos,e.pos+side*90};
 for(Vec2 at:nodes){
  const auto screen_at=screen(at);if(screen_at.x<-120||screen_at.y<-160||screen_at.x>ImGui::GetIO().DisplaySize.x+120||screen_at.y>ImGui::GetIO().DisplaySize.y+160)continue;
  const bool available=game_.skill_attack_available(e.source,at)&&!game_.smoke_at(game_.player.pos);
  const bool firing=available&&e.visual_attack_age<.48f&&length(e.visual_origin-at)<30;
  Vec2 aim=firing?e.visual_aim:e.end-at;
  const bool flip=aim.x>0;std::string action=firing?"attack":"idle";float time=firing?e.visual_attack_age+(octopus?.48f:0.f):e.age;
  // The original release frame is selected on a real shot. Recovery may be
  // interrupted by the next shot; original gameplay cadence is unchanged.
  if(octopus&&e.count==0&&e.age<e.delay){action="spawn";time=e.age;}
  if(!sprites_.count(prefix+"."+action))action="idle";
  d->AddEllipseFilled({screen_at.x,screen_at.y+2*zoom_},{18*zoom_,5*zoom_},IM_COL32(15,23,30,75));
  battle_sprite(d,prefix+"."+action,at,scale,time,opacity(available?0xffffffffu:IM_COL32(163,173,190,255),(.85f+.15f*friendly_effect_alpha_)),0,action=="idle",false,flip);
  // A short ally underline remains distinct from enemy sprites reused by the
  // explicitly documented mechanical substitutes.
  d->AddLine({screen_at.x-10*zoom_,screen_at.y+4*zoom_},{screen_at.x+10*zoom_,screen_at.y+4*zoom_},IM_COL32(148,222,200,150),2);
  if(!available){auto q=screen(at+Vec2{0,-66*scale});d->AddCircle(q,5*zoom_,IM_COL32(170,185,205,190),12,1.5f);d->AddLine({q.x-4*zoom_,q.y+4*zoom_},{q.x+4*zoom_,q.y-4*zoom_},IM_COL32(205,212,221,200),1.5f);}
 }
}

void App::draw_battle_field(ImDrawList* d,const DangerZone& z){
 const bool warning=z.age<z.warning;const float form=warning?.12f:std::clamp((z.age-z.warning)/.35f,.15f,1.f);
 // Never lose the visual identity while a suppressing field is still active.
 const float ending=.45f+.55f*std::clamp((z.duration-z.age)/.8f,0.f,1.f),strength=form*ending;
 const float t=game_.combat_elapsed,r=z.radius;const auto at=screen(z.pos);
 const bool lean=low_effects_||game_.enemies.size()>240;
 if(z.kind==DangerKind::Smoke){
  const int count=lean?7:11;
  for(int n=0;n<count;++n){float phase=n*2.39996f+z.id*.71f;float distance=n==0?0:r*(n<5?.34f:.65f);
   Vec2 p=z.pos+radial(distance,phase)+Vec2{std::sin(t*.36f+phase)*r*.035f,std::cos(t*.27f+phase)*r*.025f};
   const ImU32 tint=n%3==0?IM_COL32(52,72,87,255):n%3==1?IM_COL32(99,121,139,255):IM_COL32(168,185,199,255);
   battle_sprite(d,"battle.field.smoke"+std::to_string(n%3),p,r*(n<5?.90f:.66f),t+n*.31f,opacity(tint,strength*(lean?.76f:.69f)));
  }
 }else if(z.kind==DangerKind::Distortion){
  // The original Time Leap clock is a soft ground texture. Hands/ticks are
  // intentionally quiet: they indicate projectile time, never damage sectors.
  battle_sprite(d,"battle.field.clock",z.pos,r*1.7f,.72f,opacity(IM_COL32(154,183,204,255),.27f*strength),0,false);
  d->AddCircle(at,r*.84f*zoom_,opacity(IM_COL32(146,181,204,255),strength*.24f),64,1.2f);
  for(int n=0;n<60;++n){float a=n*pi/30-pi*.5f;const float inner=n%5==0?.75f:.80f;
   d->AddLine(radial(at,r*inner*zoom_,a),radial(at,r*.84f*zoom_,a),opacity(IM_COL32(179,201,219,255),strength*(n%5==0?.53f:.23f)),n%5==0?2.f:1.f);}
  auto hand=[&](float angle,float length,float width,float alpha){
   ImVec2 tip=radial(at,r*length*zoom_,angle),left=radial(at,width*zoom_,angle+pi*.5f),right=radial(at,width*zoom_,angle-pi*.5f),tail=radial(at,r*.08f*zoom_,angle+pi);
   d->AddQuadFilled(tip,left,tail,right,opacity(IM_COL32(177,201,222,255),strength*alpha));
   d->AddQuad(tip,left,tail,right,opacity(IM_COL32(205,222,231,255),strength*alpha*.8f),1);
  };
  if(!lean)hand(t*.12f-.12f,.69f,3,.075f);
  hand(t*.12f,.69f,3,.31f);hand(-1.1f+t*.037f,.43f,4.5f,.38f);
  d->AddCircleFilled(at,3*zoom_,opacity(IM_COL32(196,215,227,255),strength*.5f));
 }else if(z.kind==DangerKind::NaturalOrder){
  battle_sprite(d,"battle.field.nature",z.pos,r*1.45f,.56f,opacity(IM_COL32(156,180,118,255),strength*.30f),0,false);
  for(int n=0;n<(lean?4:7);++n){float a=n*2*pi/(lean?4:7)+.06f*t;Vec2 p=z.pos+radial(r*.65f,a);
   battle_sprite(d,"map.ellinia.leaf_patch",p,r*.42f,0,opacity(IM_COL32(164,180,129,255),strength*.43f),a, true);
  }
 }else if(z.kind==DangerKind::Regression){
  battle_sprite(d,"battle.field.seal",z.pos,r*1.5f,.33f,opacity(IM_COL32(171,145,188,255),strength*.27f),0,false);
  for(int n=0;n<(lean?3:5);++n){float phase=std::fmod(t*.24f+n*.2f,1.f),a=n*2.39996f-t*.09f;
   battle_sprite(d,"battle.field.seal",z.pos+radial(r*(.75f-.38f*phase),a),r*.33f,.33f,opacity(IM_COL32(166,146,187,255),strength*(1-phase)*.33f),-a,false);
  }
 }else if(z.kind==DangerKind::Explosion&&!warning){
  visual_sprite(d,"vfx.skills.explosion.hit",z.pos,r*2,(z.age-z.warning)/std::max(.05f,z.duration-z.warning),0xdfffffff);
 }
}

void App::draw_hostile_shot(ImDrawList* d,const Effect& e){
 const std::string actor=e.enemy_kind==EnemyKind::Boss?Game::boss_sprite_key(e.boss_profile):Game::enemy_visual_key(e.enemy_visual);
 auto art=enemy_art_.find(actor);const std::string key=art!=enemy_art_.end()?art->second.projectile:"battle.shot.arcane";
 const float angle=std::atan2(e.velocity.y,e.velocity.x)-forward_angles_[key];
 const auto p=screen(e.pos);const bool lean=low_effects_||game_.enemies.size()>240;
 // Actual sampled world positions keep trails inside local 50% speed fields.
 for(int n=0;n<e.visual_trail_count&&n<(lean?1:3);++n){battle_sprite(d,key,e.visual_trail[n],e.radius*2.4f,e.age,opacity(0xffffffffu,.17f-n*.04f),angle);}
 // Dark edge + open chevrons distinguish hostility without tinting source art red.
 d->AddCircle(p,(e.radius+2)*zoom_,IM_COL32(35,28,36,175),16,2.6f);
 battle_sprite(d,key,e.pos,e.radius*2.5f,e.age,0xffffffffu,angle);
 const float a=std::atan2(e.velocity.y,e.velocity.x);for(float side:{-1.f,1.f}){
  auto q=radial(p,(e.radius+4)*zoom_,a+side*pi*.5f);d->AddLine(q,radial(q,5*zoom_,a+pi*.8f*side),IM_COL32(255,187,135,230),1.4f);
 }
}

void App::draw_hostile_impacts(ImDrawList* d){
 for(const auto& e:game_.hostile_impacts){const auto p=screen(e.pos);const float fade=std::max(0.f,1-e.age/.55f);
  if(e.result==ImpactResult::Damage||e.result==ImpactResult::Wall){
   const std::string actor=e.enemy_kind==EnemyKind::Boss?Game::boss_sprite_key(e.boss_profile):Game::enemy_visual_key(e.enemy_visual);auto art=enemy_art_.find(actor);
   battle_sprite(d,art!=enemy_art_.end()?art->second.impact:"vfx.skills.gun.hit",e.pos,std::max(27.f,e.radius*3.2f),e.age,opacity(0xffffffffu,fade*(e.result==ImpactResult::Wall?.55f:.9f)),0,false);
  }else if(e.result==ImpactResult::Shield){d->PathArcTo(p,(e.radius+12)*zoom_,.4f,5.7f,20);d->PathStroke(opacity(IM_COL32(157,218,236,255),fade*.8f),0,2);}
  else if(e.result==ImpactResult::Immune){d->AddCircle(p,(e.radius+6)*zoom_,opacity(IM_COL32(176,194,202,255),fade*.35f),16,1);}
  else battle_sprite(d,"battle.field.smoke1",e.pos,e.radius*2.4f,e.age,opacity(IM_COL32(175,185,197,255),fade*.3f),0,false);
 }
}

void App::draw_hostile_release(ImDrawList* d,const Effect& e){
 if(e.evolved||!e.triggered||e.damage<=0)return;
 const float age=e.age-e.delay;if(age<0||age>.35f)return;
 const std::string actor=e.enemy_kind==EnemyKind::Boss?Game::boss_sprite_key(e.boss_profile):Game::enemy_visual_key(e.enemy_visual);auto art=enemy_art_.find(actor);
 const std::string key=art!=enemy_art_.end()?art->second.impact:"vfx.skills.explosion.hit";
 if(e.pattern==4){
  const float axis=std::atan2(e.end.y,e.end.x);const int rings=low_effects_?2:3;
  for(int row=0;row<rings;++row){float distance=e.radius*(.3f+.55f*row/std::max(1,rings-1));float diameter=std::min(e.radius*.28f,2*distance*std::sin(e.angle*.42f));
   for(int lane=-1;lane<=1;++lane){float angle=axis+lane*e.angle*.55f;Vec2 at=e.pos+Vec2{std::cos(angle),std::sin(angle)}*distance;battle_sprite(d,key,at,diameter,age,opacity(0xffffffffu,.65f*(1-age/.35f)),angle,false);}
  }
 }else if(e.pattern==1){
  // Texture patches follow the real finite capsule; no new beam collision.
  float distance=length(e.end-e.pos);int count=std::clamp(int(distance/std::max(18.f,e.radius*1.3f)),1,low_effects_?5:10);
  for(int n=0;n<count;++n)battle_sprite(d,key,e.pos+(e.end-e.pos)*((n+.5f)/count),e.radius*2,age,opacity(0xffffffffu,.56f*(1-age/.35f)),0,false);
 }else battle_sprite(d,key,e.pos,e.radius*1.8f,age,opacity(0xffffffffu,.65f*(1-age/.35f)),0,false);
}
}

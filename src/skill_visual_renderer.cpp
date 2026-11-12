#include "app.hpp"
#include "adventure_content.hpp"
#include "battle_art.hpp"
#include <cmath>

namespace maple {
void App::preview_visuals(int scene,bool legacy){
 preview_boss(int(BossProfile::GiantCrab));legacy_visuals_=legacy;fixed_visual_review=true;preview_static_=true;show_stats_=false;
 game_.skills.fill(0);game_.projectiles.clear();game_.effects.clear();game_.skill_visuals.clear();game_.events.clear();game_.scrolls={};game_.consumed_skills=0;game_.metrics={};
 game_.config.enable_pressure=false;game_.config.enable_director=false;game_.config.enable_scene_objects=false;game_.config.first_event_time=10000;game_.config.max_enemies=scene==12?500:12;
 game_.elapsed=720;game_.player.invulnerable=10000;game_.player.xp_next=1e9f;game_.timing.speed=1;
 std::vector<Skill> skills;
 switch(scene){
 case 0:skills={Skill::Homing,Skill::Frost,Skill::Lightning};break;
 case 1:skills={Skill::TripleStar,Skill::ShadowClone,Skill::Assassinate};break;
 case 2:skills={Skill::DualGuns,Skill::Grenade,Skill::Uppercut,Skill::EnergyBlast};break;
 case 3:skills={Skill::Missiles,Skill::Engineer,Skill::OctopusTurret};break;
 case 4:skills={Skill::DragonBreath,Skill::DragonConcert,Skill::GlacierSpear};break;
 case 5:skills={Skill::PhantomCards,Skill::RoseFinale};break;
 case 6:skills={Skill::ThousandStars};break;
 case 7:skills={Skill::ThunderShadow};break;
 case 8:skills={Skill::SeaFortress};break;
 case 9:skills={Skill::ToxicThunder};break;
 case 10:skills={Skill::ElementSanctum};break;
 case 11:skills={Skill::EternalMask};break;
 case 12:skills={Skill::SeaFortress,Skill::ToxicThunder,Skill::StarRain,Skill::DragonFistArmor,Skill::PhantomFinale,Skill::Grenade,Skill::TripleStar,Skill::GlacierSpear};break;
 default:game_.skills[int(Skill::TripleStar)]=game_.skills[int(Skill::ShadowClone)]=5;game_.auto_fuse_skills();game_.skills[int(Skill::ThousandStars)]=game_.skills[int(Skill::Lightning)]=5;game_.auto_fuse_skills();game_.skills[int(Skill::ThunderShadow)]=game_.skills[int(Skill::PoisonDart)]=5;game_.auto_fuse_skills();break;
 }
 for(Skill s:skills)game_.skills[int(s)]=3;
 Enemy boss=game_.enemies.front();boss.kind=EnemyKind::Boss;boss.pos={170,-25};boss.hp=boss.max_hp=10000000;boss.speed=0;boss.attack_cooldown=.4f;boss.windup=0;game_.enemies={boss};
 for(int i=1;i<(scene==12?500:10);++i){Enemy e;e.id=5000+i;e.visual=i%enemy_visual_count;e.pos={65.f+(i%9)*38.f,float(i/9)*28-120};if(scene!=12)e.pos={75.f+float(i%3)*72,float(i/3)*65-65};e.hp=e.max_hp=10000000;e.speed=0;e.radius=17;game_.enemies.push_back(e);}
 DangerZone smoke;smoke.kind=DangerKind::Smoke;smoke.pos={-145,95};smoke.radius=65;smoke.warning=0;smoke.duration=60;game_.add_danger_zone(smoke);
 prepare_chapter_resources();
}
namespace {
constexpr float pi=3.14159265f;
struct Palette {ImU32 main,light,shade;};
Palette palette(FxStyle s){
 static const Palette colors[]={
  {IM_COL32(163,221,242,255),IM_COL32(236,250,255,255),IM_COL32(55,92,134,255)},
  {IM_COL32(170,117,246,255),IM_COL32(226,201,255,255),IM_COL32(39,24,76,255)},
  {IM_COL32(126,222,108,255),IM_COL32(222,250,138,255),IM_COL32(42,79,71,255)},
  {IM_COL32(255,173,73,255),IM_COL32(255,236,169,255),IM_COL32(119,62,45,255)},
  {IM_COL32(76,218,218,255),IM_COL32(224,248,184,255),IM_COL32(44,73,91,255)},
  {IM_COL32(255,182,92,255),IM_COL32(255,242,201,255),IM_COL32(64,113,164,255)},
  {IM_COL32(88,205,215,255),IM_COL32(232,242,163,255),IM_COL32(46,81,144,255)},
  {IM_COL32(126,210,253,255),IM_COL32(220,253,255,255),IM_COL32(51,100,174,255)},
  {IM_COL32(143,164,255,255),IM_COL32(245,239,172,255),IM_COL32(68,69,140,255)},
  {IM_COL32(255,146,79,255),IM_COL32(255,231,159,255),IM_COL32(139,57,63,255)},
  {IM_COL32(235,212,139,255),IM_COL32(255,250,219,255),IM_COL32(122,107,169,255)},
  {IM_COL32(143,159,244,255),IM_COL32(217,239,255,255),IM_COL32(66,57,132,255)},
  {IM_COL32(203,156,245,255),IM_COL32(255,233,189,255),IM_COL32(86,47,103,255)},
  {IM_COL32(235,113,161,255),IM_COL32(255,224,209,255),IM_COL32(112,45,91,255)}
 };return colors[int(s)];
}
ImU32 alpha(ImU32 c,float a){return (c&0xffffffu)|(ImU32(std::clamp(a,0.f,1.f)*255)<<24);}
ImVec2 polar(ImVec2 p,float r,float a){return {p.x+std::cos(a)*r,p.y+std::sin(a)*r};}
void diamond(ImDrawList* d,ImVec2 p,float r,float a,ImU32 c){d->AddQuadFilled(polar(p,r,a),polar(p,r*.4f,a+pi*.5f),polar(p,r,a+pi),polar(p,r*.4f,a+pi*1.5f),c);}
void arc(ImDrawList* d,ImVec2 p,float r,float a,float b,ImU32 c,float width){d->PathArcTo(p,r,a,b,20);d->PathStroke(c,0,width);}
void brackets(ImDrawList* d,ImVec2 p,float r,ImU32 c){for(int x:{-1,1})for(int y:{-1,1}){ImVec2 q{p.x+x*r,p.y+y*r};d->AddLine(q,{q.x-x*r*.4f,q.y},c,1.5f);d->AddLine(q,{q.x,q.y-y*r*.4f},c,1.5f);}}
const char* cast_art(FxStyle s){switch(s){case FxStyle::Shadow:return "vfx.skills.shadow.cast";case FxStyle::Gun:return "vfx.skills.gun.cast";case FxStyle::Fist:return "vfx.skills.fist.cast";case FxStyle::Frost:return "vfx.skills.ice.cast";case FxStyle::Dragon:return "skill.dragon_thrust.cast";case FxStyle::Holy:return "skill.holy_light.cast";case FxStyle::Fire:return "skill.flame_dance.cast";case FxStyle::Steel:return "skill.spear_combo.cast";case FxStyle::Machine:return "vfx.skills.gun.cast";case FxStyle::Poison:return "skill.poison_mist.zone";case FxStyle::Thunder:return "skill.lightning";case FxStyle::Arcane:return "skill.rune";default:return nullptr;}}
const char* hit_art(FxStyle s){switch(s){case FxStyle::Shadow:return "vfx.skills.shadow.hit";case FxStyle::Gun:return "vfx.skills.gun.hit";case FxStyle::Fist:return "vfx.skills.fist.hit";case FxStyle::Frost:return "vfx.skills.ice.hit";case FxStyle::Fire:return "vfx.skills.explosion.hit";case FxStyle::Holy:return "skill.holy_dragon.hit";case FxStyle::Steel:return "vfx.skills.blade.hit";case FxStyle::Machine:return "vfx.skills.explosion.hit";case FxStyle::Poison:return "skill.poison_mist.zone";case FxStyle::Thunder:return "skill.lightning";case FxStyle::Dragon:return "skill.dragon_thrust.hit";case FxStyle::Card:case FxStyle::Rose:return "vfx.skills.blade.hit";case FxStyle::Arcane:return "skill.star_bow.hit";default:return nullptr;}}
}

// Fit original animation bounds once, then reuse its native origin and frame timings.
void App::visual_sprite(ImDrawList* d,const char* key,Vec2 pos,float extent,float phase,ImU32 tint,float rotation){
 if(!key)return;auto frames=sprites_.find(key);if(frames==sprites_.end())return;
 auto info=visual_sprite_metrics_.find(key);
 if(info==visual_sprite_metrics_.end()){
  float span=1,duration=0;for(const auto& f:frames->second){const auto& t=texture(f.file);span=std::max(span,float(std::max(t.w,t.h)));duration+=f.duration;}
  info=visual_sprite_metrics_.emplace(key,ImVec2{span,duration}).first;
 }
 sprite(d,key,screen(pos),extent*zoom_/info->second.x,std::clamp(phase,0.f,.999f)*info->second.y,false,tint,rotation,false);
}

void App::draw_skill_visuals(ImDrawList* d,bool foreground){
 if(legacy_visuals_)return;
 const auto viewport=ImGui::GetIO().DisplaySize;
 const bool lean=low_effects_||simulation_clock_.backlog_seconds()>.25||game_.enemies.size()>240||game_.skill_visuals.size()>96;
 int particles=lean?48:180,ghosts=lean?2:8;
 if(!foreground){
  int row=0;for(Skill skill:{Skill::EnergyBlast,Skill::ShadowBats,Skill::CoinBomb})if(game_.skills[int(skill)]>0){
   const int total=skill==Skill::EnergyBlast?8:skill==Skill::ShadowBats?4:6,filled=game_.skill_visual_charge(skill);
   const auto c=palette(skill==Skill::EnergyBlast?FxStyle::Machine:skill==Skill::ShadowBats?FxStyle::Shadow:FxStyle::Gun);
   auto at=screen(game_.player.pos);at.y+=(22+row++*7)*zoom_;
   for(int i=0;i<total;++i)diamond(d,{at.x+(i-(total-1)*.5f)*6*zoom_,at.y},2.5f*zoom_,pi*.5f,alpha(i<filled?c.light:c.main,friendly_effect_alpha_*(i<filled?.95f:.24f)));
  }
 }
 for(const auto& v:game_.skill_visuals){
  const bool top=v.stage==FxStage::Impact||v.stage==FxStage::End;
  if(top!=foreground)continue;
  auto p=screen(v.pos),q=screen(v.end);float r=v.radius*zoom_;
  const float pad=std::max(100.f,r);
  if(std::max(p.x,q.x)+pad<0||std::min(p.x,q.x)-pad>viewport.x||std::max(p.y,q.y)+pad<0||std::min(p.y,q.y)-pad>viewport.y)continue;
  const auto style=skill_visual_style(v.source,v.elements);auto c=palette(style);
  if(v.elements&Cold)c=palette(FxStyle::Frost);else if(v.elements&Fire)c=palette(FxStyle::Fire);else if(v.elements&Thunder)c=palette(FxStyle::Thunder);
  const float t=std::clamp(v.age/v.duration,0.f,1.f),fade=(1-t)*friendly_effect_alpha_,angle=std::atan2(q.y-p.y,q.x-p.x);
  const int tier=v.tier;const bool grand=tier>=3;
  auto sparks=[&](ImVec2 at,float radius,int count){
   for(int i=0;i<count&&particles>0;++i,--particles){float a=i*2.39996f+int(v.source)*.37f;ImVec2 x=polar(at,radius*(.25f+t*.7f),a);diamond(d,x,(2.8f-1.5f*t)*zoom_,a,alpha(i%2?c.main:c.light,fade));}
  };
  if(v.stage==FxStage::Cast){
   const float circle=(19+4*tier)*(1-.35f*t)*zoom_;
   for(int n=0;n<(tier==1?2:tier);++n)arc(d,p,circle,n*2*pi/std::max(2,tier)+t,n*2*pi/std::max(2,tier)+t+.85f,alpha(c.main,fade*.7f),1.5f);
   const bool shadow=style==FxStyle::Shadow||v.source==Skill::ShadowClone;
   if(shadow&&ghosts>0){--ghosts;auto ghost=screen(v.pos+Vec2{-26*(1-t),0});sprite(d,player_sprite(int(game_.character),"idle"),ghost,zoom_,v.age,game_.player.facing_left,alpha(c.main,fade*.45f));}
   if(style==FxStyle::Gun||style==FxStyle::Machine){ImVec2 muzzle=polar(p,22*zoom_,angle);diamond(d,muzzle,(22-12*t)*zoom_,angle,alpha(c.light,fade));brackets(d,muzzle,10*zoom_,alpha(c.main,fade));}
   else if(style==FxStyle::Card||style==FxStyle::Rose){for(int n=0;n<3;++n)diamond(d,polar(p,21*zoom_,n*2*pi/3-t),9*zoom_,n*pi/3,alpha(c.light,fade));}
   if(style==FxStyle::Card||style==FxStyle::Rose)for(int n=0;n<3;++n)battle_sprite(d,"battle.shot.card",v.pos+Vec2{std::cos(n*2*pi/3+t)*22,std::sin(n*2*pi/3+t)*22},18,v.age,alpha(0xffffffffu,fade),n*2*pi/3+t);
   const char* art=v.source==Skill::Assassinate?"vfx.skills.assassin.cast":v.source==Skill::ShadowClone?"vfx.skills.clone.cast":v.source==Skill::EnergyBlast?"vfx.skills.energy.cast":cast_art(style);
   if(!lean||tier>1)visual_sprite(d,art,v.pos,75.f+10.f*tier,t,alpha(0xffffffffu,fade*.8f));
   continue;
  }
  if(v.stage==FxStage::Fusion){
   for(int n=0;n<tier+2;++n){float a=n*2*pi/(tier+2)+t;ImVec2 start=polar(p,(60-30*t)*zoom_,a);diamond(d,start,6*zoom_,a,alpha(c.light,fade));d->AddLine(start,polar(p,24*zoom_,a+.18f),alpha(c.main,fade*.7f),2);}
   arc(d,p,(24+45*t)*zoom_,t,2*pi-.15f+t,alpha(c.light,fade),2.5f);
   continue;
  }
  if(v.stage==FxStage::Impact){
   const float size=std::min(39.f,14+v.radius*.3f+(v.heavy?8:0)+tier*2)*zoom_;
   if(v.age<.065f){diamond(d,p,size*(1-t),angle+.4f,alpha(c.light,fade));diamond(d,p,size*.65f,angle+pi*.5f,alpha(c.main,fade));}
   if(style==FxStyle::Thunder){for(int i=0;i<3;++i){float a=i*2*pi/3;auto mid=polar(p,size*.5f,a+.25f);d->AddLine(p,mid,alpha(c.light,fade),2);d->AddLine(mid,polar(p,size,a-.15f),alpha(c.main,fade),1.4f);}}
   else if(style==FxStyle::Machine)brackets(d,p,size*(.4f+t),alpha(c.main,fade));
   else if(style==FxStyle::Card||style==FxStyle::Rose){for(int n=0;n<3;++n)diamond(d,polar(p,size*t,n*2*pi/3),5*zoom_*(1-t),n+t*2,alpha(c.main,fade));}
   else if(style==FxStyle::Poison){for(int n=0;n<3;++n)d->AddCircle(polar(p,size*t,n*2*pi/3),4*zoom_*(1-t),alpha(c.main,fade),8,1.5f);}
   else arc(d,p,size*(.5f+t*.5f),angle-.8f,angle+2.4f,alpha(c.main,fade),2);
   if(!lean||v.heavy)visual_sprite(d,(v.source==Skill::OctopusTurret||v.source==Skill::DeepFleet)?"battle.octopus.hit":hit_art(style),v.pos,size/zoom_*2.1f,t,alpha(0xffffffffu,fade*.85f));
   sparks(p,size*1.2f,lean?2:4);continue;
  }
  if(v.stage==FxStage::End){
   if(battle_mount(v.source)!=BattleMount::None){const char* key=battle_mount(v.source)==BattleMount::Octopus?"battle.octopus.end":battle_mount(v.source)==BattleMount::Engineer?"battle.engineer.end":"battle.launcher.end";battle_sprite(d,key,v.pos,battle_mount(v.source)==BattleMount::Octopus?.9f:battle_mount(v.source)==BattleMount::Engineer?.85f:.6f,v.age,alpha(0xffffffffu,fade),0,false,false,v.end.x>v.pos.x);continue;}
   if(v.source==Skill::Summon||v.source==Skill::HolyDragon||v.source==Skill::FireBeast||v.source==Skill::SilverEagle||v.source==Skill::HeavenDragon){const char* key=v.source==Skill::Summon?"battle.phoenix.end":v.source==Skill::FireBeast?"skill.fire_beast.die":v.source==Skill::SilverEagle?"skill.silver_eagle.die":"skill.holy_dragon.die";battle_sprite(d,key,v.pos,v.source==Skill::Summon?.75f:v.source==Skill::FireBeast?.5f:v.source==Skill::SilverEagle?.65f:.55f,v.age,alpha(0xffffffffu,fade),0,false,false,v.end.x>v.pos.x);continue;}
   if(!grand&&lean)continue;float size=std::min(r,100*zoom_);
   for(int n=0;n<(grand?tier+2:3);++n){float a=n*2*pi/(grand?tier+2:3);auto x=polar(p,size*(1-t),a);diamond(d,x,(grand?5:3)*zoom_*(1-t),a+t,alpha(c.light,fade*.75f));}
   if(grand)arc(d,p,size*(.75f+.25f*t),.4f,5.8f,alpha(c.main,fade*.6f),2);
   continue;
  }
  // Only these three modules draw damage footprints, using the actual combat dimensions.
  if(v.stage==FxStage::Disk){
   d->AddCircle(p,r,alpha(c.main,fade*.78f),48,grand?2.5f:1.5f);
   const char* field=style==FxStyle::Fire?"battle.field.fire":style==FxStyle::Poison?"skill.poison_mist.zone":style==FxStyle::Frost?"skill.ice_arrow.hit":style==FxStyle::Holy?"battle.field.nature":style==FxStyle::Shadow?"battle.field.seal":style==FxStyle::Machine?"battle.field.clock":nullptr;
   if(field){int patches=lean?3:5;for(int n=0;n<patches;++n){float a=n*2.39996f;Vec2 at=v.pos+Vec2{std::cos(a),std::sin(a)}*(n==0?0:v.radius*.50f);battle_sprite(d,field,at,v.radius*.85f,v.age+n*.09f,alpha(0xffffffffu,fade*.42f));}}
   if(style==FxStyle::Card||style==FxStyle::Rose)for(int n=0;n<(lean?3:6);++n){float a=n*pi/3+t*.5f;battle_sprite(d,"battle.shot.card",v.pos+Vec2{std::cos(a),std::sin(a)}*(v.radius*(.35f+t*.35f)),22,v.age,alpha(0xffffffffu,fade*.8f),a);}

   arc(d,p,r*(.5f+.5f*t),angle+t,angle+5.3f+t,alpha(c.light,fade*.8f),grand?3.f:2.f);
   if(grand)for(int n=0;n<tier+1;++n){float a=n*2*pi/(tier+1);d->AddLine(polar(p,r*.72f,a),polar(p,r*.98f,a),alpha(c.light,fade*.8f),2);}
   if(style==FxStyle::Rose){for(int n=0;n<6;++n){auto a=polar(p,r*.15f,n*pi/3),b=polar(p,r*.7f,n*pi/3+.4f),c2=polar(p,r*.7f,n*pi/3-.4f);d->AddBezierCubic(a,b,c2,a,alpha(c.main,fade),3,12);}}
   else if(style==FxStyle::Frost){for(int n=0;n<6;++n){float a=n*pi/3;d->AddLine(p,polar(p,r*.7f,a),alpha(c.main,fade*.65f),2);diamond(d,polar(p,r*.65f,a),12*zoom_*(1-t),a,alpha(c.light,fade*.7f));}}
   else if(style==FxStyle::Machine){brackets(d,p,r*.55f,alpha(c.light,fade*.7f));}
   if(!lean)visual_sprite(d,style==FxStyle::Dragon?"skill.dragon_thrust.cast":hit_art(style),v.pos,std::min(v.radius*1.4f,160.f),t,alpha(style==FxStyle::Dragon?c.light:0xffffffffu,fade*.7f));
   sparks(p,r*.85f,grand?10:5);
  }else if(v.stage==FxStage::Line){
   const ImVec2 side{-std::sin(angle)*r,std::cos(angle)*r};
   d->AddQuadFilled({p.x+side.x,p.y+side.y},{q.x+side.x,q.y+side.y},{q.x-side.x,q.y-side.y},{p.x-side.x,p.y-side.y},alpha(c.shade,fade*.13f));
   for(float sign:{-1.f,1.f})d->AddLine({p.x+side.x*sign,p.y+side.y*sign},{q.x+side.x*sign,q.y+side.y*sign},alpha(c.main,fade*.8f),1.5f);
   const char* line_art=style==FxStyle::Frost?"skill.ice_arrow.hit":style==FxStyle::Thunder?"skill.lightning":style==FxStyle::Fist?"skill.earth_shatter.impact":style==FxStyle::Shadow?"vfx.skills.blade.cast":style==FxStyle::Dragon?"skill.dragon_thrust.cast":style==FxStyle::Holy?"skill.holy_light.cast":"vfx.skills.gun.hit";
   for(int n=0;n<(lean?2:4);++n){float f=(n+.5f)/(lean?2:4);battle_sprite(d,line_art,v.pos+(v.end-v.pos)*f,std::max(30.f,std::min(90.f,v.radius*3)),v.age,alpha(0xffffffffu,fade*.5f),angle+pi*.5f,false);}
   d->AddLine(p,q,alpha(c.light,fade*.55f),1.2f);
   if(style==FxStyle::Dragon){Vec2 at=v.pos+(v.end-v.pos)*std::min(1.f,t*1.5f);visual_sprite(d,"skill.dragon_thrust.cast",at,std::min(140.f,v.radius*3),t,alpha(0xffffffffu,fade),angle+pi*.5f);}
   if(grand&&!lean)for(int n=0;n<tier;++n){float f=(n+t)/tier;diamond(d,{p.x+(q.x-p.x)*f,p.y+(q.y-p.y)*f},7*zoom_,angle,alpha(c.light,fade));}
  }else if(v.stage==FxStage::Cone){
   d->PathLineTo(p);d->PathArcTo(p,r,angle-v.half_angle,angle+v.half_angle,24);d->PathFillConvex(alpha(c.shade,fade*.12f));
   arc(d,p,r,angle-v.half_angle,angle+v.half_angle,alpha(c.main,fade),grand?4.f:2.f);
   for(int n=0;n<(lean?1:tier+1);++n)arc(d,p,r*(.55f+.12f*n),angle-v.half_angle+t*.25f,angle+v.half_angle,alpha(c.light,fade*(.8f-.12f*n)),std::max(1.f,4.f-n));
   if(style!=FxStyle::Dragon&&style!=FxStyle::Fist)for(int n=0;n<(lean?2:3);++n){float a=angle+(n-1)*v.half_angle*.65f;battle_sprite(d,style==FxStyle::Frost?"vfx.skills.ice.cast":"vfx.skills.blade.cast",v.pos+Vec2{std::cos(a),std::sin(a)}*v.radius*.55f,v.radius*.7f,v.age,alpha(0xffffffffu,fade*.55f),a-pi,false);}
   if(style==FxStyle::Dragon)visual_sprite(d,"skill.dragon_thrust.cast",v.pos+(v.end-v.pos)*.55f,std::min(v.radius,190.f),t,alpha(0xffffffffu,fade*.8f),angle+pi*.5f);
   else if(style==FxStyle::Fist)visual_sprite(d,"vfx.skills.fist.cast",v.pos,110,t,alpha(0xffffffffu,fade));
  }
 }
}

void App::draw_adventure_loop(ImDrawList* d,const Effect& e){
 const auto style=skill_visual_style(e.source);auto c=palette(style);const auto roles=Game::skill_roles(e.source);
 // The caller applies friendly opacity once to the complete loop, including its sprites.
 const float alpha_base=1,deploy=std::clamp(e.age/.3f,0.f,1.f),end=std::clamp((e.duration-e.age)/.45f,0.f,1.f);
 Vec2 direction{std::cos(e.angle),std::sin(e.angle)},side{-direction.y,direction.x};
 const float flash=e.count?std::clamp(1-(e.secondary_tick-e.tick)/.22f,0.f,1.f):0;
 if(e.pattern==1){if(e.age<e.delay){auto q=screen(e.end);brackets(d,q,12*zoom_,alpha(c.main,alpha_base));if(e.source==Skill::ShadowClone)sprite(d,player_sprite(int(game_.character),"idle"),screen(e.pos+side*32),zoom_,e.age,false,alpha(c.main,alpha_base*.45f));}return;}
 if(e.pattern==2){if(e.age<e.delay){auto q=screen(e.end);brackets(d,q,14*zoom_,alpha(c.main,alpha_base));}return;}
 if((roles&Node)&&battle_mount(e.source)==BattleMount::None){
  battle_sprite(d,"battle.field.clock",e.pos,48,.72f,alpha(c.main,.22f*end),0,false);
 }
 if((roles&(Landing|Rose))&&e.age<e.delay){
  auto q=screen(e.end);brackets(d,q,(12+10*(1-deploy))*zoom_,alpha(c.main,alpha_base*.85f));
  if(e.source==Skill::Grenade){float f=std::clamp(e.age/e.delay,0.f,1.f);Vec2 at=e.pos+(e.end-e.pos)*f;at.y-=60*std::sin(f*pi);visual_sprite(d,"vfx.skills.grenade.travel",at,25,f,alpha(0xffffffffu,alpha_base));}
 }
 if(roles&Rose){auto q=screen(e.end);float r=14*zoom_*(.7f+.3f*std::sin(e.age*4));for(int n=0;n<5;++n)diamond(d,polar(q,r,n*2*pi/5),6*zoom_,n*2*pi/5,alpha(c.main,alpha_base*.45f*end));}
 if(e.source==Skill::Assassinate||e.source==Skill::Penetrator||e.source==Skill::ShipBreaker){if(e.age<e.delay){brackets(d,screen(e.end),16*zoom_,alpha(c.light,alpha_base));d->AddLine(screen(e.pos),screen(e.end),alpha(c.main,alpha_base*.35f),1);}}
}

bool App::draw_skill_projectile(ImDrawList* d,const Projectile& p){
 if(legacy_visuals_)return false;
 const auto style=skill_visual_style(p.skill);const auto c=palette(style);const int tier=Game::fusion_tier(p.skill);
 const float a=std::atan2(p.velocity.y,p.velocity.x),fade=friendly_effect_alpha_;auto at=screen(p.pos);
 const bool lean=low_effects_||game_.projectiles.size()>180;
 ImVec2 head=at;for(int i=0;i<p.visual_trail_count&&i<(lean?2:4);++i){auto tail=screen(p.visual_trail[i]);d->AddLine(head,tail,alpha(c.main,fade*(.65f-i*.13f)),std::max(1.f,(3.f-i*.55f)*zoom_));head=tail;}
 if(p.skill<Skill::TripleStar)return false;
 float r=std::clamp(p.radius*zoom_,5.f,16.f);
 std::string key="battle.shot.cannon";float size=p.radius*3.1f;
 if(p.skill==Skill::OctopusTurret||p.skill==Skill::DeepFleet){key="battle.octopus.ball";size=29;}
 else if(p.skill==Skill::ShadowBats||p.skill==Skill::GhostWings){key="battle.bat.fly";size=32;}
 else if(style==FxStyle::Machine){key="battle.shot.rocket";size=34;}
 else if(style==FxStyle::Shadow||style==FxStyle::Poison){key="skill.feather_storm.projectile";size=24;}
 else if(style==FxStyle::Dragon){key="skill.star_bow.projectile";size=32;}
 else if(style==FxStyle::Card){key="battle.shot.card";size=25;}
 const float rotation=key=="battle.bat.fly"?0:a-forward_angles_[key];
 battle_sprite(d,key,p.pos,size,p.age,alpha(0xffffffffu,fade),rotation,true,true,key=="battle.bat.fly"&&p.velocity.x>0);
 if(tier>=3&&!lean)arc(d,at,r*1.8f,a+pi*.5f,a+pi*1.5f,alpha(c.light,fade*.55f),1.3f);
 return true;
}
}

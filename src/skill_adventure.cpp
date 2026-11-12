#include "adventure_content.hpp"
#include "battle_art.hpp"
#include <cmath>
#include <limits>

namespace maple {
namespace {constexpr float pi=3.14159265f;constexpr size_t none=std::numeric_limits<size_t>::max();Vec2 turn(Vec2 v,float a){return {v.x*std::cos(a)-v.y*std::sin(a),v.x*std::sin(a)+v.y*std::cos(a)};}}
bool Game::cast_adventure(Skill skill){
 const int id=int(skill);const auto* def=adventure_skill(skill);if(!def)return false;
 if(skill==Skill::ShadowClone){shadow_ready_=1;return true;}
 if(skill==Skill::ShadowBats){if(bat_hits_<4)return false;bat_hits_=0;}
 if(skill==Skill::EnergyBlast){if(energy_hits_<8)return false;energy_hits_=0;}
 if(skill==Skill::CoinBomb&&combat_coins_==0)return false;
 const float range=skill==Skill::Shotgun?290.f:550.f;const auto target=skill_role_target(skill,player.pos,range);const DangerZone* altar=altar_target(skill,player.pos,range,target);if(target==none&&!altar)return false;
 Effect e;e.kind=EffectKind::Adventure;e.source=skill;e.pos=player.pos;e.end=altar?altar->pos:enemies[target].pos;e.target_id=altar||target==none?0:enemies[target].id;e.altar_target_id=altar?altar->id:0;e.proc_generated=skill==Skill::ShadowBats||skill==Skill::EnergyBlast;e.damage=skill_power(skill)*(fusion_tier(skill)>=3?1.25f:1.f);e.radius=90*area_multiplier;e.angle=std::atan2(e.end.y-e.pos.y,e.end.x-e.pos.x);e.energy=float(adventure_phase_[id]++%3);
 if(battle_mount(skill)!=BattleMount::None){const auto mount=battle_mount(skill);e.pos+=mount==BattleMount::Octopus?Vec2{-48,36}:mount==BattleMount::Engineer?Vec2{48,36}:Vec2{-14,-52};}
 e.count=0;e.pattern=0;int beats=3;e.delay=.12f;
 switch(skill){
 case Skill::Shotgun:case Skill::PoisonDart:case Skill::EnergyBlast:case Skill::CoinBomb:case Skill::ShadowBats:beats=1;break;
 case Skill::ShadowBoomerang:case Skill::ToxicDance:beats=2;break;
 case Skill::Assassinate:case Skill::Penetrator:case Skill::ShipBreaker:case Skill::Grenade:beats=1;e.delay=.65f;break;
 case Skill::SixBlade:case Skill::DualGuns:case Skill::AsuraBlades:beats=6;break;
 case Skill::OctopusTurret:case Skill::Engineer:case Skill::DeepFleet:case Skill::GravityFoundry:beats=8;break;
 case Skill::ThousandStars:case Skill::MirrorGuns:case Skill::ThunderShadow:beats=6;break;
 case Skill::FrostCombo:case Skill::SeaSerpent:case Skill::SkySwarm:case Skill::GunFeast:case Skill::BladeFist:case Skill::CardOrbit:case Skill::DragonOrrery:beats=4;break;
 default:if(fusion_tier(skill)==4)beats=4;break;
 }
 if(skill==Skill::CoinBomb||skill==Skill::CoinPrison||skill==Skill::GoldenFlame){e.energy=float(combat_coins_);combat_coins_=0;}
 e.secondary_tick=std::max(.08f,(skill_interval(skill)*.82f-e.delay)/beats);e.duration=e.delay+beats*e.secondary_tick+.15f;e.tick=e.delay;e.velocity={float(beats),0};
 // One authored sequence/node per skill. Recasting replaces its previous deployment.
 effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect& old){return old.source==skill&&old.kind==EffectKind::Adventure&&old.pattern==0;}),effects.end());add_effect(e);return true;
}
void Game::on_adventure_hit(size_t target,float actual,Skill source){
 if(target>=enemies.size()||actual<=0||current_attack_id_==adventure_root_)return;
 adventure_root_=current_attack_id_;
 if(source!=Skill::CoinBomb&&source!=Skill::CoinPrison&&source!=Skill::GoldenFlame&&(skill_attack_available(Skill::CoinBomb,player.pos)||skill_attack_available(Skill::CoinPrison,player.pos)||skill_attack_available(Skill::GoldenFlame,player.pos)))combat_coins_=std::min(6,combat_coins_+1);
 if(skill_attack_available(Skill::EnergyBlast,player.pos))energy_hits_=std::min(8,energy_hits_+1);
 if(skill_attack_available(Skill::ShadowBats,player.pos))bat_hits_=std::min(4,bat_hits_+1);
 const uint32_t roles=skill_roles(source);const bool copyable=source!=Skill::ShadowClone&&source!=Skill::ShadowBats&&(roles&(Bolt|Blade|Fist))&&!(roles&(Companion|Turret));
 if(shadow_ready_>0&&copyable&&skill_attack_available(Skill::ShadowClone,player.pos)){
  shadow_ready_=0;Effect echo;echo.kind=EffectKind::Adventure;echo.source=Skill::ShadowClone;echo.pattern=1;echo.target_id=enemies[target].id;echo.pos=player.pos;echo.end=enemies[target].pos;echo.damage=actual*.4f;echo.delay=.25f;echo.duration=.6f;echo.proc_generated=true;echo.root_attack_id=current_attack_id_;pending_resonance_effects_.push_back(echo);
 }
}
void Game::adventure_dash(Vec2 from,Vec2 to,bool ended){
 if(!adventure_dashing_){adventure_dash_start_=from;adventure_dashing_=true;}if(!ended)return;adventure_dashing_=false;
 if(gear_mechanisms_[8]>0&&gear_cooldowns_[8]<=0){gear_cooldowns_[8]=2;gear_shield_seconds_=std::max(gear_shield_seconds_,.6f);skill_area(to,70,20*gear_mechanisms_[8],Skill::Count,0,false);}
 for(Skill skill:{Skill::SmokeAssault,Skill::NightChase,Skill::FlashRaid,Skill::ShadowBastion})if(skills[int(skill)]&&cooldown_[int(skill)]<=0&&try_skill_cast(skill)){
  current_attack_id_=next_attack_id_++;emit_skill_visual(FxStage::Cast,skill,from,to,24);const float power=skill_power(skill);skill_line(adventure_dash_start_,to,26*area_multiplier,power*1.7f,skill,.2f);
  Effect e;e.kind=EffectKind::Adventure;e.source=skill;e.pos=to;e.radius=75*area_multiplier;e.damage=power;e.delay=.35f;e.tick=e.delay;e.duration=1;e.velocity={1,0};e.secondary_tick=.3f;add_effect(e);
  cooldown_[int(skill)]=skill_interval(skill);cooldown_period_[int(skill)]=cooldown_[int(skill)];
 }
}
void Game::update_adventure_projectile(Projectile& p,float dt){
 if(p.skill<Skill::TripleStar)return;
 const bool returning=p.skill==Skill::ShadowBoomerang||p.skill==Skill::ToxicDance;
 if(returning&&p.age>.55f){if(!p.returned){p.returned=true;p.hit_ids.clear();}p.velocity=normalized(player.pos-p.pos)*440;if(length(player.pos-p.pos)<18)p.lifetime=p.age;}
 if(p.skill==Skill::Missiles||p.skill==Skill::GuidedSalvo||p.skill==Skill::SkySwarm||p.skill==Skill::ShadowBats||p.skill==Skill::GhostWings||p.skill==Skill::SeaFortress){
  size_t target=target_by_id(p.target,p.pos,450);
  if(target==none){target=skill_role_target(p.skill,p.pos,450);p.target=target==none?0:enemies[target].id;}
  const DangerZone* altar=altar_target(p.skill,p.pos,450,target,p.altar_target_id);p.altar_target_id=altar?altar->id:0;
  if(altar){p.target=0;p.velocity=normalized(altar->pos-p.pos)*390;}else if(target!=none)p.velocity=normalized(enemies[target].pos-p.pos)*390;
 }
}
void Game::on_adventure_projectile_hit(Projectile& p,size_t target){
 if(p.skill<Skill::TripleStar||target>=enemies.size())return;auto& e=enemies[target];uint32_t roles=skill_roles(p.skill);
 if(roles&Poison)apply_poison(target,p.damage*.18f,4,p.skill);
 if(roles&Cold)apply_frost(e,2.4f,p.skill);
 if(roles&Fire)apply_burn(e,p.damage*.2f,2,p.skill);
 if(roles&Mark){e.mark_remaining=3;e.mark_source=p.skill;}
 if(p.skill==Skill::ShadowBoomerang){if(!p.returned){e.mark_remaining=2;e.mark_source=p.skill;}else if(e.mark_source==p.skill&&e.mark_remaining>0){hurt_enemy(target,p.damage*.65f,{},0,p.skill,false,false);e.mark_remaining=0;}}
 if(p.skill==Skill::ToxicDance&&p.returned&&e.poison_layers>=2){e.poison_layers-=2;hurt_enemy(target,p.damage*1.1f,{},0,p.skill,false,false);}
 if(p.skill==Skill::ShipBreaker){e.armor_break_remaining=3;e.armor_source=p.skill;}
}
bool Game::update_adventure_effect(Effect& e,float dt){
 if(e.kind!=EffectKind::Adventure)return false;
 if(e.resonance_id==-2&&equipment_suppressed())return true;
 if(e.pattern==2){if(!e.triggered&&e.age>=e.delay){e.triggered=true;skill_area(e.end,e.radius,e.damage,e.source,0,false);}return true;}
 if(e.pattern==1){if(!e.triggered&&e.age>=e.delay){e.triggered=true;for(size_t i=0;i<enemies.size();++i)if(enemies[i].id==e.target_id&&enemies[i].hp>0&&length(enemies[i].pos-player.pos)<550){e.end=enemies[i].pos;hurt_enemy(i,e.damage,{},0,e.source,false,false);break;}}return true;}
 e.tick-=dt;if(e.tick>0||e.count>=int(e.velocity.x))return true;e.tick+=e.secondary_tick;e.triggered=true;
 const int beat=e.count++,last=int(e.velocity.x)-1;const Skill skill=e.source;const float d=e.damage;Vec2 axis{std::cos(e.angle),std::sin(e.angle)},side{-axis.y,axis.x};
 const float range=skill==Skill::Shotgun?290.f:600.f;size_t target=skill_role_target(skill,e.pos,range,e.target_id);const DangerZone* altar=altar_target(skill,e.pos,range,target,e.altar_target_id);e.altar_target_id=altar?altar->id:0;
 if(target==none&&!altar&&battle_mount(skill)!=BattleMount::None){e.target_id=0;return true;}Vec2 point=altar?altar->pos:target==none?e.end:enemies[target].pos;uint32_t target_id=altar||target==none?0:enemies[target].id;e.target_id=target_id;
 auto status=[&](Vec2 at,float radius,uint32_t roles){for(auto it=skill_visuals.rbegin();it!=skill_visuals.rend();++it)if(it->source==skill&&it->age==0&&(it->stage==FxStage::Disk||it->stage==FxStage::Line||it->stage==FxStage::Cone)){it->elements=roles;break;}for(size_t i:nearby(at,radius+65))if(enemies[i].hp>0&&length(enemies[i].pos-at)<radius+enemies[i].radius){auto& enemy=enemies[i];if(roles&Cold)apply_frost(enemy,2,skill);if(roles&Fire)apply_burn(enemy,d*.16f,2,skill);if(roles&Poison)apply_poison(i,d*.14f,3,skill);if(roles&Mark){enemy.mark_remaining=3;enemy.mark_source=skill;}if((roles&Shadow)&&!control_immune(enemy)){enemy.slow_remaining=1.1f;enemy.slow_factor=.45f;}}};
 auto area=[&](Vec2 at,float radius,float scale,uint32_t roles=0){skill_area(at,radius*area_multiplier,d*scale,skill,.2f,!e.proc_generated);status(at,radius*area_multiplier,roles);e.radius=radius*area_multiplier;};
 auto line=[&](Vec2 from,Vec2 to,float width,float scale){skill_line(from,to,width*area_multiplier,d*scale,skill,.25f,!e.proc_generated);};
 auto fan=[&](Vec2 from,Vec2 toward,int count,float spread,float scale,float speed=430.f,int pierce=2){if(projectiles.size()>=config.max_projectiles)return;e.visual_origin=from;e.visual_aim=normalized(toward-from);e.visual_attack_age=0;from=battle_muzzle(skill,from,e.visual_aim);const Vec2 direction=normalized(toward-from);for(int n=0;n<count;++n){Projectile p;p.skill=skill;p.pos=from;p.velocity=turn(direction,(n-(count-1)*.5f)*spread)*speed;p.damage=d*scale;p.radius=6;p.pierce=pierce;p.target=target_id;p.altar_target_id=target_id?0:e.altar_target_id;p.lifetime=skill==Skill::Shotgun? .65f:1.7f;p.proc_generated=e.proc_generated;add_projectile(p);}};
 auto cone=[&](float angle,float radius,float scale){skill_cone(player.pos,turn(axis,angle),radius*area_multiplier,.72f,d*scale,skill,.3f);e.end=player.pos+turn(axis,angle)*radius;e.radius=radius;};
 auto allocated=[&](Vec2 from,int count,float scale){uint32_t omit=0;for(int n=0;n<count;++n){if(n==0&&altar){uint32_t old=target_id;target_id=0;fan(from,altar->pos,1,0,scale,340,1);target_id=old;continue;}size_t t=priority_enemy(from,600,omit);if(t==none)break;omit=enemies[t].id;uint32_t old=target_id;target_id=omit;fan(from,enemies[t].pos,1,0,scale,340,1);target_id=old;}};
 auto poison_burst=[&](Vec2 at,float radius){emit_skill_visual(FxStage::Disk,skill,at,at,radius,0,true,Poison);int count=0;for(size_t i:nearby(at,radius+60))if(enemies[i].hp>0&&length(enemies[i].pos-at)<radius&&enemies[i].poison_layers>0&&count++<6){int stacks=std::min(3,enemies[i].poison_layers);enemies[i].poison_layers-=stacks;hurt_enemy(i,d*(.6f+.25f*stacks),{},0,skill,false,false);}};
 switch(skill){
 case Skill::TripleStar:fan(player.pos,point,1,0,.95f);break;
 case Skill::ShadowBoomerang:fan(player.pos,point,1,0,1.2f,420,6);break;
 case Skill::Assassinate:line(e.pos,e.end,24,5.f);break;
 case Skill::SixBlade:cone(beat==5?pi:(beat%2?-.75f:.75f),beat==5?145:110,beat==5?1.7f:.65f);break;
 case Skill::CoinBomb:area(e.end,80,1.3f+e.energy*.35f);break;
 case Skill::PoisonDart:fan(player.pos,point,2,.08f,.8f);break;
 case Skill::ShadowStitch:area(e.end,95,.6f,Mark|Shadow);break;
 case Skill::ShadowBats:allocated(player.pos,2,1.3f);break;
 case Skill::DualGuns:{size_t t=beat%2?priority_enemy(player.pos,420,target_id):target;fan(player.pos,t==none?point:enemies[t].pos,1,0,.7f,540);break;}
 case Skill::Shotgun:fan(player.pos,point,5,.19f,.65f,450,1);break;
 case Skill::Penetrator:fan(e.pos,e.end,1,0,4.2f,600,12);break;
 case Skill::Grenade:area(e.end,95,3.2f,Fire);break;
 case Skill::EnergyBlast:area(player.pos,155,3.4f);break;
 case Skill::Uppercut:if(beat<2)cone((beat%2-.5f)*.5f,105,.85f);else line(player.pos,player.pos+axis*250,38,1.6f);break;
 case Skill::Shockwave:area(e.pos+axis*(75+beat*75.f),62,1.3f);break;
 case Skill::SeaSerpent:line(e.pos+axis*(beat==3?330:beat*90.f),e.pos+axis*(beat==3?0:(beat+1)*110.f),28,beat==3?1.3f:1.f);break;
 case Skill::Cannon:area(e.end+side*((beat-1)*65.f),82,2.4f);break;
 case Skill::OctopusTurret:fan(e.pos,point,1,0,.75f,470);break;
 case Skill::FrostCombo:cone((beat%2?-.35f:.35f),beat==3?175:115,beat==3?2.2f:.65f);status(player.pos,150,Cold);break;
 case Skill::GlacierSpear:if(beat==0)line(e.pos,e.end+axis*150,30,2.3f);else area(e.pos+axis*(beat*125.f),62,1.f,Cold);break;
 case Skill::DragonBreath:cone((beat-1)*.6f,270,1.1f);status(player.pos+turn(axis,(beat-1)*.6f)*170,70,Fire);break;
 case Skill::DragonConcert:area(e.end+side*((beat-1)*65.f),75,1.2f,int(e.energy)==0?Cold:int(e.energy)==1?Fire:Thunder);break;
 case Skill::Missiles:allocated(e.pos,beat==0?2:1,.85f);break;
 case Skill::Engineer:fan(e.pos,e.pos+turn(axis,beat*pi*.5f)*250,1,0,.8f,350,4);break;
 case Skill::PhantomCards:if(int(e.energy)==0)fan(player.pos,point,3,.22f,.6f);else if(int(e.energy)==1)fan(player.pos,e.end,1,0,1.f,560,6);else fan(e.end,player.pos,2,.2f,.65f,320,3);break;
 case Skill::RoseFinale:area(e.end+(beat?side*(beat==1?75.f:-75.f):Vec2{}),beat?65:80,beat?1.2f:2.2f);break;
 case Skill::ThousandStars:emit_skill_visual(FxStage::Cast,skill,e.pos+side*(beat%2?70.f:0),point,22);fan(e.pos+side*(beat%2?70.f:0),point,beat%2?2:1,.08f,beat%2?.65f:1.2f);break;
 case Skill::ToxicDance:fan(e.pos+side*(beat?35.f:-35.f),point,2,.16f,1.1f,390,7);break;
 case Skill::SmokeAssault:area(e.pos,55,1.f,Shadow);break;
 case Skill::NightChase:line(e.pos,point,24,3.f);status(point,55,Mark);break;
 case Skill::FlashRaid:line(e.pos-axis*90,e.pos+axis*90,40,2.f);break;
 case Skill::ShadowBastion:shield_remaining=std::max(shield_remaining,1.2f);area(e.pos,100,1.2f,Shadow);break;
 case Skill::CoinPrison:area(e.end,beat==0?100:65,.8f+e.energy*.24f,Mark|Shadow);break;
 case Skill::AsuraBlades:if(beat<4)cone(beat*pi*.5f,125,.9f);else{fan(player.pos,player.pos+turn(axis,beat*pi)*250,5,.6f,.55f,320,5);}break;
 case Skill::TwilightHunt:if(beat<2)allocated(e.pos,2,1.1f);else{area(e.end,95,1.4f);if(holy_support_cooldown_<=0){heal_player(2);holy_support_cooldown_=4;}}break;
 case Skill::GunFeast:fan(player.pos,point,beat==3?5:1,beat==3?.24f:0,beat==3?.8f:1.4f);break;
 case Skill::ShipBreaker:fan(e.pos,e.end,1,0,5.8f,650,15);break;
 case Skill::BurningSea:if(beat<2)area(e.end+side*(beat?80.f:-80.f),90,1.3f,Fire);else line(e.end-side*90,e.end+side*90,55,2.3f);break;
 case Skill::SoulUppercut:cone(0,100+beat*55.f,1.1f+beat*.5f);break;
 case Skill::ThunderSerpent:area(e.pos+axis*(80+beat*120.f),75,1.8f,Thunder);break;
 case Skill::DeepFleet:if(beat%3<2)fan(e.pos,point,1,0,.7f);else area(e.end,105,2.1f);break;
 case Skill::ThunderStars:if(beat<2){fan(e.pos,point,2,.11f,.9f);status(point,60,Mark);}else{line(e.pos,point,24,1.7f);area(point,70,.7f);}break;
 case Skill::PoisonRain:area(e.end,beat==0?45:110,beat==0?.6f:1.3f,Poison);break;
 case Skill::MirrorGuns:fan(e.pos+side*(beat%2?65.f:-65.f),beat>=4?e.end:point,1,0,beat>=4?1.6f:.8f,540);break;
 case Skill::StarPrison:area(e.end,beat==0?100:65,beat==0?.4f:2.3f,Shadow|Mark);break;
 case Skill::GoldenFlame:area(e.end+axis*(beat-1)*80.f,65,1.1f+e.energy*.16f,Fire);break;
 case Skill::GhostWings:if(beat==0)status(point,60,Mark);else{allocated(e.pos+side*(beat==1?-80.f:80.f),3,1.1f);}break;
 case Skill::BladeFist:if(beat<3)cone(beat%2?.6f:-.6f,130,1.f);else line(player.pos,player.pos+axis*280,45,2.6f);break;
 case Skill::IceSeaSpear:if(beat<2)area(e.pos+axis*(100+beat*150.f),75,1.3f,Cold);else line(e.pos+axis*380,e.pos,40,3.f);break;
 case Skill::ElementDragon:cone((beat-1)*.65f,300,1.7f);status(e.pos+turn(axis,(beat-1)*.65f)*180,95,beat==0?Cold:beat==1?Fire:Thunder);break;
 case Skill::SkySwarm:allocated(e.pos+side*(beat%2?65.f:-65.f),beat<2?3:2,beat<2?.8f:1.1f);break;
 case Skill::PhantomFinale:if(beat<2)fan(e.pos,point,3,.2f,.7f);else area(e.end+side*((int(e.energy)-1)*60.f),95,3.2f);break;
 case Skill::MarkedConstellation:status(point,60,Mark);fan(e.pos+side*(beat-1)*70.f,point,2,.08f,1.2f);break;
 case Skill::GuidedSalvo:allocated(e.pos,beat==last?1:2,beat==last?2.2f:1.1f);break;
 case Skill::BreachMines:area(e.pos+side*(beat-1)*90.f,70,1.5f);fan(e.pos+side*(beat-1)*90.f,point,3,.28f,.55f);break;
 case Skill::SnareExecution:if(beat==0)area(e.end,110,.5f,Shadow|Mark);else fan(e.end+side*(beat==1?130.f:-130.f),e.end,4,.12f,1.f);break;
 case Skill::CardOrbit:fan(e.pos+turn(axis,beat*pi*.5f)*80,beat==3?e.pos:point,2,.16f,1.2f);break;
 case Skill::ElementOrrery:{Vec2 at=e.pos+turn(axis,beat*pi*2/3)*120;area(at,95,1.5f,beat==0?Cold:beat==1?Fire:Thunder);break;}
 case Skill::GravityBreaker:if(beat<2){Effect pull=e;pull.pos=e.end;pull.radius=150;pull_enemies(pull,.4f,90);area(e.end,110,.6f);}else line(e.pos,e.end+axis*100,55,3.5f);break;
 case Skill::GravityFoundry:{Effect pull=e;pull.radius=160;pull_enemies(pull,.35f,70);if(beat==last)area(e.pos,170,2.2f);else fan(e.pos,e.pos+turn(axis,beat*pi*.5f)*250,2,.12f,.7f);break;}
 case Skill::TitanCombo:if(beat==0)cone(0,110,.9f);else if(beat==last)cone(.3f,210,4.f);break;
 case Skill::GlacierHammer:if(beat==0)line(e.pos,e.end,35,2.1f);else area(e.end,beat==last?135:75,beat==last?2.8f:.7f,Cold);break;
 case Skill::EnergyBastion:if(beat==0)shield_remaining=std::max(shield_remaining,1.4f);else line(e.pos,e.pos+axis*(beat*160.f),30,2.1f);break;
 case Skill::ThunderShadow:if(beat<4){fan(e.pos+side*(beat%2?95.f:-95.f),point,2,.12f,.85f);status(point,70,Mark);}else{line(e.pos+side*(beat==4?95.f:-95.f),point,25,2.f);}break;
 case Skill::BreachFireweb:if(beat==0)fan(e.pos,point,3,.06f,1.2f);else if(beat==1)fan(player.pos,point,6,.2f,.8f);else area(e.end+axis*80,120,3.1f,Fire);break;
 case Skill::MechanicalSiege:if(beat%2==0)allocated(e.pos+axis*90,4,1.1f);else fan(e.pos-axis*90,point,5,.12f,1.1f);break;
 case Skill::ColdPrison:{Effect pull=e;pull.pos=e.end;pull.radius=165;pull_enemies(pull,.55f,80);area(e.end,beat%2?65:155,beat%2?2.4f:1.1f,Cold);if(beat==last)line(e.end-side*160,e.end+side*160,25,1.4f);break;}
 case Skill::DragonOath:if(beat==0){shield_remaining=std::max(shield_remaining,1.5f);fan(e.pos,point,2,.15f,1.3f);}else if(beat==1)cone(0,230,2.5f);else area(player.pos,150,2.f);break;
 case Skill::FrostFlame:if(beat<2)area(e.end+side*(beat==0?-100.f:100.f),100,1.8f,beat==0?Cold:Fire);else line(e.end-axis*160,e.end+axis*160,50,3.2f);break;
 case Skill::HolyPhoenix:if(beat<2)line(e.pos+side*(beat==0?-100.f:100.f),e.end+side*(beat==0?-80.f:80.f),45,2.f);else{line(e.pos-side*100,e.end+side*100,45,1.8f);line(e.pos+side*100,e.end-side*100,45,1.8f);}break;
 case Skill::NightBanquet:if(beat==0)fan(e.pos,e.end,3,.24f,1.f);else if(beat==1)area(e.end,125,2.8f);else if(target!=none&&length(point-e.end)<150)line(e.pos,point,24,4.f);break;
 case Skill::VenomLock:if(beat==0)area(e.end,140,1.f,Poison|Shadow);else if(beat==1){fan(e.end-side*130,e.end,2,.1f,1.4f);fan(e.end+side*130,e.end,2,.1f,1.4f);}else poison_burst(e.end,155);break;
 case Skill::TidalFist:if(beat<2)area(e.pos+axis*(100+beat*110.f),90,2.f);else{Effect pull=e;pull.radius=210;pull_enemies(pull,.4f,90);area(player.pos,145,2.8f);}break;
 case Skill::PolarDragon:if(beat<2)area(e.pos+axis*(100+beat*130.f),95,2.f,Cold);else line(e.pos+axis*440,e.pos,45,3.6f);break;
 case Skill::DragonOrrery:if(beat<3){Vec2 at=e.pos+turn(axis,beat*pi*2/3)*130;line(at,e.end,45,2.1f);status(e.end,80,beat==0?Cold:beat==1?Fire:Thunder);}else fan(e.end,e.pos,6,.35f,1.2f);break;
 case Skill::ToxicThunder:if(beat<2){for(int n=0;n<2;++n)fan(e.pos+side*(n?110.f:-110.f)+axis*(beat*100.f),point,2,.1f,1.f);status(point,90,Poison|Mark);}else if(beat==2){line(e.pos-side*140,e.end+side*140,35,3.f);line(e.pos+side*140,e.end-side*140,35,3.f);}else poison_burst(e.end,180);break;
 case Skill::PirateAssault:if(beat==0)fan(e.pos,point,4,.06f,1.3f);else if(beat==1)fan(player.pos,point,7,.2f,1.f);else if(beat==2){area(e.end-side*110,90,2.f,Fire);area(e.end+side*110,90,2.f,Fire);}else area(e.end,130,4.2f);break;
 case Skill::SeaFortress:if(beat==0)status(e.end,180,Mark);else if(beat<3)allocated(e.pos+side*(beat==1?-130.f:130.f),5,1.4f);else{area(e.end,150,4.3f);fan(e.pos+axis*100,point,4,.18f,1.f);}break;
 case Skill::StarfallPrison:if(beat<2){Effect pull=e;pull.pos=e.end;pull.radius=180;pull_enemies(pull,.6f,95);area(e.end,beat?75:170,beat?2.8f:1.5f,Cold);}else if(beat==2){status(e.end,175,Cold);e.radius=175;}else area(e.end,135,6.f,Fire);break;
 case Skill::DragonFistArmor:if(beat==0){shield_remaining=std::max(shield_remaining,1.8f);fan(e.pos,point,2,.12f,1.4f);}else if(beat==1)area(player.pos,120,2.2f);else if(beat==2)cone(0,260,3.f);else line(player.pos,player.pos+axis*370,48,4.5f);break;
 case Skill::StarRain:if(beat<2)area(e.end+side*(beat?-110.f:110.f),115,2.1f,beat?Fire:Cold);else if(beat==2){area(e.end-side*95,85,2.6f);area(e.end+side*95,85,2.6f);}else area(e.end,115,4.2f,Fire);break;
 case Skill::ElementSanctum:if(beat<3){line(e.pos+side*(beat%2?110.f:-110.f),e.end,55,2.8f);status(e.end,120,beat==0?Cold:beat==1?Fire:Thunder);}else{area(e.end,150,3.8f);if(holy_support_cooldown_<=0){heal_player(3);holy_support_cooldown_=5;}}break;
 case Skill::EternalMask:if(beat==0)fan(e.pos,e.end,3,.3f,1.5f);else if(beat==1)for(int n=-1;n<2;++n)area(e.end+side*(n*100.f),90,2.5f);else if(beat==2)allocated(e.end,3,1.7f);else if(target!=none)line(e.end,point,24,4.8f);break;
 case Skill::WinterMarch:if(beat<2){line(e.pos+side*(beat?55.f:-55.f),e.pos+side*(beat?55.f:-55.f)+axis*300,28,1.6f);status(e.pos+axis*180,90,Cold);}else area(e.pos+axis*310,95,2.5f,Cold);break;
 case Skill::TideBreaker:if(beat==0){line(e.pos,e.end,35,2.f);if(target!=none){enemies[target].armor_break_remaining=3;enemies[target].armor_source=skill;}}else line(e.end+side*(beat==1?-150.f:150.f),e.end+side*(beat==1?150.f:-150.f),35,1.8f);break;
 case Skill::ArcaneBarrage:if(beat==0)allocated(e.pos,4,.85f);else fan(e.pos,point,1,0,1.7f,350,3);break;
 case Skill::CardVolley:if(beat<2)fan(e.pos+side*(beat?50.f:0),point,beat?2:1,.15f,beat?1.f:1.6f);else fan(e.pos,e.end,4,.09f,.85f);break;
 case Skill::RiftCannon:if(beat==0)line(e.pos,e.end,60,.8f);else if(beat==1)fan(e.pos,e.end,1,0,3.8f,600,12);else area(e.end,110,2.f);break;
 default:break;
 }
 if(beat==last)emit_skill_visual(FxStage::End,skill,e.end,e.pos,e.radius,0,fusion_tier(skill)>=3);
 return true;
}
}

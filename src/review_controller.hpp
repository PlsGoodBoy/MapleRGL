#pragma once
#include "game.hpp"
#include "boss_combat.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
// Opt-in diagnostic input, shared by native review and fixed-seed simulations.
// This observes live entities and makes legal inputs; it never edits game state.
struct ReviewController {
 Vec2 move{1,0};float think=0;
 static float build_value(Skill skill){
  switch(skill){
  case Skill::Homing:case Skill::Rune:case Skill::Lightning:case Skill::Frost:return 95;
  case Skill::HolyLight:case Skill::Bulwark:case Skill::Zone:case Skill::Meteor:return 80;
  default:return Game::is_fusion(skill)?110.f:40.f;
  }
 }
 int choice_index(const Game& game)const{
  int best=-1;float score=-1;
  for(size_t i=0;i<game.choices.size();++i){const auto& c=game.choices[i];if(!game.upgrade_choice_available(c))continue;float value=35.f;
   if(c.kind==UpgradeKind::Skill||c.kind==UpgradeKind::Study){const int rank=game.skills[int(c.skill)];value=rank>0?105+rank*7+build_value(c.skill)*.2f:build_value(c.skill);
    if(rank==0&&game.active_skill_count()>=Game::active_skill_limit)value-=50;
   }else if(c.kind==UpgradeKind::Fusion)value=160;
   else if(c.kind==UpgradeKind::Damage)value=70;
   else if(c.kind==UpgradeKind::Vitality)value=game.player.hp<game.player.max_hp*.55f?90.f:40.f;
   else if(c.kind==UpgradeKind::Relic)value=100;
   if(value>score){score=value;best=int(i);}
  }return best;
 }
 Skill replacement(const Game& game)const{
  Skill result=Skill::Count;float lowest=std::numeric_limits<float>::max();
  for(Skill skill:game.skill_slots())if(skill!=Skill::Count){const float value=build_value(skill)+game.skills[int(skill)]*12;
   if(value<lowest){lowest=value;result=skill;}}
  return result;
 }
 Input input(const Game& game,float dt){
  think-=dt;
  auto risk=[&](Vec2 at,float future){float value=0;
   for(const auto& enemy:game.enemies)if(enemy.hp>0){const float d=length(at-enemy.pos),safe=enemy.radius+40;
    if(d<safe)value+=180+(safe-d)*6;else if(d<safe+65)value+=(safe+65-d)*.5f;}
   for(const auto& e:game.effects)if(e.hostile&&e.age<e.duration){
    if(e.kind==EffectKind::Warning&&e.damage>0&&e.age+future>=e.delay&&hostile_shape_contains(e,at,18))value+=220;
    else if(e.kind==EffectKind::EnemyShot){const float d=length(at-(e.pos+e.velocity*future));if(d<e.radius+32)value+=190+(e.radius+32-d)*3;}
   }
   for(const auto& zone:game.danger_zones)if(zone.age<zone.duration&&zone.age+future>=zone.warning&&length(at-zone.pos)<zone.radius+18)value+=zone.kind==DangerKind::Explosion?220:zone.kind==DangerKind::Smoke?95:55;
   for(const auto& object:game.scene_objects)if(object.state==SceneObjectState::Active&&(object.kind==SceneObjectKind::Crate||object.kind==SceneObjectKind::Barrel)&&length(at-object.pos)<object.radius+22)value+=160;
   return value;
  };
  if(think<=0){think=.12f;Vec2 desired{std::cos(game.combat_elapsed*.07f),std::sin(game.combat_elapsed*.07f)};float nearest=450;
   for(const auto& gem:game.gems){const float d=length(gem.pos-game.player.pos);if(d>8&&d<nearest){nearest=d;desired=normalized(gem.pos-game.player.pos);}}
   if(nearest==450)for(const auto& enemy:game.enemies)if(enemy.kind==EnemyKind::Boss&&enemy.hp>0){const Vec2 to=enemy.pos-game.player.pos;const float d=length(to);const Vec2 radial=normalized(to);desired=normalized(Vec2{-radial.y,radial.x}+radial*(d>250?.8f:d<190?-.8f:0));break;}
   float best=-std::numeric_limits<float>::max();
   for(int i=0;i<16;++i){const float angle=i*6.283185307f/16;Vec2 heading{std::cos(angle),std::sin(angle)};
    const Vec2 projected_near=game.player.pos+heading*(game.player.speed*.4f),projected_far=game.player.pos+heading*(game.player.speed*.8f);
    const float score=(heading.x*desired.x+heading.y*desired.y)*60+(heading.x*move.x+heading.y*move.y)*12-risk(projected_near,.4f)-risk(projected_far,.8f)*.6f;
    if(score>best){best=score;move=heading;}
   }
  }
  Input result;result.move=move;
  if(game.player.dash_cooldown<=0&&risk(game.player.pos,.25f)>160&&risk(game.player.pos+move*90,.35f)<risk(game.player.pos,.25f))result.dash=true;
  for(const auto& object:game.scene_objects)if(object.kind==SceneObjectKind::HealingWell&&object.state==SceneObjectState::Active&&game.player.hp<game.player.max_hp*.7f&&length(object.pos-game.player.pos)<65){result.interact=true;break;}
  return result;
 }
};
}

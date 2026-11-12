#pragma once
#include "game.hpp"

namespace maple {
// These mounts use original sprite ground origins. The gunslinger cannon's
// muzzle comes from Skill/5211001/summon/attack1/info/range/sp, scaled with body.
enum class BattleMount { None, Octopus, Engineer, Launcher };
inline BattleMount battle_mount(Skill s) {
 switch(s){
 case Skill::OctopusTurret:case Skill::DeepFleet:return BattleMount::Octopus;
 case Skill::Engineer:case Skill::GravityFoundry:case Skill::BreachMines:return BattleMount::Engineer;
 case Skill::Missiles:case Skill::GuidedSalvo:case Skill::SkySwarm:case Skill::MechanicalSiege:case Skill::SeaFortress:return BattleMount::Launcher;
 default:return BattleMount::None;
 }
}
inline bool battle_actor(const Effect& e){
 if(e.kind==EffectKind::Adventure)return e.pattern==0&&battle_mount(e.source)!=BattleMount::None;
 if(e.pattern!=0)return false;
 return e.kind==EffectKind::Summon||e.kind==EffectKind::HolyDragon||e.kind==EffectKind::FireBeast||e.kind==EffectKind::SilverEagle||e.kind==EffectKind::HeavenDragon;
}
inline Vec2 battle_muzzle(Skill s,Vec2 ground,Vec2 aim){
 if(s==Skill::Summon||s==Skill::SilverEagle){const float scale=s==Skill::Summon?.75f:.65f;return ground+Vec2{(aim.x>0?24.f:-24.f)*scale,-37.f*scale};}
 const auto mount=battle_mount(s);if(mount==BattleMount::None)return ground;
 const Vec2 native=mount==BattleMount::Octopus?Vec2{-23*.9f,-18*.9f}:mount==BattleMount::Engineer?Vec2{-18*.85f,-34*.85f}:Vec2{-22*.6f,-28*.6f};
 return ground+Vec2{aim.x>0?-native.x:native.x,native.y};
}
}

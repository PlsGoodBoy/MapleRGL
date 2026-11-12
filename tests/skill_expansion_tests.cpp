#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <cassert>
#include <iostream>
using namespace maple;
namespace {
Game sample(CharacterClass character){Config c;c.first_event_time=c.boss_time=100000;c.item_drop_chance=0;c.enable_director=true;c.director_config.first_encounter=c.director_config.ambient_interval=100000;Game g(c);g.start(character,77);g.skills.fill(0);g.enemies.clear();return g;}
Enemy target(uint32_t id,Vec2 pos,EnemyKind kind=EnemyKind::Slime){Enemy e;e.id=id;e.pos=pos;e.hp=e.max_hp=100000;e.speed=0;e.damage=12;e.kind=kind;return e;}
void advance(Game& g,float seconds){for(int i=0;i<int(seconds*60);++i)g.update(1.f/60);assert(g.state==State::Playing);}
}
int main(){
 // Three short, ordinary combat fixtures. No random scan or natural-win claim.
 auto archer=sample(CharacterClass::Archer);
 for(auto skill:{Skill::Arrow,Skill::HunterMark,Skill::ArrowTrap})archer.skills[int(skill)]=3;
 archer.enemies={target(100,{105,0}),target(101,{-45,25})};archer.enemies[0].max_hp=200000;
 advance(archer,4);
 assert(archer.metrics.skill_damage[int(Skill::HunterMark)]>0&&archer.metrics.synergy_triggers[2]>0);
 assert(archer.metrics.skill_damage[int(Skill::ArrowTrap)]>0);

 auto mage=sample(CharacterClass::Mage);
 for(auto skill:{Skill::Frost,Skill::Lightning,Skill::Zone,Skill::Meteor,Skill::OrbitalOrb,Skill::GravityWell})mage.skills[int(skill)]=3;
 mage.enemies={target(100,{95,0}),target(101,{140,20}),target(102,{160,-20})};advance(mage,9);
 for(auto skill:{Skill::OrbitalOrb,Skill::GravityWell})assert(mage.metrics.skill_damage[int(skill)]>0);
 for(int synergy:{0,1,5})assert(mage.metrics.synergy_triggers[synergy]>0);

 auto warrior=sample(CharacterClass::Warrior);
 for(auto skill:{Skill::Cleave,Skill::HeavyStrike,Skill::Bulwark,Skill::Retaliate})warrior.skills[int(skill)]=3;
 warrior.enemies={target(100,{95,0}),target(101,{0,150},EnemyKind::Ranger)};advance(warrior,5);
 assert(warrior.metrics.skill_damage[int(Skill::HeavyStrike)]>0&&warrior.metrics.synergy_triggers[3]>0);
 assert(warrior.guard_charge<32&&warrior.guard_charge>=0&&warrior.metrics.synergy_triggers[4]>0);
 float guard=warrior.guard_charge;warrior.toggle_pause();warrior.update(.2f);assert(warrior.guard_charge==guard);
 warrior.start(CharacterClass::Warrior,1);assert(warrior.guard_charge==0&&warrior.metrics.synergy_triggers[4]==0);
 for(auto* g:{&archer,&mage,&warrior}){int bases=0,fusions=0;for(int i=0;i<skill_count;++i)if(g->skill_allowed(Skill(i))){if(Game::is_fusion(Skill(i)))++fusions;else ++bases;}assert(bases==8&&fusions==3);}
 std::cout<<"PASS three combat fixtures: six new skills, six bounded synergies, pause/reset and eight-base/three-fusion pools\n";
}

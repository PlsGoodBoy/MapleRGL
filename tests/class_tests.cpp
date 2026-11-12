#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace maple;
void seed(Game& g,CharacterClass c){g.start(c,5);g.enemies.clear();for(int i=0;i<8;++i){Enemy e;e.id=100+i;e.pos={80.f+i*25.f,0};e.hp=e.max_hp=10000;e.speed=0;g.enemies.push_back(e);}}
int main(){Game archer,mage,warrior;seed(archer,CharacterClass::Archer);seed(mage,CharacterClass::Mage);seed(warrior,CharacterClass::Warrior);assert(archer.skills[int(Skill::Arrow)]==1&&archer.skills[int(Skill::Pierce)]==1);assert(mage.skills[0]==1&&mage.skills[int(Skill::Lightning)]==1);assert(warrior.skills[int(Skill::Nova)]==1&&warrior.player.max_hp==125);bool arrow=false,pierce=false,chain=false,nova=false;for(int i=0;i<20;++i){archer.update(.02f);mage.update(.02f);warrior.update(.02f);for(auto&p:archer.projectiles){if(p.skill==Skill::Arrow){arrow=true;assert(length(p.velocity)>429);}if(p.skill==Skill::Pierce){pierce=true;assert(length(p.velocity)>429);}}int links=0;for(auto&e:mage.effects)if(e.kind==EffectKind::Lightning)++links;if(links>=5)chain=true;for(auto&e:warrior.effects)if(e.kind==EffectKind::Nova){nova=true;assert(e.radius>120);}}assert(arrow&&pierce&&chain&&nova);Game turn;turn.start();turn.skills.fill(0);turn.enemies.clear();Enemy e;e.id=100;e.pos={.01f,100};e.speed=0;e.hp=e.max_hp=100;e.facing_left=false;turn.enemies.push_back(e);turn.update(.01f);assert(!turn.enemies[0].facing_left);turn.enemies[0].pos={-.01f,100};turn.update(.01f);assert(!turn.enemies[0].facing_left);turn.enemies[0].pos={100,100};turn.update(.01f);assert(turn.enemies[0].facing_left);turn.player.facing_left=true;turn.update(.01f,{{0,1},false});assert(turn.player.facing_left);std::cout<<"PASS class starting skills, arrow velocity, mage chains, warrior area and stable vertical facing\n";}

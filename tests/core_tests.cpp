#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
using namespace maple;
static bool close(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 Game a,b; a.start(CharacterClass::Archer,42);b.start(CharacterClass::Archer,42);
 assert(a.skills[(int)Skill::Arrow]==1&&a.player.hp==a.player.max_hp);
 for(int i=0;i<60;i++){a.update(1.f/60,{{1,0},false});b.update(1.f/60,{{1,1},false});}
 assert(std::abs(length(a.player.pos)-length(b.player.pos))<.1f);
 float time=a.elapsed;Vec2 before=a.player.pos;a.toggle_pause();a.update(.2f,{{1,0},true});assert(close(a.elapsed,time)&&close(a.player.pos.x,before.x));a.toggle_pause();
 a.update(1.f/60,{{1,0},true});assert(a.player.dash_cooldown>0&&a.player.invulnerable>0);
 a.gems.push_back({a.player.pos,100,0});a.update(.01f);assert(a.state==State::Upgrade&&a.choices.size()==3);time=a.elapsed;a.update(.1f);assert(close(a.elapsed,time));assert(!a.choose_upgrade(99));int count=0;while(a.state==State::Upgrade){assert(a.choose_upgrade(0));assert(++count<20);}assert(a.player.level>1);
 a.player.hp=1;a.player.invulnerable=0;a.enemies.clear();Enemy e;e.id=100;e.hp=e.max_hp=100;e.damage=10;e.pos=a.player.pos;e.speed=0;e.radius=20;a.enemies.push_back(e);a.update(.01f);assert(a.state==State::Defeat&&a.player.hp==0);a.start(CharacterClass::Mage,2);assert(a.state==State::Playing&&a.kills==0&&a.player.level==1&&a.skills[0]==1);
 Game c,d;c.start(CharacterClass::Warrior,87);d.start(CharacterClass::Warrior,87);for(int i=0;i<240;i++){c.update(1.f/60,{{1,0}});d.update(1.f/60,{{1,0}});}assert(c.enemies.size()==d.enemies.size()&&c.kills==d.kills&&c.player.pos.x==d.player.pos.x);for(size_t i=0;i<c.enemies.size();++i)assert(c.enemies[i].pos.x==d.enemies[i].pos.x);
 Config quick;quick.boss_time=.1f;quick.duration=.3f;Game short_game(quick);short_game.start();for(int i=0;i<20;i++)short_game.update(.02f);assert(short_game.boss_spawned&&short_game.state==State::Victory);
 // Exercise all skills, collision, progression, elites and complete timeline without visual assertions.
 Game full;full.start(CharacterClass::Mage,123);full.skills.fill(5);full.passives.fill(2);full.player.hp=100000;full.player.max_hp=100000;auto begin=std::chrono::steady_clock::now();for(int i=0;i<36060&&full.state!=State::Victory;i++){float angle=i*.003f;full.update(1.f/60,{{std::cos(angle),std::sin(angle)},i%120==0});while(full.state==State::Upgrade)full.choose_upgrade(0);full.player.hp=100000;assert(full.enemies.size()<=full.config.max_enemies);assert(full.projectiles.size()<=full.config.max_projectiles);assert(full.gems.size()<=full.config.max_gems);assert(std::isfinite(full.player.pos.x));}assert(full.state==State::Victory&&full.boss_spawned);auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();std::cout<<"PASS core: move, dash, pause, upgrades, defeat/restart, determinism, 7 skills, boss and 10-minute timeline.\nSimulation elapsed="<<full.elapsed<<" kills="<<full.kills<<" steps="<<full.simulation_steps<<" wall_ms="<<ms<<" (headless, not rendering FPS)\n";
 // Stationary dense encounter performance measurement, not a claim of game FPS.
 Game stress;stress.start();stress.skills.fill(5);stress.player.hp=1e9f;stress.enemies.clear();for(int i=0;i<500;i++){Enemy enemy;enemy.id=i+1;enemy.pos={float(i%25*12-150),float(i/25*12-120)};enemy.hp=enemy.max_hp=1e6f;enemy.speed=0;stress.enemies.push_back(enemy);}begin=std::chrono::steady_clock::now();for(int i=0;i<600;i++)stress.update(1.f/60);ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();std::cout<<"500-enemy core benchmark: "<<ms/600<<" ms per simulated tick, candidates="<<stress.broadphase_candidates<<"\n";
}

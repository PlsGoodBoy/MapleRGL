#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace maple;
static void advance(Game& g,float seconds){for(int n=0;n<int(seconds*100)+1&&g.state==State::Playing;++n)g.update(.01f);}
static Enemy enemy(uint32_t id,Vec2 pos){Enemy e;e.id=id;e.pos=pos;e.hp=e.max_hp=1000;e.speed=0;e.damage=0;return e;}
static float hp(const Game&g,uint32_t id){for(auto&e:g.enemies)if(e.id==id)return e.hp;return 0;}
static void assert_tree(const Game&g){for(int i=0;i<skill_count;++i)if(g.skills[i])assert(g.skill_allowed(Skill(i)));for(auto&c:g.choices)if(c.kind==UpgradeKind::Skill)assert(g.skill_allowed(c.skill));}
int main(){
 Config quiet;quiet.enabled_events=3;quiet.first_event_time=1e6f;quiet.item_drop_chance=0;
 for(int cls=0;cls<3;++cls)for(int seed=1;seed<=8;++seed){Game g(quiet);g.start(CharacterClass(cls),seed);g.enemies.clear();g.player.hp=g.player.max_hp=1e6f;assert_tree(g);for(int round=0;round<24;++round){g.gems.push_back({g.player.pos,g.player.xp_next,0});g.update(.01f);assert(g.state==State::Upgrade);while(g.state==State::Upgrade){assert_tree(g);assert(g.choose_upgrade(0));}assert_tree(g);}}
 Game forged(quiet);forged.start(CharacterClass::Mage);forged.state=State::Upgrade;forged.choices={{UpgradeKind::Skill,Skill::Pierce,"bad","bad"}};assert(!forged.choose_upgrade(0)&&forged.skills[int(Skill::Pierce)]==0);
 Game cleave(quiet);cleave.start(CharacterClass::Warrior);cleave.skills.fill(0);cleave.skills[int(Skill::Cleave)]=1;cleave.enemies={enemy(100,{60,0}),enemy(101,{-90,0}),enemy(102,{0,90})};advance(cleave,.34f);assert(hp(cleave,100)<1000&&hp(cleave,101)==1000&&hp(cleave,102)==1000);
 Game orbit(quiet);orbit.start(CharacterClass::Warrior);orbit.skills.fill(0);orbit.skills[int(Skill::Orbit)]=1;orbit.enemies={enemy(100,{69,0})};advance(orbit,2.0f);assert(hp(orbit,100)<1000);bool rendered=false;for(auto&e:orbit.effects)if(e.kind==EffectKind::Orbit){rendered=true;assert(std::abs(length(e.end-orbit.player.pos)-e.radius)<.01f);}assert(rendered);
 Config short_buff=quiet;short_buff.fury_duration=.08f;short_buff.magnet_duration=.08f;Game items(short_buff);items.start(CharacterClass::Mage);items.player.hp=50;items.items={{ItemKind::Health,items.player.pos,0},{ItemKind::Fury,items.player.pos,0},{ItemKind::Magnet,items.player.pos,0}};items.update(.01f);assert(items.player.hp==90&&items.items.empty()&&items.damage_multiplier>1.39f&&items.attack_speed>1.24f&&items.pickup_radius==1600);items.toggle_pause();float timer=items.fury_remaining;items.update(.2f);assert(items.fury_remaining==timer);items.toggle_pause();advance(items,.1f);assert(items.fury_remaining==0&&items.magnet_remaining==0&&items.damage_multiplier==1&&items.pickup_radius==65);
 int seen=0;for(int seed=1;seed<30;++seed){Config c=quiet;c.first_event_time=.01f;c.event_duration=30;Game g(c);g.start(CharacterClass::Mage,seed);g.update(.02f);auto kind=g.special_event.kind;assert(kind!=SpecialEventKind::None);seen|=1<<int(kind);float remain=g.special_event.remaining;g.toggle_pause();g.update(.1f);assert(g.special_event.remaining==remain);g.toggle_pause();if(kind==SpecialEventKind::Blessing){g.player.hp=100;g.player.pos=g.special_event.pos;g.update(.01f);assert(g.special_event.kind==SpecialEventKind::None&&g.player.hp==85&&g.fury_remaining>c.fury_duration);}else{for(auto id:g.special_event.guardians)for(auto&e:g.enemies)if(e.id==id)e.hp=0;g.player.pos=g.special_event.pos;g.update(.01f);assert(g.special_event.kind==SpecialEventKind::None&&g.items.size()>=3);assert(!g.gems.empty());}assert_tree(g);}
 assert(seen==14);
 Game timeout(quiet);timeout.start();timeout.special_event.kind=SpecialEventKind::GuardedChest;timeout.special_event.pos={1000,1000};timeout.special_event.remaining=.001f;timeout.update(.01f);assert(timeout.special_event.kind==SpecialEventKind::None&&timeout.next_event_in>=quiet.event_interval_min&&timeout.items.empty());
 Game reset(quiet);reset.start();reset.items.push_back({ItemKind::Fury,{0,0},0});reset.update(.01f);reset.start();assert(reset.items.empty()&&reset.fury_remaining==0&&reset.special_event.kind==SpecialEventKind::None);
 std::cout<<"PASS strict class trees across 576 upgrades, forged foreign skill rejection, cleave cone, orbit contact, items/buff expiry/pause, all three event rewards/timeouts and reset\n";
}

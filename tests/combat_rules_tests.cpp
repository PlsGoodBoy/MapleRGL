#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>
using namespace maple;
static Config quiet(){Config c;c.first_event_time=1e6f;c.elite_interval=1e6f;c.boss_time=1e6f;c.duration=1e6f;c.item_drop_chance=0;c.max_enemies=0;return c;}
static Enemy foe(uint32_t id,Vec2 pos,float hp=10000){Enemy e;e.id=id;e.pos=pos;e.hp=e.max_hp=hp;e.speed=0;e.damage=0;return e;}
static void advance(Game& g,float seconds,Input input={}){for(int i=0;i<int(std::ceil(seconds*100));++i){g.update(.01f,input);input.dash=false;while(g.state==State::Upgrade)assert(g.choose_upgrade(0));}}
static double total(const Game& g){double result=g.metrics.other_damage;for(double d:g.metrics.skill_damage)result+=d;return result;}
static bool near(double a,double b){return std::abs(a-b)<.01;}
int main(){
 // Original eighteen active behaviors remain covered here; Fusion support skills/fusions have focused fixtures.
 for(int cls=0;cls<3;++cls){Game g(quiet());g.start(CharacterClass(cls),3);int allowed=0;
  for(int s=0;s<18;++s)if(g.skill_allowed(Skill(s))){++allowed;Game one(quiet());one.start(CharacterClass(cls),3);one.skills.fill(0);one.skills[s]=1;
   one.enemies={foe(100,{65,0}),foe(101,{105,10}),foe(102,{-65,0}),foe(103,{0,68})};
   if(Skill(s)==Skill::Retaliate){advance(one,.31f);advance(one,.01f,{{1,0},true});}
   advance(one,2.2f);assert(one.metrics.skill_damage[s]>0);assert(total(one)==one.metrics.skill_damage[s]);
  }assert(allowed==6);
 }
 // Every upgrade route is class-bound, caps cannot be bypassed with forged choices.
 for(int cls=0;cls<3;++cls)for(int seed=1;seed<=12;++seed){Game g(quiet());g.start(CharacterClass(cls),seed);
  for(int round=0;round<110;++round){g.gems.push_back({g.player.pos,g.player.xp_next,0});g.update(.01f);assert(g.state==State::Upgrade);std::set<std::pair<int,int>> unique;
   for(auto& c:g.choices){assert(unique.insert({int(c.kind),c.kind==UpgradeKind::Skill?int(c.skill):-1}).second);if(c.kind==UpgradeKind::Skill){assert(g.skill_allowed(c.skill));assert(g.skills[int(c.skill)]<5);}else if(int(c.kind)<passive_count)assert(g.passives[int(c.kind)]<8);}
   assert(g.choices.size()==3);while(g.state==State::Upgrade)assert(g.choose_upgrade(round%g.choices.size()));for(int s=0;s<skill_count;++s)if(g.skills[s])assert(g.skill_allowed(Skill(s)));
  }
 }
 Game illegal(quiet());illegal.start(CharacterClass::Mage);illegal.state=State::Upgrade;illegal.choices={{UpgradeKind::Skill,Skill::Spread,"",""},{UpgradeKind::Skill,Skill::Count,"",""},{UpgradeKind(999),Skill::Count,"",""}};
 for(int i=0;i<3;++i)assert(!illegal.choose_upgrade(i));illegal.state=State::Playing;illegal.skills.fill(5);illegal.enemies={foe(100,{60,0})};advance(illegal,1);for(int s=0;s<skill_count;++s)if(!illegal.skill_allowed(Skill(s)))assert(illegal.metrics.skill_damage[s]==0);
 Game full(quiet());full.start();full.skills.fill(5);full.passives.fill(8);full.gems.push_back({full.player.pos,full.player.xp_next,0});full.update(.01f);assert(full.choices.size()==3);assert(full.choices[0].kind==UpgradeKind::Overcharge);assert(full.choose_upgrade(0)&&full.overcharge_levels==1);assert(full.damage_multiplier>2.23f);
 // All 18 evolution requirements are reachable only in their own class; each class has mechanics beyond damage scaling.
 const int required[]={1,4,6,4,3,9,9,1,3,2,9,4,3,8,9,5,4,6};
 for(int cls=0;cls<3;++cls){Game g(quiet());g.start(CharacterClass(cls));g.skills.fill(5);g.passives.fill(0);for(int s=0;s<18;++s)if(g.skill_allowed(Skill(s))){assert(!g.evolved(Skill(s)));g.passives[required[s]]=2;assert(g.evolved(Skill(s)));g.passives[required[s]]=0;}}
 Game returning(quiet());returning.start();returning.skills.fill(0);returning.skills[int(Skill::Pierce)]=5;returning.passives[4]=2;returning.enemies={foe(100,{150,0})};advance(returning,1.05f);bool did_return=false;for(auto& p:returning.projectiles)if(p.skill==Skill::Pierce&&p.returned)did_return=true;assert(did_return);
 Game lightning(quiet());lightning.start(CharacterClass::Mage);lightning.skills.fill(0);lightning.skills[int(Skill::Lightning)]=5;lightning.passives[3]=2;lightning.enemies={foe(100,{100,0})};advance(lightning,.4f);bool echo=false;for(auto& e:lightning.effects)if(e.kind==EffectKind::Rune&&e.source==Skill::Lightning&&e.delay>0)echo=true;assert(echo);
 Game dragon(quiet());dragon.start(CharacterClass::Warrior);dragon.skills.fill(0);dragon.skills[int(Skill::Nova)]=5;dragon.passives[6]=2;dragon.enemies={foe(100,{100,0})};advance(dragon,.4f);assert(dragon.shield_remaining>0);echo=false;for(auto& e:dragon.effects)if(e.source==Skill::Nova&&e.kind==EffectKind::Rune)echo=true;assert(echo);
 // All passive categories affect their actual public combat attributes.
 Game stats(quiet());stats.start(CharacterClass::Mage);stats.passives.fill(2);stats.update(.01f);assert(near(stats.damage_multiplier,1.3)&&near(stats.attack_speed,1.24)&&near(stats.area_multiplier,1.24));assert(stats.extra_projectiles==2&&stats.player.max_hp==140&&stats.pickup_radius==109);assert(stats.cooldown_multiplier<1&&stats.duration_multiplier>1&&stats.crit_chance>.16&&stats.crit_multiplier>2);
 // Damage accounting clamps overkill and preserves delayed/DOT source attribution.
 Game damage(quiet());damage.start(CharacterClass::Mage);damage.skills.fill(0);damage.enemies={foe(100,{80,0},7)};Effect blast;blast.kind=EffectKind::Meteor;blast.pos={80,0};blast.radius=50;blast.damage=10000;blast.duration=1;blast.delay=.2f;blast.source=Skill::Meteor;damage.effects.push_back(blast);advance(damage,.1f);assert(total(damage)==0);advance(damage,.15f);assert(near(total(damage),7)&&near(damage.metrics.skill_damage[int(Skill::Meteor)],7));
 Game burn(quiet());burn.start(CharacterClass::Mage);burn.skills.fill(0);burn.enemies={foe(100,{80,0},1000)};Effect fire;fire.kind=EffectKind::Zone;fire.pos={80,0};fire.radius=40;fire.duration=.05f;fire.damage=20;fire.source=Skill::Zone;burn.effects.push_back(fire);advance(burn,.08f);double first=burn.metrics.skill_damage[int(Skill::Zone)];burn.enemies[0].pos={800,0};advance(burn,1.f);assert(burn.metrics.skill_damage[int(Skill::Zone)]>first);assert(near(1000-burn.enemies[0].hp,burn.metrics.skill_damage[int(Skill::Zone)]));
 Game hurt(quiet());hurt.start();hurt.skills.fill(0);hurt.player.hp=5;Enemy contact=foe(100,{0,0});contact.damage=100;hurt.enemies={contact};hurt.update(.01f);assert(hurt.state==State::Defeat&&near(hurt.metrics.damage_taken,5));
 Game protected_game(quiet());protected_game.start(CharacterClass::Warrior);protected_game.skills.fill(0);protected_game.skills[int(Skill::Retaliate)]=5;protected_game.shield_remaining=5;protected_game.enemies={contact};advance(protected_game,1);assert(protected_game.metrics.damage_taken==0&&protected_game.metrics.skill_damage[int(Skill::Retaliate)]>0);
 Game xp(quiet());xp.start();xp.skills.fill(0);xp.double_xp_remaining=20;xp.gems.push_back({xp.player.pos,3,0});xp.update(.01f);assert(xp.metrics.xp_acquired==6&&xp.metrics.xp_by_minute[0]==6);
 // Early XP curve stays unchanged; later growth becomes a deliberate build choice.
 Game growth(quiet());growth.start();growth.player.level=11;growth.player.xp=0;growth.player.xp_next=1;growth.gems.push_back({growth.player.pos,1,0});growth.update(.01f);assert(near(growth.player.xp_next,8+12*3.2));while(growth.state==State::Upgrade)growth.choose_upgrade(0);growth.gems.push_back({growth.player.pos,growth.player.xp_next,0});growth.update(.01f);assert(growth.player.level==13&&near(growth.player.xp_next,8+13*3.2+.45));
 Config boss_window=quiet();boss_window.first_event_time=.001f;boss_window.boss_time=30;Game window(boss_window);window.start();window.update(.01f);assert(window.special_event.kind==SpecialEventKind::None&&window.metrics.events_started==0);
 // Every event type can be scheduled; interaction costs are separate from incoming damage.
 unsigned seen=0;for(int seed=1;seed<=160;++seed){Config c=quiet();c.max_enemies=60;c.first_event_time=.001f;Game g(c);g.start(CharacterClass::Mage,seed);g.update(.01f);assert(g.special_event.kind!=SpecialEventKind::None);seen|=1u<<int(g.special_event.kind);assert(g.metrics.events_started==1);assert(!g.special_event_objective().empty());}assert(seen==510);
 Game altar(quiet());altar.start();altar.special_event.kind=SpecialEventKind::Blessing;altar.special_event.pos=altar.player.pos;altar.special_event.remaining=10;altar.update(.01f);assert(altar.player.hp==85&&altar.metrics.altar_health_spent==15&&altar.metrics.damage_taken==0&&altar.metrics.events_succeeded==1);
 Game merchant(quiet());merchant.start();merchant.coins=24;merchant.special_event.kind=SpecialEventKind::Merchant;merchant.special_event.remaining=10;merchant.update(.01f);assert(merchant.special_event.kind==SpecialEventKind::Merchant&&merchant.coins==24);merchant.coins=25;merchant.update(.01f);assert(merchant.special_event.kind==SpecialEventKind::None&&merchant.coins==0);
 Game rift(quiet());rift.start();rift.special_event.kind=SpecialEventKind::Rift;rift.special_event.remaining=20;rift.special_event.target=8;advance(rift,2);assert(rift.special_event.progress>1.9f&&rift.rift_remaining>0&&rift.cooldown_multiplier<1);rift.player.pos={200,0};advance(rift,1);assert(rift.special_event.progress<1.7f);rift.player.pos={0,0};advance(rift,7);assert(rift.special_event.kind==SpecialEventKind::None&&rift.metrics.events_succeeded==1&&rift.rift_remaining==0);
 // Item rewards use the same class filter even at full skill capacity.
 Game chest(quiet());chest.start(CharacterClass::Mage);for(int i=0;i<40;++i){chest.items={{ItemKind::Chest,chest.player.pos,0}};advance(chest,.01f);}for(int s=0;s<skill_count;++s)if(chest.skills[s])assert(chest.skill_allowed(Skill(s)));assert(chest.overcharge_levels>0);
 // Frozen charging enemies stop; ranged attacks have warning before projectiles.
 Config foes=quiet();foes.max_enemies=20;Game ranged(foes);ranged.start();ranged.skills.fill(0);ranged.enemies.clear();Enemy ranger=foe(100,{220,0});ranger.kind=EnemyKind::Ranger;ranger.attack_clock=3.19f;ranger.damage=10;ranged.enemies.push_back(ranger);ranged.update(.02f);bool warning=false;for(auto& e:ranged.effects)if(e.hostile&&e.kind==EffectKind::Warning)warning=true;assert(warning&&ranged.metrics.damage_taken==0);advance(ranged,.7f);bool shot=false;for(auto& e:ranged.effects)if(e.hostile&&e.kind==EffectKind::EnemyShot)shot=true;assert(shot);
 Game split(foes);split.start();split.skills.fill(0);split.enemies={foe(100,{100,0},0)};split.enemies[0].kind=EnemyKind::Splitter;split.update(.01f);int minions=0;for(auto& e:split.enemies)if(e.kind==EnemyKind::Minion)++minions;assert(minions==2);
 std::cout<<"PASS Combat: 18 active behaviors, 3960 legal upgrades, caps/fallbacks, real evolution mechanics, passives, precise damage/XP accounting, 8 event types, item pools, telegraphs and splitting enemies\n";
}


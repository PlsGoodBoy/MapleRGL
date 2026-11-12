#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
using namespace maple;
namespace {
bool near(double a,double b){return std::abs(a-b)<.001;}
Config quiet(){Config c;c.first_event_time=c.elite_interval=c.boss_time=c.duration=1e6f;c.item_drop_chance=0;c.max_enemies=0;return c;}
Enemy foe(uint32_t id,EnemyKind kind,Vec2 pos,float hp=10000,float damage=0){Enemy e;e.id=id;e.kind=kind;e.pos=pos;e.hp=e.max_hp=hp;e.speed=0;e.damage=damage;return e;}
void resolve_upgrades(Game& g){int budget=100;while(g.state==State::Upgrade){assert(--budget>0);assert(g.choose_upgrade(0));}}
std::set<std::pair<int,int>> identities(const std::vector<UpgradeChoice>& choices){std::set<std::pair<int,int>> out;for(const auto& c:choices)out.insert({int(c.kind),c.kind==UpgradeKind::Skill?int(c.skill):-1});return out;}
void grant_level(Game& g){g.gems.push_back({g.player.pos,g.player.xp_next,0});g.update(.01f);assert(g.state==State::Upgrade);}
RunBonuses full_bonuses(){RunBonuses b;b.damage_bonus=.10f;b.crit_bonus=.06f;b.health_bonus=20;b.move_speed_bonus=.06f;b.xp_bonus=.10f;b.cooldown_reduction=.06f;b.upgrade_rerolls=2;b.death_protections=1;b.first_elite_reward=1;return b;}
}
int main(){
 // Optional third parameter keeps the original call sites and default seeded sequence intact.
 for(int cls=0;cls<3;++cls){Game a,b;a.start(CharacterClass(cls),72);b.start(CharacterClass(cls),72,{});
  for(int i=0;i<900;++i){Input input{{std::cos(i*.01f),std::sin(i*.01f)},i%90==0};a.update(.016f,input);b.update(.016f,input);resolve_upgrades(a);resolve_upgrades(b);}
  assert(a.state==b.state&&a.kills==b.kills&&a.elapsed==b.elapsed&&a.player.hp==b.player.hp&&a.player.pos.x==b.player.pos.x&&a.player.pos.y==b.player.pos.y);
  assert(a.enemies.size()==b.enemies.size()&&a.projectiles.size()==b.projectiles.size());assert(a.metrics.skill_damage==b.metrics.skill_damage);assert(a.metrics.enemy_kills_by_kind==b.metrics.enemy_kills_by_kind);
  assert(a.metrics.character==CharacterClass(cls)&&a.metrics.dash_uses==b.metrics.dash_uses);
 }
 // Every permanent stat is modest, actual, and recomputed rather than re-applied each frame.
 for(int cls=0;cls<3;++cls){RunBonuses bonuses=full_bonuses();Game g(quiet());g.start(CharacterClass(cls),2,bonuses);bonuses.health_bonus=1000;
  assert(near(g.damage_multiplier,1.1)&&near(g.crit_chance,.16)&&near(g.cooldown_multiplier,.94));
  assert(near(g.player.max_hp,cls==2?145:120)&&near(g.player.hp,g.player.max_hp));assert(near(g.player.speed,(cls==0?175:160)*1.06));
  g.skills.fill(0);g.player.hp=40;for(int i=0;i<200;++i)g.update(.01f);
  assert(g.player.hp==40&&near(g.player.max_hp,cls==2?145:120));assert(near(g.run_bonuses().health_bonus,20));
  g.passives[1]=2;g.passives[6]=2;g.passives[8]=2;g.passives[10]=2;g.update(.01f);
  assert(near(g.damage_multiplier,1.3*1.1)&&near(g.player.max_hp,cls==2?185:160));assert(near(g.cooldown_multiplier,.94/1.16));assert(near(g.crit_chance,.23));
  g.fury_remaining=10;g.power_remaining=10;g.update(.01f);assert(near(g.damage_multiplier,1.3*1.1*1.4*1.5));
  for(int i=0;i<50;++i)g.update(.01f);assert(near(g.damage_multiplier,1.3*1.1*1.4*1.5));
  g.double_xp_remaining=20;g.gems.push_back({g.player.pos,3,0});g.update(.01f);assert(near(g.metrics.xp_acquired,6.6)&&near(g.player.xp,6.6));
 }
 // Invalid profile values cannot turn into NaN stats, unlimited charges or excessive strength.
 Game capped(quiet());RunBonuses extreme=full_bonuses();extreme.damage_bonus=100;extreme.crit_bonus=100;extreme.health_bonus=100;extreme.move_speed_bonus=100;extreme.xp_bonus=100;extreme.cooldown_reduction=100;extreme.upgrade_rerolls=999;extreme.death_protections=999;extreme.first_elite_reward=999;
 capped.start(CharacterClass::Mage,4,extreme);assert(near(capped.damage_multiplier,1.1)&&near(capped.player.max_hp,120)&&near(capped.cooldown_multiplier,.94));assert(capped.upgrade_rerolls_remaining==2&&capped.death_protections_remaining==1&&capped.run_bonuses().first_elite_reward==1);
 extreme.damage_bonus=std::numeric_limits<float>::quiet_NaN();extreme.health_bonus=std::numeric_limits<float>::infinity();extreme.cooldown_reduction=-5;extreme.upgrade_rerolls=-3;
 capped.start(CharacterClass::Mage,4,extreme);assert(capped.damage_multiplier==1&&capped.player.max_hp==100&&capped.cooldown_multiplier==1&&capped.upgrade_rerolls_remaining==0);
 // Reroll is legal only at an upgrade, does not grant a level, and provides a genuinely new choice.
 for(int cls=0;cls<3;++cls)for(int seed=1;seed<=24;++seed){Game g(quiet());g.start(CharacterClass(cls),seed,full_bonuses());assert(!g.reroll_upgrade()&&g.upgrade_rerolls_remaining==2);g.skills.fill(0);grant_level(g);
  const int level=g.player.level;const float elapsed=g.elapsed;const auto skills=g.skills;const auto passives=g.passives;
  for(int charge=0;charge<2;++charge){auto old=identities(g.choices);assert(g.reroll_upgrade());auto next=identities(g.choices);assert(next!=old&&next.size()==3);for(auto& c:g.choices)if(c.kind==UpgradeKind::Skill)assert(g.skill_allowed(c.skill));}
  assert(!g.reroll_upgrade()&&g.metrics.upgrade_rerolls_used==2&&g.upgrade_rerolls_remaining==0);assert(g.player.level==level&&g.elapsed==elapsed&&g.skills==skills&&g.passives==passives&&g.state==State::Upgrade);
  assert(g.choose_upgrade(0)&&g.state==State::Playing);assert(!g.reroll_upgrade());
 }
 Game exhausted(quiet());exhausted.start(CharacterClass::Archer,7,full_bonuses());exhausted.skills.fill(5);exhausted.passives.fill(8);grant_level(exhausted);auto old=identities(exhausted.choices);assert(!exhausted.reroll_upgrade());assert(exhausted.upgrade_rerolls_remaining==2&&exhausted.metrics.upgrade_rerolls_used==0&&old==identities(exhausted.choices));
 // Death protection is consumed once by real lethal damage and safely clears simultaneous danger.
 Game saved(quiet());saved.start(CharacterClass::Mage,9,full_bonuses());saved.skills.fill(0);saved.player.hp=5;
 saved.enemies={foe(100,EnemyKind::Slime,{0,0},10000,100),foe(101,EnemyKind::Elite,{20,0},10000,100)};saved.enemies[1].action_remaining=2;
 Effect shot;shot.kind=EffectKind::EnemyShot;shot.pos=saved.player.pos;shot.damage=500;shot.hostile=true;shot.duration=10;shot.radius=10;saved.effects={shot,shot};saved.update(.01f);
 assert(saved.state==State::Playing&&near(saved.player.hp,42)&&near(saved.metrics.damage_taken,5));assert(saved.death_protections_remaining==0&&saved.metrics.death_protections_used==1&&saved.player.invulnerable>2.9f&&saved.shield_remaining>2.9f);
 assert(saved.notice_remaining>0&&!saved.notice_text.empty());for(const auto& e:saved.enemies)assert(length(e.pos-saved.player.pos)>=175&&e.action_remaining==0);for(const auto& effect:saved.effects)assert(!effect.hostile||effect.damage==0);assert(saved.kills==0);
 saved.update(.02f);assert(saved.metrics.death_protections_used==1&&saved.death_protections_remaining==0);saved.shield_remaining=saved.player.invulnerable=0;saved.player.hp=1;saved.enemies={foe(102,EnemyKind::Slime,saved.player.pos,10000,100)};saved.update(.01f);assert(saved.state==State::Defeat&&saved.player.hp==0&&saved.metrics.death_protections_used==1);
 // Protection triggered inside the hostile-effects loop never erases/reallocates the vector mid-iteration.
 Game effect_saved(quiet());effect_saved.start(CharacterClass::Archer,9,full_bonuses());effect_saved.skills.fill(0);effect_saved.player.hp=1;shot.pos=effect_saved.player.pos;effect_saved.effects.assign(100,shot);effect_saved.update(.01f);assert(effect_saved.state==State::Playing&&effect_saved.metrics.death_protections_used==1&&effect_saved.effects.empty());
 // Nonlethal altar spending is not incoming damage and cannot consume the protection.
 Game altar(quiet());altar.start(CharacterClass::Archer,8,full_bonuses());altar.skills.fill(0);altar.special_event.kind=SpecialEventKind::Blessing;altar.special_event.pos=altar.player.pos;altar.special_event.remaining=10;altar.update(.01f);assert(altar.death_protections_remaining==1&&altar.metrics.death_protections_used==0&&altar.metrics.damage_taken==0&&altar.metrics.altar_health_spent==15);
 // Actual elite kills, not spawning or despawning, trigger the boon once; it is not camp currency.
 Game elite(quiet());elite.start(CharacterClass::Archer,11,full_bonuses());elite.skills.fill(0);elite.enemies={foe(100,EnemyKind::Elite,{200,0})};elite.update(.01f);assert(!elite.first_elite_reward_claimed&&elite.metrics.first_elite_rewards==0);
 elite.enemies[0].pos={10000,0};elite.update(.01f);assert(elite.enemies.empty()&&elite.kills==0&&!elite.first_elite_reward_claimed);
 elite.enemies={foe(101,EnemyKind::Elite,{200,0},0)};elite.update(.01f);assert(elite.kills==1&&elite.coins==18&&elite.first_elite_reward_claimed&&elite.metrics.first_elite_rewards==1&&near(elite.metrics.xp_acquired,5.5));
 elite.enemies={foe(102,EnemyKind::Elite,{200,0},0)};elite.update(.01f);assert(elite.kills==2&&elite.coins==26&&elite.metrics.first_elite_rewards==1&&near(elite.metrics.xp_acquired,5.5));assert(elite.metrics.enemy_kills_by_kind[int(EnemyKind::Elite)]==2);
 // Measured metrics include each killed kind and only successful dash activations.
 Game measured(quiet());measured.start(CharacterClass::Warrior,1);measured.skills.fill(0);measured.update(.01f,{{1,0},true});assert(measured.metrics.dash_uses==1);measured.update(.01f,{{1,0},true});assert(measured.metrics.dash_uses==1);measured.toggle_pause();measured.update(.2f,{{1,0},true});assert(measured.metrics.dash_uses==1);measured.toggle_pause();measured.player.dash_cooldown=0;measured.update(.01f,{{1,0},true});assert(measured.metrics.dash_uses==2);
 for(int kind=0;kind<enemy_kind_count;++kind)measured.enemies.push_back(foe(200+kind,EnemyKind(kind),{500,0},0));measured.update(.01f);assert(measured.kills==enemy_kind_count);for(int count:measured.metrics.enemy_kills_by_kind)assert(count==1);assert(measured.metrics.character==CharacterClass::Warrior);
 // New runs reset all permanent-effect charges and metrics; default restart does not retain talents.
 saved.start(CharacterClass::Mage,9,full_bonuses());assert(saved.metrics.death_protections_used==0&&saved.death_protections_remaining==1&&saved.upgrade_rerolls_remaining==2&&!saved.first_elite_reward_claimed&&saved.kills==0);
 saved.start();assert(saved.player.max_hp==100&&saved.damage_multiplier==1&&saved.upgrade_rerolls_remaining==0&&saved.death_protections_remaining==0&&saved.metrics.dash_uses==0&&saved.metrics.character==CharacterClass::Archer);
 std::cout<<"PASS Progression runtime: clamped talent snapshots, nine actual effects, legal rerolls, one-shot death protection, first elite reward, measured dash/class/kill metrics, zero-bonus determinism and clean restarts\n";
}

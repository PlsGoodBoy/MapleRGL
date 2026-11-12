#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
using namespace maple;

static Config quiet(){
 Config c;c.first_event_time=c.elite_interval=c.boss_time=c.duration=1e6f;
 c.item_drop_chance=0;c.max_enemies=500;return c;
}
static Enemy filler(uint32_t id){
 Enemy e;e.id=id;e.pos={700,0};e.hp=e.max_hp=1e6f;e.speed=e.damage=0;return e;
}
static Game ready(uint32_t seed,size_t count){
 Game g(quiet());g.start(CharacterClass::Mage,seed);g.skills.fill(0);g.shield_remaining=1e6f;
 // Settle the normal spawn clock before installing the deterministic capacity fixture.
 g.update(.01f);g.enemies.clear();
 for(size_t i=0;i<count;++i)g.enemies.push_back(filler(10000+uint32_t(i)));
 g.next_event_in=0;return g;
}
static uint32_t seed_for(SpecialEventKind kind){
 for(uint32_t seed=1;seed<=160;++seed){auto g=ready(seed,0);g.update(.01f);if(g.special_event.kind==kind)return seed;}
 assert(false&&"event seed not found");return 0;
}
static bool has(const Game& g,uint32_t id){
 return std::any_of(g.enemies.begin(),g.enemies.end(),[&](const Enemy& e){return e.id==id;});
}
static void tick(Game& g){
 for(auto& e:g.enemies){e.speed=0;e.damage=0;}
 g.update(.01f);assert(g.state==State::Playing);assert(g.enemies.size()<=g.config.max_enemies);
}
static void kill(Game& g,uint32_t id){
 auto it=std::find_if(g.enemies.begin(),g.enemies.end(),[&](const Enemy& e){return e.id==id;});
 assert(it!=g.enemies.end());it->hp=0;
}

int main(){
 const auto rush_seed=seed_for(SpecialEventKind::MushroomRush);
 // Full and insufficient capacity must delay, without starting/failing/rewarding an event.
 for(size_t count:{size_t(500),size_t(489)}){
  auto g=ready(rush_seed,count);tick(g);
  assert(g.special_event.kind==SpecialEventKind::None&&g.next_event_in==10);
  assert(g.metrics.events_started==0&&g.metrics.events_failed==0&&g.metrics.events_succeeded==0);
  assert(g.enemies.size()==count&&g.kills==0&&g.items.empty()&&g.gems.empty());
  for(size_t i=0;i<count;++i)assert(has(g,10000+uint32_t(i)));
 }
 // A delayed event can retry once room is available (the normal scheduler may reroll its kind).
 auto delayed=ready(rush_seed,500);tick(delayed);delayed.enemies.resize(488);
 delayed.next_event_in=0;tick(delayed);
 assert(delayed.special_event.kind!=SpecialEventKind::None&&delayed.metrics.events_started==1);
 assert(delayed.metrics.events_succeeded==0&&delayed.items.empty());
 for(size_t limit:{size_t(0),size_t(11)}){
  auto small=ready(rush_seed,0);small.config.max_enemies=limit;tick(small);
  assert(small.special_event.kind==SpecialEventKind::None&&small.metrics.events_started==0);
 }
 // Exactly 12 free slots start the full challenge, never a zero-target reward.
 auto g=ready(rush_seed,488);tick(g);
 assert(g.special_event.kind==SpecialEventKind::MushroomRush&&g.special_event.target==12);
 assert(g.special_event.spawned==0&&g.special_event.progress==0&&g.metrics.events_started==1);
 tick(g);assert(g.special_event.spawned==4&&g.enemies.size()==492);
 g.config.elite_interval=1;
 for(int i=0;i<115;++i)tick(g); // Normal and periodic elite spawns both respect the reservation.
 assert(g.special_event.spawned==4&&g.enemies.size()==492);
 for(int i=0;i<300;++i)tick(g);
 assert(g.special_event.spawned==12&&g.special_event.guardians.size()==12&&g.enemies.size()==500);
 assert(g.special_event.progress==0&&g.metrics.events_succeeded==0&&g.items.empty());
 for(uint32_t id=10000;id<10488;++id)assert(has(g,id));
 std::set<uint32_t> marked(g.special_event.guardians.begin(),g.special_event.guardians.end());
 assert(marked.size()==12);for(auto id:marked)assert(has(g,id));
 // Failed optional spawns do not count. Unmarked kills do not count either.
 g.special_event.tick=0;tick(g);assert(g.special_event.spawned==12&&g.special_event.progress==0);
 g.enemies.front().kind=EnemyKind::Mushroom;kill(g,10000);tick(g);
 assert(g.special_event.progress==0&&g.metrics.events_succeeded==0);
 int killed=0;for(auto id:marked)if(++killed<=11)kill(g,id);
 tick(g);assert(g.special_event.kind==SpecialEventKind::MushroomRush&&g.special_event.progress==11);
 assert(g.metrics.events_succeeded==0&&g.items.empty());
 kill(g,*marked.rbegin());tick(g);
 assert(g.special_event.kind==SpecialEventKind::None&&g.metrics.events_succeeded==1);
 assert(g.items.size()==3&&g.kills==13);

 // Splitting enemies can use surplus capacity but cannot consume reserved event slots.
 auto split=ready(rush_seed,488);tick(split);tick(split);
 split.enemies.front().kind=EnemyKind::Splitter;kill(split,10000);tick(split);
 assert(split.enemies.size()==492&&split.special_event.spawned==4&&split.special_event.progress==0);
 assert(std::count_if(split.enemies.begin(),split.enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Minion;})==1);

 // Existing guarded events retain their own capacity checks and all real guardian IDs.
 for(auto kind:{SpecialEventKind::GuardedChest,SpecialEventKind::BountyElite,SpecialEventKind::EliteHunt}){
  const auto seed=seed_for(kind);auto full=ready(seed,500);tick(full);
  assert(full.special_event.kind==SpecialEventKind::None&&full.metrics.events_started==0);
  size_t count=kind==SpecialEventKind::BountyElite?1:3;
  auto enough=ready(seed,500-count);tick(enough);
  assert(enough.special_event.kind==kind&&enough.special_event.guardians.size()==count);
  assert(enough.enemies.size()==500&&enough.metrics.events_succeeded==0);
  for(auto id:enough.special_event.guardians)assert(has(enough,id));
  tick(enough);assert(enough.special_event.kind==kind&&enough.metrics.events_succeeded==0);
 }
 // Boss spawning is not blocked by reserved event slots, and still works at the hard cap.
 auto boss=ready(rush_seed,488);tick(boss);tick(boss);
 auto guardian_ids=boss.special_event.guardians;
 boss.config.boss_time=boss.elapsed+.005f;tick(boss);
 assert(boss.boss_spawned&&boss.metrics.boss_spawn_time>=0&&boss.enemies.size()==493);
 for(auto id:guardian_ids)assert(has(boss,id));
 assert(std::count_if(boss.enemies.begin(),boss.enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss;})==1);
 auto full_boss=ready(rush_seed,500);full_boss.next_event_in=1e6f;
 full_boss.config.boss_time=full_boss.elapsed+.005f;tick(full_boss);
 assert(full_boss.boss_spawned&&full_boss.enemies.size()==500);
 assert(std::count_if(full_boss.enemies.begin(),full_boss.enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss;})==1);

 std::cout<<"PASS event capacity: full 500/insufficient delay, reserved 12 real targets, no free reward, only marked kills, guarded objectives, Boss priority\n";
}

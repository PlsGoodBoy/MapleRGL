#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <set>
using namespace maple;
int main(){
 // Bounded recipe metadata check; no complete run per route.
 assert(fusion_recipes().size()==48);std::set<int> results;int cross=0;std::array<int,3> own{};
 for(const auto& recipe:fusion_recipes()){
  assert(results.insert(int(recipe.result)).second&&recipe.first!=recipe.second);
  assert(!Game::is_fusion(recipe.first)&&!Game::is_fusion(recipe.second));
  assert(recipe.released&&recipe.weight>0&&recipe.form_mask==uint32_t((1<<Game::form_count(recipe.result))-1));
  assert(Game::skill_owner(recipe.first)==recipe.character||int(Game::skill_owner(recipe.first))==recipe.other_class);
  assert(Game::skill_owner(recipe.second)==recipe.character||int(Game::skill_owner(recipe.second))==recipe.other_class);
  if(recipe.other_class>=0)++cross;else ++own[int(recipe.character)];
 }assert(cross==18&&own[0]==10&&own[1]==10&&own[2]==10);
 // Each fresh class has its twelve standard basics. Existing extra skills remain upgradeable.
 for(auto character:{CharacterClass::Warrior,CharacterClass::Mage,CharacterClass::Archer}){
  Game game;game.start(character,20261111);int available=0;
  for(int id=0;id<skill_count;++id)if(!Game::is_fusion(Skill(id))&&game.can_gain_skill(Skill(id)))++available;
  assert(available==12);
  Skill legacy=character==CharacterClass::Warrior?Skill::HeavyStrike:character==CharacterClass::Mage?Skill::OrbitalOrb:Skill::HunterMark;
  assert(!game.can_gain_skill(legacy));game.skills[int(legacy)]=1;assert(game.can_gain_skill(legacy));
 }
 // Charge must wait for a real player dash; its hit follows the actual movement segment.
 {Config config;config.max_enemies=0;config.first_event_time=config.boss_time=10000;Game game(config);game.start(CharacterClass::Warrior,20261111);game.skills.fill(0);game.skills[int(Skill::Charge)]=1;
  Enemy enemy;enemy.id=500;enemy.hp=enemy.max_hp=1000;enemy.pos={65,0};enemy.speed=enemy.damage=0;game.enemies.push_back(enemy);Vec2 origin=game.player.pos;
  for(int frame=0;frame<60;++frame)game.update(1.f/60);
  assert(length(game.player.pos-origin)<.01f&&game.metrics.skill_damage[int(Skill::Charge)]==0);
  game.update(1.f/60,{{1,0},true});for(int frame=0;frame<12;++frame)game.update(1.f/60);
  assert(game.metrics.dash_uses==1&&game.metrics.skill_damage[int(Skill::Charge)]>0);
 }
 // Directed functional fixtures, not a natural-run or balance claim.
 for(const auto& recipe:fusion_recipes())for(int form=0;form<Game::form_count(recipe.result);++form){
  Config config;config.first_event_time=10000;config.boss_time=10000;
  Game game(config);game.start(recipe.character,20261111);game.skills.fill(0);
  game.skills[int(recipe.first)]=3;game.skills[int(recipe.second)]=3;
  assert(!game.fusion_ready(recipe.result));
  game.scrolls.offers={{recipe.result,form}};game.scrolls.eligible_rewards=game.scrolls.opportunities=1;game.scrolls.first_opportunity_time=0;game.state=State::ScrollChoice;assert(game.choose_scroll(0));assert(game.fusion_ready(recipe.result));
  Projectile old;old.skill=recipe.first;old.lifetime=100;game.projectiles.push_back(old);
  Effect old_field;old_field.source=recipe.second;old_field.kind=EffectKind::Zone;old_field.duration=100;game.effects.push_back(old_field);
  const uint32_t scroll_id=game.held_scroll(recipe.result)->id;game.toggle_pause();assert(game.state==State::Paused&&game.confirm_scroll_fusion(scroll_id));game.toggle_pause();
  assert(game.active_skill_count()==1&&game.skills[int(recipe.first)]==0&&game.skills[int(recipe.second)]==0&&game.skills[int(recipe.result)]==1);
  assert(!game.can_gain_skill(recipe.first)&&!game.can_gain_skill(recipe.second)&&!game.fusion_ready(recipe.result));
  for(const auto& p:game.projectiles)assert(p.skill!=recipe.first&&p.skill!=recipe.second);
  for(const auto& e:game.effects)assert(e.source!=recipe.first&&e.source!=recipe.second);
  assert(game.fusion_form(recipe.result)==form&&game.metrics.fusions_created==1&&game.metrics.first_fusion_time>=0);
  while(game.state==State::Upgrade){size_t offered=0;for(size_t i=0;i<game.choices.size();++i)if(game.choices[i].kind!=UpgradeKind::Skill){offered=i;break;}assert(game.choose_upgrade(offered));}
  game.enemies.clear();Enemy enemy;enemy.id=500;enemy.hp=enemy.max_hp=10000;enemy.pos={65,0};enemy.speed=0;enemy.damage=0;game.enemies.push_back(enemy);
  if(recipe.result==Skill::DragonArmor)game.dragon_soul=100;
  if(recipe.result==Skill::FrostArmor){game.enemies[0].pos=game.player.pos;game.enemies[0].damage=1;game.enemies[0].radius=50;}
  const Vec2 before_dash=game.player.pos;
  // Twelve seconds gives charging, delayed traps and complete attack sequences one normal cycle.
  for(int step=0;step<720&&game.state==State::Playing;++step){
   Input input;
   if((recipe.result==Skill::DashBreaker||recipe.result==Skill::MountainBreaker||recipe.result==Skill::PhoenixAssault)&&step==12){assert(length(game.player.pos-before_dash)<.01f);input.move={1,0};input.dash=true;}
   game.update(1.f/60,input);
  }
  if(game.metrics.skill_damage[int(recipe.result)]<=0)std::cerr<<"No damage: "<<Game::skill_key(recipe.result)<<"\n";
  assert(game.metrics.skill_damage[int(recipe.result)]>0);
  assert(game.projectiles.size()<=config.max_projectiles&&game.effects.size()<=config.max_effects);
  game.start(recipe.character,4);assert(game.consumed_skills==0&&game.dragon_soul==0&&game.metrics.fusions_created==0);
 }
 std::cout<<"PASS fusion: scroll-gated routes, atomic two-to-one replacement, cleanup, consumed pool exclusion, distinct damage and restart\n";
}

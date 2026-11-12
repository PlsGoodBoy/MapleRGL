#include "encounter_pool.hpp"
#include "boss_combat.hpp"
#include <algorithm>
#include <random>

namespace maple {
namespace {
constexpr uint32_t role(EnemyKind kind){return 1u<<int(kind);}
constexpr uint32_t basic=role(EnemyKind::Slime)|role(EnemyKind::Mushroom);
constexpr uint32_t heavy=role(EnemyKind::BlueMushroom)|role(EnemyKind::Elite);
constexpr uint32_t rush=role(EnemyKind::Pig),ranged=role(EnemyKind::Ranger);
constexpr uint32_t split=role(EnemyKind::Splitter),support=role(EnemyKind::Support);
constexpr uint32_t ground=role(EnemyKind::GroundController),charger=role(EnemyKind::WindupCharger);
template<class Weight> size_t weighted_index(size_t count,std::mt19937& rng,Weight weight){
 int total=0;for(size_t i=0;i<count;++i)total+=std::max(1,weight(i));
 int draw=int(rng()%uint32_t(total));for(size_t i=0;i<count;++i){draw-=std::max(1,weight(i));if(draw<0)return i;}return count-1;
}
}
const std::array<BossDefinition,boss_profile_count>& boss_definitions(){
 // Requirements: 1=limited summons, 2=ground warnings, 4=projectiles.
 // Exclusion 1 keeps the primary boss separate from high-pressure encounters.
 static const std::array<BossDefinition,boss_profile_count> values={{
  {BossProfile::MushroomKing,"mushroom_king","boss","蘑菇王","spores_charge",0,1,15,7,7,100,8,1,3,1,true,1.50f,1.10f,50,32,1.f,1u<<1},
  {BossProfile::ForestGuardian,"forest_guardian","boss_stumpy","森林守卫","roots_seed_fans",1,2,15,7,7,100,10,1,7,1,true,1.f,1.80f,46,29,1.04f,1u<<6},
  {BossProfile::KingSlime,"king_slime","boss_king_slime","超级绿水灵","leap_split",0,1,15,7,7,100,8,1,3,1,true,1.05f,.92f,48,35,.95f,1u<<0},
  {BossProfile::SnailKing,"snail_king","boss_snail_king","红蜗牛王","shell_spiral",0,1,15,7,7,100,14,1,4,1,true,1.50f,1.65f,40,23,1.02f},
  {BossProfile::Golem,"golem","boss_golem","巨岩统领","fist_fissures",2,2,15,7,7,100,10,1,2,1,true,1.15f,1.26f,48,24,1.12f},
  {BossProfile::Balrog,"balrog","boss_balrog","蝙蝠魔","dive_crossfire",2,3,15,7,7,100,14,1,6,1,true,.60f,1.44f,32,48,.92f},
  {BossProfile::Snowman,"snowman","boss_snowman","驮狼雪人王","snowballs_ice_slams",0,2,15,7,7,100,12,1,6,1,true,.95f,2.10f,44,27,1.05f},
  {BossProfile::Pirate,"pirate","boss_pirate","海盗船长","cannon_reposition",0,2,15,7,7,100,12,1,4,1,true,1.f,1.92f,42,31,1.f},
  {BossProfile::Clockwork,"clockwork","boss_clockwork","玩具机械统领","beam_clock_grid",2,3,15,7,7,90,16,1,2,1,true,.65f,1.13f,42,23,1.08f},
  {BossProfile::Leviathan,"leviathan","boss_leviathan","苍海龙王","breath_sweep",1,3,15,7,7,90,14,1,6,1,true,.75f,1.62f,40,39,1.03f},
  {BossProfile::Cactus,"cactus","boss_cactus","沙漠仙人掌王","sand_spines_dash",2,1,15,7,7,100,12,1,2,1,true,1.15f,1.20f,39,26,.97f},
  {BossProfile::Faust,"faust","boss_faust","浮士德","hex_ghosts",1,2,15,7,7,100,14,4,7,1,true,.70f,1.95f,32,30,.98f,1u<<14},
#include "adventure_boss_rows.inc"
 }};return values;
}
const BossDefinition& boss_definition(BossProfile profile){return boss_definitions()[std::clamp(int(profile),0,boss_profile_count-1)];}
const std::array<EnemyDefinition,enemy_visual_count>& enemy_definitions(){
 static const std::array<EnemyDefinition,enemy_visual_count> values={{
  {0,"enemy.slime","绿水灵",0,basic|split|role(EnemyKind::Minion),.85f,16,.84f},
  {1,"enemy.mushroom","蘑菇仔",0,basic,1.20f,18,.66f},
  {2,"enemy.blue_mushroom","蓝蘑菇",0,heavy|split,.85f,22,.66f},
  {3,"enemy.pig","小猪",0,rush,.90f,16,.66f},
  {4,"enemy.wild_boar","野猪",0,rush|role(EnemyKind::Elite),1.05f,25,.48f},
  {5,"enemy.snail","蜗牛",0,basic,1.25f,14,1.26f},
  {6,"enemy.stump","木妖",1,basic|heavy,.95f,20,.66f},
  {7,"enemy.octopus","章鱼",1,ranged|split,.90f,21,.84f},
  {8,"enemy.snake","青蛇",1,rush|role(EnemyKind::Minion),1.35f,13,.95f},
  {9,"enemy.lupin","猴子",1,ranged|rush,1.f,18,.63f},
  {10,"enemy.wood_mask","木面怪",1,heavy|support,.80f,22,1.07f},
  {11,"enemy.stone_mask","石面怪",2,heavy,.90f,23,1.07f},
  {12,"enemy.skeleton","骷髅",2,basic|heavy|ranged,.85f,21,1.05f},
  {13,"enemy.scorpion","沙漠蝎",2,rush|split|role(EnemyKind::Minion),.95f,21,1.35f},
  {14,"enemy.ghost","幽灵",1,basic|split|support,1.10f,15,.95f},
  {15,"enemy.fairy","精灵",1,support,1.f,15,1.02f},
  {16,"enemy.crocodile","鳄鱼",1,rush,1.f,22,.48f},
  {17,"enemy.toy_mouse","玩具黑鼠",0,rush|basic,1.1f,18,.90f},
  {18,"enemy.robot","洛伊德",2,heavy,1.1f,17,1.52f},
  {19,"enemy.yeti","小白雪人",0,basic,1.2f,18,.81f},
  {20,"enemy.moon_pixie","月光精灵",1,support|ranged,1.f,16,.82f},
  {21,"enemy.drake","青龙",2,charger|rush,.85f,27,1.35f},
  {22,"enemy.zombie","僵尸",2,ground|basic,.9f,20,1.30f},
  {23,"enemy.panda","柔道猫熊",1,charger,1.f,24,1.42f},
  {24,"enemy.sand_giant","沙漠巨人",2,heavy,.85f,27,1.65f},
  {25,"enemy.stone_bug","石头虫",2,split,.8f,26,1.80f},
  {26,"enemy.skeleton_dog","骷髅犬",2,rush|role(EnemyKind::Minion),1.f,18,1.20f},
  {27,"enemy.hedgehog","豪猪",1,ground|basic,1.1f,20,.78f},
  {28,"enemy.bubble_fish","吹泡泡双鱼",0,ranged,1.15f,18,.95f},
  {29,"enemy.pirate_crew","海盗克鲁",0,ranged,.8f,25,1.34f},
#include "adventure_enemy_rows.inc"
 }};return values;
}
const EnemyDefinition& enemy_definition(int id){return enemy_definitions()[std::clamp(id,0,enemy_visual_count-1)];}
std::vector<BossProfile> legal_boss_pool(const Game& game){
 std::vector<BossProfile> pool;
 const int chapter=std::clamp(game.chapter_index(),0,2);
 const int threat=game.run_mode()==RunMode::Campaign?chapter+1:3;
 for(const auto& boss:boss_definitions()){
  if(!boss.released||!(game.config.available_boss_mask&(1u<<int(boss.id)))||boss.threat>threat)continue;
  if(boss.required_enemy_mask&~game.config.available_enemy_mask)continue;
  if(!(boss.mode_mask&(1u<<int(game.run_mode())))||!(boss.chapter_mask&(1u<<chapter))||!(boss.difficulty_mask&(1u<<int(game.difficulty()))))continue;
  const int node=boss_combat_definition(boss.id).node_visual;
  if(node>=0&&!(game.config.available_enemy_mask&(uint64_t(1)<<node)))continue;
  if(game.config.max_effects<std::max(size_t(24),size_t(boss.min_effect_slots))||game.config.max_enemies<std::max(size_t(node>=0?4:1),size_t(boss.min_enemy_slots)))continue;
  pool.push_back(boss.id);
 }return pool;
}
void Game::prepare_encounter_plan(){
 const int epoch=run_mode_==RunMode::Endless?metrics.endless_cycle:chapter_index_;
 if(encounter_plan.epoch==epoch&&encounter_plan.boss>=0&&!encounter_plan.roster.empty())return;
 const int previous=encounter_plan.boss;const uint32_t used=encounter_plan.used_boss_mask;
 encounter_plan={};encounter_plan.epoch=epoch;encounter_plan.region=run_mode_==RunMode::Classic?int((run_seed_^0x51f47u)%6):std::clamp(region_index(),0,2);encounter_plan.used_boss_mask=used;
 std::mt19937 content_rng(run_seed_^0xa6b37e21u^(uint32_t(epoch+1)*0x9e3779b9u)^(uint32_t(run_mode_)*0x85ebca6bu));
 auto pool=legal_boss_pool(*this);
 // First prefer an unvisited legal boss. Once exhausted, relax once, never reroll.
 const bool fresh=std::any_of(pool.begin(),pool.end(),[&](BossProfile id){return !(used&(1u<<int(id)));});
 if(fresh)pool.erase(std::remove_if(pool.begin(),pool.end(),[&](BossProfile id){return (used&(1u<<int(id)))!=0;}),pool.end());
 if(!pool.empty()){
  const size_t index=weighted_index(pool.size(),content_rng,[&](size_t i){const auto& boss=boss_definition(pool[i]);int weight=boss.weight*(boss.theme==encounter_plan.region?3:1);if(pool.size()>1&&int(boss.id)==previous)weight=std::max(1,weight/4);return weight;});
  encounter_plan.boss=int(pool[index]);
 }
 auto add_role=[&](EnemyKind kind){
  std::vector<int> candidates;for(const auto& enemy:enemy_definitions())if(enemy.released&&(config.available_enemy_mask&(uint64_t(1)<<enemy.id))&&(enemy.roles&role(kind))&&std::find(encounter_plan.roster.begin(),encounter_plan.roster.end(),enemy.id)==encounter_plan.roster.end())candidates.push_back(enemy.id);
  if(candidates.empty())return;
  const size_t index=weighted_index(candidates.size(),content_rng,[&](size_t i){return enemy_definition(candidates[i]).theme==encounter_plan.region?5:1;});
  encounter_plan.roster.push_back(candidates[index]);
 };
 // The ordinary swarm remains the foundation; special roles are chosen once per chapter.
 for(EnemyKind kind:{EnemyKind::Slime,EnemyKind::Mushroom,EnemyKind::BlueMushroom,EnemyKind::Pig,EnemyKind::Ranger,EnemyKind::Support,EnemyKind::Splitter,EnemyKind::GroundController,EnemyKind::WindupCharger,EnemyKind::Slime})add_role(kind);
 if(run_mode_==RunMode::Classic){const int first_theme=encounter_plan.region;encounter_plan.region=(first_theme+1+int(run_seed_%3))%6;for(EnemyKind kind:{EnemyKind::Slime,EnemyKind::Pig,EnemyKind::BlueMushroom,EnemyKind::Ranger,EnemyKind::Support,EnemyKind::Splitter,EnemyKind::GroundController,EnemyKind::WindupCharger,EnemyKind::Elite,EnemyKind::Slime})add_role(kind);encounter_plan.region=first_theme;}
}
BossProfile Game::planned_boss_profile(int encounter) const {
 const auto final=static_cast<BossProfile>(encounter_plan.boss);
 if(scheduled_boss_count()==1||encounter>=scheduled_boss_count()-1)return final;
 auto pool=legal_boss_pool(*this);pool.erase(std::remove(pool.begin(),pool.end(),final),pool.end());
 std::mt19937 random(run_seed_^0x49f27a31u^(uint32_t(encounter_plan.epoch+1)*0x9e3779b9u));
 BossProfile selected=final;
 for(int stage=0;stage<=encounter&&!pool.empty();++stage){
  auto candidates=pool;candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](BossProfile id){return boss_definition(id).threat>stage+1;}),candidates.end());
  if(candidates.empty())candidates=pool;
  selected=candidates[weighted_index(candidates.size(),random,[&](size_t i){const auto& boss=boss_definition(candidates[i]);return boss.weight*(boss.theme==encounter_plan.region?3:1);})];
  pool.erase(std::remove(pool.begin(),pool.end(),selected),pool.end());
 }
 return selected;
}
BossProfile Game::next_boss_profile() const {return planned_boss_profile(boss_encounters_spawned_);}
int Game::select_enemy_visual(EnemyKind kind,bool event_target){
 if(event_target&&kind==EnemyKind::Mushroom){for(int id:{1,2})if(config.available_enemy_mask&(uint64_t(1)<<id))return id;return -1;}
 if(event_target&&kind==EnemyKind::BlueMushroom&&(config.available_enemy_mask&(1u<<2)))return 2;
 std::vector<int> candidates;
 for(size_t slot=0;slot<encounter_plan.roster.size();++slot){if(run_mode_==RunMode::Classic&&elapsed<600&&slot>=10)continue;int id=encounter_plan.roster[slot];if((config.available_enemy_mask&(uint64_t(1)<<id))&&(enemy_definition(id).roles&role(kind)))candidates.push_back(id);}
 // Boss minions and temporary event guests may use a legal species outside the chapter roster.
 if(candidates.empty())for(const auto& enemy:enemy_definitions())if(enemy.released&&(config.available_enemy_mask&(uint64_t(1)<<enemy.id))&&(enemy.roles&role(kind)))candidates.push_back(enemy.id);
 if(elapsed>=600&&run_mode_==RunMode::Classic)for(size_t slot=10;slot<encounter_plan.roster.size();++slot){int id=encounter_plan.roster[slot];if(std::find(candidates.begin(),candidates.end(),id)!=candidates.end())candidates.push_back(id);}
 return candidates.empty()?-1:candidates[rng_()%candidates.size()];
}
}

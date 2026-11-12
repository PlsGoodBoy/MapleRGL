#pragma once
#include "game.hpp"

namespace maple {
struct BossDefinition {
 BossProfile id;
 const char* key;
 const char* sprite;
 const char* name;
 const char* behavior;
 int theme=0,threat=1;
 uint32_t mode_mask=15,chapter_mask=7,difficulty_mask=7;
 int weight=100,min_effect_slots=8,min_enemy_slots=1;
 uint32_t requirements=0,exclusions=1;
 bool released=true;
 float scale=1,death_duration=1,radius=40,speed=30,health_scale=1;
 uint32_t required_enemy_mask=0;
};
struct EnemyDefinition {
 int id;
 const char* key;
 const char* name;
 int theme;
 uint32_t roles;
 float scale,radius,death_duration;
 bool released=true;
};
const std::array<BossDefinition,boss_profile_count>& boss_definitions();
const BossDefinition& boss_definition(BossProfile profile);
const std::array<EnemyDefinition,enemy_visual_count>& enemy_definitions();
const EnemyDefinition& enemy_definition(int id);
std::vector<BossProfile> legal_boss_pool(const Game& game);
}

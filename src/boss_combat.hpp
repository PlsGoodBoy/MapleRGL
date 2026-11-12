#pragma once
#include "game.hpp"

namespace maple {
struct BossMove {const char* key;const char* name;float windup,recovery;};
struct BossCombatDefinition {
 std::array<BossMove,3> moves;
 const char* mechanism;
 const char* counterplay;
 const char* node_name;
 int node_visual;
 float phase_two,phase_three;
};
const BossCombatDefinition& boss_combat_definition(BossProfile profile);
constexpr int boss_move_id(BossProfile profile,int move){return 200+int(profile)*3+move;}
// The same finite geometry drives warnings, impacts and collision. End is an
// absolute endpoint for capsules and a direction for sectors.
bool hostile_shape_contains(const Effect& effect,Vec2 position,float body_radius=12);
}

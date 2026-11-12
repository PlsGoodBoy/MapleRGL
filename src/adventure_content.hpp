#pragma once
#include "game.hpp"
namespace maple {
struct AdventureSkill {Skill id;const char* key;const char* name;const char* description;float interval;uint32_t roles;int style;const char* icon;};
const AdventureSkill* adventure_skill(Skill skill);
void append_adventure_recipes(std::vector<FusionRecipe>& recipes);
std::vector<Skill> fusion_inputs(const FusionRecipe& recipe);
const FusionRecipe* next_fusion(const std::array<int,skill_count>& levels);
std::vector<const FusionRecipe*> fusion_plan(std::array<int,skill_count> levels);
int fusion_rank(const FusionRecipe& recipe);
}

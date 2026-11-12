#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace maple {
// Camp equipment remains a separate departure source. These stacks last for one adventure.
struct GearStack {std::string id;int level=1;uint64_t count=1;};
struct GearDefinition {const char* id;const char* name;const char* description;int group,rarity,stat;uint32_t role;float amount;};
const std::vector<GearDefinition>& gear_definitions();
const GearDefinition* gear_definition(const std::string& id);
float gear_contribution(uint64_t count,int level,float amount);
}

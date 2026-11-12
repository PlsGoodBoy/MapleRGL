#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace maple {
// A chapter/Endless-cycle choice is made once, then retained across UI changes.
struct EncounterPlan {
 int version=1,epoch=-1,region=0,boss=-1;
 uint32_t used_boss_mask=0;
 std::vector<int> roster;
};
// Content IDs are append-only, separate from the broad EnemyKind behavior roles.
struct ContentStatistics {
 uint32_t bosses_seen=0;uint64_t enemies_seen=0;
 std::array<int,32> boss_kills{};std::array<int,64> enemy_kills{};
 std::array<int,32> boss_encounters{},boss_moves_seen{};
};
struct RunTiming {
 int rules_version=1;
 int speed=1; // Encounter pace only; never a combat timescale.
 uint32_t used_speeds=0; // bit 0: standard; bit 1: compact; both: mixed pace.
 double combat_seconds=0; // Simulated combat, excluding menus, choices and pauses.
 double effective_real_seconds=0,total_real_seconds=0;
 bool recorded=false; // Old saves must not invent real elapsed time.
};
}

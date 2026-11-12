#include "review_controller.hpp"
#include "../third_party/json.hpp"
#include <fstream>
#include <iostream>

using namespace maple;
int main(int argc,char** argv){
 if(argc!=2){std::cerr<<"review_combat OUTPUT.json\n";return 2;}
 nlohmann::json report={{"input","automated legal inputs; no HP, RNG, build or reward edits"},{"rendered",false},{"seeds",{20261112,20261113,20261114}},{"runs",nlohmann::json::array()}};
 for(uint32_t seed:{20261112u,20261113u,20261114u}){
  Config config;config.enable_director=config.enable_scene_objects=config.enable_boss_phases=true;
  Game game(config);RunSetup setup;setup.enable_loot=true;setup.unlocked_relic_mask=initial_relic_mask();setup.equipment={EquipmentId::TravelerKnife,EquipmentId::LeatherVest,EquipmentId::AmberPendant};
  game.start(CharacterClass::Mage,seed,{},setup);game.timing.speed=2;game.set_combat_view(600,337.5f);ReviewController bot;
  for(int step=0;step<80000&&game.state!=State::Victory&&game.state!=State::Defeat;++step){
   if(game.pending_study.active){auto replacement=bot.replacement(game);if(replacement!=Skill::Count){game.select_study_replacement(replacement);game.confirm_study_replacement();}else game.cancel_study_replacement();}
   else if(game.state==State::Upgrade){int choice=bot.choice_index(game);if(choice>=0)game.choose_upgrade(size_t(choice));else game.skip_upgrade();}
   else if(game.state==State::Playing)game.update(1.f/60,bot.input(game,1.f/60));
   else break;
  }
  const char* outcome=game.state==State::Victory?"victory":game.state==State::Defeat?"defeat":"unfinished";
  report["runs"].push_back({{"seed",seed},{"outcome",outcome},{"progress_seconds",game.elapsed},{"combat_seconds",game.combat_elapsed},{"level",game.player.level},{"hp",game.player.hp},{"kills",game.kills},{"bosses_defeated",game.metrics.bosses_defeated},{"boss_encounters",game.metrics.content.boss_encounters},{"boss_moves_seen",game.metrics.content.boss_moves_seen},{"final_skills",game.skills},{"fusions",game.metrics.fusions_created},{"gear_count",game.gear_count()},{"damage_taken",game.metrics.damage_taken},{"last_damage",game.pressure_metrics.death_cause},{"nodes_destroyed",game.combat_diagnostics.boss_nodes_destroyed},{"spawn_capacity_limited",game.pressure_metrics.capacity_limited}});
  std::cout<<seed<<" "<<outcome<<" progress="<<game.elapsed<<" combat="<<game.combat_elapsed<<" bosses="<<game.metrics.bosses_defeated<<std::endl;
 }
 std::ofstream output(argv[1]);output<<report.dump(2)<<'\n';return output?0:1;
}

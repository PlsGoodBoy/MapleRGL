// Six fixed routes, two fixed target layouts. A bounded damage/coverage diagnostic, not a win-rate test.
#include "game.hpp"
#include "../third_party/json.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
using namespace maple;
using nlohmann::json;
json measure(const FusionRecipe& recipe,bool fused,bool crowd){
 Config config;config.enable_pressure=false;config.max_enemies=1;config.boss_time=config.first_event_time=10000;config.item_drop_chance=0;
 Game game(config);game.start(CharacterClass::Mage,20261112);game.skills.fill(0);game.player.invulnerable=10000;
 if(fused){game.scrolls.held={{1,recipe.result,0,false}};game.scrolls.next_id=2;game.skills[int(recipe.first)]=game.skills[int(recipe.second)]=2;game.confirm_scroll_fusion(1);}
 else game.skills[int(recipe.first)]=game.skills[int(recipe.second)]=2;
 const int count=crowd?18:1;for(int i=0;i<count;++i){Enemy e;e.id=100+i;e.kind=crowd?EnemyKind::Mushroom:EnemyKind::Boss;e.hp=e.max_hp=100000;e.radius=crowd?18.f:45.f;e.attack_clock=e.attack_cooldown=10000;e.speed=crowd?12.f:0.f;float angle=i*6.2831853f/count,radius=90.f+(i%3)*75;e.pos=crowd?Vec2{std::cos(angle)*radius,std::sin(angle)*radius}:Vec2{180,0};game.enemies.push_back(e);}
 size_t peak_effects=0,peak_projectiles=0;for(int frame=0;frame<1440;++frame){game.update(1.f/60);peak_effects=std::max(peak_effects,game.effects.size());peak_projectiles=std::max(peak_projectiles,game.projectiles.size());}
 double damage=0;int touched=0;for(double value:game.metrics.skill_damage)damage+=value;for(const auto& e:game.enemies)if(e.hp<e.max_hp)++touched;
 return {{"damage",damage},{"dps",damage/24},{"targets_hit",touched},{"hits",game.combat_diagnostics.actual_hits},{"peak_effects",peak_effects},{"peak_projectiles",peak_projectiles}};
}
int main(int argc,char** argv){json result={{"fixture","24 combat seconds; two Lv2 materials versus their Lv1 fusion; stationary Boss or 18 approaching enemies; same seed, no upgrades"},{"routes",json::array()}};
 for(Skill selected:{Skill::ArcaneRing,Skill::SwordCyclone,Skill::DragonFormation,Skill::ThunderLance,Skill::TwinHunt,Skill::DragonPhoenix})for(const auto& recipe:fusion_recipes())if(recipe.result==selected){json route={{"skill",Game::skill_key(selected)}};for(bool crowd:{false,true}){auto materials=measure(recipe,false,crowd),fusion=measure(recipe,true,crowd);route[crowd?"crowd":"boss"]={{"materials",materials},{"fusion",fusion},{"damage_ratio",double(fusion["damage"])/std::max(1.,double(materials["damage"]))}};}result["routes"].push_back(route);std::cout<<Game::skill_key(selected)<<" boss="<<route["boss"]["damage_ratio"]<<" crowd="<<route["crowd"]["damage_ratio"]<<"\n";}
 std::ofstream output(argc>1?argv[1]:"docs/build_fusion_comparison.json");output<<result.dump(2);return output?0:1;
}

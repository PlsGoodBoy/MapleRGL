// Pressure: six fixed ordinary runs, a speed replay, and optional replay of one recorded seed.
#define main expansion_balance_reference_main
#include "simulate_balance.cpp"
#undef main
#include "encounter_pool.hpp"
#include "simulation_clock.hpp"

namespace {
Config sample_config(){
 Config config;config.enable_director=config.enable_scene_objects=config.enable_boss_phases=true;
 std::ifstream input("assets/encounter_rules.json");json rules;input>>rules;auto& director=config.director_config;
#define LOAD_DIRECTOR(name) director.name=rules.value(#name,director.name)
 LOAD_DIRECTOR(first_encounter);LOAD_DIRECTOR(encounter_gap_min);LOAD_DIRECTOR(encounter_gap_max);LOAD_DIRECTOR(min_safe_distance);LOAD_DIRECTOR(escape_half_angle);LOAD_DIRECTOR(low_health);LOAD_DIRECTOR(high_pressure);LOAD_DIRECTOR(breather_duration);LOAD_DIRECTOR(boss_telegraph);LOAD_DIRECTOR(max_spawns_per_update);
#undef LOAD_DIRECTOR
 const char* keys[]={"swarm","encirclement","rush","elite_hunt","crisis","breather"};
 for(const auto& entry:rules.at("rules"))for(int i=0;i<6;++i)if(entry.at("kind")==keys[i]){auto& rule=director.rules[i];
#define LOAD_RULE(name) rule.name=entry.value(#name,rule.name)
  LOAD_RULE(weight);LOAD_RULE(unlock_progress);LOAD_RULE(telegraph);LOAD_RULE(duration);LOAD_RULE(cooldown);LOAD_RULE(spawn_interval);LOAD_RULE(base_budget);LOAD_RULE(batch_size);
#undef LOAD_RULE
 }
 return config;
}
const FusionRecipe* sample_recipe(Skill result){for(const auto& recipe:fusion_recipes())if(recipe.result==result)return &recipe;return nullptr;}
json ranks(const Game& game){json out=json::object();for(int i=0;i<skill_count;++i)if(game.skills[i])out[Game::skill_key(Skill(i))]=game.skills[i];return out;}
int needed_rank(const Game& game,Skill skill){int needed=0;for(const auto& scroll:game.scrolls.held)if(game.scroll_reachable(scroll.result)){auto recipe=sample_recipe(scroll.result);if(recipe&&(recipe->first==skill||recipe->second==skill))needed=std::max(needed,recipe->material_level);}return needed;}
size_t select_sample_upgrade(const Game& game){
 size_t selected=0;float best=-1e9f;const float health=game.player.hp/std::max(1.f,game.player.max_hp);
 for(size_t i=0;i<game.choices.size();++i){const auto& choice=game.choices[i];float value=30;
  if(choice.kind==UpgradeKind::Study)value=170;
  else if(choice.kind==UpgradeKind::Skill){int rank=game.skills[int(choice.skill)],need=needed_rank(game,choice.skill);value=need>rank?180.f:Game::is_fusion(choice.skill)?130.f:rank==0?83.f:92.f+(rank==4?15.f:0.f);}
  else if(choice.kind==UpgradeKind::Relic)value=85;
  else{const int id=int(choice.kind);const int rank=id>=0&&id<passive_count?game.passives[id]:0;value=50-rank*3.f;
   if(choice.kind==UpgradeKind::Damage||choice.kind==UpgradeKind::AttackSpeed||choice.kind==UpgradeKind::Cooldown)value+=34;
   if(choice.kind==UpgradeKind::Area)value+=game.character==CharacterClass::Warrior?30.f:12.f;
   if(choice.kind==UpgradeKind::Magnet&&rank<2)value+=43;
   if(choice.kind==UpgradeKind::MoveSpeed&&rank<2)value+=25;
   if(choice.kind==UpgradeKind::Projectiles)value+=game.character==CharacterClass::Archer?27.f:8.f;
   if(choice.kind==UpgradeKind::Health||choice.kind==UpgradeKind::Recover||choice.kind==UpgradeKind::Vitality)value=health<.35f?500.f:health<.65f?180.f:45.f;
  }
  if(value>best){best=value;selected=i;}
 }
 return selected;
}
Skill select_sample_replacement(const Game& game){
 const auto& pending=game.pending_study;const FusionRecipe* current=nullptr;for(const auto& scroll:game.scrolls.held)if(scroll.id==pending.scroll_id)current=sample_recipe(scroll.result);
 Skill selected=Skill::Count;int best=100000;
 for(int i=0;i<skill_count;++i){const Skill skill=Skill(i);if(game.skills[i]<=0||Game::is_fusion(skill)||(current&&(current->first==skill||current->second==skill)))continue;
  const int cost=game.skills[i]*10+(needed_rank(game,skill)>0?100:0);if(cost<best){best=cost;selected=skill;}
 }
 return selected;
}
json pressure_summary(const Game& game){
 const auto& m=game.pressure_metrics;json sequence=json::array(),abilities=json::object(),casts=json::object();
 for(const auto& event:m.sequence)sequence.push_back({{"event",pressure_definitions()[int(event.kind)-1].key},{"started",event.started},{"ended",event.ended},{"damage_taken",event.damage_taken}});
 for(int i=0;i<enemy_ability_count;++i)abilities[enemy_ability_name(EnemyAbility(i))]=m.abilities[i];
 for(int i=0;i<skill_count;++i)if(m.attempts[i])casts[Game::skill_key(Skill(i))]={{"attempts",m.attempts[i]},{"failures",m.failures[i]},{"protected",m.protected_casts[i]},{"successes",m.successful_casts[i]}};
 return {{"events",sequence},{"assigned_abilities",abilities},{"casts",casts},{"normal_budget",m.normal_spawn_budget},{"event_budget",m.event_spawn_budget},{"requested",m.requested_spawns},{"actual",m.actual_spawns},{"capacity_limited",m.capacity_limited},{"max_enemies",m.max_enemies},{"max_zones",m.max_zones},{"displacements_blocked",m.displacement_blocked},{"promise_triggers",m.promise_triggers},{"refractions",m.refractions},{"death_blasts",m.death_blasts},{"revivals",m.revivals},{"last_damage_source",m.death_cause}};
}
json ordinary_sample(CharacterClass character,uint32_t seed,int policy,int speed=1){
 Game game(sample_config());game.set_combat_view(400,250);game.start(character,seed);
 Controller controller{2,2,seed};Input input{};SimulationClock clock;double effective_real=0;json decisions=json::array(),route_choices=json::array(),fusions=json::array(),replacements=json::array();
 const int selected_boss=game.encounter_plan.boss;const auto roster=game.encounter_plan.roster;
 json last_hurt=nullptr;uint64_t turns=0,steps=0;size_t peak_effects=0,peak_enemies=0,peak_projectiles=0;
 while(game.state!=State::Victory&&game.state!=State::Defeat&&steps<36600&&turns++<38000){
  if(game.state==State::ScrollChoice){
   size_t selected=0;float best=-1e9f;json offered=json::array();
   for(size_t i=0;i<game.scrolls.offers.size();++i){const auto& offer=game.scrolls.offers[i];auto recipe=sample_recipe(offer.result);if(!recipe)continue;
    float value=20.f*((game.skills[int(recipe->first)]>0)+(game.skills[int(recipe->second)]>0));value+=5.f*(game.skills[int(recipe->first)]+game.skills[int(recipe->second)]);value+=(Game::is_cross_fusion(offer.result)==bool(policy)?12.f:0.f);
    offered.push_back({{"result",Game::skill_key(offer.result)},{"form",offer.form}});if(value>best){best=value;selected=i;}
   }
   if(game.scrolls.offers.empty())throw std::runtime_error("Scroll choice had no actual routes");auto chosen=game.scrolls.offers[selected];
   if(!game.choose_scroll(selected))throw std::runtime_error("Offered route could not be selected");
   route_choices.push_back({{"game_seconds",game.elapsed},{"offered",offered},{"chosen",Game::skill_key(chosen.result)},{"form",chosen.form},{"foreign_class",game.scrolls.foreign_class}});continue;
  }
  if(game.state==State::Upgrade){
   if(game.pending_study.active){Skill replace=select_sample_replacement(game);auto pending=game.pending_study;
    if(replace==Skill::Count||!game.select_study_replacement(replace))throw std::runtime_error("Reachable study has no replacement");const int lost=game.skills[int(replace)];
    if(!game.confirm_study_replacement())throw std::runtime_error("Study confirmation failed");
    replacements.push_back({{"game_seconds",game.elapsed},{"lost",Game::skill_key(replace)},{"lost_rank",lost},{"learned",Game::skill_key(pending.learn)}});continue;
   }
   if(game.choices.empty())throw std::runtime_error("Upgrade has no choices");const size_t index=select_sample_upgrade(game);const auto choice=game.choices[index];
   decisions.push_back({{"game_seconds",game.elapsed},{"kind",int(choice.kind)},{"skill",Game::skill_key(choice.skill)},{"title",choice.title}});
   if(!game.choose_upgrade(index))throw std::runtime_error("Normal offered upgrade failed");continue;
  }
  if(game.state!=State::Playing)break;
  bool fused=false;
  for(const auto& scroll:game.scrolls.held)if(game.fusion_ready(scroll.result)){const auto result=scroll.result;const auto form=scroll.form;
   if(!game.confirm_scroll_fusion(scroll.id))throw std::runtime_error("Ready fusion did not commit");
   fusions.push_back({{"game_seconds",game.elapsed},{"result",Game::skill_key(result)},{"form",form},{"rank",game.skills[int(result)]}});fused=true;break;
  }
  if(fused)continue;
  effective_real+=1.0/60;clock.advance(1.0/60,speed,[&](float dt){
   if(steps%decision_ticks==0)input=controller.steer(game);else{input.dash=false;input.interact=false;}
  const double boss_damage_before=game.metrics.boss_damage_taken,environment_damage_before=game.metrics.environment_damage_taken;game.update(1.f/60,input);++steps;
  for(const auto& event:game.events)if(event.kind==EventKind::Hurt)last_hurt={{"game_seconds",game.elapsed},{"position",{event.pos.x,event.pos.y}},{"hp_after",game.player.hp},{"actual_hit_damage",event.actual_damage},{"cause",game.metrics.boss_damage_taken>boss_damage_before?"boss_attack":game.metrics.environment_damage_taken>environment_damage_before?"environment":"enemy_contact_or_projectile"},{"total_damage_taken",game.metrics.damage_taken},{"boss_damage_taken",game.metrics.boss_damage_taken},{"environment_damage",game.metrics.environment_damage_taken}};
  peak_effects=std::max(peak_effects,game.effects.size());peak_enemies=std::max(peak_enemies,game.enemies.size());peak_projectiles=std::max(peak_projectiles,game.projectiles.size());
   return game.state==State::Playing;
  });
 }
 json incomplete=json::array(),damage=json::object();double fusion_damage=0,total_damage=game.metrics.other_damage;
 for(const auto& scroll:game.scrolls.held)incomplete.push_back({{"result",Game::skill_key(scroll.result)},{"status",game.scroll_status(scroll.id)}});
 for(int i=0;i<skill_count;++i)if(game.metrics.skill_damage[i]>0){damage[Game::skill_key(Skill(i))]=game.metrics.skill_damage[i];total_damage+=game.metrics.skill_damage[i];if(Game::is_fusion(Skill(i)))fusion_damage+=game.metrics.skill_damage[i];}
 json bosses=json::array();for(const auto& enemy:game.enemies)if(enemy.kind==EnemyKind::Boss)bosses.push_back({{"id",int(enemy.boss_profile)},{"hp",enemy.hp},{"max_hp",enemy.max_hp},{"stage",enemy.boss_stage}});
 return {{"class",class_names[int(character)]},{"seed",seed},{"route_preference",policy?"cross_if_available":"own_if_available"},{"outcome",game.state==State::Victory?"victory":game.state==State::Defeat?"defeat":"bounded_stop"},
  {"game_seconds",game.elapsed},{"steps",steps},{"speed",speed},{"effective_virtual_real_seconds",steps/60.0/speed},{"clock_input_seconds",effective_real},{"pressure",pressure_summary(game)},{"selected_boss",selected_boss},{"roster",roster},{"boss_spawned",game.boss_spawned},{"boss_defeated",game.boss_defeated},{"boss_spawn_seconds",game.metrics.boss_spawn_time},{"bosses_remaining",bosses},
  {"hp",game.player.hp},{"level",game.player.level},{"kills",game.kills},{"xp",game.metrics.xp_acquired},{"first_scroll_seconds",game.scrolls.first_opportunity_time},{"eligible_rewards",game.scrolls.eligible_rewards},{"scroll_opportunities",game.scrolls.opportunities},
  {"routes",route_choices},{"first_fusion_seconds",game.metrics.first_fusion_time},{"fusions",fusions},{"incomplete_routes",incomplete},{"studies_used",game.scrolls.studies_used},{"replacements",replacements},{"upgrade_decisions",decisions},{"final_build",ranks(game)},
  {"damage_by_skill",damage},{"fusion_damage",fusion_damage},{"fusion_damage_fraction",total_damage>0?fusion_damage/total_damage:0},{"damage_taken",game.metrics.damage_taken},{"boss_damage_taken",game.metrics.boss_damage_taken},{"environment_damage_taken",game.metrics.environment_damage_taken},{"last_hurt",last_hurt},
  {"events_succeeded",game.metrics.events_succeeded},{"events_failed",game.metrics.events_failed},{"peak_enemies",peak_enemies},{"peak_effects",peak_effects},{"peak_projectiles",peak_projectiles}};
}
}
int main(int argc,char** argv){
 try{
  std::string output="docs/pressure_normal_runs.json";
  if(argc==5&&std::string(argv[1])=="--replay-seed"&&std::string(argv[3])=="--output"){
   const uint32_t seed=uint32_t(std::stoul(argv[2]));
   if(seed<20261112||seed>20261117)throw std::runtime_error("Replay accepts only the six recorded seeds");
   const int index=int(seed-20261112);auto replay=ordinary_sample(CharacterClass(index/2),seed,index%2,2);
   std::ofstream file(argv[4]);file<<replay.dump(2);if(!file)throw std::runtime_error("Cannot write replay output");
   std::cout<<"Single 2x replay seed="<<seed<<" outcome="<<replay["outcome"]<<"\n";return 0;
  }
  if(argc==3&&std::string(argv[1])=="--output")output=argv[2];else if(argc!=1)throw std::runtime_error("Usage: simulate_pressure [--output file] | --replay-seed recorded_seed --output file");
  json result={{"fixture",false},{"ordinary_normal_runs",true},{"seed_policy","Six predetermined consecutive seeds, no search or retries"},{"controller","Existing visible-state movement; chooses actual offered scrolls/materials; no combat state injection"},{"limitations","Headless fixed controller, not visual acceptance or an estimate of human win rate"},{"runs",json::array()}};
  const auto start=std::chrono::steady_clock::now();
  for(int cls=0;cls<3;++cls)for(int policy=0;policy<2;++policy){auto run=ordinary_sample(CharacterClass(cls),20261112u+uint32_t(cls*2+policy),policy);std::cout<<run["class"]<<" seed="<<run["seed"]<<" "<<run["outcome"]<<" t="<<run["game_seconds"]<<" fusions="<<run["fusions"].size()<<"\n";result["runs"].push_back(std::move(run));}
  auto replay=ordinary_sample(CharacterClass::Mage,20261114,0,2);auto baseline=result["runs"][2];auto compared=replay;
  baseline.erase("speed");baseline.erase("effective_virtual_real_seconds");baseline.erase("clock_input_seconds");compared.erase("speed");compared.erase("effective_virtual_real_seconds");compared.erase("clock_input_seconds");
  result["speed_comparison"]={{"seed",20261114},{"equal_gameplay_result",baseline==compared},{"one_x_real_seconds",result["runs"][2]["effective_virtual_real_seconds"]},{"two_x_real_seconds",replay["effective_virtual_real_seconds"]},{"two_x",replay}};
  if(baseline!=compared)throw std::runtime_error("1x/2x gameplay results differ");
  result["process_wall_seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::ofstream file(output);file<<result.dump(2);if(!file)throw std::runtime_error("Cannot write output");return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}

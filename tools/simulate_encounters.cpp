// Encounters bounded legal-input sampling. No profile, live save or GUI is opened.
// Reuse Combat's unchanged controller in the same translation unit so its
// movement/skill policy cannot silently drift between the two experiments.
#define main combat_reference_main
#include "simulate_balance.cpp"
#undef main
#include <ctime>
#include <numeric>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
double process_cpu_seconds() {
#ifdef _WIN32
    // MSVC's std::clock measures elapsed wall time. Use actual kernel/user CPU.
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))
        throw std::runtime_error("Cannot measure process CPU time");
    auto seconds=[](FILETIME value){ULARGE_INTEGER ticks{};ticks.LowPart=value.dwLowDateTime;
        ticks.HighPart=value.dwHighDateTime;return double(ticks.QuadPart)*1e-7;};
    return seconds(kernel)+seconds(user);
#else
    return double(std::clock())/CLOCKS_PER_SEC;
#endif
}
RunBonuses capped_talents() {
    // Exact achievable caps in progression.cpp, not arbitrary combat buffs.
    RunBonuses b;
    b.damage_bonus=.10f;b.crit_bonus=.06f;b.health_bonus=20;
    b.move_speed_bonus=.06f;b.xp_bonus=.10f;b.cooldown_reduction=.06f;
    b.upgrade_rerolls=2;b.death_protections=1;b.first_elite_reward=1;
    return b;
}
json bonuses_json_progression(const RunBonuses& b) {
    return {{"damage_bonus",b.damage_bonus},{"crit_bonus",b.crit_bonus},
      {"health_bonus",b.health_bonus},{"move_speed_bonus",b.move_speed_bonus},
      {"xp_bonus",b.xp_bonus},{"cooldown_reduction",b.cooldown_reduction},
      {"upgrade_rerolls",b.upgrade_rerolls},{"death_protections",b.death_protections},
      {"first_elite_reward",b.first_elite_reward}};
}
size_t choose_progression(Game& game, Controller& bot) {
    if (!game.choosing_relic()) return bot.choose(game);
    if (game.choices.empty()) throw std::runtime_error("Empty relic selection");
    float best=-1e30f;size_t selected=game.choices.size();uint32_t offered=0;
    for (size_t i=0;i<game.choices.size();++i) {
        const auto& choice=game.choices[i];const int id=int(choice.relic);
        // With fewer than three remaining relics, core legally fills the menu
        // with these two choices. Prefer the real relic without rejecting them.
        if(choice.kind==UpgradeKind::Overcharge || choice.kind==UpgradeKind::Vitality)continue;
        if(choice.kind!=UpgradeKind::Relic || id<0 || id>=relic_count ||
           !relic_allowed(choice.relic,game.character) || game.owns_relic(choice.relic) ||
           (offered&(1u<<id))) throw std::runtime_error("Illegal or duplicate relic offered");
        offered|=1u<<id;
        // Stable policy based only on visible offer and current health.
        float value=50.f;
        if(choice.relic==RelicId::LifeSeed) value=game.player.hp<game.player.max_hp*.7f?100.f:65.f;
        if(choice.relic==RelicId::DashWard) value=70.f;
        if(choice.relic==RelicId::ThunderToken) value=75.f;
        value+=float((bot.seed+uint32_t(id)*17u)%11u)*.03f;
        if(value>best){best=value;selected=i;}
    }
    if(selected==game.choices.size())throw std::runtime_error("Relic menu has no offered relic");
    return selected;
}
const char* mode_key(RunMode mode) {
    return mode==RunMode::Campaign?"campaign":mode==RunMode::Endless?"endless":"classic";
}

json sample_encounters(CharacterClass cls,uint32_t seed,RunMode mode) {
    Config config;config.enable_director=config.enable_scene_objects=config.enable_boss_phases=true;
    RunSetup setup;setup.mode=mode;setup.difficulty=Difficulty::Normal;
    setup.enable_loot=true;setup.unlocked_relic_mask=initial_relic_mask();
    setup.equipment={EquipmentId::TravelerKnife,EquipmentId::LeatherVest,EquipmentId::AmberPendant};
    Game game(config);game.start(cls,seed,{},setup);
    Controller bot{2,2,seed};Input input{};
    const float cutoff=mode==RunMode::Endless?1200.f:610.f;
    const auto wall_start=std::chrono::steady_clock::now();const double cpu_start=process_cpu_seconds();
    uint64_t ticks=0,interactions=0;int picked=0;bool censored=false;
    std::array<int,8> announced{},started{},ended{},aborted{};
    std::array<double,8> active_seconds{},telegraph_seconds{};
    std::array<uint64_t,8> spawns_by_role{};
    std::array<int,4> objects_seen{};
    std::array<double,skill_count> skill_seconds{};
    std::array<double,xp_bucket_count> minute_seconds{},minute_enemy_sum{},minute_pressure{},minute_damage{};
    std::array<uint64_t,xp_bucket_count> minute_ticks{};
    std::array<int,xp_bucket_count> minute_levels{};
    std::array<size_t,7> peaks{};std::array<std::array<size_t,7>,xp_bucket_count> minute_peaks{};
    std::array<double,xp_bucket_count> minute_cpu{};
    std::unordered_map<uint32_t,bool> seen_objects;
    std::unordered_map<uint32_t,size_t> boss_indices;
    json encounters=json::array(),bosses=json::array(),choices=json::array(),breathers=json::array();
    double cooldown_seconds=0,idle_seconds=0,mud_seconds=0,low_hp_seconds=0,max_pressure=0;
    double breather_start_hp=0,breather_start_damage=0,breather_start_time=0;
    int breather_start_enemies=0,breather_start_items=0;
    bool in_breather=false;int unsafe_commands=0,major_overlap_ticks=0;
    while(true) {
        while(game.state==State::Upgrade) {
            const size_t chosen=choose_progression(game,bot);const auto choice=game.choices[chosen];
            choices.push_back({{"time",game.total_elapsed()},{"level",game.player.level},{"kind",int(choice.kind)},
              {"skill",choice.kind==UpgradeKind::Skill?int(choice.skill):-1},{"relic",choice.kind==UpgradeKind::Relic?int(choice.relic):-1}});
            if(!game.choose_upgrade(chosen))throw std::runtime_error("Legal offered upgrade rejected");
            if(++picked>10000)throw std::runtime_error("Upgrade loop guard");
        }
        if(game.state==State::Victory||game.state==State::Defeat)break;
        if(game.state!=State::Playing)throw std::runtime_error("Unexpected game state");
        if(game.total_elapsed()>=cutoff){if(mode!=RunMode::Endless)throw std::runtime_error("Natural terminal state overdue");censored=true;break;}
        if(ticks%decision_ticks==0){
            input=bot.steer(game);
            // Ordinary interact only for a nearby visible object; do not target or create props.
            // A barrel is only struck if enemies are in its blast radius. The unchanged
            // Progression movement policy does not explicitly avoid armed barrels or seek wells.
            const SceneObject* nearby_object=nullptr;float distance=76;
            for(const auto& o:game.scene_objects)if(o.state==SceneObjectState::Active&&o.kind!=SceneObjectKind::Mud&&length(o.pos-game.player.pos)<distance){distance=length(o.pos-game.player.pos);nearby_object=&o;}
            if(nearby_object){
                bool use=nearby_object->kind==SceneObjectKind::Crate||(nearby_object->kind==SceneObjectKind::HealingWell&&game.player.hp<game.player.max_hp-1);
                if(nearby_object->kind==SceneObjectKind::Barrel)use=std::any_of(game.enemies.begin(),game.enemies.end(),[&](const Enemy& e){return e.hp>0&&length(e.pos-nearby_object->pos)<118+e.radius;});
                input.interact=use;if(use)++interactions;
            }
        }
        const float before=game.total_elapsed();const double before_damage=game.metrics.damage_taken;
        game.update(dt,input);input.dash=input.interact=false;++ticks;
        const float after=game.total_elapsed();const double step=after-before;
        if(step<0||!std::isfinite(after))throw std::runtime_error("Non-monotone elapsed time");
        int minute=std::clamp(int((before+after)*.5f/60),0,xp_bucket_count-1);
        minute_seconds[minute]+=step;++minute_ticks[minute];minute_levels[minute]=game.player.level;
        minute_enemy_sum[minute]+=game.enemies.size();minute_pressure[minute]+=game.director_state.pressure;
        minute_damage[minute]+=game.metrics.damage_taken-before_damage;
        minute_cpu[minute]=process_cpu_seconds()-cpu_start;
        for(int s=0;s<skill_count;++s)if(game.skills[s])skill_seconds[s]+=step;
        const auto& d=game.director_state;int k=int(d.kind);
        if(d.announced)++announced[k];if(d.started)++started[k];
        if(d.ended){++ended[int(d.ended_kind)];if(d.aborted)++aborted[int(d.ended_kind)];}
        if(d.phase==EncounterPhase::Active)active_seconds[k]+=step;
        if(d.phase==EncounterPhase::Telegraph)telegraph_seconds[k]+=step;
        if(d.phase==EncounterPhase::Cooldown)cooldown_seconds+=step;
        if(d.phase==EncounterPhase::Idle)idle_seconds+=step;
        if(d.announced||d.started||d.ended)encounters.push_back({{"time",after},{"kind",k},{"phase",int(d.phase)},
          {"id",d.encounter_id},{"announced",d.announced},{"started",d.started},{"ended",d.ended},
          {"ended_kind",int(d.ended_kind)},{"aborted",d.aborted},{"hp_ratio",game.player.hp/game.player.max_hp},
          {"pressure",d.pressure},{"enemies",game.enemies.size()}});
        for(const auto& spawn:d.spawns){++spawns_by_role[int(spawn.role)];if(std::hypot(spawn.offset_x,spawn.offset_y)+.1f<d.min_spawn_distance)++unsafe_commands;}
        if(d.important_active&&game.special_event.kind!=SpecialEventKind::None)++major_overlap_ticks;
        max_pressure=std::max(max_pressure,double(d.pressure));
        if(game.player.hp<game.player.max_hp*.32f)low_hp_seconds+=step;
        bool now_breather=d.kind==EncounterKind::Breather&&d.phase==EncounterPhase::Active;
        if(now_breather&&!in_breather){breather_start_hp=game.player.hp;breather_start_damage=game.metrics.damage_taken;breather_start_time=after;breather_start_enemies=int(game.enemies.size());breather_start_items=game.metrics.items_collected;}
        if(!now_breather&&in_breather)breathers.push_back({{"start",breather_start_time},{"end",after},{"hp_before",breather_start_hp},
          {"hp_after",game.player.hp},{"damage_taken",game.metrics.damage_taken-breather_start_damage},{"enemies_before",breather_start_enemies},
          {"enemies_after",game.enemies.size()},{"items_collected",game.metrics.items_collected-breather_start_items},{"partial",false}});
        in_breather=now_breather;
        bool in_mud=false;
        for(const auto& o:game.scene_objects){if(seen_objects.emplace(o.id,true).second)++objects_seen[int(o.kind)];if(o.kind==SceneObjectKind::Mud&&o.state==SceneObjectState::Active&&length(game.player.pos-o.pos)<o.radius+13)in_mud=true;}
        if(in_mud)mud_seconds+=step;
        for(const auto& e:game.enemies)if(e.kind==EnemyKind::Boss){
            auto found=boss_indices.find(e.id);
            if(found==boss_indices.end()){
                boss_indices[e.id]=bosses.size();bosses.push_back({{"id",e.id},{"profile",int(e.boss_profile)},{"spawn_time",after},
                  {"max_hp",e.max_hp},{"player_level",game.player.level},{"last_seen_time",after},{"last_hp_fraction",e.hp/e.max_hp},
                  {"max_stage",e.boss_stage},{"stages",json::array({{{"stage",e.boss_stage},{"time",after},{"hp_fraction",e.hp/e.max_hp}}})}});
                found=boss_indices.find(e.id);
            }
            auto& b=bosses[found->second];b["last_seen_time"]=after;b["last_hp_fraction"]=e.hp/e.max_hp;
            if(e.boss_stage>b["max_stage"].get<int>()){b["max_stage"]=e.boss_stage;b["stages"].push_back({{"stage",e.boss_stage},{"time",after},{"hp_fraction",e.hp/e.max_hp}});}
        }
        std::array<size_t,7> sizes={game.enemies.size(),game.projectiles.size(),game.gems.size(),game.effects.size(),game.items.size(),game.damage_numbers.size(),game.scene_objects.size()};
        std::array<size_t,7> caps={config.max_enemies,config.max_projectiles,config.max_gems,config.max_effects,config.max_items,config.max_damage_numbers,config.max_scene_objects};
        for(size_t i=0;i<sizes.size();++i){peaks[i]=std::max(peaks[i],sizes[i]);minute_peaks[minute][i]=std::max(minute_peaks[minute][i],sizes[i]);if(sizes[i]>caps[i])throw std::runtime_error("Entity container cap exceeded");}
        if(!std::isfinite(game.player.hp)||!std::isfinite(game.player.pos.x)||!std::isfinite(game.player.pos.y))throw std::runtime_error("Non-finite player state");
        if(ticks>uint64_t((cutoff+10)*120))throw std::runtime_error("Tick guard");
    }
    if(in_breather)breathers.push_back({{"start",breather_start_time},{"end",game.total_elapsed()},{"hp_before",breather_start_hp},
      {"hp_after",game.player.hp},{"damage_taken",game.metrics.damage_taken-breather_start_damage},{"enemies_before",breather_start_enemies},
      {"enemies_after",game.enemies.size()},{"items_collected",game.metrics.items_collected-breather_start_items},{"partial",true}});
    const auto& m=game.metrics;const auto& c=game.combat_diagnostics;
    json per_minute=json::array(),skills=json::array(),encounter_summary=json::array();double last_cpu=0;
    for(int i=0;i<xp_bucket_count;++i)if(minute_ticks[i]){
        per_minute.push_back({{"minute",i+1},{"exposure_seconds",minute_seconds[i]},{"end_level",minute_levels[i]},{"xp",m.xp_by_minute[i]},
          {"damage_taken",minute_damage[i]},{"mean_enemies",minute_enemy_sum[i]/minute_ticks[i]},{"mean_pressure",minute_pressure[i]/minute_ticks[i]},
          {"peak_containers",minute_peaks[i]},{"process_cpu_seconds",minute_cpu[i]-last_cpu}});last_cpu=minute_cpu[i];
    }
    for(int i=0;i<skill_count;++i)if(game.skill_allowed(Skill(i)))skills.push_back({{"id",i},{"level",game.skills[i]},{"evolved",game.evolved(Skill(i))},{"actual_hp_damage",m.skill_damage[i]},{"owned_seconds",skill_seconds[i]}});
    const char* kinds[]={"none","swarm","encirclement","rush","elite_hunt","crisis","breather","boss"};
    for(int i=1;i<8;++i)encounter_summary.push_back({{"kind",kinds[i]},{"announced",announced[i]},{"started",started[i]},{"ended",ended[i]},
      {"aborted",aborted[i]},{"active_seconds",active_seconds[i]},{"telegraph_seconds",telegraph_seconds[i]}});
    return {{"class",class_names[int(cls)]},{"seed",seed},{"mode",mode_key(mode)},{"difficulty","normal"},{"growth","zero_talents"},
      {"outcome",censored?"censored_alive":game.state==State::Victory?"victory":"defeat"},{"censored",censored},
      {"survival_seconds",game.total_elapsed()},{"level",game.player.level},{"kills",game.kills},{"hp",game.player.hp},{"max_hp",game.player.max_hp},
      {"enable_loot",true},{"equipment_ids",{1,2,3}},{"bonuses",bonuses_json_progression(game.run_bonuses())},
      {"xp_acquired",m.xp_acquired},{"actual_enemy_hp_damage",std::accumulate(m.skill_damage.begin(),m.skill_damage.end(),m.other_damage)},
      {"actual_player_hp_damage_taken",m.damage_taken},{"skills",skills},{"passives",game.passives},{"other_damage",m.other_damage},
      {"per_minute",per_minute},{"encounter_summary",encounter_summary},{"encounter_timeline",encounters},
      {"breather_windows",breathers},{"director_idle_seconds",idle_seconds},{"director_cooldown_seconds",cooldown_seconds},
      {"director_max_pressure",max_pressure},{"low_hp_seconds",low_hp_seconds},{"director_spawns_by_role",spawns_by_role},
      {"director_unsafe_spawn_commands",unsafe_commands},{"major_overlap_ticks",major_overlap_ticks},
      {"bosses",bosses},{"bosses_defeated",m.bosses_defeated},{"boss_attacks",m.boss_attacks},{"boss_damage_to_player",m.boss_damage_taken},
      {"boss_spawn_time",m.boss_spawn_time},{"boss_defeat_time",m.boss_defeat_time},{"boss_phase_changes",c.boss_phase_changes},{"boss_summons",c.boss_summons},{"endless_cycle",game.endless_cycle()},
      {"actual_hits",c.actual_hits},{"critical_hits",c.critical_hits},
      {"scene_objects_created",c.scene_objects_created},{"scene_objects_seen_by_kind",objects_seen},{"objects_destroyed",c.objects_destroyed},
      {"barrels_exploded",c.barrels_exploded},{"healing_uses",c.healing_uses},{"interaction_inputs",interactions},
      {"scene_object_damage",c.object_damage},{"collision_resolutions",c.collision_resolutions},{"escape_resolutions",c.escape_resolutions},{"mud_contact_seconds",mud_seconds},
      {"environment_damage_taken",m.environment_damage_taken},{"environment_hits",m.environment_hits},{"hazards_spawned",m.hazards_spawned},
      {"events_started",m.events_started},{"events_succeeded",m.events_succeeded},{"events_failed",m.events_failed},
      {"relics_collected",m.relics_collected},{"relic_procs",m.relic_procs},{"equipment_procs",m.equipment_procs},
      {"items_collected",m.items_collected},{"coins_remaining",game.coins},{"dash_uses",m.dash_uses},{"upgrades_chosen",picked},{"upgrade_history",choices},
      {"simulation_steps",game.simulation_steps},{"runner_ticks",ticks},{"process_cpu_seconds",process_cpu_seconds()-cpu_start},
      {"wall_seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-wall_start).count()},
      {"peak_containers",peaks},{"container_caps",{config.max_enemies,config.max_projectiles,config.max_gems,config.max_effects,config.max_items,config.max_damage_numbers,config.max_scene_objects}}};
}
}
int main(int argc,char** argv){
    try{
        std::string output="docs/encounters_simulation_runs.json";uint32_t seed=20261111;bool tuning_check=false;
        for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--help"){std::cout<<"simulate_encounters [--seed UINT] [--output FILE] [--tuning-check]\nDefault: six Classic and one bounded Endless samples. --tuning-check: Archer seed, Mage seed, Warrior seed+1 only.\n";return 0;}
            if(arg=="--tuning-check"){tuning_check=true;continue;} if(i+1>=argc)throw std::runtime_error("Missing CLI value");if(arg=="--output")output=argv[++i];else if(arg=="--seed")seed=uint32_t(std::stoul(argv[++i]));else throw std::runtime_error("Unknown option: "+arg);}
        json report={{"schema_version",1},{"tool","MapleRGL simulate_encounters"},{"complete",false},{"expected_runs",tuning_check?3:7},{"seed",seed},
          {"config",{{"enable_director",true},{"enable_scene_objects",true},{"enable_boss_phases",true},{"director_boss_health_scale",Config{}.director_boss_health_scale}}},
          {"simulation_dt_seconds",dt},{"decision_frequency_hz",10},{"endless_censor_seconds",1200},
          {"container_order",{"enemies","projectiles","gems","effects","items","damage_numbers","scene_objects"}},
          {"director_role_order",{"common","swarm","fast","armored","ranged","support","elite","boss"}},
          {"scene_object_order",{"crate","barrel","mud","healing_well"}},
          {"limitations",{
            "Small deterministic correlated heuristic samples are not player win rates or a balance verdict.",
            "Combat/03 event/balanced movement, upgrade and offered-relic policy is reused unchanged. The only added Input is nearby interact at 10 Hz.",
            "The bot neither seeks healing wells nor explicitly avoids armed barrels; object interaction coverage is incidental, not human usability validation.",
            "All runs use Normal, zero permanent talents, starter equipment IDs 1/2/3, initial relic mask and loot enabled. No live profile/save IO.",
            "No HP, skill, enemy, gameplay duration or resource injection. Game config only enables the three Encounters features before start.",
            "Classic victory can occur at 600 seconds without killing its boss; bosses_defeated and last observed boss HP are separately recorded.",
            "Endless stops at natural defeat or 1200 simulated seconds. A live cutoff is censored, never victory.",
            "60 Hz core updates preserve transient director flags. Step counts can differ slightly from nominal elapsed time due to floating-point accumulation.",
            "CPU/wall numbers measure only headless simulation with logging/instrumentation, never rendered FPS or frame time.",
            "Per-minute container peaks and headless CPU are resource trend indicators, not OS memory measurements.",
            "Damage is actual removed enemy/player HP. Boss last observed HP can remain above zero in the frame before death; use bosses_defeated.",
            "No Campaign, Hard/Nightmare, manual GUI, audiovisual, long-duration endurance or statistical balance validation is claimed."}},
          {"runs",json::array()}};
        auto save=[&](){std::ofstream file(output,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Cannot open "+output);file<<report.dump(2)<<'\n';if(!file)throw std::runtime_error("Report write failed");};
        auto run=[&](CharacterClass cls,uint32_t s,RunMode mode){auto result=sample_encounters(cls,s,mode);std::cout<<class_names[int(cls)]<<" seed="<<s<<" "<<mode_key(mode)<<" "<<result["outcome"].get<std::string>()<<" t="<<result["survival_seconds"]<<" level="<<result["level"]<<" kills="<<result["kills"]<<" boss="<<result["bosses_defeated"]<<'\n'<<std::flush;report["runs"].push_back(std::move(result));save();};
        if(tuning_check){run(CharacterClass::Archer,seed,RunMode::Classic);run(CharacterClass::Mage,seed,RunMode::Classic);run(CharacterClass::Warrior,seed+1,RunMode::Classic);}else{for(int cls=0;cls<3;++cls)for(uint32_t offset=0;offset<2;++offset)run(CharacterClass(cls),seed+offset,RunMode::Classic);
        run(CharacterClass::Archer,seed,RunMode::Endless);}report["complete"]=true;save();std::cout<<"Wrote bounded runs to "<<output<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"Encounters simulation failed: "<<e.what()<<'\n';return 1;}
}
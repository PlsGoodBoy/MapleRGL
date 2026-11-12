// Progression bounded, legal-input smoke sampling. No profile or live save is opened.
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

json sample_progression(CharacterClass cls, uint32_t seed, bool grown, RunMode mode) {
    RunSetup setup;setup.mode=mode;setup.difficulty=Difficulty::Normal;
    setup.enable_loot=true;setup.unlocked_relic_mask=initial_relic_mask();
    setup.equipment={EquipmentId::TravelerKnife,EquipmentId::LeatherVest,EquipmentId::AmberPendant};
    const RunBonuses bonuses=grown?capped_talents():RunBonuses{};
    Game game;game.start(cls,seed,bonuses,setup);
    Controller bot{2,2,seed};Input input{};
    json choices=json::array(),chapters=json::array(),boss_spawns=json::array();
    std::array<double,xp_bucket_count> exposure{};
    std::array<int,xp_bucket_count> levels{};
    std::array<double,skill_count> skill_exposure{};
    std::array<int,9> event_starts{},event_successes{},event_failures{};
    size_t pe=game.enemies.size(),pp=0,pg=0,pf=0,pi_items=0,pd=0;
    uint64_t ticks=0;int picked=0,last_boss_id=-1,last_boss_stage=-1;
    bool censored=false;
    // Campaign has three normal-duration chapters. This is only a watchdog;
    // no early win, duration/config edits or forced chapter completions.
    const float cutoff=mode==RunMode::Endless?1200.f:mode==RunMode::Campaign?1810.f:610.f;
    const auto wall_start=std::chrono::steady_clock::now();
    const double cpu_start=process_cpu_seconds();
    while(true) {
        while(game.state==State::Upgrade) {
            const size_t chosen=choose_progression(game,bot);const auto choice=game.choices[chosen];
            choices.push_back({{"time",game.total_elapsed()},{"chapter",game.chapter_index()},
              {"level",game.player.level},{"kind",int(choice.kind)},
              {"skill",choice.kind==UpgradeKind::Skill?int(choice.skill):-1},
              {"relic",choice.kind==UpgradeKind::Relic?int(choice.relic):-1}});
            if(!game.choose_upgrade(chosen)) throw std::runtime_error("Offered legal choice rejected");
            if(++picked>10000)throw std::runtime_error("Upgrade loop guard");
        }
        if(game.state==State::ChapterComplete) {
            const float before_hp=game.player.hp;const int chapter=game.chapter_index();
            if(!game.apply_chapter_reward(0))throw std::runtime_error("Legal chapter supply rejected");
            chapters.push_back({{"chapter",chapter},{"time",game.total_elapsed()},
              {"level",game.player.level},{"hp_before",before_hp},{"hp_after",game.player.hp},
              {"reward_index",0},{"boss_defeated",game.boss_defeated}});
            if(!game.advance_chapter())throw std::runtime_error("Legal chapter advance rejected");
            input={};bot.previous={};
        }
        if(game.state==State::Victory || game.state==State::Defeat)break;
        if(game.state!=State::Playing)throw std::runtime_error("Unexpected game state");
        if(game.total_elapsed()>=cutoff) {
            if(mode!=RunMode::Endless)throw std::runtime_error("Natural terminal state overdue");
            censored=true;break;
        }
        if(ticks%decision_ticks==0)input=bot.steer(game);
        const float before_time=game.total_elapsed();const auto before_metrics=game.metrics;
        const int old_event=int(game.special_event.kind);
        game.update(dt,input);input.dash=false;++ticks;
        const float after_time=game.total_elapsed();const double step=after_time-before_time;
        if(step<0 || !std::isfinite(after_time))throw std::runtime_error("Non-monotone elapsed time");
        const int minute=std::clamp(int((before_time+after_time)*.5f/60),0,xp_bucket_count-1);
        exposure[minute]+=step;levels[minute]=game.player.level;
        for(int s=0;s<skill_count;++s)if(game.skills[s]>0)skill_exposure[s]+=step;
        if(game.metrics.events_started>before_metrics.events_started)++event_starts[int(game.special_event.kind)];
        if(game.metrics.events_succeeded>before_metrics.events_succeeded)++event_successes[old_event];
        if(game.metrics.events_failed>before_metrics.events_failed)++event_failures[old_event];
        const int stage=mode==RunMode::Campaign?game.chapter_index():game.endless_cycle();
        for(const auto& enemy:game.enemies)if(enemy.kind==EnemyKind::Boss &&
           (int(enemy.id)!=last_boss_id || stage!=last_boss_stage)) {
            boss_spawns.push_back({{"time",after_time},{"chapter_or_cycle",stage},{"id",enemy.id},
              {"max_hp",enemy.max_hp},{"player_level",game.player.level}});
            last_boss_id=int(enemy.id);last_boss_stage=stage;
        }
        pe=std::max(pe,game.enemies.size());pp=std::max(pp,game.projectiles.size());
        pg=std::max(pg,game.gems.size());pf=std::max(pf,game.effects.size());
        pi_items=std::max(pi_items,game.items.size());pd=std::max(pd,game.damage_numbers.size());
        if(!std::isfinite(game.player.hp)||!std::isfinite(game.player.pos.x)||!std::isfinite(game.player.pos.y))
            throw std::runtime_error("Non-finite player state");
        if(pe>game.config.max_enemies || pp>game.config.max_projectiles || pg>game.config.max_gems ||
           pf>game.config.max_effects || pi_items>game.config.max_items || pd>game.config.max_damage_numbers)
            throw std::runtime_error("Entity container cap exceeded");
        if(ticks>uint64_t((cutoff+10)*120))throw std::runtime_error("Simulation tick guard");
    }
    const double cpu_seconds=process_cpu_seconds()-cpu_start;
    const double wall_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-wall_start).count();
    const auto& m=game.metrics;
    const double actual_damage=std::accumulate(m.skill_damage.begin(),m.skill_damage.end(),m.other_damage);
    json per_minute=json::array(),skills=json::array(),events=json::array(),relics=json::array();
    for(int minute=0;minute<xp_bucket_count;++minute)if(exposure[minute]>0 || m.xp_by_minute[minute]>0) {
        const double seconds=exposure[minute];
        per_minute.push_back({{"minute",minute+1},{"xp",m.xp_by_minute[minute]},
          {"exposure_seconds",seconds},{"observed_xp_per_minute",seconds>0?json(m.xp_by_minute[minute]*60/seconds):json(nullptr)},
          {"end_level",levels[minute]}});
    }
    for(int s=0;s<skill_count;++s)if(game.skill_allowed(Skill(s)))
        skills.push_back({{"id",s},{"level",game.skills[s]},{"evolved",game.evolved(Skill(s))},
          {"actual_hp_damage",m.skill_damage[s]},{"owned_seconds",skill_exposure[s]}});
    for(int i=1;i<9;++i)events.push_back({{"id",i},{"started",event_starts[i]},
      {"succeeded",event_successes[i]},{"failed",event_failures[i]}});
    for(int i=0;i<relic_count;++i)relics.push_back({{"id",i},{"owned",game.owns_relic(RelicId(i))},{"procs",m.relic_procs[i]}});
    json boss_hp=nullptr;
    for(const auto& e:game.enemies)if(e.kind==EnemyKind::Boss)boss_hp=e.hp/e.max_hp;
    return {{"class",class_names[int(cls)]},{"seed",seed},{"growth",grown?"capped_talents":"zero_talents"},
      {"mode",mode_key(mode)},{"difficulty","normal"},{"movement_policy","event"},{"upgrade_policy","balanced"},
      {"outcome",censored?"censored_alive":game.state==State::Victory?"victory":"defeat"},
      {"censored",censored},{"survival_seconds",game.total_elapsed()},{"level",game.player.level},
      {"kills",game.kills},{"hp",game.player.hp},{"max_hp",game.player.max_hp},
      {"bonuses",bonuses_json_progression(game.run_bonuses())},{"enable_loot",true},
      {"unlocked_relic_mask",setup.unlocked_relic_mask},{"equipment_ids",{1,2,3}},
      {"xp_acquired",m.xp_acquired},{"xp_by_minute",per_minute},
      {"actual_enemy_hp_damage",actual_damage},{"actual_player_hp_damage_taken",m.damage_taken},
      {"altar_health_spent",m.altar_health_spent},{"environment_damage_taken",m.environment_damage_taken},
      {"environment_hits",m.environment_hits},{"hazards_spawned",m.hazards_spawned},
      {"skills",skills},{"passives",game.passives},{"other_damage",m.other_damage},
      {"events_started",m.events_started},{"events_succeeded",m.events_succeeded},{"events_failed",m.events_failed},
      {"event_types",events},{"event_active_at_end",game.special_event.kind!=SpecialEventKind::None},
      {"boss_spawns",boss_spawns},{"bosses_defeated",m.bosses_defeated},{"boss_defeated_current_stage",game.boss_defeated},
      {"boss_hp_fraction_remaining",boss_hp},{"boss_attacks",m.boss_attacks},{"boss_damage_to_player",m.boss_damage_taken},
      {"chapters_cleared",m.chapters_cleared},{"chapter_supplies",chapters},{"endless_cycle",game.endless_cycle()},
      {"relics_collected",m.relics_collected},{"relics",relics},{"equipment_procs",m.equipment_procs},
      {"items_collected",m.items_collected},{"coins_remaining",game.coins},{"upgrades_chosen",picked},
      {"upgrade_history",choices},{"upgrade_rerolls_used",m.upgrade_rerolls_used},
      {"death_protections_used",m.death_protections_used},{"first_elite_rewards",m.first_elite_rewards},
      {"dash_uses",m.dash_uses},{"simulation_steps",game.simulation_steps},{"runner_ticks",ticks},
      {"process_cpu_seconds",cpu_seconds},{"wall_seconds",wall_seconds},
      {"peak_containers",{{"enemies",pe},{"projectiles",pp},{"gems",pg},{"effects",pf},{"items",pi_items},{"damage_numbers",pd}}},
      {"container_caps",{{"enemies",game.config.max_enemies},{"projectiles",game.config.max_projectiles},
        {"gems",game.config.max_gems},{"effects",game.config.max_effects},{"items",game.config.max_items},
        {"damage_numbers",game.config.max_damage_numbers}}}};
}
}

int main(int argc,char** argv) {
    try {
        std::string output="docs/simulation_progression_runs.json";uint32_t seed=20261111;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help") {std::cout<<"simulate_progression [--seed 20261111] [--output FILE]\nExactly 8 fixed-policy legal-input runs; no save/profile IO.\n";return 0;}
            if(i+1>=argc)throw std::runtime_error("Missing CLI value");
            if(arg=="--output")output=argv[++i];
            else if(arg=="--seed")seed=uint32_t(std::stoul(argv[++i]));
            else throw std::runtime_error("Unknown argument: "+arg);
        }
        json report={{"schema_version",1},{"tool","MapleRGL simulate_progression"},{"expected_runs",8},
          {"seed",seed},{"complete",false},{"simulation_dt_seconds",dt},{"decision_frequency_hz",60/decision_ticks},
          {"classic_duration_seconds",600},{"endless_censor_seconds",1200},
          {"limitations",{"Eight correlated deterministic runs, not player win rates or a balance verdict.",
            "Reuses Combat event/balanced movement and upgrade policy with only legal offered relic selection added.",
            "Only start, ordinary movement/dash, choose_upgrade, apply_chapter_reward and advance_chapter mutate the game.",
            "No HP, skill, state, enemy, duration or resource injection; no profile/save files are read or written.",
            "Both growth groups share starter equipment, loot enabled and initial unlock mask; max talents are a legal capped snapshot.",
            "Rerolls are available in grown starts but intentionally unused by this fixed policy.",
            "Campaign and Endless use the archer with capped talents and the same fixed seed; a death remains a death.",
            "Campaign chooses offered life supply index 0 between naturally completed chapters; no forced traversal.",
            "Endless ends at natural defeat or 1200 simulated seconds and records a live cutoff as censored, never victory.",
            "CPU/wall measurements are headless simulation costs, not rendered FPS or frame-time claims.",
            "Damage is actual removed HP, excluding overkill; altar spending is separate from enemy damage.",
            "Tiny partial minute XP rates can be unstable; raw XP and observed seconds are included.",
            "The heuristic sees current public world positions, reacts at 10 Hz and does not foresee RNG or optimize rollouts."}},
          {"runs",json::array()}};
        auto save=[&](){std::ofstream file(output,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Cannot open "+output);file<<report.dump(2)<<'\n';if(!file)throw std::runtime_error("Report write failed");};
        auto run=[&](CharacterClass cls,bool grown,RunMode mode){
            auto result=sample_progression(cls,seed,grown,mode);
            std::cout<<class_names[int(cls)]<<" "<<mode_key(mode)<<" "<<(grown?"capped":"zero")
              <<" "<<result["outcome"].get<std::string>()<<" t="<<result["survival_seconds"]
              <<" level="<<result["level"]<<" kills="<<result["kills"]<<'\n'<<std::flush;
            report["runs"].push_back(std::move(result));save();
        };
        for(int cls=0;cls<3;++cls){run(CharacterClass(cls),false,RunMode::Classic);run(CharacterClass(cls),true,RunMode::Classic);}
        run(CharacterClass::Archer,true,RunMode::Campaign);
        run(CharacterClass::Archer,true,RunMode::Endless);
        report["complete"]=true;save();
        std::cout<<"Wrote 8 bounded runs to "<<output<<'\n';return 0;
    } catch(const std::exception& e){std::cerr<<"Progression simulation failed: "<<e.what()<<'\n';return 1;}
}

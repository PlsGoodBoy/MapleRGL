// Six fixed, bounded Fusion gameplay samples. This tool never opens a profile.
// Reuse the existing movement controller verbatim; only offered-choice scoring
// follows the named fusion route. No injected HP, XP, skills, enemies or wins.
#define main combat_reference_main
#include "simulate_balance.cpp"
#undef main
#include <ctime>
#include <deque>
#include <filesystem>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
struct FusionCase {
    CharacterClass character;
    Skill first, second, result, new_skill;
    const char* route;
};
constexpr std::array<FusionCase,6> fusion_cases{{
    {CharacterClass::Archer,Skill::Pierce,Skill::Ricochet,Skill::StarReturn,Skill::HunterMark,"A"},
    {CharacterClass::Archer,Skill::Summon,Skill::Rain,Skill::PhoenixRain,Skill::ArrowTrap,"B"},
    {CharacterClass::Mage,Skill::Frost,Skill::Lightning,Skill::FrostStorm,Skill::OrbitalOrb,"A"},
    {CharacterClass::Mage,Skill::Zone,Skill::Meteor,Skill::MoltenStar,Skill::GravityWell,"B"},
    {CharacterClass::Warrior,Skill::Nova,Skill::Retaliate,Skill::DragonArmor,Skill::HeavyStrike,"A"},
    {CharacterClass::Warrior,Skill::Orbit,Skill::BladeWave,Skill::SwordCyclone,Skill::Bulwark,"B"}
}};
constexpr uint32_t first_fusion_seed=20261112; // Consecutive residues 0..5, no seed search.
constexpr float view_width=1280,view_height=800,view_zoom=1.6f;

double fusion_cpu_seconds() {
#ifdef _WIN32
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))
        throw std::runtime_error("Cannot read process CPU time");
    auto seconds=[](FILETIME value){ULARGE_INTEGER ticks{};ticks.LowPart=value.dwLowDateTime;
        ticks.HighPart=value.dwHighDateTime;return double(ticks.QuadPart)*1e-7;};
    return seconds(kernel)+seconds(user);
#else
    return double(std::clock())/CLOCKS_PER_SEC;
#endif
}

Config fusion_config(json& rules) {
    std::ifstream input("assets/encounter_rules.json");
    if(!input)throw std::runtime_error("Run from the project/staging directory containing assets/encounter_rules.json");
    input>>rules;
    if(rules.value("schema",0)!=1)throw std::runtime_error("Unsupported director rules schema");
    Config config;config.enable_director=config.enable_scene_objects=config.enable_boss_phases=true;
    auto& cfg=config.director_config;
    // Same JSON fields and fallback values as App::App; no synthetic timing edits.
#define FUSION_LOAD_DIRECTOR(name) cfg.name=rules.value(#name,cfg.name)
    FUSION_LOAD_DIRECTOR(first_encounter);FUSION_LOAD_DIRECTOR(encounter_gap_min);
    FUSION_LOAD_DIRECTOR(encounter_gap_max);FUSION_LOAD_DIRECTOR(min_safe_distance);
    FUSION_LOAD_DIRECTOR(escape_half_angle);FUSION_LOAD_DIRECTOR(low_health);
    FUSION_LOAD_DIRECTOR(high_pressure);FUSION_LOAD_DIRECTOR(breather_duration);
    FUSION_LOAD_DIRECTOR(boss_telegraph);FUSION_LOAD_DIRECTOR(max_spawns_per_update);
#undef FUSION_LOAD_DIRECTOR
    static const char* keys[]={"swarm","encirclement","rush","elite_hunt","crisis","breather"};
    for(const auto& entry:rules.at("rules")){
        const std::string key=entry.at("kind").get<std::string>();int index=-1;
        for(int i=0;i<6;++i)if(key==keys[i])index=i;
        if(index<0)throw std::runtime_error("Unknown director encounter kind");
        auto& rule=cfg.rules[index];
#define FUSION_LOAD_RULE(name) rule.name=entry.value(#name,rule.name)
        FUSION_LOAD_RULE(weight);FUSION_LOAD_RULE(unlock_progress);FUSION_LOAD_RULE(telegraph);
        FUSION_LOAD_RULE(duration);FUSION_LOAD_RULE(cooldown);FUSION_LOAD_RULE(spawn_interval);
        FUSION_LOAD_RULE(base_budget);FUSION_LOAD_RULE(batch_size);
#undef FUSION_LOAD_RULE
    }
    return config;
}

int route_material_level(const FusionCase& sample) {
    for(const auto& recipe:fusion_recipes())if(recipe.result==sample.result&&recipe.character==sample.character&&
        recipe.first==sample.first&&recipe.second==sample.second)return recipe.material_level;
    throw std::runtime_error("Requested fixed route is not implemented in this build");
}

const char* choice_kind(UpgradeKind kind) {
    if(kind==UpgradeKind::Skill)return "skill";
    if(kind==UpgradeKind::Fusion)return "fusion";
    if(kind==UpgradeKind::Relic)return "relic";
    if(kind==UpgradeKind::Recover)return "recover";
    if(kind==UpgradeKind::Overcharge)return "overcharge";
    if(kind==UpgradeKind::Vitality)return "vitality";
    return "passive";
}
json offered_choice(const Game& game,const UpgradeChoice& choice) {
    json result={{"kind",choice_kind(choice.kind)},{"kind_id",int(choice.kind)},{"title",choice.title}};
    if(choice.kind==UpgradeKind::Skill||choice.kind==UpgradeKind::Fusion){
        result["skill_key"]=Game::skill_key(choice.skill);result["skill_id"]=int(choice.skill);
        result["level_before"]=game.skills[int(choice.skill)];
    }else if(choice.kind==UpgradeKind::Relic)result["relic_id"]=int(choice.relic);
    else if(int(choice.kind)>0&&int(choice.kind)<passive_count)result["level_before"]=game.passives[int(choice.kind)];
    return result;
}
json skill_build(const Game& game) {
    json result=json::object();
    for(int i=0;i<skill_count;++i)if(game.skills[i]>0)result[Game::skill_key(Skill(i))]=game.skills[i];
    return result;
}

size_t choose_fusion(const Game& game,const FusionCase& sample,uint32_t seed,int required) {
    if(game.choices.empty())throw std::runtime_error("Upgrade state has no offered choices");
    const float hp=game.player.hp/std::max(1.f,game.player.max_hp);
    const bool target_fused=game.skills[int(sample.result)]>0;
    const int reserved=target_fused?0:int(game.skills[int(sample.first)]==0)+int(game.skills[int(sample.second)]==0);
    float best=-1e30f;size_t selected=0;
    for(size_t i=0;i<game.choices.size();++i){
        const auto& choice=game.choices[i];float score=35;
        if(choice.kind==UpgradeKind::Fusion)score=choice.skill==sample.result?500.f:450.f;
        else if(choice.kind==UpgradeKind::Relic){
            score=50;
            if(choice.relic==RelicId::LifeSeed)score=hp<.7f?100.f:65.f;
            if(choice.relic==RelicId::DashWard)score=70;
            if(choice.relic==RelicId::ThunderToken)score=75;
        }else if(choice.kind==UpgradeKind::Skill){
            const int rank=game.skills[int(choice.skill)];
            const bool material=choice.skill==sample.first||choice.skill==sample.second;
            score=rank==0?80.f:95.f+(rank==4?15.f:0.f);
            if(!target_fused&&material)score=rank<required?260.f+(required-rank)*20.f:45.f;
            if(choice.skill==sample.result)score=rank<3?240.f:190.f;
            if(choice.skill==sample.new_skill)score=target_fused?(rank==0?210.f:155.f):(rank==0?140.f:105.f);
            if(rank==0&&!material&&Game::active_skill_limit-game.active_skill_count()<=reserved)score=-500;
        }else{
            const int kind=int(choice.kind),rank=kind<passive_count?game.passives[kind]:0;
            score=45.f-rank*2;
            if(choice.kind==UpgradeKind::Damage||choice.kind==UpgradeKind::AttackSpeed||choice.kind==UpgradeKind::Cooldown)score+=28;
            if(choice.kind==UpgradeKind::Area)score+=sample.character==CharacterClass::Warrior?32.f:16.f;
            if(choice.kind==UpgradeKind::Projectiles)score+=sample.character==CharacterClass::Archer?30.f:8.f;
            if(choice.kind==UpgradeKind::Duration)score+=20;
            if(choice.kind==UpgradeKind::Magnet&&rank<2)score+=50;
            if(choice.kind==UpgradeKind::MoveSpeed&&rank<2)score+=22;
            if(choice.kind==UpgradeKind::Health||choice.kind==UpgradeKind::Recover||choice.kind==UpgradeKind::Vitality)
                score=hp<.35f?700.f:hp<.6f?250.f:55.f+(1-hp)*70.f;
        }
        score+=float((seed+uint32_t(i)*17u+uint32_t(game.player.level)*13u)%11u)*.03f;
        if(score>best){best=score;selected=i;}
    }
    return selected;
}

const char* damage_category(double total,double boss,double environment,double altar) {
    if(total<=0)return altar>0?"altar_health_spent":"unclassified";
    if(boss>0&&environment>0)return "mixed_or_overlapping_boss_environment";
    if(boss>0)return total>boss+.001?"mixed_boss_and_other":"boss_tagged";
    if(environment>0)return total>environment+.001?"mixed_environment_and_other":"environment_tagged";
    return "ordinary_or_other";
}

json sample_fusion(int index,const Config& config) {
    const auto& sample=fusion_cases[size_t(index)];const uint32_t seed=first_fusion_seed+uint32_t(index);
    const int required=route_material_level(sample);
    RunSetup setup;setup.mode=RunMode::Classic;setup.difficulty=Difficulty::Normal;
    setup.enable_loot=true;setup.unlocked_relic_mask=initial_relic_mask();
    setup.equipment={EquipmentId::TravelerKnife,EquipmentId::LeatherVest,EquipmentId::AmberPendant};
    Game game(config);game.start(sample.character,seed,{},setup);
    game.set_combat_view(view_width/(2*view_zoom),view_height/(2*view_zoom));
    const BossProfile scheduled=game.next_boss_profile();
    Controller bot{2,2,seed};Input input{};
    const auto wall_begin=std::chrono::steady_clock::now();const double cpu_begin=fusion_cpu_seconds();
    uint64_t ticks=0,interaction_inputs=0;int picked=0;float first_upgrade=-1,first_choice=-1,first_three_skills=-1;
    constexpr size_t encounters_count=size_t(EncounterKind::Count);
    std::array<int,encounters_count> announced{},started{},ended{};
    std::array<double,encounters_count> active_seconds{};
    std::array<int,9> event_started{},event_succeeded{},event_failed{};
    std::array<double,skill_count> owned_seconds{};
    std::array<size_t,7> peaks{};
    const std::array<size_t,7> caps={config.max_enemies,config.max_projectiles,config.max_gems,
        config.max_effects,config.max_items,config.max_damage_numbers,config.max_scene_objects};
    std::array<double,xp_bucket_count> minute_seconds{},minute_damage{};
    std::array<int,xp_bucket_count> minute_levels{};
    std::array<int,enemy_visual_count> visual_spawns{};
    std::unordered_set<uint32_t> seen_enemies;
    std::unordered_map<uint32_t,size_t> boss_indices;
    json choices=json::array(),automatic_growth=json::array(),encounters=json::array(),bosses=json::array();
    json fatal_update=nullptr;std::deque<json> recent_damage;
    auto observe_bosses=[&](){
        for(const auto& enemy:game.enemies){
            if(seen_enemies.insert(enemy.id).second&&enemy.kind!=EnemyKind::Boss&&enemy.visual>=0&&enemy.visual<enemy_visual_count)
                ++visual_spawns[enemy.visual];
            if(enemy.kind!=EnemyKind::Boss)continue;
            auto found=boss_indices.find(enemy.id);
            if(found==boss_indices.end()){
                found=boss_indices.emplace(enemy.id,bosses.size()).first;
                bosses.push_back({{"entity_id",enemy.id},{"profile",int(enemy.boss_profile)},
                    {"name",Game::boss_profile_name(enemy.boss_profile)},{"sprite_key_from_core",Game::boss_sprite_key(enemy.boss_profile)},
                    {"spawn_time",game.metrics.boss_spawn_time},{"first_observed_time",game.total_elapsed()},
                    {"max_hp",enemy.max_hp},{"player_level_at_spawn",game.player.level},{"defeated",false},
                    {"defeat_time",nullptr},{"max_stage",0},{"stage_entries",json::array()},
                    {"stage_seconds",std::array<double,3>{}},{"telegraphs",json::array()},{"last_telegraph_serial",-1}});
            }
            auto& record=bosses[found->second];record["last_observed_time"]=game.total_elapsed();
            record["last_observed_hp_fraction"]=enemy.hp/std::max(1.f,enemy.max_hp);
            if(enemy.boss_stage>record["max_stage"].get<int>()){
                record["max_stage"]=enemy.boss_stage;
                record["stage_entries"].push_back({{"stage",enemy.boss_stage},{"time",game.total_elapsed()},
                    {"hp_fraction",enemy.hp/std::max(1.f,enemy.max_hp)}});
            }
            if(enemy.windup>0&&enemy.phase!=record["last_telegraph_serial"].get<int>()){
                record["last_telegraph_serial"]=enemy.phase;
                record["telegraphs"].push_back({{"time",game.total_elapsed()},{"attack",enemy.boss_attack},
                    {"name",Game::boss_attack_name(enemy.boss_attack)},{"stage",enemy.boss_stage}});
            }
        }
    };
    observe_bosses();
    while(true){
        while(game.state==State::Upgrade){
            const size_t selected=choose_fusion(game,sample,seed,required);const auto choice=game.choices[selected];
            json offers=json::array();for(const auto& offered:game.choices)offers.push_back(offered_choice(game,offered));
            json decision={{"time",game.total_elapsed()},{"player_level",game.player.level},{"selected_index",selected},
                {"offered",offers},{"selected",offered_choice(game,choice)},{"skills_before",skill_build(game)},
                {"hp_before",game.player.hp},{"active_slots_before",game.active_skill_count()}};
            if(!game.choosing_relic()&&first_choice<0)first_choice=game.total_elapsed();
            if(!game.choose_upgrade(selected))throw std::runtime_error("Core rejected a visible offered choice");
            decision["skills_after"]=skill_build(game);decision["hp_after"]=game.player.hp;
            decision["active_slots_after"]=game.active_skill_count();choices.push_back(std::move(decision));
            if(first_three_skills<0&&game.active_skill_count()>=3)first_three_skills=game.total_elapsed();
            if(++picked>1000)throw std::runtime_error("Upgrade selection did not terminate within bounded sample");
        }
        if(game.state==State::Victory||game.state==State::Defeat)break;
        if(game.state!=State::Playing)throw std::runtime_error("Unexpected state in Classic sample");
        if(game.total_elapsed()>config.duration+10||ticks>uint64_t((config.duration+15)*120))
            throw std::runtime_error("Natural terminal state overdue; sample stopped without forcing a result");
        if(ticks%decision_ticks==0){
            input=bot.steer(game);
            // Same nearby-object policy as Encounters. The inherited steering does
            // not seek wells or explicitly avoid an armed burst flower.
            const SceneObject* object=nullptr;float distance=76;
            for(const auto& candidate:game.scene_objects)if(candidate.state==SceneObjectState::Active&&candidate.kind!=SceneObjectKind::Mud){
                const float candidate_distance=length(candidate.pos-game.player.pos);
                if(candidate_distance<distance){distance=candidate_distance;object=&candidate;}
            }
            if(object){
                bool use=object->kind==SceneObjectKind::Crate||(object->kind==SceneObjectKind::HealingWell&&game.player.hp<game.player.max_hp-1);
                if(object->kind==SceneObjectKind::Barrel)use=std::any_of(game.enemies.begin(),game.enemies.end(),[&](const Enemy& enemy){return enemy.hp>0&&length(enemy.pos-object->pos)<118+enemy.radius;});
                input.interact=use;if(use)++interaction_inputs;
            }
        }
        const float before=game.total_elapsed(),hp_before=game.player.hp;
        const auto metrics_before=game.metrics;const auto skills_before=game.skills;
        const int old_event=int(game.special_event.kind);
        std::vector<std::pair<uint32_t,int>> active_bosses;
        for(const auto& enemy:game.enemies)if(enemy.kind==EnemyKind::Boss)active_bosses.emplace_back(enemy.id,enemy.boss_stage);
        game.update(dt,input);input.dash=input.interact=false;++ticks;
        const float after=game.total_elapsed();const double elapsed=after-before;
        if(!std::isfinite(after)||elapsed<0||!std::isfinite(game.player.hp)||!std::isfinite(game.player.pos.x)||!std::isfinite(game.player.pos.y))
            throw std::runtime_error("Non-finite or non-monotone simulation state");
        if(first_upgrade<0&&game.player.level>1)first_upgrade=after;
        const int minute=std::clamp(int((before+after)*.5f/60),0,xp_bucket_count-1);
        minute_seconds[minute]+=elapsed;minute_levels[minute]=game.player.level;
        minute_damage[minute]+=game.metrics.damage_taken-metrics_before.damage_taken;
        for(int i=0;i<skill_count;++i){
            if(skills_before[i]>0)owned_seconds[i]+=elapsed;
            if(skills_before[i]!=game.skills[i])automatic_growth.push_back({{"time",after},{"skill_key",Game::skill_key(Skill(i))},
                {"from",skills_before[i]},{"to",game.skills[i]},{"note","Core gameplay change outside offered-choice selection, e.g. a chest reward"}});
        }
        if(first_three_skills<0&&game.active_skill_count()>=3)first_three_skills=after;
        if(game.metrics.events_started>metrics_before.events_started)++event_started[int(game.special_event.kind)];
        if(game.metrics.events_succeeded>metrics_before.events_succeeded)++event_succeeded[old_event];
        if(game.metrics.events_failed>metrics_before.events_failed)++event_failed[old_event];
        const auto& director=game.director_state;const int kind=int(director.kind);
        if(director.announced)++announced[kind];if(director.started)++started[kind];
        if(director.ended)++ended[int(director.ended_kind)];
        if(director.phase==EncounterPhase::Active)active_seconds[kind]+=elapsed;
        if(director.announced||director.started||director.ended)encounters.push_back({{"time",after},{"id",director.encounter_id},
            {"kind",kind},{"name",EncounterDirector::encounter_name(director.kind)},{"phase",int(director.phase)},
            {"announced",director.announced},{"started",director.started},{"ended",director.ended},
            {"ended_kind",int(director.ended_kind)},{"aborted",director.aborted}});
        observe_bosses();
        for(const auto& previous:active_bosses){
            auto found=boss_indices.find(previous.first);if(found==boss_indices.end())continue;
            auto& record=bosses[found->second];const int stage=std::clamp(previous.second,1,3)-1;
            record["stage_seconds"][stage]=record["stage_seconds"][stage].get<double>()+elapsed;
            const bool still_present=std::any_of(game.enemies.begin(),game.enemies.end(),[&](const Enemy& enemy){return enemy.id==previous.first;});
            // Disappearance at a time-limit victory is not a kill. Classic has
            // one scheduled Boss; require the actual counter to increase.
            if(!still_present&&game.metrics.bosses_defeated>metrics_before.bosses_defeated){
                record["defeated"]=true;record["defeat_time"]=game.metrics.boss_defeat_time;
                record["confirmed_remaining_hp_fraction"]=0;
            }
        }
        const double damage=game.metrics.damage_taken-metrics_before.damage_taken;
        const double boss_damage=game.metrics.boss_damage_taken-metrics_before.boss_damage_taken;
        const double environment_damage=game.metrics.environment_damage_taken-metrics_before.environment_damage_taken;
        const double altar=game.metrics.altar_health_spent-metrics_before.altar_health_spent;
        while(!recent_damage.empty()&&recent_damage.front()["time"].get<float>()<after-10)recent_damage.pop_front();
        if(damage>0||altar>0){
            json hurt={{"time",after},{"hp_before",hp_before},{"hp_after",game.player.hp},{"actual_damage_taken",damage},
                {"boss_tagged_damage",boss_damage},{"environment_tagged_damage",environment_damage},{"altar_health_spent",altar},
                {"category",damage_category(damage,boss_damage,environment_damage,altar)}};
            recent_damage.push_back(hurt);if(game.state==State::Defeat)fatal_update=std::move(hurt);
        }
        const std::array<size_t,7> sizes={game.enemies.size(),game.projectiles.size(),game.gems.size(),game.effects.size(),
            game.items.size(),game.damage_numbers.size(),game.scene_objects.size()};
        for(size_t i=0;i<sizes.size();++i){peaks[i]=std::max(peaks[i],sizes[i]);
            if(sizes[i]>caps[i])throw std::runtime_error("Gameplay entity cap exceeded");}
    }
    const auto& metrics=game.metrics;
    json skills=json::array(),synergies=json::array(),encounter_summary=json::array(),event_summary=json::array();
    json per_minute=json::array(),visuals=json::array(),consumed=json::array(),last_damage=json::array();
    for(const auto& hurt:recent_damage)last_damage.push_back(hurt);
    for(int i=0;i<skill_count;++i)if(game.skill_allowed(Skill(i))){
        skills.push_back({{"key",Game::skill_key(Skill(i))},{"id",i},{"name",Game::skill_name(Skill(i))},
            {"level",game.skills[i]},{"fusion",Game::is_fusion(Skill(i))},{"evolved",game.evolved(Skill(i))},
            {"actual_hp_damage",metrics.skill_damage[i]},{"owned_seconds",owned_seconds[i]}});
        if(game.consumed_skills&(uint64_t(1)<<i))consumed.push_back(Game::skill_key(Skill(i)));
    }
    for(int i=0;i<synergy_count;++i)synergies.push_back({{"id",i},{"name",Game::synergy_name(i)},{"triggers",metrics.synergy_triggers[i]}});
    for(int i=1;i<int(EncounterKind::Count);++i)encounter_summary.push_back({{"kind",i},{"name",EncounterDirector::encounter_name(EncounterKind(i))},
        {"announced",announced[i]},{"started",started[i]},{"ended",ended[i]},{"active_seconds",active_seconds[i]}});
    for(int i=1;i<9;++i)event_summary.push_back({{"kind",i},{"name",Game::special_event_name(SpecialEventKind(i))},
        {"started",event_started[i]},{"succeeded",event_succeeded[i]},{"failed",event_failed[i]}});
    for(int i=0;i<xp_bucket_count;++i)if(minute_seconds[i]>0)per_minute.push_back({{"minute",i+1},{"exposure_seconds",minute_seconds[i]},
        {"end_level",minute_levels[i]},{"xp_acquired",metrics.xp_by_minute[i]},{"damage_taken",minute_damage[i]}});
    for(int i=0;i<enemy_visual_count;++i)if(visual_spawns[i])visuals.push_back({{"id",i},{"key",Game::enemy_visual_key(i)},
        {"name",Game::enemy_visual_name(i)},{"observed_unique_entities",visual_spawns[i]}});
    for(auto& boss:bosses)boss.erase("last_telegraph_serial");
    const bool victory=game.state==State::Victory;
    return {{"sample",index+1},{"class",class_names[int(sample.character)]},{"seed",seed},{"seed_mod_6",seed%6},
        {"route",sample.route},{"target_recipe",{{"first",Game::skill_key(sample.first)},{"second",Game::skill_key(sample.second)},
            {"result",Game::skill_key(sample.result)},{"material_level",required}}},{"preferred_new_skill",Game::skill_key(sample.new_skill)},
        {"mode","classic"},{"difficulty","normal"},{"permanent_talents","zero"},{"equipment_ids",{1,2,3}},
        {"initial_relic_unlock_mask",initial_relic_mask()},{"final_owned_relic_mask",game.owned_relic_mask()},
        {"outcome",victory?(game.boss_defeated?"boss_defeat_victory":"survival_time_victory"):"defeat"},
        {"victory",victory},{"boss_defeated",game.boss_defeated},{"bosses_defeated",metrics.bosses_defeated},
        {"survival_seconds",game.total_elapsed()},{"level",game.player.level},{"kills",game.kills},{"hp",game.player.hp},
        {"max_hp",game.player.max_hp},{"coins",game.coins},{"xp_acquired",metrics.xp_acquired},
        {"first_upgrade_time",first_upgrade},{"first_upgrade_choice_time",first_choice},{"first_three_skills_time",first_three_skills},
        {"first_fusion_time",metrics.first_fusion_time},{"fusions_created",metrics.fusions_created},
        {"target_fusion_obtained",game.skills[int(sample.result)]>0},{"skills",skills},{"consumed_materials",consumed},
        {"passives",game.passives},{"synergies",synergies},{"synergy_triggers",metrics.synergy_triggers},
        {"actual_enemy_hp_damage",std::accumulate(metrics.skill_damage.begin(),metrics.skill_damage.end(),metrics.other_damage)},
        {"other_damage",metrics.other_damage},{"damage_taken",metrics.damage_taken},{"boss_damage_taken",metrics.boss_damage_taken},
        {"environment_damage_taken",metrics.environment_damage_taken},{"altar_health_spent",metrics.altar_health_spent},
        {"fatal_damaging_update",fatal_update},{"damage_updates_in_last_10_seconds",last_damage},
        {"scheduled_boss_profile",int(scheduled)},{"boss_spawn_time",metrics.boss_spawn_time},{"boss_defeat_time",metrics.boss_defeat_time},
        {"bosses",bosses},{"boss_attacks",metrics.boss_attacks},{"boss_phase_changes",game.combat_diagnostics.boss_phase_changes},
        {"encounter_summary",encounter_summary},{"encounter_timeline",encounters},{"special_events",event_summary},
        {"observed_enemy_visuals",visuals},{"growth_choices",choices},{"automatic_growth",automatic_growth},{"per_minute",per_minute},
        {"items_collected",metrics.items_collected},{"relic_procs",metrics.relic_procs},{"equipment_procs",metrics.equipment_procs},
        {"dash_uses",metrics.dash_uses},{"interaction_inputs",interaction_inputs},{"healing_well_uses",game.combat_diagnostics.healing_uses},
        {"peak_containers",peaks},{"container_caps",caps},{"runner_ticks",ticks},{"simulation_steps",game.simulation_steps},
        {"process_cpu_seconds",fusion_cpu_seconds()-cpu_begin},
        {"headless_wall_seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-wall_begin).count()}};
}

void write_fusion_report(const std::string& path,const json& report) {
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    if(!output)throw std::runtime_error("Cannot open output JSON");
    output<<report.dump(2)<<'\n';if(!output)throw std::runtime_error("Cannot finish output JSON");
}
} // namespace

int main(int argc,char** argv) {
    std::string output="docs/fusion_simulation_runs.json";int selected=-1;json report;
    try{
        for(int i=1;i<argc;++i){
            const std::string argument=argv[i];
            if(argument=="--help"){
                std::cout<<"simulate_fusions [--output FILE] [--sample 1..6]\n"
                         <<"Default: six fixed Classic/Normal samples, two routes per class. No scan mode.\n";
                return 0;
            }
            if(i+1>=argc)throw std::runtime_error("Missing CLI value");
            if(argument=="--output")output=argv[++i];
            else if(argument=="--sample"){
                const std::string value=argv[++i];size_t consumed=0;selected=std::stoi(value,&consumed)-1;
                if(consumed!=value.size()||selected<0||selected>=6)throw std::runtime_error("sample must be 1..6");
            }else throw std::runtime_error("Unknown argument: "+argument);
        }
        json rules;const Config config=fusion_config(rules);
        report={{"schema_version",1},{"tool","MapleRGL simulate_fusions"},{"complete",false},
            {"expected_runs",selected<0?6:1},{"selected_sample",selected<0?json(nullptr):json(selected+1)},
            {"simulation_dt_seconds",dt},{"decision_frequency_hz",60/decision_ticks},
            {"rules_path",std::filesystem::absolute("assets/encounter_rules.json").u8string()},
            {"director_rules",rules},{"config",{{"duration",config.duration},{"boss_time",config.boss_time},
                {"director_boss_health_scale",config.director_boss_health_scale},{"enable_director",true},
                {"enable_scene_objects",true},{"enable_boss_phases",true}}},
            {"assumed_combat_view",{{"client_width",view_width},{"client_height",view_height},{"zoom",view_zoom}}},
            {"container_order",{"enemies","projectiles","gems","effects","items","damage_numbers","scene_objects"}},
            {"limitations",{
                "Six fixed heuristic samples, not player win rates or exhaustive balance coverage. A single-case rerun retains its original sample ID and seed.",
                "Seeds 20261112..20261117 have residues 0..5 for the normal Classic Boss pool. No enemy is spawned by the runner; death can prevent a scheduled Boss encounter.",
                "Movement is the unchanged simulate_balance Controller::steer at 10 Hz. The policy reads public current positions, not future RNG or candidate-world rollouts.",
                "Only start, set_combat_view, update(Input) and choose_upgrade mutate the game. No live profile, HP/XP/skill injection, forced Boss kill or altered duration.",
                "Fusion routes score only actual offers. Low HP may prioritize an offered recovery. Chests and random offers can alter the final build; fusion is not guaranteed.",
                "Nearby interaction is the Encounters policy; the inherited steering does not seek wells or explicitly evade armed burst flowers.",
                "First-time values of -1 mean not reached. Survival victory does not imply Boss defeat. Last observed Boss HP is a sample before removal, not an invented lethal-frame value.",
                "Death attribution uses metric increments during the final damaging update. Boss/environment tags can overlap; mixed categories do not identify an exact lethal actor or effect.",
                "Headless process CPU and wall time include logging but exclude graphics/audio/GUI. They are not FPS, frame-time or memory-leak evidence.",
                "No screenshot, human input, source artwork, save roundtrip, Campaign, Endless or Hard/Nightmare validation is performed by this runner."}},
            {"runs",json::array()}};
        write_fusion_report(output,report);
        for(int index=0;index<6;++index){
            if(selected>=0&&selected!=index)continue;
            auto result=sample_fusion(index,config);
            std::cout<<"sample "<<index+1<<" "<<result["class"].get<std::string>()<<" "
                     <<result["outcome"].get<std::string>()<<" fusion="<<result["first_fusion_time"]<<'\n';
            report["runs"].push_back(std::move(result));write_fusion_report(output,report);
        }
        report["complete"]=true;write_fusion_report(output,report);return 0;
    }catch(const std::exception& error){
        if(report.is_object()){
            report["complete"]=false;report["error"]=error.what();
            try{write_fusion_report(output,report);}catch(...){/* Keep the original diagnostic. */}
        }
        std::cerr<<"simulate_fusions: "<<error.what()<<'\n';return 1;
    }
}

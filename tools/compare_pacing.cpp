// One bounded record/replay pair, not a balance or natural-survival sample.
// Reuse only the existing visible-state movement policy. Never open a profile.
#define main expansion_reference_balance_main
#include "simulate_balance.cpp"
#undef main
#include "simulation_clock.hpp"
#include "progression.hpp"
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <type_traits>

namespace {
constexpr uint32_t comparison_seed=20261111;
constexpr CharacterClass comparison_class=CharacterClass::Mage;
constexpr uint64_t maximum_frames=90000,maximum_steps=36600,maximum_actions=512;
// A virtual 120 Hz presentation lets both passes deliver each recorded edge on
// the same 60 Hz simulation step, once per frame. The short check below also
// exercises multiple steps in one frame, including the eight-step work limit.
constexpr double virtual_frame_seconds=1.0/120.0,menu_action_seconds=.25;
constexpr float initial_invulnerability_seconds=630;
constexpr size_t domain_count=8;
const std::array<const char*,domain_count> domain_names={
    "player_and_progress","encounter_plan_and_director","enemies_and_boss_phases",
    "projectiles","effects_and_objects","experience_and_drops","scrolls_and_choices","skills_and_buffs"};

struct Fingerprint {
    uint64_t value=14695981039346656037ull;
    template<class T> void add(T item){
        static_assert(std::is_arithmetic<T>::value||std::is_enum<T>::value,"scalar only");
        unsigned char bytes[sizeof(T)];std::memcpy(bytes,&item,sizeof(item));
        for(unsigned char byte:bytes){value^=byte;value*=1099511628211ull;}
    }
    void add(Vec2 item){add(item.x);add(item.y);}
    void add(const std::string& item){add(uint64_t(item.size()));for(unsigned char byte:item)add(byte);}
    template<class T,class... Rest> void values(T first,Rest... rest){add(first);(add(rest),...);}
    template<class Container> void sequence(const Container& entries){add(uint64_t(entries.size()));for(auto entry:entries)add(entry);}
};
using Digests=std::array<uint64_t,domain_count>;
std::string hex_digest(uint64_t value){std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<value;return out.str();}
json digest_json(const Digests& values){json out=json::object();for(size_t i=0;i<domain_count;++i)out[domain_names[i]]=hex_digest(values[i]);return out;}

const FusionRecipe* comparison_recipe(Skill result){for(const auto& recipe:fusion_recipes())if(recipe.result==result)return &recipe;return nullptr;}
json scroll_json(const ScrollState& scrolls){
    json held=json::array(),offers=json::array(),completed=json::array(),studied=json::array();
    for(const auto& entry:scrolls.held)held.push_back({{"id",entry.id},{"result",Game::skill_key(entry.result)},{"form",entry.form},{"study_used",entry.study_used}});
    for(const auto& entry:scrolls.offers)offers.push_back({{"result",Game::skill_key(entry.result)},{"form",entry.form}});
    for(const auto& entry:scrolls.completed)completed.push_back({{"result",Game::skill_key(entry.result)},{"form",entry.form}});
    for(auto result:scrolls.studied_results)studied.push_back(Game::skill_key(result));
    return {{"held",held},{"offers",offers},{"completed",completed},{"foreign_class",scrolls.foreign_class},
        {"authorized_foreign",scrolls.authorized_foreign},{"learned_foreign",scrolls.learned_foreign},{"studied_results",studied},
        {"next_id",scrolls.next_id},{"failed_rewards",scrolls.failed_rewards},{"eligible_rewards",scrolls.eligible_rewards},
        {"opportunities",scrolls.opportunities},{"studies_used",scrolls.studies_used},{"replacements",scrolls.replacements},
        {"first_opportunity_time",scrolls.first_opportunity_time},{"challenge_offered",scrolls.challenge_offered}};
}
json choices_json(const Game& game){json result=json::array();for(const auto& choice:game.choices)result.push_back({
    {"kind",int(choice.kind)},{"skill",Game::skill_key(choice.skill)},{"relic",int(choice.relic)},
    {"scroll_id",choice.scroll_id},{"title",choice.title}});return result;}
json plan_json(const Game& game){const auto& p=game.encounter_plan;return {{"version",p.version},{"epoch",p.epoch},
    {"region",p.region},{"boss",p.boss},{"used_boss_mask",p.used_boss_mask},{"roster",p.roster}};}
json buffs_json(const Game& game){return {{"fury",game.fury_remaining},{"magnet",game.magnet_remaining},
    {"shield",game.shield_remaining},{"double_xp",game.double_xp_remaining},{"power",game.power_remaining},
    {"rift",game.rift_remaining},{"curse",game.curse_remaining},{"relic_haste",game.relic_haste_remaining()},
    {"invulnerability",game.player.invulnerable},{"guard_charge",game.guard_charge},{"dragon_soul",game.dragon_soul}};}

Digests state_digests(const Game& g){
    std::array<Fingerprint,domain_count> h;
    const auto& p=g.player;const auto& m=g.metrics;const auto& c=g.combat_diagnostics;
    h[0].values(g.state,g.elapsed,g.simulation_steps,p.pos,p.hp,p.max_hp,p.speed,p.xp,p.xp_next,p.level,
        p.dash_cooldown,p.invulnerable,p.animation_time,p.facing_left,p.moving,g.kills,g.coins,g.boss_spawned,g.boss_defeated);
    h[0].values(m.damage_taken,m.altar_health_spent,m.boss_damage_taken,m.events_started,m.events_succeeded,m.events_failed,
        m.dash_uses,m.items_collected,m.boss_attacks,m.bosses_defeated,m.boss_spawn_time,m.boss_defeat_time,
        m.fusions_created,m.first_fusion_time,m.relics_collected,m.environment_hits,m.environment_damage_taken);
    h[0].sequence(m.enemy_kills_by_kind);h[0].sequence(m.skill_damage);h[0].sequence(m.synergy_triggers);
    h[0].sequence(m.relic_procs);h[0].sequence(m.equipment_procs);h[0].sequence(m.content.boss_kills);h[0].sequence(m.content.enemy_kills);
    h[0].values(m.content.bosses_seen,m.content.enemies_seen,c.actual_hits,c.critical_hits,c.boss_phase_changes,c.boss_summons,
        c.scene_objects_created,c.objects_destroyed,c.barrels_exploded,c.healing_uses,c.object_damage);
    const auto& plan=g.encounter_plan;const auto& director=g.director_state;
    h[1].values(plan.version,plan.epoch,plan.region,plan.boss,plan.used_boss_mask);h[1].sequence(plan.roster);
    h[1].values(director.kind,director.ended_kind,director.phase,director.geometry,director.encounter_id,director.remaining,
        director.cooldown_remaining,director.pressure,director.escape_angle,director.escape_half_angle,director.min_spawn_distance,
        director.budget_remaining,director.announced,director.started,director.ended,director.aborted,director.important_active,
        director.allow_special_events,director.suppress_environment,uint64_t(director.spawns.size()));
    for(const auto& spawn:director.spawns)h[1].values(spawn.role,spawn.offset_x,spawn.offset_y,spawn.speed_scale,spawn.health_scale,spawn.encounter_id);
    const auto& event=g.special_event;h[1].values(event.kind,event.pos,event.remaining,event.progress,event.tick,event.target,event.spawned,
        event.activated,event.scroll_challenge,g.next_event_in);h[1].sequence(event.guardians);
    h[2].add(uint64_t(g.enemies.size()));for(const auto& e:g.enemies)h[2].values(e.id,e.kind,e.pos,e.velocity,e.hp,e.max_hp,e.speed,e.radius,
        e.damage,e.age,e.hit_flash,e.attack_clock,e.facing_left,e.slow_remaining,e.slow_factor,e.burn_remaining,e.burn_tick,e.burn_damage,
        e.windup,e.action_remaining,e.shield_remaining,e.buff_remaining,e.attack_target,e.attack_direction,e.phase,e.burn_source,
        e.boss_profile,e.boss_stage,e.boss_attack,e.attack_cooldown,e.recovery_remaining,e.visual,e.attack_animation_time,
        e.action_duration,e.action_tick,e.action_step,e.attack_start,e.attack_end,e.mark_remaining,e.frost_remaining,e.armor_break_remaining,
        e.mark_source,e.frost_source,e.armor_source,e.frost_layers,e.poison_remaining,e.poison_tick,e.poison_damage,e.poison_layers,e.poison_spread,e.poison_source,
        e.lure_remaining,e.lure_point,e.lure_source,e.thermal_side,e.thermal_lock,e.thermal_source);
    h[3].add(uint64_t(g.projectiles.size()));for(const auto& q:g.projectiles){h[3].values(q.pos,q.velocity,q.skill,q.damage,q.radius,q.age,
        q.lifetime,q.pierce,q.target,q.evolved,q.returned,q.bounces,q.proc_generated,q.blocks);h[3].sequence(q.hit_ids);}
    h[4].add(uint64_t(g.effects.size()));for(const auto& e:g.effects)h[4].values(e.kind,e.pos,e.end,e.radius,e.age,e.duration,e.damage,e.tick,
        e.evolved,e.enemy_kind,e.angle,e.source,e.hostile,e.delay,e.velocity,e.triggered,e.pattern,e.environment,e.proc_generated,
        e.boss_profile,e.enemy_visual,e.count,e.target_id,e.secondary_tick,e.energy);
    h[4].add(uint64_t(g.scene_objects.size()));for(const auto& o:g.scene_objects)h[4].values(o.id,o.kind,o.state,o.pos,o.radius,o.hp,o.max_hp,
        o.age,o.hit_flash,o.countdown,o.lifetime,o.source);
    h[4].add(uint64_t(g.events.size()));for(const auto& e:g.events)h[4].values(e.kind,e.pos,e.actual_damage,e.critical,e.source,e.target_id,e.target_kind,e.secondary);
    h[5].values(m.xp_acquired,m.items_collected,g.coins,uint64_t(g.gems.size()));h[5].sequence(m.xp_by_minute);
    for(const auto& gem:g.gems)h[5].values(gem.pos,gem.value,gem.age);
    h[5].add(uint64_t(g.items.size()));for(const auto& item:g.items)h[5].values(item.kind,item.pos,item.age);
    const auto& s=g.scrolls;h[6].values(s.foreign_class,s.authorized_foreign,s.learned_foreign,s.next_id,s.failed_rewards,s.eligible_rewards,
        s.opportunities,s.studies_used,s.replacements,s.first_opportunity_time,s.challenge_offered,uint64_t(s.held.size()));
    for(const auto& entry:s.held)h[6].values(entry.id,entry.result,entry.form,entry.study_used);
    h[6].add(uint64_t(s.offers.size()));for(const auto& entry:s.offers)h[6].values(entry.result,entry.form);
    h[6].add(uint64_t(s.completed.size()));for(const auto& entry:s.completed)h[6].values(entry.result,entry.form);h[6].sequence(s.studied_results);
    h[6].add(uint64_t(g.choices.size()));for(const auto& choice:g.choices)h[6].values(choice.kind,choice.skill,choice.relic,choice.scroll_id);
    h[6].values(g.pending_study.active,g.pending_study.choice_index,g.pending_study.scroll_id,g.pending_study.learn,g.pending_study.replace);
    h[7].sequence(g.skills);h[7].sequence(g.passives);h[7].sequence(g.synergy_flash);
    h[7].values(g.consumed_skills,g.guard_charge,g.dragon_soul,g.battle_focus,g.fury_remaining,g.magnet_remaining,g.shield_remaining,
        g.double_xp_remaining,g.power_remaining,g.rift_remaining,g.curse_remaining,g.relic_haste_remaining(),g.damage_multiplier,
        g.attack_speed,g.area_multiplier,g.pickup_radius,g.extra_projectiles,g.cooldown_multiplier,g.duration_multiplier,g.crit_chance,
        g.crit_multiplier,g.overcharge_levels,g.vitality_levels,g.owned_relic_mask());
    for(int i=0;i<skill_count;++i)h[7].values(g.skill_cooldown_remaining(Skill(i)),g.skill_cooldown_total(Skill(i)),g.skill_active_remaining(Skill(i)));
    Digests out{};for(size_t i=0;i<domain_count;++i)out[i]=h[i].value;return out;
}

Config comparison_config(json& rules){
    std::ifstream input("assets/encounter_rules.json");if(!input)throw std::runtime_error("Run from the directory containing assets/encounter_rules.json");
    input>>rules;if(rules.value("schema",0)!=1)throw std::runtime_error("Unsupported director rules schema");
    Config config;config.enable_director=config.enable_scene_objects=config.enable_boss_phases=true;auto& d=config.director_config;
#define SPEED_DIRECTOR(name) d.name=rules.value(#name,d.name)
    SPEED_DIRECTOR(first_encounter);SPEED_DIRECTOR(encounter_gap_min);SPEED_DIRECTOR(encounter_gap_max);SPEED_DIRECTOR(min_safe_distance);
    SPEED_DIRECTOR(escape_half_angle);SPEED_DIRECTOR(low_health);SPEED_DIRECTOR(high_pressure);SPEED_DIRECTOR(breather_duration);
    SPEED_DIRECTOR(boss_telegraph);SPEED_DIRECTOR(max_spawns_per_update);
#undef SPEED_DIRECTOR
    static const char* keys[]={"swarm","encirclement","rush","elite_hunt","crisis","breather"};
    for(const auto& entry:rules.at("rules")){int index=-1;for(int i=0;i<6;++i)if(entry.at("kind")==keys[i])index=i;
        if(index<0)throw std::runtime_error("Unknown director rule");auto& rule=d.rules[size_t(index)];
#define SPEED_RULE(name) rule.name=entry.value(#name,rule.name)
        SPEED_RULE(weight);SPEED_RULE(unlock_progress);SPEED_RULE(telegraph);SPEED_RULE(duration);SPEED_RULE(cooldown);
        SPEED_RULE(spawn_interval);SPEED_RULE(base_budget);SPEED_RULE(batch_size);
#undef SPEED_RULE
    }return config;
}
void start_comparison(Game& game){
    RunSetup setup;setup.mode=RunMode::Classic;setup.difficulty=Difficulty::Normal;setup.enable_loot=true;
    setup.unlocked_relic_mask=initial_relic_mask();setup.equipment={EquipmentId::TravelerKnife,EquipmentId::LeatherVest,EquipmentId::AmberPendant};
    game.start(comparison_class,comparison_seed,{},setup);game.set_combat_view(1280/(2*1.6f),800/(2*1.6f));
    // The sole fixture mutation, identical in both passes. Never refreshed later.
    game.player.invulnerable=initial_invulnerability_seconds;
}

enum class ActionKind { Upgrade,Scroll,Decline,SelectReplacement,ConfirmReplacement,Pause,Fuse,Discard,Resume };
const char* action_name(ActionKind kind){static const char* names[]={"upgrade","scroll","decline_scroll","select_replacement",
    "confirm_replacement","pause","confirm_fusion","discard_scroll","resume"};return names[int(kind)];}
struct Action {uint64_t step=0;ActionKind kind=ActionKind::Upgrade;size_t index=0;uint32_t scroll_id=0;Skill skill=Skill::Count;};
struct Tape {std::vector<Input> inputs;std::vector<Digests> states;std::vector<Action> actions;std::vector<json> audits;};
json decision_state(const Game& game){return {{"state",int(game.state)},{"skills",game.skills},{"passives",game.passives},
    {"coins",game.coins},{"level",game.player.level},{"xp",game.player.xp},{"scrolls",scroll_json(game.scrolls)},
    {"choices",choices_json(game)},{"pending_study",{{"active",game.pending_study.active},{"learn",int(game.pending_study.learn)},
    {"replace",int(game.pending_study.replace)},{"scroll_id",game.pending_study.scroll_id}}}};}
json apply_action(Game& game,const Action& action){
    json audit={{"before_simulation_step",action.step},{"kind",action_name(action.kind)},{"index",action.index},
        {"scroll_id",action.scroll_id},{"skill",int(action.skill)},{"before",decision_state(game)}};bool ok=false;
    switch(action.kind){
    case ActionKind::Upgrade:ok=game.choose_upgrade(action.index);break;
    case ActionKind::Scroll:ok=game.choose_scroll(action.index);break;
    case ActionKind::Decline:ok=game.decline_scroll_offer();break;
    case ActionKind::SelectReplacement:ok=game.select_study_replacement(action.skill);break;
    case ActionKind::ConfirmReplacement:ok=game.confirm_study_replacement();break;
    case ActionKind::Pause:ok=game.state==State::Playing;if(ok)game.toggle_pause();break;
    case ActionKind::Fuse:ok=game.confirm_scroll_fusion(action.scroll_id);break;
    case ActionKind::Discard:ok=game.discard_scroll(action.scroll_id);break;
    case ActionKind::Resume:ok=game.state==State::Paused;if(ok)game.toggle_pause();break;
    }
    if(!ok)throw std::runtime_error(std::string("Recorded/selected action rejected: ")+action_name(action.kind));
    audit["after"]=decision_state(game);return audit;
}
size_t choose_comparison_upgrade(const Game& game){
    size_t selected=0;int best=-100000;
    for(size_t i=0;i<game.choices.size();++i){const auto& choice=game.choices[i];int score=50;
        if(choice.kind==UpgradeKind::Study)score=400;
        else if(choice.kind==UpgradeKind::Relic)score=100;
        else if(choice.kind==UpgradeKind::Skill){score=game.skills[int(choice.skill)]==0?100:90;
            if(Game::is_fusion(choice.skill))score=150;
            for(const auto& scroll:game.scrolls.held){const auto* recipe=comparison_recipe(scroll.result);
                if(recipe&&(recipe->first==choice.skill||recipe->second==choice.skill)&&game.skills[int(choice.skill)]<recipe->material_level)score=350;}
        }else if(choice.kind==UpgradeKind::Damage||choice.kind==UpgradeKind::AttackSpeed||choice.kind==UpgradeKind::Cooldown)score=85;
        else if(choice.kind==UpgradeKind::Magnet&&game.passives[int(choice.kind)]<2)score=110;
        if(score>best){best=score;selected=i;}
    }return selected;
}
size_t choose_comparison_scroll(const Game& game){
    size_t selected=0;int best=-1;for(size_t i=0;i<game.scrolls.offers.size();++i){const auto* recipe=comparison_recipe(game.scrolls.offers[i].result);
        if(!recipe)continue;const int score=game.skills[int(recipe->first)]+game.skills[int(recipe->second)];
        if(score>best){best=score;selected=i;}}
    return selected;
}
Skill choose_replacement(const Game& game){
    const FusionRecipe* active=nullptr;for(const auto& scroll:game.scrolls.held)if(scroll.id==game.pending_study.scroll_id)active=comparison_recipe(scroll.result);
    Skill selected=Skill::Count;int lowest=100000;
    for(int i=0;i<skill_count;++i){const auto skill=Skill(i);if(!game.skills[i]||Game::is_fusion(skill)||(active&&(active->first==skill||active->second==skill)))continue;
        int cost=game.skills[i];for(const auto& scroll:game.scrolls.held){const auto* recipe=comparison_recipe(scroll.result);if(recipe&&(recipe->first==skill||recipe->second==skill))cost+=20;}
        if(cost<lowest){lowest=cost;selected=skill;}}
    if(selected==Skill::Count)throw std::runtime_error("No legal replacement for offered full-slot study");return selected;
}
Input comparison_input(Controller& controller,Game& game,uint64_t step,Input previous){
    previous.dash=previous.interact=false;if(step%decision_ticks!=0)return previous;
    auto input=controller.steer(game);const SceneObject* nearest=nullptr;float distance=76;
    for(const auto& object:game.scene_objects)if(object.state==SceneObjectState::Active&&object.kind!=SceneObjectKind::Mud){float d=length(object.pos-game.player.pos);if(d<distance){distance=d;nearest=&object;}}
    if(nearest){input.interact=nearest->kind==SceneObjectKind::Crate||(nearest->kind==SceneObjectKind::HealingWell&&game.player.hp<game.player.max_hp-1);
        if(nearest->kind==SceneObjectKind::Barrel)input.interact=std::any_of(game.enemies.begin(),game.enemies.end(),[&](const Enemy& enemy){return enemy.hp>0&&length(enemy.pos-nearest->pos)<118+enemy.radius;});}
    return input;
}

struct Observations {
    std::set<uint32_t> enemies_seen;
    using ItemKey=std::tuple<int,float,float,int64_t>;
    std::map<ItemKey,int> items_seen;
    std::array<uint64_t,enemy_kind_count> enemies_by_kind{};
    std::array<uint64_t,32> enemies_by_visual{};
    std::array<uint64_t,int(ItemKind::Count)> item_births_observed{};
    std::array<uint64_t,int(EventKind::EventComplete)+1> event_notifications{};
    std::map<uint32_t,std::pair<int,int>> boss_states;
    uint64_t event_cap_frames=0;int old_events_started=0,old_events_succeeded=0,old_events_failed=0,old_opportunities=0;
    SpecialEventKind previous_event=SpecialEventKind::None;
    std::array<size_t,5> peaks{};
    json director=json::array(),bosses=json::array(),special_events=json::array(),scroll_opportunities=json::array();
    void observe(const Game& game,uint64_t step){
        for(const auto& enemy:game.enemies){if(enemies_seen.insert(enemy.id).second){++enemies_by_kind[int(enemy.kind)];if(enemy.kind!=EnemyKind::Boss&&enemy.visual>=0&&enemy.visual<32)++enemies_by_visual[enemy.visual];}
            if(enemy.kind==EnemyKind::Boss){const auto status=std::make_pair(enemy.boss_stage,enemy.phase);auto found=boss_states.find(enemy.id);
                if(found==boss_states.end()||found->second!=status){bosses.push_back({{"step",step},{"game_time",game.elapsed},{"id",enemy.id},{"profile",int(enemy.boss_profile)},
                    {"stage",enemy.boss_stage},{"attack_phase",enemy.phase},{"attack",enemy.boss_attack},{"hp",enemy.hp},{"max_hp",enemy.max_hp}});boss_states[enemy.id]=status;}}
        }
        std::map<ItemKey,int> current_items;
        for(const auto& item:game.items){const int64_t birth=int64_t(step)-std::llround(double(item.age)/SimulationClock::step_seconds);++current_items[{int(item.kind),item.pos.x,item.pos.y,birth}];}
        for(const auto& entry:current_items){int& observed=items_seen[entry.first];if(entry.second>observed){item_births_observed[size_t(std::get<0>(entry.first))]+=entry.second-observed;observed=entry.second;}}
        for(const auto& event:game.events)++event_notifications[int(event.kind)];if(game.events.size()>=64)++event_cap_frames;
        const std::array<size_t,5> sizes={game.enemies.size(),game.projectiles.size(),game.effects.size(),game.items.size(),game.gems.size()};for(size_t i=0;i<sizes.size();++i)peaks[i]=std::max(peaks[i],sizes[i]);
        const auto& d=game.director_state;if(d.announced||d.started||d.ended)director.push_back({{"step",step},{"game_time",game.elapsed},{"id",d.encounter_id},
            {"kind",int(d.kind)},{"phase",int(d.phase)},{"announced",d.announced},{"started",d.started},{"ended",d.ended},{"aborted",d.aborted},{"ended_kind",int(d.ended_kind)}});
        if(game.metrics.events_started!=old_events_started||game.metrics.events_succeeded!=old_events_succeeded||game.metrics.events_failed!=old_events_failed)
            special_events.push_back({{"step",step},{"game_time",game.elapsed},{"previous_kind",int(previous_event)},{"current_kind",int(game.special_event.kind)},
                {"started",game.metrics.events_started},{"succeeded",game.metrics.events_succeeded},{"failed",game.metrics.events_failed}});
        old_events_started=game.metrics.events_started;old_events_succeeded=game.metrics.events_succeeded;old_events_failed=game.metrics.events_failed;previous_event=game.special_event.kind;
        if(game.scrolls.opportunities!=old_opportunities){scroll_opportunities.push_back({{"step",step},{"game_time",game.elapsed},{"scrolls",scroll_json(game.scrolls)}});old_opportunities=game.scrolls.opportunities;}
    }
    json summary(const Game& game)const{return {{"enemy_entities_observed",enemies_seen.size()},{"enemies_by_kind_observed",enemies_by_kind},
        {"enemies_by_visual_observed",enemies_by_visual},{"item_births_observed_by_kind",item_births_observed},{"live_population_peaks",peaks},
        {"shot_notifications_observed",event_notifications[int(EventKind::Shot)]},{"notifications_by_kind",event_notifications},{"notification_cap_frames",event_cap_frames},
        {"actual_hits_core_counter",game.combat_diagnostics.actual_hits},{"critical_hits_core_counter",game.combat_diagnostics.critical_hits},
        {"skill_damage_core_totals",game.metrics.skill_damage},{"xp_acquired_core_total",game.metrics.xp_acquired},{"xp_by_minute",game.metrics.xp_by_minute},
        {"items_collected_core_total",game.metrics.items_collected},{"enemy_kills_core_totals",game.metrics.enemy_kills_by_kind},
        {"director_transitions",director},{"boss_phase_entries",bosses},{"special_events",special_events},{"scroll_opportunities",scroll_opportunities}};}
};
json checkpoint(const Game& game,uint64_t step,const Digests& rolling){
    return {{"step",step},{"game_time",game.elapsed},{"state",int(game.state)},{"plan",plan_json(game)},
        {"level",game.player.level},{"xp",game.player.xp},{"xp_acquired",game.metrics.xp_acquired},{"kills",game.kills},{"coins",game.coins},
        {"skills",game.skills},{"passives",game.passives},{"skill_damage",game.metrics.skill_damage},{"actual_hits",game.combat_diagnostics.actual_hits},
        {"buffs",buffs_json(game)},{"scrolls",scroll_json(game.scrolls)},{"boss_spawned",game.boss_spawned},{"boss_defeated",game.boss_defeated},
        {"boss_attacks",game.metrics.boss_attacks},{"events_started",game.metrics.events_started},{"events_succeeded",game.metrics.events_succeeded},
        {"events_failed",game.metrics.events_failed},{"items_collected",game.metrics.items_collected},{"trajectory_digests",digest_json(rolling)}};
}
json terminal_summary(const Game& game){
    const bool victory=game.state==State::Victory;const auto report=make_run_report(game,victory?RunOutcome::Victory:RunOutcome::Defeat,comparison_seed);
    return {{"state",int(game.state)},{"outcome",run_outcome_name(report.outcome)},{"game_time",game.total_elapsed()},
        {"base_settlement_crystals",settlement_crystals(report)},{"boss_defeated",report.boss_defeated},{"bosses_defeated",report.bosses_defeated},
        {"kills",report.kills},{"elites",report.elite_kills},{"events_succeeded",report.events_succeeded},{"level",report.level},
        {"skills",report.skills},{"passives",report.passives},{"skill_damage",report.skill_damage},{"xp_acquired",report.xp_acquired},
        {"items_collected",report.items_collected},{"damage_dealt",report.damage_dealt},{"damage_taken",report.damage_taken},
        {"consumed_skills",report.consumed_skills},{"fusions_created",report.fusions_created},{"synergies",report.synergy_triggers},
        {"scrolls",scroll_json(report.scrolls)},{"plan",plan_json(game)}};
}

struct Pass {json report;double effective_real=0;uint64_t steps=0;bool completed=false;};
Pass compare_pass(int speed,const Config& config,Tape& tape,bool recording){
    Game game(config);start_comparison(game);game.timing.speed=speed;game.timing.recorded=true;
    SimulationClock clock;Controller controller{2,2,comparison_seed};Input last_input{};
    Observations observed;uint64_t frames=0,ui_frames=0,steps=0;size_t action_cursor=0;
    double effective_real=0,total_real=0;std::array<Fingerprint,domain_count> trajectory;
    auto rolling=[&](){Digests value{};for(size_t i=0;i<domain_count;++i)value[i]=trajectory[i].value;return value;};
    json checkpoints=json::array(),audits=json::array(),difference=nullptr;std::string failure;
    checkpoints.push_back(checkpoint(game,0,rolling()));
    auto act=[&](Action action){
        action.step=steps;if(++action_cursor>maximum_actions)throw std::runtime_error("UI action bound exceeded");
        clock.suspend();ui_frames+=uint64_t(std::llround(menu_action_seconds/virtual_frame_seconds));total_real+=menu_action_seconds;
        auto audit=apply_action(game,action);audits.push_back(audit);
        if(recording){tape.actions.push_back(action);tape.audits.push_back(std::move(audit));}
        else if(audit!=tape.audits[action_cursor-1])throw std::runtime_error("UI action state diverged at recorded simulation step");
    };
    auto resolve_recorded_ui=[&](){
        for(size_t guard=0;guard<maximum_actions;++guard){
            if(game.state==State::ScrollChoice){Action action;action.kind=game.scrolls.offers.empty()?ActionKind::Decline:ActionKind::Scroll;action.index=choose_comparison_scroll(game);act(action);continue;}
            if(game.state==State::Upgrade){
                if(game.pending_study.active){Action pick;pick.kind=ActionKind::SelectReplacement;pick.skill=choose_replacement(game);act(pick);Action confirm;confirm.kind=ActionKind::ConfirmReplacement;act(confirm);}
                else{if(game.choices.empty())throw std::runtime_error("Upgrade has no choices");Action pick;pick.kind=ActionKind::Upgrade;pick.index=choose_comparison_upgrade(game);act(pick);}continue;
            }
            if(game.state==State::Playing){uint32_t id=0;ActionKind decision=ActionKind::Fuse;
                for(const auto& scroll:game.scrolls.held)if(!game.scroll_reachable(scroll.result)){id=scroll.id;decision=ActionKind::Discard;break;}
                if(!id)for(const auto& scroll:game.scrolls.held)if(game.fusion_ready(scroll.result)){id=scroll.id;break;}
                if(id){Action pause;pause.kind=ActionKind::Pause;act(pause);Action pick;pick.kind=decision;pick.scroll_id=id;act(pick);Action resume;resume.kind=ActionKind::Resume;act(resume);continue;}
            }return;
        }throw std::runtime_error("Menu resolution bound exceeded");
    };
    try{
        while(frames+ui_frames<maximum_frames&&steps<maximum_steps){
            if(recording)resolve_recorded_ui();
            else while(action_cursor<tape.actions.size()&&tape.actions[action_cursor].step==steps){const Action action=tape.actions[action_cursor];act(action);}
            if(game.state==State::Victory||game.state==State::Defeat)break;
            if(game.state!=State::Playing)throw std::runtime_error("Unexpected state without a recorded UI decision");
            ++frames;effective_real+=virtual_frame_seconds;total_real+=virtual_frame_seconds;
            clock.advance(virtual_frame_seconds,speed,[&](float step_dt){
                if(steps>=maximum_steps)throw std::runtime_error("Simulation step bound exceeded");
                Input input;
                if(recording){input=comparison_input(controller,game,steps,last_input);tape.inputs.push_back(input);last_input=input;}
                else{if(steps>=tape.inputs.size())throw std::runtime_error("Replay exhausted recorded inputs");input=tape.inputs[size_t(steps)];}
                game.update(step_dt,input);++steps;observed.observe(game,steps);const auto state=state_digests(game);
                for(size_t i=0;i<domain_count;++i)trajectory[i].add(state[i]);
                if(recording)tape.states.push_back(state);
                else if(state!=tape.states[size_t(steps-1)]){json domains=json::array();for(size_t i=0;i<domain_count;++i)if(state[i]!=tape.states[size_t(steps-1)][i])domains.push_back(domain_names[i]);difference={{"step",steps},{"game_time",game.elapsed},{"domains",domains},{"reference",digest_json(tape.states[size_t(steps-1)])},{"replay",digest_json(state)}};}
                if(steps%3600==0)checkpoints.push_back(checkpoint(game,steps,rolling()));
                return difference.is_null()&&game.state==State::Playing;
            });
            if(!difference.is_null())break;
        }
    }catch(const std::exception& error){failure=error.what();}
    if(failure.empty()&&difference.is_null()&&game.state!=State::Victory&&game.state!=State::Defeat)failure="Frame/step bound reached before normal terminal state";
    if(!recording&&failure.empty()&&(steps!=tape.inputs.size()||action_cursor!=tape.actions.size()))failure="Replay did not consume the complete recorded operation tape";
    game.timing.effective_real_seconds=effective_real;game.timing.total_real_seconds=total_real;game.timing.used_speeds=steps?1u<<(speed-1):0;
    checkpoints.push_back(checkpoint(game,steps,rolling()));
    const bool timeline_satisfied=game.elapsed>=config.duration||game.boss_defeated;
    const bool completed=failure.empty()&&difference.is_null()&&game.state==State::Victory&&timeline_satisfied;
    Pass out;out.steps=steps;out.effective_real=effective_real;out.completed=completed;
    out.report={{"speed",speed},{"completed",completed},{"full_timeline_or_core_boss_victory",timeline_satisfied},{"failure",failure},{"first_divergence",difference},
        {"virtual_render_frames",frames},{"virtual_menu_frames",ui_frames},{"simulation_steps",steps},{"fixed_game_seconds",steps*SimulationClock::step_seconds},
        {"virtual_effective_real_seconds",effective_real},{"virtual_total_real_seconds",total_real},{"virtual_menu_seconds",action_cursor*menu_action_seconds},
        {"clock_peak_backlog_seconds",clock.peak_backlog_seconds()},{"clock_limited_frames",clock.limited_frames()},
        {"checkpoints",checkpoints},{"observations",observed.summary(game)},{"actions",audits},{"terminal",terminal_summary(game)}};
    return out;
}

json short_clock_check(const Config& config){
    Game game(config);game.start(CharacterClass::Archer,comparison_seed);SimulationClock clock;json segments=json::array();
    bool speed_ok=true;for(int speed:{1,2,1}){const auto before=game.simulation_steps;for(int frame=0;frame<10;++frame)
        clock.advance(1.0/60.0,speed,[&](float step_dt){game.update(step_dt,{});return game.state==State::Playing;});
        const auto count=game.simulation_steps-before;speed_ok&=count==uint64_t(10*speed);segments.push_back({{"speed",speed},{"real_frames",10},{"simulation_steps",count}});}
    const auto dash_before=game.metrics.dash_uses;bool dash=true,interact=true;int dash_deliveries=0,interaction_deliveries=0;
    const auto capped=clock.advance(.25,2,[&](float step_dt){Input input{{1,0},dash,interact};dash_deliveries+=int(input.dash);interaction_deliveries+=int(input.interact);game.update(step_dt,input);dash=interact=false;return game.state==State::Playing;});
    const double backlog=clock.backlog_seconds();const bool cap_ok=capped==SimulationClock::max_steps_per_frame&&std::abs(backlog-(.5-8*SimulationClock::step_seconds))<1e-9;
    game.toggle_pause();clock.suspend();const auto paused_steps=game.simulation_steps;const float paused_time=game.elapsed;
    // Five virtual seconds with no clock advance, matching App's paused branch.
    game.update(float(SimulationClock::step_seconds),{});
    const bool pause_ok=game.state==State::Paused&&game.simulation_steps==paused_steps&&game.elapsed==paused_time&&clock.backlog_seconds()==0;
    game.toggle_pause();const auto resumed=clock.advance(1.0/60.0,1,[&](float step_dt){game.update(step_dt,{});return game.state==State::Playing;});
    const bool resume_ok=resumed==1&&clock.backlog_seconds()<1e-9;
    const bool edges_ok=dash_deliveries==1&&interaction_deliveries==1&&game.metrics.dash_uses-dash_before==1;
    return {{"normal_unprotected_segment",true},{"speed_segments",segments},{"speed_switch_steps_correct",speed_ok},
        {"limited_frame_steps",capped},{"retained_backlog_seconds",backlog},{"eight_step_limit_and_backlog_correct",cap_ok},
        {"one_shot_dash_deliveries",dash_deliveries},{"one_shot_interact_deliveries",interaction_deliveries},{"one_shots_once_per_frame",edges_ok},
        {"virtual_pause_seconds",5},{"pause_stopped_game_clock",pause_ok},{"resume_steps",resumed},{"resume_without_catchup",resume_ok},
        {"passed",speed_ok&&cap_ok&&pause_ok&&resume_ok&&edges_ok}};
}
json input_tape_json(const Tape& tape){
    json changes=json::array();Input previous{};for(size_t i=0;i<tape.inputs.size();++i){const auto& input=tape.inputs[i];
        if(i==0||input.move.x!=previous.move.x||input.move.y!=previous.move.y||input.dash!=previous.dash||input.interact!=previous.interact)
            changes.push_back({i,input.move.x,input.move.y,input.dash,input.interact});previous=input;}
    return {{"encoding","zero-based simulation-step changes; carry values until the next entry"},{"columns",{"step","move_x","move_y","dash","interact"}},
        {"total_steps",tape.inputs.size()},{"changes",changes}};
}
}

int main(int argc,char** argv){
    std::filesystem::path output="docs/expansion_speed_comparison.json";json report;int code=1;
    try{
        for(int i=1;i<argc;++i){const std::string argument=argv[i];if(argument=="--output"&&i+1<argc)output=argv[++i];
            else if(argument=="--help"){std::cout<<"compare_pacing [--output path.json]\nOne fixed-seed protected timing fixture at 1x, then one identical-operation replay at 2x.\n";return 0;}
            else throw std::runtime_error("Expected --output path.json or --help; seed and run count are fixed");}
        json rules;const Config config=comparison_config(rules);Tape tape;const auto reference=compare_pass(1,config,tape,true);
        Pass replay;if(reference.completed)replay=compare_pass(2,config,tape,false);else replay.report={{"completed",false},{"skipped","The single reference pass did not reach its normal terminal state; no retry or seed search."}};
        const auto short_check=short_clock_check(config);
        const bool same=reference.completed&&replay.completed&&reference.report["checkpoints"]==replay.report["checkpoints"]&&
            reference.report["observations"]==replay.report["observations"]&&reference.report["actions"]==replay.report["actions"]&&reference.report["terminal"]==replay.report["terminal"];
        const double ratio=reference.effective_real>0?replay.effective_real/reference.effective_real:0;
        const bool half_time=replay.completed&&std::abs(ratio-.5)<.01;
        const bool passed=same&&half_time&&short_check["passed"].get<bool>();code=passed?0:1;
        report={{"schema_version",1},{"tool","compare_pacing"},{"passed",passed},{"same_operations_and_state",same},{"two_x_approximately_half_virtual_effective_time",half_time},
            {"two_x_to_one_x_virtual_effective_ratio",ratio},{"seed",comparison_seed},{"mode",Game::mode_name(RunMode::Classic)},
            {"class_id",int(comparison_class)},{"fixed_step_seconds",SimulationClock::step_seconds},{"virtual_frame_seconds",virtual_frame_seconds},
            {"bounds",{{"frames_per_speed",maximum_frames},{"steps_per_speed",maximum_steps},{"ui_actions_per_speed",maximum_actions}}},
            {"fixture",{{"kind","time-equivalence fixture; not natural completion or a player win-rate sample"},{"initial_invulnerability_game_seconds",initial_invulnerability_seconds},
                {"target_game_seconds",config.duration},{"terminal_policy","Respect the core's earlier Boss victory, otherwise the complete 600-second timeline; never force a terminal state."},
                {"menu_action_virtual_seconds",menu_action_seconds},{"rendering_audio_hitstop","Excluded; these are virtual time budgets, not measured FPS or wall-clock performance."}}},
            {"observation_limits",{"Enemy counts include entity IDs observed after fixed steps; transient spawn-and-remove within one step is not counted.",
                "Item births are observed by kind, position and inferred birth step; collected-before-observation drops are not included.",
                "Shot notifications have no skill source and share a 64-event queue. Cap-touch frames are reported; these are not universal cast totals.",
                "Actual hit counters, per-skill damage, XP, kills and base settlement come directly from the core. No profile or permanent achievement reward is created.",
                "Per-step eight-domain fingerprints compare public state at every simulation step; checkpoints also expose actual values each 3600 steps and at termination."}},
            {"director_rules",rules},{"input_tape",input_tape_json(tape)},{"one_x",reference.report},{"two_x",replay.report},{"short_clock_check",short_check}};
    }catch(const std::exception& error){report={{"schema_version",1},{"tool","compare_pacing"},{"passed",false},{"error",error.what()}};}
    try{if(!output.parent_path().empty())std::filesystem::create_directories(output.parent_path());std::ofstream file(output);if(!file)throw std::runtime_error("Cannot open output");file<<report.dump(2)<<'\n';if(!file)throw std::runtime_error("Cannot finish output");}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
    std::cout<<(code==0?"PASS ":"FAIL ")<<output.string()<<'\n';return code;
}

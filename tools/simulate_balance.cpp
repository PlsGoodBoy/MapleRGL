// Deterministic, headless balance sampling. The controller only uses start(),
// update(Input), and choose_upgrade(); it never edits gameplay state/config.
#include "game.hpp"
#include "../third_party/json.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace maple;
using json = nlohmann::json;
namespace {
constexpr float pi = 3.14159265358979323846f;
constexpr float dt = 1.f / 60.f;
constexpr int decision_ticks = 6; // 10 Hz reaction, simulation remains 60 Hz.
const char* class_names[] = {"archer", "mage", "warrior"};
const char* movement_names[] = {"evade", "collect", "event"};
const char* upgrade_names[] = {"new_skill", "specialize", "balanced"};
float dot(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }
float squared(Vec2 a) { return dot(a,a); }
float segment_distance(Vec2 p,Vec2 a,Vec2 b) {
    Vec2 ab=b-a;
    float t=squared(ab)>1e-5f ? std::clamp(dot(p-a,ab)/squared(ab),0.f,1.f) : 0;
    return length(p-(a+ab*t));
}
float hazard_distance(const Effect& e,Vec2 p) {
    if (e.kind==EffectKind::EnemyShot) return segment_distance(p,e.pos,e.pos+e.velocity*.25f);
    return e.pattern==1 ? segment_distance(p,e.pos,e.end) : length(p-e.pos);
}

struct Controller {
    int movement, upgrades;
    uint32_t seed;
    Vec2 previous{};
    Skill focus = Skill::Count;

    size_t choose(const Game& g) {
        if (g.choices.empty()) throw std::runtime_error("Upgrade state has no choices");
        float best = -1e30f;
        size_t chosen = 0;
        for (size_t i=0; i<g.choices.size(); ++i) {
            const auto& c = g.choices[i];
            float value = 0;
            if (c.kind == UpgradeKind::Skill) {
                if (!g.skill_allowed(c.skill) || g.skills[static_cast<int>(c.skill)] >= 5)
                    throw std::runtime_error("Illegal or capped skill offered");
                const int level = g.skills[static_cast<int>(c.skill)];
                if (upgrades == 0) value = level == 0 ? 150.f : 65.f - level * 5;
                else if (upgrades == 1) value = c.skill == focus ? 160.f : level > 0 ? 95.f + level * 7 : 20.f;
                else value = level == 0 ? 95.f : 78.f + (level == 4 ? 22.f : 0.f);
            } else {
                const int k = static_cast<int>(c.kind);
                const int level = k < passive_count ? g.passives[k] : 0;
                value = 38.f - level * 2;
                if (c.kind == UpgradeKind::Damage) value += 12;
                if (c.kind == UpgradeKind::AttackSpeed) value += 15;
                if (c.kind == UpgradeKind::Cooldown) value += 15;
                if (c.kind == UpgradeKind::Duration) value += 10;
                if (c.kind == UpgradeKind::CritChance || c.kind == UpgradeKind::CritDamage) value += 10;
                if (c.kind == UpgradeKind::Area) value += g.character == CharacterClass::Warrior ? 25.f : 12.f;
                if (c.kind == UpgradeKind::Projectiles) value += g.character == CharacterClass::Archer ? 24.f : 4.f;
                if (c.kind == UpgradeKind::Magnet && level < 2) value += 18;
                if (c.kind == UpgradeKind::MoveSpeed && level < 2) value += 6;
                // These are actual upgrades, not free healing or injected stats.
                if (c.kind == UpgradeKind::Health || c.kind == UpgradeKind::Recover || c.kind == UpgradeKind::Vitality)
                    value += (1-g.player.hp/g.player.max_hp)*115;
                if (upgrades == 2) value += 22;
                // See final evolution requirements below when choosing a focus.
                if (upgrades == 1 && focus != Skill::Count && level < 2 &&
                    evolution_passive(focus) == k) value += 100;
            }
            // Stable, seed-specific tie breaking independent of the game's RNG.
            value += float((seed + static_cast<uint32_t>(i)*17u + static_cast<uint32_t>(g.player.level)*13u)%11u)*.03f;
            if (value > best) { best=value; chosen=i; }
        }
        if (focus == Skill::Count && g.choices[chosen].kind == UpgradeKind::Skill)
            focus=g.choices[chosen].skill;
        for (size_t i=0; i<g.choices.size(); ++i)
            for (size_t j=0; j<i; ++j)
                if (g.choices[i].kind == g.choices[j].kind &&
                    (g.choices[i].kind != UpgradeKind::Skill || g.choices[i].skill == g.choices[j].skill))
                    throw std::runtime_error("Duplicate upgrade choices");
        return chosen;
    }

    static int evolution_passive(Skill skill) {
        // Kept in sync with core evolution prerequisites; used only for policy scoring.
        static const int requirements[] = {1,4,6,4,3,9,9,1,3,2,9,4,3,8,9,5,4,6};
        const int s=static_cast<int>(skill);
        return s < int(sizeof(requirements)/sizeof(requirements[0])) ? requirements[s] : -1;
    }

    Input steer(const Game& g) {
        const Vec2 p=g.player.pos;
        Vec2 target=p;
        float best_objective=-1;
        bool objective=false;
        const float range=g.character==CharacterClass::Warrior ? 85.f : 200.f;
        const Enemy* nearest=nullptr;
        const Enemy* boss=nullptr;
        std::vector<const Enemy*> threats;
        std::vector<const Effect*> hazards;
        for (const auto& effect:g.effects)
            if (effect.hostile && (effect.kind==EffectKind::Warning || effect.kind==EffectKind::EnemyShot))
                hazards.push_back(&effect);
        float nearest_distance=1e30f;
        for (const auto& e:g.enemies) {
            if (e.hp<=0) continue;
            float d=length(e.pos-p);
            if (d<360) threats.push_back(&e);
            if (d<nearest_distance) { nearest=&e; nearest_distance=d; }
            if (e.kind==EnemyKind::Boss) boss=&e;
        }
        // All policies gather XP; collect prioritizes it and tolerates more risk.
        for (const auto& gem:g.gems) {
            float distance=length(gem.pos-p);
            if (distance>900 || gem.value<=0) continue;
            const float value=std::min(12.f,gem.value)*60.f/(distance+50.f);
            if (value>best_objective) {best_objective=value;target=gem.pos;objective=true;}
        }
        for (const auto& item:g.items) {
            const float distance=length(item.pos-p);
            if (distance>750) continue;
            float value=150.f/(distance+40.f);
            if (item.kind==ItemKind::Health) value *= g.player.hp<g.player.max_hp*.65f ? 9.f : .1f;
            if (item.kind==ItemKind::Magnet) value *= 3.f;
            if (value>best_objective) {best_objective=value;target=item.pos;objective=true;}
        }
        const auto& event=g.special_event;
        if (movement==2 && event.kind!=SpecialEventKind::None && g.player.hp>g.player.max_hp*.38f &&
            (event.kind!=SpecialEventKind::Merchant || g.coins>=25)) {
            target=event.pos;objective=true;
            // Guardians must be fought, not rammed. Approach them to attack range.
            const Enemy* guardian=nullptr;
            float d=1e30f;
            for (const auto& e:g.enemies)
                if (std::find(event.guardians.begin(),event.guardians.end(),e.id)!=event.guardians.end()) {
                    float distance=length(e.pos-p);
                    if (distance<d) {d=distance;guardian=&e;}
                }
            if (guardian && d>range) target=guardian->pos+normalized(p-guardian->pos)*range;
        }
        if (boss && g.player.hp>g.player.max_hp*.30f) {
            target=boss->pos+normalized(p-boss->pos)*range;objective=true;
        }
        if (!objective) {
            if (nearest) target=nearest->pos+normalized(p-nearest->pos)*range;
            else {
                float a=g.elapsed*.09f+float(seed%360)*pi/180;
                target=p+Vec2{std::cos(a),std::sin(a)}*80;
            }
        }
        Vec2 desired=normalized(target-p);
        const float safety=movement==0 ? 5.f : movement==1 ? 2.9f : 3.7f;
        const float approach=movement==1 ? 3.2f : 2.5f;
        float best=-1e30f;
        Vec2 direction{};
        // 16 headings + stand still. Predict only visible positions/velocities,
        // not future random spawns, hidden cooldowns or candidate-world rollouts.
        for (int i=0;i<17;++i) {
            float angle=2*pi*i/16;
            Vec2 candidate=i==16 ? Vec2{} : Vec2{std::cos(angle),std::sin(angle)};
            Vec2 next=p+candidate*(g.player.speed*.35f);
            float score=dot(candidate,desired)*approach+.12f*dot(candidate,previous);
            if (length(target-p)<20) score-=length(candidate)*1.5f;
            for (const auto* enemy:threats) {
                const auto& e=*enemy;
                Vec2 prediction=e.pos+normalized(p-e.pos)*(e.speed*.22f)+e.velocity*.08f;
                float distance=length(next-prediction);
                float safe_radius=e.radius+(g.character==CharacterClass::Warrior ? 32.f : 65.f);
                float penetration=std::max(0.f,(safe_radius-distance)/safe_radius);
                score-=safety*penetration*penetration*5;
                float contact=std::max(0.f,(e.radius+20.f-distance)/(e.radius+20.f));
                score-=contact*contact*16;
            }
            for (const auto* effect:hazards) {
                float distance=hazard_distance(*effect,next);
                if (distance<effect->radius+20) score-=16.f*(1-distance/(effect->radius+20));
            }
            if (score>best) {best=score;direction=candidate;}
        }
        bool in_warning=false;
        for (const auto* e:hazards)
            if (hazard_distance(*e,p)<e->radius+15) in_warning=true;
        bool dash=g.player.dash_cooldown<=0 && length(direction)>.1f &&
                  ((nearest && nearest_distance<nearest->radius+40) || in_warning);
        previous=direction;
        return {direction,dash};
    }
};

json simulate(CharacterClass cls,uint32_t seed,int movement,int upgrades) {
    Game game;
    game.start(cls,seed);
    Controller bot{movement,upgrades,seed};
    Input input{};
    json decisions=json::array();
    std::array<double,skill_count> exposure{};
    std::array<double,10> minute_exposure{};
    std::array<int,10> minute_levels{};
    size_t peak_enemies=0,peak_projectiles=0,peak_effects=0;
    int upgrades_chosen=0,dashes=0;
    std::array<int,9> event_starts{},event_successes{},event_failures{};
    const auto begin=std::chrono::steady_clock::now();
    uint64_t ticks=0;
    while (game.state==State::Playing || game.state==State::Upgrade) {
        while (game.state==State::Upgrade) {
            size_t index=bot.choose(game);
            const auto choice=game.choices[index];
            decisions.push_back({{"time",game.elapsed},{"level",game.player.level},{"kind",int(choice.kind)},
                                 {"skill",choice.kind==UpgradeKind::Skill ? int(choice.skill) : -1},{"title",choice.title}});
            if (!game.choose_upgrade(index)) throw std::runtime_error("Legal upgrade rejected");
            if (++upgrades_chosen>10000) throw std::runtime_error("Upgrade loop guard");
        }
        if (game.state!=State::Playing) break;
        if (ticks%decision_ticks==0) input=bot.steer(game);
        const float before=game.elapsed;
        const int minute=std::clamp(int(before/60),0,9);
        const auto before_metrics=game.metrics;
        const int before_event=int(game.special_event.kind);
        game.update(dt,input);
        if (game.metrics.events_started>before_metrics.events_started)
            ++event_starts[int(game.special_event.kind)];
        if (game.metrics.events_succeeded>before_metrics.events_succeeded) ++event_successes[before_event];
        if (game.metrics.events_failed>before_metrics.events_failed) ++event_failures[before_event];
        if (input.dash) ++dashes;
        input.dash=false;
        const float elapsed=game.elapsed-before;
        minute_exposure[minute]+=elapsed;
        minute_levels[minute]=game.player.level;
        for (int s=0;s<skill_count;++s) if (game.skills[s]>0) exposure[s]+=elapsed;
        peak_enemies=std::max(peak_enemies,game.enemies.size());
        peak_projectiles=std::max(peak_projectiles,game.projectiles.size());
        peak_effects=std::max(peak_effects,game.effects.size());
        if (!std::isfinite(game.player.hp) || !std::isfinite(game.player.pos.x) || !std::isfinite(game.player.pos.y))
            throw std::runtime_error("Non-finite player state");
        if (game.enemies.size()>game.config.max_enemies || game.projectiles.size()>game.config.max_projectiles ||
            game.effects.size()>game.config.max_effects) throw std::runtime_error("Entity cap violated");
        if (++ticks>uint64_t((game.config.duration+5)*60)) throw std::runtime_error("No natural terminal state");
    }
    const auto wall_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    const auto& m=game.metrics;
    json skills=json::array();
    json event_types=json::array();
    for (int i=1;i<9;++i)
        event_types.push_back({{"id",i},{"name",Game::special_event_name(SpecialEventKind(i))},
                               {"started",event_starts[i]},{"succeeded",event_successes[i]},{"failed",event_failures[i]}});
    json boss_hp_fraction=nullptr;
    if (game.boss_defeated) boss_hp_fraction=0;
    for (const auto& enemy:game.enemies) if (enemy.kind==EnemyKind::Boss)
        boss_hp_fraction=enemy.hp/enemy.max_hp;
    for (int s=0;s<skill_count;++s) if (game.skill_allowed(Skill(s)))
        skills.push_back({{"id",s},{"name",Game::skill_name(Skill(s))},{"level",game.skills[s]},
                          {"evolved",game.evolved(Skill(s))},{"damage",m.skill_damage[s]},
                          {"owned_seconds",exposure[s]}});
    return {{"class",class_names[int(cls)]},{"seed",seed},{"movement",movement_names[movement]},
            {"upgrade_policy",upgrade_names[upgrades]},{"outcome",game.state==State::Victory ? "victory" : "defeat"},
            {"survival_seconds",game.elapsed},{"level",game.player.level},{"kills",game.kills},
            {"hp",game.player.hp},{"max_hp",game.player.max_hp},{"damage_taken",m.damage_taken},
            {"altar_health_spent",m.altar_health_spent},{"other_damage",m.other_damage},
            {"xp_acquired",m.xp_acquired},{"xp_by_minute",m.xp_by_minute},{"minute_exposure_seconds",minute_exposure},
            {"level_by_minute",minute_levels},{"skills",skills},{"passives",game.passives},
            {"events_started",m.events_started},{"events_succeeded",m.events_succeeded},{"events_failed",m.events_failed},
            {"event_types",event_types},{"boss_hp_fraction_remaining",boss_hp_fraction},
            {"event_active_at_end",game.special_event.kind!=SpecialEventKind::None},
            {"boss_spawned",game.boss_spawned},{"boss_defeated",game.boss_defeated},
            {"boss_spawn_time",m.boss_spawn_time},{"boss_defeat_time",m.boss_defeat_time},
            {"boss_damage_taken",m.boss_damage_taken},{"boss_attacks",m.boss_attacks},
            {"items_collected",m.items_collected},{"coins_remaining",game.coins},
            {"upgrades_chosen",upgrades_chosen},{"upgrade_history",decisions},
            {"dash_inputs",dashes},{"simulation_steps",game.simulation_steps},{"wall_ms",wall_ms},
            {"peak_enemies",peak_enemies},{"peak_projectiles",peak_projectiles},{"peak_effects",peak_effects}};
}
}

int main(int argc,char** argv) {
    try {
        int seed_count=4;
        uint32_t first_seed=20261111;
        std::string output="docs/simulation_combat_runs.json";
        int selected_class=-1,selected_movement=-1,selected_upgrades=-1;
        auto parse_policy=[](const std::string& value,const char* const* names) {
            for(int i=0;i<3;++i) if(value==names[i]) return i;
            throw std::runtime_error("Unknown policy: "+value);
        };
        for (int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if (arg=="--help") {
                std::cout<<"simulate_balance [--seed-count 4] [--seed 20261111] [--output FILE]\n"
                         <<"[--class archer|mage|warrior] [--movement evade|collect|event]\n"
                         <<"[--upgrade-policy new_skill|specialize|balanced]\n"
                         <<"By default runs 9 policies per seed per class with the unmodified 600-second game.\n";
                return 0;
            }
            if (i+1>=argc) throw std::runtime_error("Missing CLI value");
            if (arg=="--seed-count") seed_count=std::stoi(argv[++i]);
            else if (arg=="--seed") first_seed=static_cast<uint32_t>(std::stoul(argv[++i]));
            else if (arg=="--output") output=argv[++i];
            else if (arg=="--class") selected_class=parse_policy(argv[++i],class_names);
            else if (arg=="--movement") selected_movement=parse_policy(argv[++i],movement_names);
            else if (arg=="--upgrade-policy") selected_upgrades=parse_policy(argv[++i],upgrade_names);
            else throw std::runtime_error("Unknown argument: "+arg);
        }
        if (seed_count<1 || seed_count>100) throw std::runtime_error("seed-count must be 1..100");
        const int per_class=seed_count*(selected_movement<0?3:1)*(selected_upgrades<0?3:1);
        json report={{"schema_version",1},{"tool","MapleRGL simulate_balance"},{"simulation_dt_seconds",dt},
                     {"decision_frequency_hz",60/decision_ticks},{"seed_count",seed_count},{"first_seed",first_seed},
                     {"runs_per_class",per_class},{"expected_runs",per_class*(selected_class<0?3:1)},{"default_config",true},{"game_duration_seconds",600},
                     {"limitations",{"Headless CPU simulation, no graphics/audio/FPS or manual play validation.",
                       "Heuristic controller reads public current world positions, uses 10 Hz decisions and no future RNG.",
                       "Only start, ordinary movement/dash input and offered legal upgrades; no HP, skill, enemy or config injection.",
                       "Victory may mean surviving 600 seconds; boss defeat is reported separately.",
                       "Samples share seeds across policies; they are correlated and not a human win-rate estimate.",
                       "Per-minute XP after death is zero; exposure and conditional summaries distinguish censoring.",
                       "Per-skill effective HP damage and ownership time are measured, not theoretical DPS or causal rankings."}},
                     {"runs",json::array()}};
        const auto start=std::chrono::steady_clock::now();
        for (int cls=0;cls<3;++cls) for (int s=0;s<seed_count;++s)
            for (int movement=0;movement<3;++movement) for (int upgrades=0;upgrades<3;++upgrades) {
                if ((selected_class>=0 && cls!=selected_class) ||
                    (selected_movement>=0 && movement!=selected_movement) ||
                    (selected_upgrades>=0 && upgrades!=selected_upgrades)) continue;
                // Separate seeds remain deterministic, regardless of traversal order.
                const uint32_t seed=first_seed+uint32_t(s)*104729u;
                auto run=simulate(CharacterClass(cls),seed,movement,upgrades);
                std::cout<<class_names[cls]<<" seed="<<seed<<" "<<movement_names[movement]<<"/"<<upgrade_names[upgrades]
                         <<" "<<run["outcome"].get<std::string>()<<" t="<<run["survival_seconds"]
                         <<" level="<<run["level"]<<" kills="<<run["kills"]<<"\n"<<std::flush;
                report["runs"].push_back(std::move(run));
                std::ofstream file(output,std::ios::binary|std::ios::trunc);
                if (!file) throw std::runtime_error("Cannot open report path: "+output);
                file<<report.dump(2)<<'\n';
                if (!file) throw std::runtime_error("Report write failed");
            }
        report["complete"]=true;
        report["wall_seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::ofstream file(output,std::ios::binary|std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot finalize report");
        file<<report.dump(2)<<'\n';
        if (!file) throw std::runtime_error("Final report write failed");
        std::cout<<"Wrote "<<report["runs"].size()<<" complete runs to "<<output<<"\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"Simulation failed: "<<e.what()<<'\n';
        return 1;
    }
}




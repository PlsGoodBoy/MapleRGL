#ifdef NDEBUG
#undef NDEBUG
#endif
#include "progression.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace maple;
namespace fs=std::filesystem;
using json=nlohmann::json;
namespace {
Config quiet(){Config c;c.duration=10000;c.max_enemies=0;c.first_event_time=c.boss_time=100000;return c;}
void choose_book(Game& game,Skill result,int form=0){game.scrolls.offers={{result,form}};++game.scrolls.eligible_rewards;++game.scrolls.opportunities;if(game.scrolls.first_opportunity_time<0)game.scrolls.first_opportunity_time=game.total_elapsed();game.state=State::ScrollChoice;assert(game.choose_scroll(0));}
size_t study_choice(const Game& game){for(size_t i=0;i<game.choices.size();++i)if(game.choices[i].kind==UpgradeKind::Study)return i;assert(false);return 0;}
struct Scratch {
 fs::path path=fs::temp_directory_path()/("MapleRGL_scroll_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Scratch(){assert(fs::create_directory(path));}
 ~Scratch(){std::error_code error;auto root=fs::absolute(fs::temp_directory_path(error)).lexically_normal(),target=fs::absolute(path).lexically_normal();if(!error&&target.parent_path()==root&&target.filename().u8string().find("MapleRGL_scroll_")==0)fs::remove_all(target,error);}
};
json read(const fs::path& path){std::ifstream input(path);json document;input>>document;return document;}
void write(const fs::path& path,const json& document){std::ofstream output(path);output<<document.dump();}
}
int main(){
 // A normal completed optional chest presents a retained legal choice once.
 {Game game(quiet());game.start(CharacterClass::Warrior,61);game.skills[int(Skill::Orbit)]=2;game.skills[int(Skill::BladeWave)]=2;assert(!game.fusion_ready(Skill::SwordCyclone));
  game.special_event.kind=SpecialEventKind::GuardedChest;game.special_event.remaining=10;game.special_event.scroll_challenge=true;game.special_event.pos=game.player.pos;game.update(.01f);
  assert(game.state==State::ScrollChoice&&game.scrolls.eligible_rewards==1&&game.scrolls.opportunities==1&&!game.scrolls.offers.empty());auto offers=game.scrolls.offers;game.update(.1f);assert(game.scrolls.eligible_rewards==1&&game.scrolls.offers.size()==offers.size());assert(game.decline_scroll_offer()&&game.scrolls.opportunities==1&&game.scrolls.failed_rewards==0);
 }
 // Full-slot study is two explicit steps; cancelling preserves the same upgrade.
 {Game game(quiet());game.start(CharacterClass::Warrior,62);game.skills.fill(0);for(Skill s:{Skill::Orbit,Skill::Cleave,Skill::Charge,Skill::BladeWave,Skill::Nova,Skill::HeavyStrike})game.skills[int(s)]=2;
  choose_book(game,Skill::FlameDance);uint32_t id=game.held_scroll(Skill::FlameDance)->id;assert(game.skill_allowed(Skill::Zone)&&!game.skill_allowed(Skill::Lightning)&&!game.scroll_reachable(Skill::FeatherStorm));
  game.gems.push_back({game.player.pos,8,0});game.update(.01f);assert(game.state==State::Upgrade);for(const auto& choice:game.choices)assert(choice.kind!=UpgradeKind::Fusion);size_t choice=study_choice(game);auto before=game.choices;
  assert(game.choose_upgrade(choice)&&game.pending_study.active&&game.skills[int(Skill::Zone)]==0);assert(game.select_study_replacement(Skill::Cleave));game.cancel_study_replacement();assert(game.choices.size()==before.size()&&game.choices[choice].skill==before[choice].skill&&game.skills[int(Skill::Cleave)]==2);
  assert(game.choose_upgrade(choice)&&game.select_study_replacement(Skill::Cleave)&&game.confirm_study_replacement());assert(game.skills[int(Skill::Zone)]==1&&game.skills[int(Skill::Cleave)]==0&&game.active_skill_count()==6&&game.scrolls.studies_used==1);
  assert(game.discard_scroll(id));choose_book(game,Skill::FlameDance);assert(game.held_scroll(Skill::FlameDance)->study_used&&game.scrolls.foreign_class==int(CharacterClass::Mage));
  game.skills[int(Skill::Zone)]=2;choose_book(game,Skill::SwordCyclone);id=game.held_scroll(Skill::FlameDance)->id;assert(!game.scroll_conflicts(Skill::FlameDance).empty());game.toggle_pause();assert(game.confirm_scroll_fusion(id));assert(game.skills[int(Skill::FlameDance)]==1&&game.cross_fusion_count()==1&&!game.scroll_reachable(Skill::SwordCyclone));assert(game.scrolls.learned_foreign&(uint64_t(1)<<int(Skill::Zone)));assert(game.discard_scroll(game.held_scroll(Skill::SwordCyclone)->id));
 }
 // Real boundary persistence: scroll study, chosen roster, RNG and observed time.
 Scratch scratch;uint64_t run=0;int64_t crystals=0;
 {ProgressionStore store(scratch.path);auto begun=store.begin_run(CharacterClass::Warrior,63,RunMode::Campaign);assert(begun);run=begun.run_id;Config config=quiet();config.duration=.05f;Game game(config);game.start(CharacterClass::Warrior,63,store.run_bonuses(),store.profile().active_run->setup);choose_book(game,Skill::FlameDance);game.update(.1f);assert(game.state==State::ChapterComplete);
  game.skills[int(Skill::BattleFocus)]=1;game.battle_focus=37.5f;game.timing={1,1,.05,2,true};RunBuild build;assert(game.export_build(build)&&!build.rng_state.empty());assert(store.claim_chapter(run,0,make_run_report(game,RunOutcome::Victory,63),build));crystals=store.profile().crystals;
  RunTiming observed{2,3,.05,15,true};assert(store.update_checkpoint_timing(run,observed)&&store.update_checkpoint_timing(run,observed));assert(store.profile().crystals==crystals&&store.claim_chapter(run,0,make_run_report(game,RunOutcome::Victory,63),build).already_claimed);assert(store.choose_chapter_supply(run,2));
  RunBuild saved;assert(store.checkpoint_build(saved)&&saved.timing.total_real_seconds==15&&saved.timing.used_speeds==3&&saved.timing.speed==2&&saved.scrolls.held.size()==1);Game restored(config);assert(restored.restore_build(saved)&&restored.battle_focus==37.5f);assert(restored.encounter_plan.boss==saved.encounter_plan.boss&&restored.encounter_plan.roster==saved.encounter_plan.roster&&restored.scrolls.foreign_class==int(CharacterClass::Mage));assert(restored.advance_chapter()&&restored.scrolls.held.size()==1&&restored.timing.speed==2);
 }
 // The selected alternative form survives a real chapter file round trip.
 {Scratch form_root;
  {ProgressionStore store(form_root.path);auto begun=store.begin_run(CharacterClass::Mage,65,RunMode::Campaign);assert(begun);Config config=quiet();config.duration=.05f;Game game(config);game.start(CharacterClass::Mage,65,store.run_bonuses(),store.profile().active_run->setup);
   game.skills[int(Skill::Frost)]=game.skills[int(Skill::Lightning)]=2;choose_book(game,Skill::FrostStorm,1);assert(game.confirm_scroll_fusion(game.held_scroll(Skill::FrostStorm)->id)&&game.fusion_form(Skill::FrostStorm)==1);game.update(.1f);RunBuild build;assert(game.export_build(build));assert(store.claim_chapter(begun.run_id,0,make_run_report(game,RunOutcome::Victory,65),build));
   auto document=read(store.profile_path());assert(document["chapter_checkpoint"]["build"]["scrolls"]["completed"][0]["form"]=="thunder");
  }
  ProgressionStore loaded(form_root.path);RunBuild saved;assert(!loaded.read_only()&&loaded.checkpoint_build(saved));Game restored;assert(restored.restore_build(saved)&&restored.fusion_form(Skill::FrostStorm)==1&&!restored.scroll_reachable(Skill::FrostStorm));
 }
 // A schema-5 archive with three already-completed fusions keeps all three.
 {Scratch legacy_root;json document;
  {ProgressionStore store(legacy_root.path);auto begun=store.begin_run(CharacterClass::Archer,64,RunMode::Campaign);assert(begun);Game game(quiet());game.start(CharacterClass::Archer,64,store.run_bonuses(),store.profile().active_run->setup);game.skills.fill(0);
   game.skills[int(Skill::StarReturn)]=game.skills[int(Skill::PhoenixRain)]=game.skills[int(Skill::HunterVolley)]=1;for(Skill material:{Skill::Arrow,Skill::Pierce,Skill::Summon,Skill::Spread,Skill::Rain,Skill::Ricochet})game.consumed_skills|=uint64_t(1)<<int(material);
   game.elapsed=120;game.metrics.chapters_cleared=1;game.metrics.fusions_created=3;game.metrics.first_fusion_time=60;game.state=State::ChapterComplete;RunBuild build;assert(game.export_build(build));assert(store.claim_chapter(begun.run_id,0,make_run_report(game,RunOutcome::Victory,64),build));document=read(store.profile_path());
  }
  document["schema_version"]=5;document["achievement_stats"]["enemy_kills"]=std::vector<int>(10,0);auto& build=document["chapter_checkpoint"]["build"];for(const char* key:{"scrolls","timing","encounter_plan","rng_state"})build.erase(key);build["metrics"].erase("content");build["metrics"]["enemy_kills_by_kind"]=std::vector<int>(10,0);for(const char* report:{"claimed_report","accounted_report"})document["active_run"][report]["enemy_kills"]=std::vector<int>(10,0);
  for(const char* name:{"claimed_report","accounted_report"})for(const char* key:{"scrolls","timing","content"})document["active_run"][name].erase(key);write(legacy_root.path/"profile.json",document);
  ProgressionStore migrated(legacy_root.path);assert(migrated.load_state()==ProfileLoadState::Migrated&&!migrated.read_only());RunBuild saved;assert(migrated.checkpoint_build(saved));Game restored;assert(restored.restore_build(saved)&&restored.fusion_count()==3&&restored.active_skill_count()==3&&restored.scrolls.completed.size()==3&&restored.metrics.enemy_kills_by_kind[int(EnemyKind::GroundController)]==0&&restored.metrics.enemy_kills_by_kind[int(EnemyKind::WindupCharger)]==0);assert(!restored.can_gain_skill(Skill::Arrow)&&restored.scrolls.held.empty());
 }
 std::cout<<"PASS scrolls: real reward opportunity, explicit study replacement, foreign authorization/conflicts, boundary RNG/timing and legacy fusion preservation\n";
}

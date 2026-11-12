#include "progression.hpp"
#include "encounter_pool.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <regex>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace maple {
namespace {
using json=nlohmann::json;
namespace fs=std::filesystem;
constexpr uint64_t serial_limit=9000000000000000ULL;
constexpr int64_t currency_limit=1000000000000LL;
constexpr size_t max_profile_bytes=4*1024*1024;
const std::array<TalentDefinition,talent_count> definitions={{
 {TalentId::Damage,"damage","磨砺锋芒","每级伤害 +2%",0,5,8,5},
 {TalentId::Crit,"crit","精准直觉","每级暴击率 +2%",0,3,12,8},
 {TalentId::FirstEliteBonus,"first_elite_bonus","精英战利品","每局首次精英击杀额外获得 10 金币与 5 基础经验",0,1,25,0},
 {TalentId::Health,"health","坚韧体魄","每级最大生命 +4",1,5,8,5},
 {TalentId::MoveSpeed,"move_speed","轻盈步伐","每级移动速度 +2%",1,3,12,8},
 {TalentId::DeathProtection,"death_protection","不屈意志","每局抵挡一次致命伤害，恢复部分生命",1,1,40,0},
 {TalentId::XP,"xp","求知之心","每级获得经验 +2%",2,5,8,5},
 {TalentId::Reroll,"reroll","灵感再现","每级每局额外获得一次升级重选",2,2,20,15},
 {TalentId::Cooldown,"cooldown","法术节律","每级技能冷却缩短 2%",2,3,12,8}
}};
const std::array<AchievementDefinition,achievement_count> achievement_catalog={{
 {AchievementId::FirstHundred,"first_hundred","初露锋芒","累计击败100个敌人",AchievementMetric::Kills,100,EquipmentId::None,0,false,3},
 {AchievementId::SeasonedHunter,"seasoned_hunter","森林猎手","累计击败500个敌人",AchievementMetric::Kills,500,EquipmentId::ScoutCloak,0,false,5},
 {AchievementId::ThousandFoes,"thousand_foes","千敌磨练","累计击败1000个敌人",AchievementMetric::Kills,1000,EquipmentId::None,0,false,8},
 {AchievementId::EliteHunter,"elite_hunter","精英猎人","累计击败10个精英",AchievementMetric::Elites,10,EquipmentId::None,0,false,5},
 {AchievementId::BossSlayer,"boss_slayer","领主挑战者","实际击败一个Boss",AchievementMetric::Bosses,1,EquipmentId::ScholarRobe,0,false,8},
 {AchievementId::EventHelper,"event_helper","热心冒险家","累计成功完成5个事件",AchievementMetric::Events,5,EquipmentId::None,0,false,3},
 {AchievementId::TreasureFinder,"treasure_finder","战利品收集者","累计拾取20件掉落物",AchievementMetric::Items,20,EquipmentId::MagnetPendant,0,false,3},
 {AchievementId::FirstChapter,"first_chapter","踏出森林","完成远征第一章",AchievementMetric::Chapters,1,EquipmentId::None,0,false,5},
 {AchievementId::CampaignVictor,"campaign_victor","远征归来","完成一次三章远征",AchievementMetric::CampaignWins,1,EquipmentId::None,1u<<int(RelicId::IronWill),true,15},
 {AchievementId::ArcherVictory,"archer_victory","箭术传承","使用弓术技能造成伤害并完成经典冒险或完整远征",AchievementMetric::ArcherWins,1,EquipmentId::ReedBow,1u<<int(RelicId::BarbedQuiver),false,8},
 {AchievementId::MageVictory,"mage_victory","奥术传承","使用法术技能造成伤害并完成经典冒险或完整远征",AchievementMetric::MageWins,1,EquipmentId::ApprenticeWand,1u<<int(RelicId::EmberCore),false,8},
 {AchievementId::WarriorVictory,"warrior_victory","剑术传承","使用战技技能造成伤害并完成经典冒险或完整远征",AchievementMetric::WarriorWins,1,EquipmentId::IronBlade,1u<<int(RelicId::WarCry),false,8},
 {AchievementId::LevelTwenty,"level_twenty","渐入佳境","单局达到20级",AchievementMetric::Level,20,EquipmentId::None,0,false,5},
 {AchievementId::FirstEvolution,"first_evolution","突破极限","单局完成至少1项技能进化",AchievementMetric::Evolutions,1,EquipmentId::None,1u<<int(RelicId::WindFeather),false,5},
 {AchievementId::TripleEvolution,"triple_evolution","进化大师","单局同时达成3项技能进化",AchievementMetric::Evolutions,3,EquipmentId::None,1u<<int(RelicId::EchoRune),false,10},
 {AchievementId::Dashing,"dashing","身法练习","累计成功冲刺50次",AchievementMetric::Dashes,50,EquipmentId::None,0,false,3},
 {AchievementId::SixSkills,"six_skills","完整流派","单局装备8项主动技能",AchievementMetric::Skills,8,EquipmentId::None,0,false,5},
 {AchievementId::EventVeteran,"event_veteran","森林守望者","累计成功完成25个事件",AchievementMetric::Events,25,EquipmentId::None,0,false,8},
 {AchievementId::RunSlayer,"run_slayer","势如破竹","单局击败500个敌人",AchievementMetric::RunKills,500,EquipmentId::None,0,false,5},
 {AchievementId::UnscathedVictory,"unscathed_victory","从容制胜","存活至少180秒、击败100敌人且总受伤不超过30后通关",AchievementMetric::LowDamageWins,1,EquipmentId::None,0,false,10}
}};
int64_t now_seconds(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
bool valid_class(CharacterClass c){return int(c)>=0&&int(c)<3;}
uint64_t number_u(const json& j,const char* key,uint64_t fallback,uint64_t max=serial_limit){
 auto it=j.find(key);if(it==j.end())return fallback;
 if(!it->is_number_integer()||(!it->is_number_unsigned()&&it->get<int64_t>()<0))throw std::runtime_error(std::string("Invalid integer: ")+key);
 const auto value=it->get<uint64_t>();if(value>max)throw std::runtime_error(std::string("Integer out of range: ")+key);return value;
}
double number_f(const json& j,const char* key,double fallback,double min=0,double max=1e15){
 auto it=j.find(key);if(it==j.end())return fallback;if(!it->is_number())throw std::runtime_error(std::string("Invalid number: ")+key);
 double value=it->get<double>();if(!std::isfinite(value)||value<min||value>max)throw std::runtime_error(std::string("Number out of range: ")+key);return value;
}
bool boolean(const json& j,const char* key,bool fallback){auto it=j.find(key);if(it==j.end())return fallback;if(!it->is_boolean())throw std::runtime_error(std::string("Invalid boolean: ")+key);return it->get<bool>();}
void object_field(const json& j,const char* key){if(j.contains(key)&&!j.at(key).is_object())throw std::runtime_error(std::string("Invalid object: ")+key);}
Profile fresh_profile(){Profile p;p.unlocks={{"classes",{"archer","mage","warrior"}},{"skills",json::array()},{"equipment",json::array()},{"relics",json::array()}};for(int i=0;i<skill_count;++i)p.unlocks["skills"].push_back(i);p.chapter_progress={{"highest_unlocked",1},{"completed",json::array()}};for(const auto& d:equipment_definitions())if(d.id!=EquipmentId::None&&d.initially_owned)p.owned_equipment[int(d.id)]=true;p.equipped={EquipmentId::TravelerKnife,EquipmentId::LeatherVest,EquipmentId::AmberPendant};p.unlocked_relic_mask=initial_relic_mask();return p;}
template<size_t N> std::array<int,N> int_array(const json& j,const char* key,int cap);
template<size_t N> std::array<double,N> double_array(const json& j,const char* key);
// Skill keys are the file format. The old 18-entry ordering is frozen here for
// schema 0-4 saves; current arrays are only an in-memory lookup convenience.
constexpr std::array<Skill,18> legacy_skill_order={Skill::Homing,Skill::Arrow,Skill::Nova,Skill::Pierce,Skill::Lightning,Skill::Zone,Skill::Summon,Skill::Cleave,Skill::Orbit,Skill::Spread,Skill::Rain,Skill::Ricochet,Skill::Frost,Skill::Meteor,Skill::Rune,Skill::Charge,Skill::BladeWave,Skill::Retaliate};
struct UnsupportedSkillContent : std::runtime_error {using std::runtime_error::runtime_error;};
int skill_index(const std::string& key){for(int i=0;i<skill_count;++i)if(key==Game::skill_key(Skill(i)))return i;throw UnsupportedSkillContent("The save contains a skill from a newer game version: "+key);}
template<class T> json skill_table_json(const std::array<T,skill_count>& values){json out=json::object();for(int i=0;i<skill_count;++i)out[Game::skill_key(Skill(i))]=values[i];return out;}
template<class T,class Reader> std::array<T,skill_count> read_skill_table(const json& j,const char* key,Reader reader){
 const auto& data=j.at(key);std::array<T,skill_count> out{};
 if(data.is_array()){
  if(data.size()!=legacy_skill_order.size())throw std::runtime_error("Invalid legacy skill table");
  for(size_t i=0;i<legacy_skill_order.size();++i)out[int(legacy_skill_order[i])]=reader(data[i]);
 }else if(data.is_object()){
  for(auto it=data.begin();it!=data.end();++it)out[skill_index(it.key())]=reader(it.value());
 }else throw std::runtime_error("Invalid skill table");
 return out;
}
std::array<int,skill_count> skill_ranks(const json& j,const char* key){return read_skill_table<int>(j,key,[](const json& value){return int(number_u(json{{"n",value}},"n",0,5));});}
std::array<double,skill_count> skill_damage(const json& j,const char* key){return read_skill_table<double>(j,key,[](const json& value){return number_f(json{{"n",value}},"n",0);});}
json consumed_skills_json(uint64_t mask){json out=json::array();if(mask&~Game::material_skill_mask())throw std::runtime_error("Invalid material skill flags");for(int i=0;i<std::min(skill_count,64);++i)if(mask&(uint64_t(1)<<i))out.push_back(Game::skill_key(Skill(i)));return out;}
uint64_t read_consumed_skills(const json& j){
 if(!j.contains("consumed_skills"))return 0;const auto& data=j.at("consumed_skills");if(!data.is_array())throw std::runtime_error("Invalid consumed skill list");uint64_t mask=0;
 for(const auto& entry:data){if(!entry.is_string())throw std::runtime_error("Invalid consumed skill key");int id=skill_index(entry.get<std::string>());if(id>=64||Game::is_fusion(Skill(id)))throw std::runtime_error("A fusion result cannot be a consumed material");uint64_t flag=uint64_t(1)<<id;if(mask&flag)throw std::runtime_error("Repeated consumed skill");mask|=flag;}return mask;
}
json scroll_offer_json(const ScrollOffer& offer){
 if(!Game::is_fusion(offer.result)||offer.form<0||offer.form>=Game::form_count(offer.result))throw UnsupportedSkillContent("Unknown scroll form");
 return {{"result",Game::skill_key(offer.result)},{"form",Game::scroll_form_key(offer.result,offer.form)},{"recipe_id",offer.recipe_id},{"investment",offer.investment}};
}
ScrollOffer read_scroll_offer(const json& j){
 if(!j.is_object()||!j.contains("result")||!j.at("result").is_string())throw std::runtime_error("Invalid scroll route");
 Skill result=Skill(skill_index(j.at("result").get<std::string>()));if(!Game::is_fusion(result))throw std::runtime_error("Scroll result is not a fusion");
 const std::string key=j.value("form",std::string("original"));
 // Pre-form saves used original for every fusion; it remains an alias of form 0.
 if(key=="original")return {result,0,j.value("recipe_id",std::string()),int(number_u(j,"investment",0,1000000))};
 for(int form=0;form<Game::form_count(result);++form)if(key==Game::scroll_form_key(result,form))return {result,form,j.value("recipe_id",std::string()),int(number_u(j,"investment",0,1000000))};
 throw UnsupportedSkillContent("The save uses a newer scroll form");
}
json scrolls_json(const ScrollState& state){
 json held=json::array(),offers=json::array(),completed=json::array(),studied=json::array();
 for(const auto& scroll:state.held){auto row=scroll_offer_json({scroll.result,scroll.form});row["id"]=scroll.id;row["study_used"]=scroll.study_used;row["tracked"]=scroll.tracked;held.push_back(row);}for(const auto& offer:state.offers)offers.push_back(scroll_offer_json(offer));for(const auto& entry:state.completed)completed.push_back(scroll_offer_json(entry));for(Skill result:state.studied_results)studied.push_back(Game::skill_key(result));
 return {{"held",held},{"offers",offers},{"completed",completed},{"foreign_class",state.foreign_class},{"authorized_foreign",consumed_skills_json(state.authorized_foreign)},{"learned_foreign",consumed_skills_json(state.learned_foreign)},{"studied_results",studied},{"next_id",state.next_id},{"pending_opportunities",state.pending_opportunities},{"failed_rewards",state.failed_rewards},{"eligible_rewards",state.eligible_rewards},{"opportunities",state.opportunities},{"studies_used",state.studies_used},{"replacements",state.replacements},{"first_opportunity_time",state.first_opportunity_time},{"challenge_offered",state.challenge_offered}};
}
ScrollState read_scrolls(const json& parent,const std::array<int,skill_count>& skills){
 ScrollState out;if(parent.contains("scrolls")){const auto& j=parent.at("scrolls");if(!j.is_object())throw std::runtime_error("Invalid scroll state");
  out.foreign_class=int(number_f(j,"foreign_class",-1,-1,2));out.authorized_foreign=read_consumed_skills(json{{"consumed_skills",j.value("authorized_foreign",json::array())}});out.learned_foreign=read_consumed_skills(json{{"consumed_skills",j.value("learned_foreign",json::array())}});out.next_id=uint32_t(number_u(j,"next_id",1,UINT32_MAX));out.pending_opportunities=int(number_u(j,"pending_opportunities",0,1000000));out.failed_rewards=int(number_u(j,"failed_rewards",0,3));out.eligible_rewards=int(number_u(j,"eligible_rewards",0,100000000));out.opportunities=int(number_u(j,"opportunities",0,100000000));out.studies_used=int(number_u(j,"studies_used",0,100000000));out.replacements=int(number_u(j,"replacements",0,100000000));out.first_opportunity_time=float(number_f(j,"first_opportunity_time",-1,-1,1e8));out.challenge_offered=boolean(j,"challenge_offered",false);
  for(const char* key:{"held","offers","completed","studied_results"})if(j.contains(key)&&(!j.at(key).is_array()||j.at(key).size()>fusion_recipes().size()))throw std::runtime_error("Invalid scroll list");
  if(j.contains("held"))for(const auto& row:j.at("held")){auto offer=read_scroll_offer(row);out.held.push_back({uint32_t(number_u(row,"id",0,UINT32_MAX)),offer.result,offer.form,boolean(row,"study_used",false),boolean(row,"tracked",true)});}
  if(j.contains("offers"))for(const auto& row:j.at("offers"))out.offers.push_back(read_scroll_offer(row));if(j.contains("completed"))for(const auto& row:j.at("completed"))out.completed.push_back(read_scroll_offer(row));
  if(j.contains("studied_results"))for(const auto& row:j.at("studied_results")){if(!row.is_string())throw std::runtime_error("Invalid studied recipe");out.studied_results.push_back(Skill(skill_index(row.get<std::string>())));}
 }
 // Pre-scroll saves retain every completed fusion, even if they exceed new limits.
 for(const auto& recipe:fusion_recipes())if(skills[int(recipe.result)]>0&&std::none_of(out.completed.begin(),out.completed.end(),[&](const ScrollOffer& entry){return entry.result==recipe.result;}))out.completed.push_back({recipe.result,0});
  for(const auto& entry:out.completed)if(std::none_of(out.held.begin(),out.held.end(),[&](const FusionScroll& scroll){return scroll.result==entry.result;}))out.held.push_back({out.next_id++,entry.result,entry.form,std::find(out.studied_results.begin(),out.studied_results.end(),entry.result)!=out.studied_results.end(),false});return out;
}
json timing_json(const RunTiming& timing){return {{"rules_version",timing.rules_version},{"speed",timing.speed},{"combat_seconds",timing.combat_seconds},{"used_speeds",timing.used_speeds},{"effective_real_seconds",timing.effective_real_seconds},{"total_real_seconds",timing.total_real_seconds},{"recorded",timing.recorded}};}
RunTiming read_timing(const json& parent){RunTiming t;if(!parent.contains("timing"))return t;const auto& j=parent.at("timing");if(!j.is_object())throw std::runtime_error("Invalid run timing");t.combat_seconds=number_f(j,"combat_seconds",0,0,1e9);t.rules_version=int(number_u(j,"rules_version",1,2));t.speed=int(number_u(j,"speed",1,2));if(t.speed<1)throw std::runtime_error("Invalid speed");t.used_speeds=uint32_t(number_u(j,"used_speeds",0,3));t.effective_real_seconds=number_f(j,"effective_real_seconds",0,0,1e9);t.total_real_seconds=number_f(j,"total_real_seconds",0,t.effective_real_seconds,1e9);t.recorded=boolean(j,"recorded",false);return t;}
json content_json(const ContentStatistics& c){
 json bosses=json::object();for(const auto& boss:boss_definitions()){const int i=int(boss.id);bosses[boss.key]={{"seen",(c.bosses_seen&(1u<<i))!=0},{"encounters",c.boss_encounters[i]},{"kills",c.boss_kills[i]},{"moves_seen",c.boss_moves_seen[i]}};}
 return {{"bosses",bosses},{"enemies_seen",c.enemies_seen},{"enemy_kills",c.enemy_kills}};
}
ContentStatistics read_content(const json& parent){
 ContentStatistics c;if(!parent.contains("content"))return c;const auto& j=parent.at("content");if(!j.is_object())throw std::runtime_error("Invalid content statistics");
 c.bosses_seen=uint32_t(number_u(j,"bosses_seen",0,UINT32_MAX));c.enemies_seen=number_u(j,"enemies_seen",0,UINT64_MAX);
 if(j.contains("boss_kills"))c.boss_kills=int_array<32>(j,"boss_kills",std::numeric_limits<int>::max());
 if(j.contains("enemy_kills")){if(j.at("enemy_kills").size()==32){auto old=int_array<32>(j,"enemy_kills",std::numeric_limits<int>::max());std::copy(old.begin(),old.end(),c.enemy_kills.begin());}else c.enemy_kills=int_array<64>(j,"enemy_kills",std::numeric_limits<int>::max());}
 if(j.contains("bosses")){const auto& rows=j.at("bosses");if(!rows.is_object())throw std::runtime_error("Invalid Boss statistics");
  for(auto row=rows.begin();row!=rows.end();++row){if(!row.value().is_object())throw std::runtime_error("Invalid Boss statistics entry");const auto& defs=boss_definitions();auto found=std::find_if(defs.begin(),defs.end(),[&](const BossDefinition& d){return row.key()==d.key;});if(found==defs.end())throw UnsupportedSkillContent("Unknown Boss statistics ID");const int i=int(found->id);
   c.boss_encounters[i]=int(number_u(row.value(),"encounters",0,std::numeric_limits<int>::max()));c.boss_kills[i]=int(number_u(row.value(),"kills",0,std::numeric_limits<int>::max()));c.boss_moves_seen[i]=int(number_u(row.value(),"moves_seen",0,7));if(boolean(row.value(),"seen",false))c.bosses_seen|=1u<<i;
  }
 }
 return c;
}
void accumulate_content(ContentStatistics& total,const ContentStatistics& current,const ContentStatistics& prior={}){
 auto add=[](int& value,int now,int before){const int delta=std::max(0,now-before);if(value>std::numeric_limits<int>::max()-delta)throw std::runtime_error("Lifetime content statistics limit reached");value+=delta;};
 total.bosses_seen|=current.bosses_seen;total.enemies_seen|=current.enemies_seen;
 for(size_t i=0;i<total.boss_kills.size();++i){add(total.boss_kills[i],current.boss_kills[i],prior.boss_kills[i]);add(total.boss_encounters[i],current.boss_encounters[i],prior.boss_encounters[i]);total.boss_moves_seen[i]|=current.boss_moves_seen[i];}
 for(size_t i=0;i<total.enemy_kills.size();++i)add(total.enemy_kills[i],current.enemy_kills[i],prior.enemy_kills[i]);
}
json plan_json(const EncounterPlan& p){return {{"version",p.version},{"epoch",p.epoch},{"region",p.region},{"boss",p.boss},{"used_boss_mask",p.used_boss_mask},{"roster",p.roster}};}
EncounterPlan read_plan(const json& parent){EncounterPlan p;if(!parent.contains("encounter_plan"))return p;const auto& j=parent.at("encounter_plan");if(!j.is_object())throw std::runtime_error("Invalid encounter plan");p.version=int(number_u(j,"version",1,1));p.epoch=int(number_f(j,"epoch",-1,-1,1000000));p.region=int(number_u(j,"region",0,5));p.boss=int(number_f(j,"boss",-1,-1,31));p.used_boss_mask=uint32_t(number_u(j,"used_boss_mask",0,UINT32_MAX));if(p.boss>=boss_profile_count)throw UnsupportedSkillContent("The save uses a newer Boss");if(j.contains("roster")){if(!j.at("roster").is_array()||j.at("roster").size()>32)throw std::runtime_error("Invalid encounter roster");for(const auto& entry:j.at("roster")){int id=int(number_u(json{{"id",entry}},"id",0,63));if(id>=enemy_visual_count)throw UnsupportedSkillContent("The save uses a newer enemy visual");p.roster.push_back(id);}}return p;}
json resonance_directions_json(const std::array<int,resonance_count>& directions){json value=json::object();for(const auto& rule:resonance_definitions())if(directions[int(rule.id)])value[rule.key]=directions[int(rule.id)];return value;}
std::array<int,resonance_count> read_resonance_directions(const json& j){std::array<int,resonance_count> result{};if(j.contains("resonance_directions"))for(const auto& rule:resonance_definitions())result[int(rule.id)]=int(number_u(j.at("resonance_directions"),rule.key,0,int(rule.id)<6?2:0));return result;}
json resonance_json(const ResonanceStatistics& stats){json out=json::object();for(const auto& rule:resonance_definitions()){int i=int(rule.id);out[rule.key]={{"triggers",stats.triggers[i]},{"suppressed",stats.suppressed[i]},{"damage",stats.damage[i]},{"shield",stats.shield[i]},{"support",stats.support[i]}};}return out;}
ResonanceStatistics read_resonance(const json& j){ResonanceStatistics out;object_field(j,"resonance");if(!j.contains("resonance"))return out;const auto& all=j.at("resonance");for(const auto& rule:resonance_definitions())if(all.contains(rule.key)){const auto& r=all.at(rule.key);int i=int(rule.id);out.triggers[i]=int(number_u(r,"triggers",0,100000000));out.suppressed[i]=int(number_u(r,"suppressed",0,100000000));out.damage[i]=number_f(r,"damage",0);out.shield[i]=number_f(r,"shield",0);out.support[i]=number_f(r,"support",0);}return out;}
json gear_json(const std::vector<GearStack>& items){json out=json::array();for(const auto& s:items)out.push_back({{"id",s.id},{"level",s.level},{"count",s.count}});return out;}
std::vector<GearStack> read_gear(const json& parent){
 std::vector<GearStack> out;if(!parent.contains("run_gear"))return out;const auto& rows=parent.at("run_gear");if(!rows.is_array())throw std::runtime_error("Invalid run equipment list");uint64_t total=0;
 for(const auto& row:rows){const std::string id=row.at("id").get<std::string>();if(!gear_definition(id))throw UnsupportedSkillContent("Unknown run equipment");const int level=int(number_u(row,"level",1,3));if(!level)throw std::runtime_error("Invalid run equipment level");
  const uint64_t count=number_u(row,"count",0,UINT64_MAX);if(!count)continue;if(count>UINT64_MAX-total)throw std::runtime_error("Run equipment count exceeds numeric range");total+=count;
  auto found=std::find_if(out.begin(),out.end(),[&](const GearStack& s){return s.id==id&&s.level==level;});if(found==out.end())out.push_back({id,level,count});else found->count+=count;
 }
 return out;
}
json report_json(const RunReport& r){return {{"run_gear",gear_json(r.run_gear)},{"resonance_directions",resonance_directions_json(r.resonance_directions)},{"resonance",resonance_json(r.resonance)},{"adventurer",r.adventurer},{"upgrade_choices",r.upgrade_choices},{"bonus_choices",r.bonus_choices},{"scrolls",scrolls_json(r.scrolls)},{"timing",timing_json(r.timing)},{"content",content_json(r.content)},{"outcome",int(r.outcome)},{"character",int(r.character)},{"seed",r.seed},{"elapsed",r.elapsed},{"kills",r.kills},{"elite_kills",r.elite_kills},{"events_succeeded",r.events_succeeded},{"level",r.level},{"boss_defeated",r.boss_defeated},{"damage_dealt",r.damage_dealt},{"damage_taken",r.damage_taken},{"xp_acquired",r.xp_acquired},{"mode",int(r.mode)},{"chapters_cleared",r.chapters_cleared},{"bosses_defeated",r.bosses_defeated},{"skills",skill_table_json(r.skills)},{"passives",r.passives},{"skill_damage",skill_table_json(r.skill_damage)},{"enemy_kills",r.enemy_kills},{"dash_uses",r.dash_uses},{"items_collected",r.items_collected},{"rerolls_used",r.rerolls_used},{"death_protections_used",r.death_protections_used},{"evolved_count",r.evolved_count},{"equipment",r.equipment},{"owned_relic_mask",r.owned_relic_mask},{"detailed_metrics",r.detailed_metrics},{"difficulty",int(r.difficulty)},{"daily_key",r.daily_key},{"daily_variant",r.daily_variant},{"enemy_seen_mask",r.enemy_seen_mask},{"endless_cycle",r.endless_cycle},{"consumed_skills",consumed_skills_json(r.consumed_skills)},{"fusions_created",r.fusions_created},{"first_fusion_time",r.first_fusion_time},{"synergy_triggers",r.synergy_triggers}};}
RunReport parse_report(const json& j){
 if(!j.is_object())throw std::runtime_error("Invalid run report");RunReport r;r.run_gear=read_gear(j);r.resonance_directions=read_resonance_directions(j);r.resonance=read_resonance(j);
 r.adventurer=boolean(j,"adventurer",false);r.upgrade_choices=int(number_u(j,"upgrade_choices",0,1000000));r.bonus_choices=int(number_u(j,"bonus_choices",0,1000000));r.outcome=RunOutcome(number_u(j,"outcome",1,3));r.character=CharacterClass(number_u(j,"character",0,2));r.seed=uint32_t(number_u(j,"seed",0,UINT32_MAX));
 r.elapsed=float(number_f(j,"elapsed",0,0,86400));r.kills=int(number_u(j,"kills",0,10000000));r.elite_kills=int(number_u(j,"elite_kills",0,r.kills));r.events_succeeded=int(number_u(j,"events_succeeded",0,1000000));r.level=int(number_u(j,"level",1,100000));if(r.level<1)throw std::runtime_error("Invalid level");
 r.boss_defeated=boolean(j,"boss_defeated",false);r.damage_dealt=number_f(j,"damage_dealt",0);r.damage_taken=number_f(j,"damage_taken",0);r.xp_acquired=number_f(j,"xp_acquired",0);r.mode=RunMode(number_u(j,"mode",0,3));r.chapters_cleared=int(number_u(j,"chapters_cleared",0,3));r.bosses_defeated=int(number_u(j,"bosses_defeated",0,1000000));if(j.contains("skills"))r.skills=skill_ranks(j,"skills");if(j.contains("passives"))r.passives=int_array<passive_count>(j,"passives",8);if(j.contains("skill_damage"))r.skill_damage=skill_damage(j,"skill_damage");if(j.contains("enemy_kills"))r.enemy_kills=int_array<enemy_kind_count>(j,"enemy_kills",10000000);r.dash_uses=int(number_u(j,"dash_uses",0,10000000));r.items_collected=int(number_u(j,"items_collected",0,10000000));r.rerolls_used=int(number_u(j,"rerolls_used",0,100000));r.death_protections_used=int(number_u(j,"death_protections_used",0,100000));r.evolved_count=int(number_u(j,"evolved_count",0,Game::active_skill_limit));if(j.contains("equipment")){auto ids=int_array<equipment_slot_count>(j,"equipment",equipment_count-1);for(int i=0;i<equipment_slot_count;++i)r.equipment[i]=EquipmentId(ids[i]);}r.owned_relic_mask=uint32_t(number_u(j,"owned_relic_mask",0,(1u<<relic_count)-1));r.detailed_metrics=boolean(j,"detailed_metrics",false);r.difficulty=Difficulty(number_u(j,"difficulty",0,2));r.daily_key=uint32_t(number_u(j,"daily_key",0,99991231));r.daily_variant=int(number_u(j,"daily_variant",0,2));r.enemy_seen_mask=uint32_t(number_u(j,"enemy_seen_mask",0,(1u<<enemy_kind_count)-1));r.endless_cycle=int(number_u(j,"endless_cycle",0,1000000));r.consumed_skills=read_consumed_skills(j);r.fusions_created=int(number_u(j,"fusions_created",0,1000000));r.first_fusion_time=float(number_f(j,"first_fusion_time",-1,-1,r.elapsed));if(j.contains("synergy_triggers"))r.synergy_triggers=int_array<synergy_count>(j,"synergy_triggers",10000000);r.scrolls=read_scrolls(j,r.skills);r.timing=read_timing(j);r.content=read_content(j);return r;
}
uint64_t required_u(const json& j,const char* key,uint64_t max){j.at(key);return number_u(j,key,0,max);}
double required_f(const json& j,const char* key,double min=0,double max=1e15){j.at(key);return number_f(j,key,0,min,max);}
bool required_b(const json& j,const char* key){j.at(key);return boolean(j,key,false);}
template<size_t N> std::array<int,N> int_array(const json& j,const char* key,int cap){const auto& a=j.at(key);if(!a.is_array()||(a.size()!=N&&!(N==enemy_kind_count&&a.size()==10&&(std::string(key)=="enemy_kills"||std::string(key)=="enemy_kills_by_kind"))))throw std::runtime_error("Invalid snapshot array");std::array<int,N> out{};for(size_t i=0;i<a.size();++i)out[i]=int(number_u(json{{"n",a[i]}},"n",0,cap));return out;}
template<size_t N> std::array<double,N> double_array(const json& j,const char* key){const auto& a=j.at(key);if(!a.is_array()||a.size()!=N)throw std::runtime_error("Invalid metric array");std::array<double,N> out{};for(size_t i=0;i<N;++i)out[i]=number_f(json{{"n",a[i]}},"n",0);return out;}
json bonuses_json(const RunBonuses& b){return {{"damage_bonus",b.damage_bonus},{"crit_bonus",b.crit_bonus},{"health_bonus",b.health_bonus},{"move_speed_bonus",b.move_speed_bonus},{"xp_bonus",b.xp_bonus},{"cooldown_reduction",b.cooldown_reduction},{"upgrade_rerolls",b.upgrade_rerolls},{"death_protections",b.death_protections},{"first_elite_reward",b.first_elite_reward}};}
RunBonuses parse_bonuses(const json& j){RunBonuses b;b.damage_bonus=float(required_f(j,"damage_bonus",0,.10001));b.crit_bonus=float(required_f(j,"crit_bonus",0,.06001));b.health_bonus=float(required_f(j,"health_bonus",0,20));b.move_speed_bonus=float(required_f(j,"move_speed_bonus",0,.06001));b.xp_bonus=float(required_f(j,"xp_bonus",0,.10001));b.cooldown_reduction=float(required_f(j,"cooldown_reduction",0,.06001));b.upgrade_rerolls=int(required_u(j,"upgrade_rerolls",2));b.death_protections=int(required_u(j,"death_protections",1));b.first_elite_reward=int(required_u(j,"first_elite_reward",1));return b;}
json setup_json(const RunSetup& setup){return {{"mode",int(setup.mode)},{"enable_loot",setup.enable_loot},{"unlocked_relic_mask",setup.unlocked_relic_mask},{"equipment",setup.equipment},{"difficulty",int(setup.difficulty)},{"daily_key",setup.daily_key},{"daily_variant",setup.daily_variant},{"daily_character",int(setup.daily_character)},{"starting_relic_mask",setup.starting_relic_mask}};}
RunSetup parse_setup(const json& j,CharacterClass character){RunSetup setup;setup.mode=RunMode(required_u(j,"mode",3));setup.enable_loot=required_b(j,"enable_loot");setup.unlocked_relic_mask=uint32_t(required_u(j,"unlocked_relic_mask",(1u<<relic_count)-1));auto ids=int_array<equipment_slot_count>(j,"equipment",equipment_count-1);for(int i=0;i<equipment_slot_count;++i){setup.equipment[i]=EquipmentId(ids[i]);if(setup.equipment[i]!=EquipmentId::None&&!equipment_allowed(setup.equipment[i],EquipmentSlot(i),character))throw std::runtime_error("Illegal equipment in run snapshot");}setup.difficulty=Difficulty(number_u(j,"difficulty",0,2));setup.daily_key=uint32_t(number_u(j,"daily_key",0,99991231));setup.daily_variant=int(number_u(j,"daily_variant",0,2));setup.daily_character=CharacterClass(number_u(j,"daily_character",0,2));setup.starting_relic_mask=uint32_t(number_u(j,"starting_relic_mask",0,(1u<<relic_count)-1));if(setup.starting_relic_mask&~setup.unlocked_relic_mask)throw std::runtime_error("Starting relic is not in the run pool");return setup;}
RunSetup build_setup(const RunBuild& build){RunSetup setup;setup.mode=RunMode::Campaign;setup.enable_loot=build.enable_loot;setup.unlocked_relic_mask=build.unlocked_relic_mask;setup.equipment=build.equipment;setup.difficulty=build.difficulty;setup.daily_key=build.daily_key;setup.daily_variant=build.daily_variant;setup.daily_character=build.daily_character;setup.starting_relic_mask=build.starting_relic_mask;return setup;}
bool near_number(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=.0001*std::max(1.,std::max(std::abs(a),std::abs(b)));}
bool same_bonuses(const RunBonuses& a,const RunBonuses& b){return near_number(a.damage_bonus,b.damage_bonus)&&near_number(a.crit_bonus,b.crit_bonus)&&near_number(a.health_bonus,b.health_bonus)&&near_number(a.move_speed_bonus,b.move_speed_bonus)&&near_number(a.xp_bonus,b.xp_bonus)&&near_number(a.cooldown_reduction,b.cooldown_reduction)&&a.upgrade_rerolls==b.upgrade_rerolls&&a.death_protections==b.death_protections&&a.first_elite_reward==b.first_elite_reward;}
#define METRIC_INTS(X) X(upgrade_choices) X(bonus_choices) X(dash_uses) X(upgrade_rerolls_used) X(death_protections_used) X(first_elite_rewards) X(events_started) X(events_succeeded) X(events_failed) X(items_collected) X(boss_attacks) X(chapters_cleared) X(bosses_defeated) X(hazards_spawned) X(environment_hits) X(relics_collected) X(endless_cycle)
#define METRIC_DOUBLES(X) X(other_damage) X(damage_taken) X(altar_health_spent) X(xp_acquired) X(boss_damage_taken) X(environment_damage_taken)
json metrics_json(const RunMetrics& m){json j={{"resonance",resonance_json(m.resonance)},{"content",content_json(m.content)},{"character",int(m.character)},{"enemy_kills_by_kind",m.enemy_kills_by_kind},{"skill_damage",skill_table_json(m.skill_damage)},{"xp_by_minute",m.xp_by_minute},{"boss_spawn_time",m.boss_spawn_time},{"boss_defeat_time",m.boss_defeat_time},{"relic_procs",m.relic_procs},{"equipment_procs",m.equipment_procs},{"enemy_seen_mask",m.enemy_seen_mask},{"fusions_created",m.fusions_created},{"first_fusion_time",m.first_fusion_time},{"synergy_triggers",m.synergy_triggers}};
#define PUT_METRIC(field) j[#field]=m.field;
 METRIC_INTS(PUT_METRIC) METRIC_DOUBLES(PUT_METRIC)
#undef PUT_METRIC
 return j;}
RunMetrics parse_metrics(const json& j){RunMetrics m;m.resonance=read_resonance(j);m.content=read_content(j);m.character=CharacterClass(required_u(j,"character",2));m.enemy_kills_by_kind=int_array<enemy_kind_count>(j,"enemy_kills_by_kind",10000000);m.skill_damage=skill_damage(j,"skill_damage");const auto& buckets=j.at("xp_by_minute");if(!buckets.is_array()||(buckets.size()!=10&&buckets.size()!=xp_bucket_count))throw std::runtime_error("Invalid XP time buckets");for(size_t i=0;i<buckets.size();++i)m.xp_by_minute[i]=number_f(json{{"n",buckets[i]}},"n",0);m.enemy_seen_mask=uint32_t(number_u(j,"enemy_seen_mask",0,(1u<<enemy_kind_count)-1));m.fusions_created=int(number_u(j,"fusions_created",0,1000000));m.first_fusion_time=float(number_f(j,"first_fusion_time",-1,-1,86400));if(j.contains("synergy_triggers"))m.synergy_triggers=int_array<synergy_count>(j,"synergy_triggers",10000000);m.boss_spawn_time=float(required_f(j,"boss_spawn_time",-1,86400));m.boss_defeat_time=float(required_f(j,"boss_defeat_time",-1,86400));if(j.contains("relic_procs"))m.relic_procs=int_array<relic_count>(j,"relic_procs",10000000);if(j.contains("equipment_procs"))m.equipment_procs=int_array<equipment_count>(j,"equipment_procs",10000000);
#define GET_INT(field) m.field=int(number_u(j,#field,0,10000000));
#define GET_DOUBLE(field) m.field=required_f(j,#field);
 METRIC_INTS(GET_INT) METRIC_DOUBLES(GET_DOUBLE)
#undef GET_INT
#undef GET_DOUBLE
 return m;}
#undef METRIC_INTS
#undef METRIC_DOUBLES
json build_json(const RunBuild& b){return {{"run_gear",gear_json(b.run_gear)},{"resonance_directions",resonance_directions_json(b.resonance_directions)},{"scrolls",scrolls_json(b.scrolls)},{"timing",timing_json(b.timing)},{"encounter_plan",plan_json(b.encounter_plan)},{"rng_state",b.rng_state},{"version",b.version},{"chapter_index",b.chapter_index},{"intermission_choice",b.intermission_choice},{"seed",b.seed},{"character",int(b.character)},{"completed_elapsed",b.completed_elapsed},{"hp",b.hp},{"xp",b.xp},{"level",b.level},{"kills",b.kills},{"coins",b.coins},{"overcharge_levels",b.overcharge_levels},{"vitality_levels",b.vitality_levels},{"skills",skill_table_json(b.skills)},{"passives",b.passives},{"bonuses",bonuses_json(b.bonuses)},{"upgrade_rerolls_remaining",b.upgrade_rerolls_remaining},{"death_protections_remaining",b.death_protections_remaining},{"first_elite_reward_claimed",b.first_elite_reward_claimed},{"boss_defeated",b.boss_defeated},{"next_chapter_shield_seconds",b.next_chapter_shield_seconds},{"metrics",metrics_json(b.metrics)},{"enable_loot",b.enable_loot},{"unlocked_relic_mask",b.unlocked_relic_mask},{"owned_relic_mask",b.owned_relic_mask},{"equipment",b.equipment},{"difficulty",int(b.difficulty)},{"daily_key",b.daily_key},{"daily_variant",b.daily_variant},{"daily_character",int(b.daily_character)},{"starting_relic_mask",b.starting_relic_mask},{"consumed_skills",consumed_skills_json(b.consumed_skills)},{"dragon_soul",b.dragon_soul},{"guard_charge",b.guard_charge},{"battle_focus",b.battle_focus}};}
RunBuild parse_build(const json& j){RunBuild b;b.run_gear=read_gear(j);b.resonance_directions=read_resonance_directions(j);b.version=int(required_u(j,"version",1));b.chapter_index=int(required_u(j,"chapter_index",1));const auto& choice=j.at("intermission_choice");if(!choice.is_number_integer()||choice.get<int64_t>() < -1||choice.get<int64_t>()>2)throw std::runtime_error("Invalid supply selection");b.intermission_choice=choice.get<int>();b.seed=uint32_t(required_u(j,"seed",UINT32_MAX));b.character=CharacterClass(required_u(j,"character",2));b.completed_elapsed=float(required_f(j,"completed_elapsed",0,86400));b.hp=float(required_f(j,"hp"));b.xp=float(required_f(j,"xp"));b.level=int(required_u(j,"level",100000));b.kills=int(required_u(j,"kills",10000000));b.coins=int(required_u(j,"coins",10000000));b.overcharge_levels=int(required_u(j,"overcharge_levels",100000));b.vitality_levels=int(required_u(j,"vitality_levels",100000));b.skills=skill_ranks(j,"skills");b.passives=int_array<passive_count>(j,"passives",8);b.bonuses=parse_bonuses(j.at("bonuses"));b.upgrade_rerolls_remaining=int(required_u(j,"upgrade_rerolls_remaining",2));b.death_protections_remaining=int(required_u(j,"death_protections_remaining",1));b.first_elite_reward_claimed=required_b(j,"first_elite_reward_claimed");b.boss_defeated=required_b(j,"boss_defeated");b.next_chapter_shield_seconds=float(required_f(j,"next_chapter_shield_seconds",0,8));b.metrics=parse_metrics(j.at("metrics"));b.enable_loot=boolean(j,"enable_loot",false);b.unlocked_relic_mask=uint32_t(number_u(j,"unlocked_relic_mask",0,(1u<<relic_count)-1));b.owned_relic_mask=uint32_t(number_u(j,"owned_relic_mask",0,(1u<<relic_count)-1));if(j.contains("equipment")){auto ids=int_array<equipment_slot_count>(j,"equipment",equipment_count-1);for(int i=0;i<equipment_slot_count;++i)b.equipment[i]=EquipmentId(ids[i]);}b.difficulty=Difficulty(number_u(j,"difficulty",0,2));b.daily_key=uint32_t(number_u(j,"daily_key",0,99991231));b.daily_variant=int(number_u(j,"daily_variant",0,2));b.daily_character=CharacterClass(number_u(j,"daily_character",0,2));b.starting_relic_mask=uint32_t(number_u(j,"starting_relic_mask",0,(1u<<relic_count)-1));b.consumed_skills=read_consumed_skills(j);b.dragon_soul=float(number_f(j,"dragon_soul",0,0,100));b.guard_charge=float(number_f(j,"guard_charge",0,0,40));b.battle_focus=float(number_f(j,"battle_focus",0,0,100));b.scrolls=read_scrolls(j,b.skills);b.timing=read_timing(j);b.encounter_plan=read_plan(j);if(j.contains("rng_state")){if(!j.at("rng_state").is_string()||j.at("rng_state").get_ref<const std::string&>().size()>20000)throw std::runtime_error("Invalid random state");b.rng_state=j.at("rng_state").get<std::string>();}Game validate;if(!validate.restore_build(b))throw std::runtime_error("Invalid chapter-boundary build");b.scrolls=validate.scrolls;b.encounter_plan=validate.encounter_plan;return b;}
RunReport build_report(const RunBuild& b){RunReport r;r.run_gear=b.run_gear;r.resonance_directions=b.resonance_directions;r.scrolls=b.scrolls;r.timing=b.timing;r.content=b.metrics.content;r.outcome=RunOutcome::Victory;r.character=b.character;r.seed=b.seed;r.elapsed=b.completed_elapsed;r.kills=b.kills;r.elite_kills=b.metrics.enemy_kills_by_kind[int(EnemyKind::Elite)];r.events_succeeded=b.metrics.events_succeeded;r.level=b.level;r.boss_defeated=b.boss_defeated;r.damage_dealt=b.metrics.other_damage;for(double d:b.metrics.skill_damage)r.damage_dealt+=d;r.resonance=b.metrics.resonance;for(double d:r.resonance.damage)r.damage_dealt+=d;r.damage_taken=b.metrics.damage_taken;r.xp_acquired=b.metrics.xp_acquired;r.mode=RunMode::Campaign;r.chapters_cleared=b.metrics.chapters_cleared;r.bosses_defeated=b.metrics.bosses_defeated;r.skills=b.skills;r.passives=b.passives;r.skill_damage=b.metrics.skill_damage;r.enemy_kills=b.metrics.enemy_kills_by_kind;r.dash_uses=b.metrics.dash_uses;r.items_collected=b.metrics.items_collected;r.rerolls_used=b.metrics.upgrade_rerolls_used;r.death_protections_used=b.metrics.death_protections_used;r.equipment=b.equipment;r.owned_relic_mask=b.owned_relic_mask;r.detailed_metrics=true;r.difficulty=b.difficulty;r.daily_key=b.daily_key;r.daily_variant=b.daily_variant;r.enemy_seen_mask=b.metrics.enemy_seen_mask;r.endless_cycle=b.metrics.endless_cycle;r.consumed_skills=b.consumed_skills;r.fusions_created=b.metrics.fusions_created;r.first_fusion_time=b.metrics.first_fusion_time;r.synergy_triggers=b.metrics.synergy_triggers;Game preview;if(preview.restore_build(b))for(int i=0;i<skill_count;++i)if(preview.evolved(Skill(i)))++r.evolved_count;return r;}
bool same_progress(const RunReport& a,const RunReport& b){return gear_json(a.run_gear)==gear_json(b.run_gear)&&a.resonance_directions==b.resonance_directions&&resonance_json(a.resonance)==resonance_json(b.resonance)&&scrolls_json(a.scrolls)==scrolls_json(b.scrolls)&&content_json(a.content)==content_json(b.content)&&a.mode==b.mode&&a.difficulty==b.difficulty&&a.daily_key==b.daily_key&&a.daily_variant==b.daily_variant&&a.character==b.character&&a.seed==b.seed&&near_number(a.elapsed,b.elapsed)&&a.kills==b.kills&&a.elite_kills==b.elite_kills&&a.events_succeeded==b.events_succeeded&&a.level==b.level&&a.boss_defeated==b.boss_defeated&&near_number(a.damage_dealt,b.damage_dealt)&&near_number(a.damage_taken,b.damage_taken)&&near_number(a.xp_acquired,b.xp_acquired)&&a.chapters_cleared==b.chapters_cleared&&a.bosses_defeated==b.bosses_defeated&&a.consumed_skills==b.consumed_skills&&a.fusions_created==b.fusions_created&&near_number(a.first_fusion_time,b.first_fusion_time)&&a.synergy_triggers==b.synergy_triggers&&(!a.detailed_metrics||!b.detailed_metrics||(a.skills==b.skills&&a.passives==b.passives&&a.skill_damage==b.skill_damage&&a.enemy_kills==b.enemy_kills&&a.dash_uses==b.dash_uses&&a.items_collected==b.items_collected&&a.rerolls_used==b.rerolls_used&&a.death_protections_used==b.death_protections_used&&a.evolved_count==b.evolved_count&&a.equipment==b.equipment&&a.owned_relic_mask==b.owned_relic_mask));}
bool progress_at_least(const RunReport& a,const RunReport& b){if(a.scrolls.eligible_rewards<b.scrolls.eligible_rewards||a.scrolls.opportunities<b.scrolls.opportunities||a.scrolls.studies_used<b.scrolls.studies_used||(a.scrolls.learned_foreign&b.scrolls.learned_foreign)!=b.scrolls.learned_foreign)return false;for(size_t i=0;i<32;++i)if(a.content.boss_kills[i]<b.content.boss_kills[i])return false;for(size_t i=0;i<64;++i)if(a.content.enemy_kills[i]<b.content.enemy_kills[i])return false;for(size_t i=0;i<a.synergy_triggers.size();++i)if(a.synergy_triggers[i]<b.synergy_triggers[i])return false;return a.elapsed+.01f>=b.elapsed&&a.kills>=b.kills&&a.elite_kills>=b.elite_kills&&a.events_succeeded>=b.events_succeeded&&a.level>=b.level&&a.damage_dealt+.01>=b.damage_dealt&&a.damage_taken+.01>=b.damage_taken&&a.xp_acquired+.01>=b.xp_acquired&&a.chapters_cleared>=b.chapters_cleared&&a.bosses_defeated>=b.bosses_defeated&&a.fusions_created>=b.fusions_created&&(a.consumed_skills&b.consumed_skills)==b.consumed_skills&&(b.first_fusion_time<0||near_number(a.first_fusion_time,b.first_fusion_time));}
int campaign_entitlement(RunReport report){report.outcome=RunOutcome::Defeat;return settlement_crystals(report);}
json checkpoint_json(uint64_t id,const RunBuild& b){return {{"boundary","chapter"},{"run_id",id},{"next_chapter",b.chapter_index+2},{"completed_chapter",b.chapter_index},{"build",build_json(b)}};}
RunBuild checked_checkpoint(const json& checkpoint,const ActiveRun& active){if(!checkpoint.is_object()||checkpoint.value("boundary",std::string())!="chapter"||required_u(checkpoint,"run_id",serial_limit)!=active.id||active.mode!=RunMode::Campaign)throw std::runtime_error("Checkpoint does not match the active campaign");RunBuild b=parse_build(checkpoint.at("build"));if(b.chapter_index+1!=active.claimed_chapters||required_u(checkpoint,"completed_chapter",1)!=uint64_t(b.chapter_index)||required_u(checkpoint,"next_chapter",3)!=uint64_t(b.chapter_index+2)||b.character!=active.character||b.seed!=active.seed||!same_bonuses(b.bonuses,active.bonuses)||setup_json(build_setup(b))!=setup_json(active.setup))throw std::runtime_error("Checkpoint identity or talent snapshot mismatch");auto r=build_report(b);if(!same_progress(r,active.claimed_report))throw std::runtime_error("Checkpoint progress differs from its chapter receipt");return b;}
uint64_t achievement_value(const AchievementStats& s,AchievementMetric metric){switch(metric){case AchievementMetric::Kills:return s.kills;case AchievementMetric::Elites:return s.elites;case AchievementMetric::Bosses:return s.bosses;case AchievementMetric::Events:return s.events;case AchievementMetric::Items:return s.items;case AchievementMetric::Chapters:return s.chapters;case AchievementMetric::CampaignWins:return s.campaign_wins;case AchievementMetric::ArcherWins:return s.class_wins[0];case AchievementMetric::MageWins:return s.class_wins[1];case AchievementMetric::WarriorWins:return s.class_wins[2];case AchievementMetric::Level:return s.best_level;case AchievementMetric::Evolutions:return s.best_evolutions;case AchievementMetric::Dashes:return s.dashes;case AchievementMetric::Skills:return s.best_skills;case AchievementMetric::RunKills:return s.best_run_kills;case AchievementMetric::LowDamageWins:return s.low_damage_wins;}return 0;}
json achievement_stats_json(const AchievementStats& s){return {{"kills",s.kills},{"elites",s.elites},{"bosses",s.bosses},{"events",s.events},{"items",s.items},{"dashes",s.dashes},{"chapters",s.chapters},{"campaign_wins",s.campaign_wins},{"low_damage_wins",s.low_damage_wins},{"class_wins",s.class_wins},{"enemy_kills",s.enemy_kills},{"best_level",s.best_level},{"best_evolutions",s.best_evolutions},{"best_skills",s.best_skills},{"best_run_kills",s.best_run_kills}};}
template<size_t N> std::array<uint64_t,N> unsigned_array(const json& j,const char* key){const auto& a=j.at(key);if(!a.is_array()||(a.size()!=N&&!(N==enemy_kind_count&&a.size()==10&&(std::string(key)=="enemy_kills"||std::string(key)=="enemy_kills_by_kind"))))throw std::runtime_error("Invalid lifetime statistics array");std::array<uint64_t,N> out{};for(size_t i=0;i<a.size();++i)out[i]=number_u(json{{"n",a[i]}},"n",0);return out;}
template<size_t N> std::array<bool,N> bool_array(const json& j,const char* key){const auto& a=j.at(key);if(!a.is_array()||a.size()!=N)throw std::runtime_error("Invalid unlock array");std::array<bool,N> out{};for(size_t i=0;i<N;++i){if(!a[i].is_boolean())throw std::runtime_error("Invalid unlock boolean");out[i]=a[i].get<bool>();}return out;}
AchievementStats parse_achievement_stats(const json& j){AchievementStats s;
#define READ_STAT(field) s.field=required_u(j,#field,serial_limit);
 READ_STAT(kills) READ_STAT(elites) READ_STAT(bosses) READ_STAT(events) READ_STAT(items) READ_STAT(dashes) READ_STAT(chapters) READ_STAT(campaign_wins) READ_STAT(low_damage_wins)
#undef READ_STAT
 s.class_wins=unsigned_array<3>(j,"class_wins");s.enemy_kills=unsigned_array<enemy_kind_count>(j,"enemy_kills");s.best_level=int(required_u(j,"best_level",100000));s.best_evolutions=int(required_u(j,"best_evolutions",Game::active_skill_limit));s.best_skills=int(required_u(j,"best_skills",Game::active_skill_limit));s.best_run_kills=int(required_u(j,"best_run_kills",10000000));return s;}
struct AchievementAwards { int crystals=0;std::vector<AchievementId> completed; };
AchievementAwards account_achievements(Profile& profile,const RunReport& report,bool final){
 auto& active=*profile.active_run;auto& stats=profile.achievement_stats;const auto& prior=active.accounted_report;
 accumulate_content(profile.lifetime_content,report.content,prior.content);
 auto add=[](uint64_t& total,int now,int previous){uint64_t delta=uint64_t(std::max(0,now-previous));if(total>serial_limit-delta)throw std::runtime_error("Lifetime statistics limit reached");total+=delta;};
 add(stats.kills,report.kills,prior.kills);add(stats.elites,report.elite_kills,prior.elite_kills);add(stats.bosses,report.bosses_defeated,prior.bosses_defeated);add(stats.events,report.events_succeeded,prior.events_succeeded);add(stats.items,report.items_collected,prior.items_collected);add(stats.dashes,report.dash_uses,prior.dash_uses);add(stats.chapters,report.chapters_cleared,prior.chapters_cleared);for(int i=0;i<enemy_kind_count;++i)add(stats.enemy_kills[i],report.enemy_kills[i],prior.enemy_kills[i]);
 stats.best_level=std::max(stats.best_level,report.level);stats.best_evolutions=std::max(stats.best_evolutions,report.evolved_count);stats.best_run_kills=std::max(stats.best_run_kills,report.kills);int skills=0;for(int rank:report.skills)if(rank>0)++skills;stats.best_skills=std::max(stats.best_skills,skills);
 profile.enemy_seen_mask|=report.enemy_seen_mask;for(int i=0;i<enemy_kind_count;++i)if(report.enemy_kills[i]>0)profile.enemy_seen_mask|=1u<<i;
 if(final&&report.outcome==RunOutcome::Victory&&(report.mode==RunMode::Classic||report.mode==RunMode::Campaign)){if(report.adventurer){std::array<bool,3> used{};for(int i=0;i<skill_count;++i)if(report.skill_damage[i]>0){used[int(Game::skill_owner(Skill(i)))]=true;for(const auto& recipe:fusion_recipes())if(recipe.result==Skill(i)&&recipe.other_class>=0)used[recipe.other_class]=true;}for(int i=0;i<3;++i)if(used[i])++stats.class_wins[i];}else ++stats.class_wins[int(report.character)];if(report.mode==RunMode::Campaign&&report.chapters_cleared==3)stats.campaign_wins++;if(report.elapsed>=180&&report.kills>=100&&report.damage_taken<=30)stats.low_damage_wins++;}
 active.accounted_report=report;AchievementAwards rewards;for(const auto& d:achievement_catalog){int i=int(d.id);if(profile.achievement_completed[i]||achievement_value(stats,d.metric)<d.target)continue;profile.achievement_completed[i]=true;if(d.equipment_reward!=EquipmentId::None)profile.owned_equipment[int(d.equipment_reward)]=true;profile.unlocked_relic_mask|=d.relic_reward_mask;if(d.challenge_reward)profile.challenge_eligible=true;rewards.crystals+=d.crystal_reward;rewards.completed.push_back(d.id);}
 active.achievement_crystals+=rewards.crystals;return rewards;
}
uint32_t date_key(const std::string& date){if(date.size()!=10||date[4]!='-'||date[7]!='-')return 0;for(size_t i=0;i<date.size();++i)if(i!=4&&i!=7&&(date[i]<'0'||date[i]>'9'))return 0;int year=std::stoi(date.substr(0,4)),month=std::stoi(date.substr(5,2)),day=std::stoi(date.substr(8,2));if(year<1||month<1||month>12)return 0;int days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(year%4==0&&(year%100!=0||year%400==0))days[1]=29;if(day<1||day>days[month-1])return 0;return uint32_t(year*10000+month*100+day);}
int best_index(CharacterClass character,Difficulty difficulty,RunMode mode){int c=int(character),d=int(difficulty),m=int(mode);return c>=0&&c<3&&d>=0&&d<3&&m>=0&&m<4?(c*3+d)*4+m:-1;}
uint64_t run_score(const RunReport& report){return uint64_t(std::max(report.kills,0))*10+uint64_t(std::max(report.level,1))*100+uint64_t(std::max(report.bosses_defeated,0))*5000+uint64_t(std::max(report.elapsed,0.f))+(report.outcome==RunOutcome::Victory?100000:0);}
void update_best(RunBest& best,const RunReport& report,bool final){best.recorded=true;best.longest=std::max(best.longest,report.elapsed);best.kills=std::max(best.kills,report.kills);best.level=std::max(best.level,report.level);best.bosses=std::max(best.bosses,report.bosses_defeated);best.damage=std::max(best.damage,report.damage_dealt);best.best_score=std::max(best.best_score,run_score(report));if(final){++best.runs;if(report.outcome==RunOutcome::Victory)++best.victories;}}
json best_json(const RunBest& best){return {{"recorded",best.recorded},{"longest",best.longest},{"kills",best.kills},{"level",best.level},{"bosses",best.bosses},{"damage",best.damage},{"runs",best.runs},{"victories",best.victories},{"best_score",best.best_score}};}
RunBest parse_best(const json& j){RunBest b;b.recorded=required_b(j,"recorded");b.longest=float(required_f(j,"longest",0,86400));b.kills=int(required_u(j,"kills",10000000));b.level=int(required_u(j,"level",100000));b.bosses=int(required_u(j,"bosses",1000000));b.damage=required_f(j,"damage");b.runs=required_u(j,"runs",serial_limit);b.victories=required_u(j,"victories",serial_limit);b.best_score=required_u(j,"best_score",serial_limit);return b;}
DailyRecord& get_daily(Profile& profile,const std::string& date){for(auto& record:profile.daily_records)if(record.date==date)return record;DailyRecord fresh;fresh.date=date;profile.daily_records.push_back(fresh);return profile.daily_records.back();}
int record_progress(Profile& profile,const RunReport& report,bool final){int index=best_index(report.character,report.difficulty,report.mode);if(index>=0)update_best(profile.bests[index],report,final);if(!final)return 0;if(report.mode!=RunMode::Daily){if(report.outcome==RunOutcome::Victory&&report.difficulty==Difficulty::Hard)profile.nightmare_eligible=true;return 0;}auto& record=get_daily(profile,profile.active_run->daily_date);update_best(record.best,report,true);if(report.outcome==RunOutcome::Victory){++record.wins;if(!record.reward_claimed&&report.elapsed>=60&&report.kills>=20){record.reward_claimed=true;return 8;}}return 0;}
uint64_t schema_version(const json& j){if(!j.is_object())throw std::runtime_error("Profile is not an object");if(j.contains("schema_version"))return number_u(j,"schema_version",0,UINT64_MAX);if(j.contains("version"))return number_u(j,"version",0,UINT64_MAX);throw std::runtime_error("Profile schema version is missing");}
void required_fields(const json& j,std::initializer_list<const char*> fields){if(!j.is_object())throw std::runtime_error("Expected a profile object");for(const char* field:fields)if(!j.contains(field))throw std::runtime_error(std::string("Incomplete profile, missing: ")+field);}
constexpr const char* guide_keys[]={"eight_slots","choose_two","replace_skill","automatic_fusion","effective_level","run_equipment","compact_pace"};
void read_guidance_settings(ProfileSettings& settings,const json& j){
 settings.flash_strength=float(number_f(j,"flash_strength",1,0,1));settings.ui_scale=float(number_f(j,"ui_scale",1,.85,1.25));settings.guidance=boolean(j,"guidance",true);
 if(j.contains("seen_guides")){const auto& entries=j.at("seen_guides");if(!entries.is_array())throw std::runtime_error("Invalid seen guides");for(const auto& entry:entries){const auto key=entry.get<std::string>();auto found=std::find(std::begin(guide_keys),std::end(guide_keys),key);if(found==std::end(guide_keys))throw UnsupportedSkillContent("Unknown guide ID");settings.guide_seen|=1u<<int(found-std::begin(guide_keys));}}
 if(j.contains("seen_boss_hints")){const auto& entries=j.at("seen_boss_hints");if(!entries.is_array())throw std::runtime_error("Invalid Boss hints");for(const auto& entry:entries){const auto key=entry.get<std::string>();const auto& bosses=boss_definitions();auto found=std::find_if(bosses.begin(),bosses.end(),[&](const BossDefinition& d){return key==d.key;});if(found==bosses.end())throw UnsupportedSkillContent("Unknown Boss hint ID");settings.boss_hints_seen|=1u<<int(found->id);}}
}
void write_guidance_settings(json& j,const ProfileSettings& settings){
 j["flash_strength"]=settings.flash_strength;j["ui_scale"]=settings.ui_scale;j["guidance"]=settings.guidance;j["seen_guides"]=json::array();for(int i=0;i<7;++i)if(settings.guide_seen&(1u<<i))j["seen_guides"].push_back(guide_keys[i]);
 j["seen_boss_hints"]=json::array();for(const auto& boss:boss_definitions())if(settings.boss_hints_seen&(1u<<int(boss.id)))j["seen_boss_hints"].push_back(boss.key);
}
Profile parse_profile(const json& j){
 const auto version=schema_version(j);const bool current_schema=version>=1;
 if(current_schema){required_fields(j,{"revision","crystals","next_run_serial","closed_through_serial","talents","active_run","settings","records","unlocks","history"});required_fields(j.at("records"),{"runs","victories","total_kills","total_crystals","longest_run","best_level"});required_fields(j.at("unlocks"),{"classes","skills","equipment","relics"});for(const auto& d:definitions)required_fields(j.at("talents"),{d.key});if(!j.at("active_run").is_null())required_fields(j.at("active_run"),{"id","character","seed","started_at"});}
 Profile p=fresh_profile();p.revision=number_u(j,"revision",0);p.crystals=int64_t(number_u(j,"crystals",0,currency_limit));p.next_run_serial=number_u(j,"next_run_serial",1);p.closed_through_serial=number_u(j,"closed_through_serial",0);if(p.next_run_serial<1||p.closed_through_serial>=p.next_run_serial)throw std::runtime_error("Invalid run serial ordering");
 object_field(j,"talents");if(j.contains("talents"))for(const auto& d:definitions)p.talent_ranks[int(d.id)]=int(number_u(j["talents"],d.key,0,d.max_rank));
 if(j.contains("active_run")&&!j["active_run"].is_null()){
  const auto& a=j["active_run"];if(!a.is_object())throw std::runtime_error("Invalid active run");ActiveRun run;run.id=number_u(a,"id",0);run.character=CharacterClass(number_u(a,"character",0,2));run.seed=uint32_t(number_u(a,"seed",0,UINT32_MAX));run.started_at=int64_t(number_u(a,"started_at",0));
  if(version>=2){required_fields(a,{"mode","claimed_chapters","claimed_crystals","claimed_report","bonuses"});run.mode=RunMode(number_u(a,"mode",0,3));run.claimed_chapters=int(number_u(a,"claimed_chapters",0,2));run.claimed_crystals=int(number_u(a,"claimed_crystals",0,100000));run.claimed_report=parse_report(a.at("claimed_report"));run.bonuses=parse_bonuses(a.at("bonuses"));run.setup.mode=run.mode;if(version>=3){required_fields(a,{"setup","accounted_report","achievement_crystals"});run.setup=parse_setup(a.at("setup"),run.character);if(run.setup.mode!=run.mode)throw std::runtime_error("Run setup mode mismatch");run.accounted_report=parse_report(a.at("accounted_report"));run.achievement_crystals=int(number_u(a,"achievement_crystals",0,100000));if(version>=4){required_fields(a,{"daily_date"});if(!a.at("daily_date").is_string())throw std::runtime_error("Invalid daily date");run.daily_date=a.at("daily_date").get<std::string>();if(run.mode==RunMode::Daily){auto spec=make_daily_spec(run.daily_date);if(!spec.valid||spec.character!=run.character||spec.seed!=run.seed||setup_json(spec.setup)!=setup_json(run.setup)||!same_bonuses(run.bonuses,RunBonuses{}))throw std::runtime_error("Daily starting conditions differ from the fixed date rules");}else if(!run.daily_date.empty())throw std::runtime_error("Non-daily run has a daily date");}}if(run.mode!=RunMode::Campaign&&(run.claimed_chapters||run.claimed_crystals))throw std::runtime_error("Classic run has campaign claims");if(!run.claimed_chapters&&run.claimed_crystals)throw std::runtime_error("Crystals claimed without a chapter");if(run.claimed_chapters&&(run.claimed_report.mode!=RunMode::Campaign||run.claimed_report.character!=run.character||run.claimed_report.seed!=run.seed||run.claimed_report.chapters_cleared!=run.claimed_chapters||run.claimed_report.bosses_defeated>run.claimed_chapters||campaign_entitlement(run.claimed_report)!=run.claimed_crystals))throw std::runtime_error("Invalid cumulative chapter receipt");}
  if(run.id==0||run.id<=p.closed_through_serial||run.id>=p.next_run_serial)throw std::runtime_error("Invalid active run serial");p.active_run=run;
 }
 object_field(j,"settings");if(j.contains("settings")){auto& s=j["settings"];p.settings.music=boolean(s,"music",true);p.settings.sound=boolean(s,"sound",true);p.settings.ui_sound=boolean(s,"ui_sound",true);p.settings.ui_volume=float(number_f(s,"ui_volume",.5,0,1));p.settings.master_volume=float(number_f(s,"master_volume",1,0,1));p.settings.sound_volume=float(number_f(s,"sound_volume",.35,0,1));p.settings.music_volume=float(number_f(s,"music_volume",.35,0,1));p.settings.zoom=float(number_f(s,"zoom",1.6,.25,4));p.settings.shake=boolean(s,"shake",true);p.settings.show_stats=boolean(s,"show_stats",false);p.settings.friendly_effect_alpha=float(number_f(s,"friendly_effect_alpha",.7,.15,1));p.settings.shake_strength=float(number_f(s,"shake_strength",1,0,1));p.settings.damage_number_density=float(number_f(s,"damage_number_density",.7,0,1));p.settings.low_effects=boolean(s,"low_effects",false);p.settings.game_speed=int(number_u(s,"game_speed",1,2));if(p.settings.game_speed<1)throw std::runtime_error("Invalid saved game speed");read_guidance_settings(p.settings,s);}
 object_field(j,"records");if(j.contains("records")){auto& r=j["records"];p.records.runs=number_u(r,"runs",0);p.records.victories=number_u(r,"victories",0);p.records.total_kills=number_u(r,"total_kills",0);p.records.total_crystals=number_u(r,"total_crystals",0);p.records.longest_run=float(number_f(r,"longest_run",0,0,86400));p.records.best_level=int(number_u(r,"best_level",1,100000));p.records.total_elapsed=number_f(r,"total_elapsed",0);if(r.contains("class_runs"))p.records.class_runs=unsigned_array<3>(r,"class_runs");if(version>=4)required_fields(r,{"total_elapsed","class_runs"});}
 object_field(j,"unlocks");if(j.contains("unlocks")){p.unlocks=j["unlocks"];const auto defaults=fresh_profile().unlocks;for(const char* k:{"classes","skills","equipment","relics"}){if(!p.unlocks.contains(k))p.unlocks[k]=defaults.at(k);else if(!p.unlocks[k].is_array())throw std::runtime_error("Invalid unlock list");}}
 object_field(j,"chapter_progress");if(j.contains("chapter_progress"))p.chapter_progress=j["chapter_progress"];
 if(j.contains("achievements")){if(!j["achievements"].is_array())throw std::runtime_error("Invalid achievements");p.achievements=j["achievements"];}
 if(j.contains("chapter_checkpoint")&&!j["chapter_checkpoint"].is_null()){
  auto& c=j["chapter_checkpoint"];if(!c.is_object()||c.value("boundary",std::string())!="chapter"||!p.active_run||number_u(c,"run_id",0)!=p.active_run->id)throw std::runtime_error("Invalid chapter checkpoint");p.chapter_checkpoint=c;if(p.active_run->mode==RunMode::Campaign){if(version<3){auto legacy=parse_build(c.at("build"));p.active_run->setup=build_setup(legacy);}checked_checkpoint(c,*p.active_run);}
 }
 if(p.active_run&&p.active_run->mode==RunMode::Campaign&&p.active_run->claimed_chapters>0&&p.chapter_checkpoint.is_null())throw std::runtime_error("Claimed campaign is missing its boundary checkpoint");
 if(j.contains("history")){
  if(!j["history"].is_array()||j["history"].size()>128)throw std::runtime_error("Invalid run history");
  uint64_t previous=0;for(auto& h:j["history"]){if(current_schema){required_fields(h,{"run_id","crystals","settled_at","report"});required_fields(h.at("report"),{"outcome","character","seed","elapsed","kills","elite_kills","events_succeeded","level","boss_defeated","damage_dealt","damage_taken","xp_acquired"});}if(!h.is_object())throw std::runtime_error("Invalid settlement receipt");SettlementReceipt r;r.run_id=number_u(h,"run_id",0);r.crystals=int(number_u(h,"crystals",0,100000));r.settled_at=int64_t(number_u(h,"settled_at",0));r.report=parse_report(h.at("report"));if(r.run_id==0||r.run_id<=previous||r.run_id>p.closed_through_serial)throw std::runtime_error("Invalid settlement receipt ordering");previous=r.run_id;p.history.push_back(r);}
 }
 if(version>=3){required_fields(j,{"owned_equipment","equipped","unlocked_relic_mask","challenge_eligible","achievement_completed","achievement_stats"});p.owned_equipment=bool_array<equipment_count>(j,"owned_equipment");p.owned_equipment[int(EquipmentId::None)]=false;auto equipped=int_array<equipment_slot_count>(j,"equipped",equipment_count-1);for(int i=0;i<equipment_slot_count;++i){p.equipped[i]=EquipmentId(equipped[i]);if(p.equipped[i]!=EquipmentId::None&&(!p.owned_equipment[equipped[i]]||equipment_definitions()[equipped[i]].slot!=EquipmentSlot(i)))throw std::runtime_error("Invalid equipped inventory item");}p.unlocked_relic_mask=uint32_t(required_u(j,"unlocked_relic_mask",(1u<<relic_count)-1));p.challenge_eligible=required_b(j,"challenge_eligible");p.achievement_completed=bool_array<achievement_count>(j,"achievement_completed");p.achievement_stats=parse_achievement_stats(j.at("achievement_stats"));}else{p.achievement_stats.kills=p.records.total_kills;p.achievement_stats.best_level=p.records.best_level;for(const auto& history:p.history){const auto& r=history.report;p.achievement_stats.elites+=r.elite_kills;p.achievement_stats.bosses+=r.bosses_defeated;p.achievement_stats.events+=r.events_succeeded;p.achievement_stats.chapters+=r.chapters_cleared;if(r.outcome==RunOutcome::Victory){p.achievement_stats.class_wins[int(r.character)]++;if(r.mode==RunMode::Campaign)p.achievement_stats.campaign_wins++;}}}
 if(version>=4){required_fields(j,{"nightmare_eligible","enemy_seen_mask","bests","daily_records"});p.nightmare_eligible=required_b(j,"nightmare_eligible");p.enemy_seen_mask=uint32_t(required_u(j,"enemy_seen_mask",(1u<<enemy_kind_count)-1));if(!j.at("bests").is_array()||j.at("bests").size()!=run_best_count)throw std::runtime_error("Invalid best record slots");for(int i=0;i<run_best_count;++i)p.bests[i]=parse_best(j.at("bests")[i]);if(!j.at("daily_records").is_array())throw std::runtime_error("Invalid daily records");std::vector<std::string> dates;for(const auto& row:j.at("daily_records")){DailyRecord d;d.date=row.at("date").get<std::string>();if(!date_key(d.date)||std::find(dates.begin(),dates.end(),d.date)!=dates.end())throw std::runtime_error("Invalid or repeated daily date");dates.push_back(d.date);d.attempts=required_u(row,"attempts",serial_limit);d.wins=required_u(row,"wins",serial_limit);d.reward_claimed=required_b(row,"reward_claimed");d.best=parse_best(row.at("best"));p.daily_records.push_back(d);}}else{for(const auto& receipt:p.history){if(!j.contains("records")||!j.at("records").contains("total_elapsed"))p.records.total_elapsed+=receipt.report.elapsed;if(!j.contains("records")||!j.at("records").contains("class_runs"))p.records.class_runs[int(receipt.report.character)]++;int index=best_index(receipt.report.character,Difficulty::Normal,receipt.report.mode);if(index>=0)update_best(p.bests[index],receipt.report,true);}for(int i=0;i<enemy_kind_count;++i)if(p.achievement_stats.enemy_kills[i]>0)p.enemy_seen_mask|=1u<<i;}
 if(j.contains("lifetime_content"))p.lifetime_content=read_content(json{{"content",j.at("lifetime_content")}});
 else{for(const auto& receipt:p.history)accumulate_content(p.lifetime_content,receipt.report.content);if(p.active_run)accumulate_content(p.lifetime_content,p.active_run->accounted_report.content);}
 return p;
}
json serialize_profile(const Profile& p,json j){
 if(!j.is_object())j=json::object();j["schema_version"]=profile_schema_version;if(j.contains("version"))j["version"]=profile_schema_version;
 j["revision"]=p.revision;j["crystals"]=p.crystals;j["next_run_serial"]=p.next_run_serial;j["closed_through_serial"]=p.closed_through_serial;
 if(!j.contains("talents"))j["talents"]=json::object();for(auto& d:definitions)j["talents"][d.key]=p.talent_ranks[int(d.id)];
 j["active_run"]=nullptr;if(p.active_run){auto& a=*p.active_run;j["active_run"]={{"id",a.id},{"character",int(a.character)},{"seed",a.seed},{"started_at",a.started_at},{"mode",int(a.mode)},{"claimed_chapters",a.claimed_chapters},{"claimed_crystals",a.claimed_crystals},{"claimed_report",report_json(a.claimed_report)},{"bonuses",bonuses_json(a.bonuses)},{"setup",setup_json(a.setup)},{"accounted_report",report_json(a.accounted_report)},{"achievement_crystals",a.achievement_crystals},{"daily_date",a.daily_date}};}
 if(!j.contains("settings"))j["settings"]=json::object();j["settings"]["music"]=p.settings.music;j["settings"]["sound"]=p.settings.sound;j["settings"]["ui_sound"]=p.settings.ui_sound;j["settings"]["ui_volume"]=p.settings.ui_volume;j["settings"]["master_volume"]=p.settings.master_volume;j["settings"]["sound_volume"]=p.settings.sound_volume;j["settings"]["music_volume"]=p.settings.music_volume;j["settings"]["zoom"]=p.settings.zoom;j["settings"]["shake"]=p.settings.shake;j["settings"]["show_stats"]=p.settings.show_stats;j["settings"]["friendly_effect_alpha"]=p.settings.friendly_effect_alpha;j["settings"]["shake_strength"]=p.settings.shake_strength;j["settings"]["damage_number_density"]=p.settings.damage_number_density;j["settings"]["low_effects"]=p.settings.low_effects;j["settings"]["game_speed"]=p.settings.game_speed;write_guidance_settings(j["settings"],p.settings);j["lifetime_content"]=content_json(p.lifetime_content);
 if(!j.contains("records"))j["records"]=json::object();auto& r=j["records"];r["runs"]=p.records.runs;r["victories"]=p.records.victories;r["total_kills"]=p.records.total_kills;r["total_crystals"]=p.records.total_crystals;r["longest_run"]=p.records.longest_run;r["best_level"]=p.records.best_level;r["total_elapsed"]=p.records.total_elapsed;r["class_runs"]=p.records.class_runs;
 j["nightmare_eligible"]=p.nightmare_eligible;j["enemy_seen_mask"]=p.enemy_seen_mask;j["bests"]=json::array();for(const auto& best:p.bests)j["bests"].push_back(best_json(best));j["daily_records"]=json::array();for(const auto& d:p.daily_records)j["daily_records"].push_back({{"date",d.date},{"attempts",d.attempts},{"wins",d.wins},{"reward_claimed",d.reward_claimed},{"best",best_json(d.best)}});
 j["owned_equipment"]=p.owned_equipment;j["equipped"]=p.equipped;j["unlocked_relic_mask"]=p.unlocked_relic_mask;j["challenge_eligible"]=p.challenge_eligible;j["achievement_completed"]=p.achievement_completed;j["achievement_stats"]=achievement_stats_json(p.achievement_stats);
 j["unlocks"]=p.unlocks;j["achievements"]=p.achievements;j["chapter_progress"]=p.chapter_progress;j["chapter_checkpoint"]=p.chapter_checkpoint;if(!p.chapter_checkpoint.is_null())j["chapter_checkpoint"]["build"]=build_json(parse_build(p.chapter_checkpoint.at("build")));
 j["history"]=json::array();for(auto& h:p.history)j["history"].push_back({{"run_id",h.run_id},{"crystals",h.crystals},{"settled_at",h.settled_at},{"report",report_json(h.report)}});
 return j;
}
void append_receipt(Profile& p,SettlementReceipt receipt){p.history.push_back(std::move(receipt));if(p.history.size()>64)p.history.erase(p.history.begin(),p.history.begin()+(p.history.size()-64));}
bool read_file(const fs::path& path,std::string& out,bool& exists,std::string& error,bool* oversized=nullptr){
 if(oversized)*oversized=false;out.clear();
 std::error_code ec;exists=fs::exists(path,ec);if(ec){error="Cannot inspect profile: "+ec.message();return false;}if(!exists){out.clear();return true;}
 auto size=fs::file_size(path,ec);if(ec){error="Cannot read profile size: "+ec.message();return false;}if(size>max_profile_bytes){if(oversized)*oversized=true;error="Profile exceeds the safe size limit";return false;}
 std::ifstream in(path,std::ios::binary);if(!in){error="Cannot open profile for reading";return false;}out.assign(std::istreambuf_iterator<char>(in),{});if(in.bad()){error="Cannot finish reading profile";return false;}return true;
}
class FileLock {
public:
#ifdef _WIN32
 HANDLE handle=INVALID_HANDLE_VALUE;
 explicit FileLock(const fs::path& p){handle=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);}
 bool good()const{return handle!=INVALID_HANDLE_VALUE;}
 ~FileLock(){if(good())CloseHandle(handle);}
#else
 int handle=-1;bool locked=false;
 explicit FileLock(const fs::path& p){handle=::open(p.c_str(),O_CREAT|O_RDWR,0600);if(handle>=0)locked=::flock(handle,LOCK_EX|LOCK_NB)==0;}
 bool good()const{return locked;}
 ~FileLock(){if(handle>=0){if(locked)::flock(handle,LOCK_UN);::close(handle);}}
#endif
};
fs::path temporary_path(const fs::path& target){static std::atomic<unsigned long> serial{0};
#ifdef _WIN32
 const auto pid=GetCurrentProcessId();
#else
 const auto pid=getpid();
#endif
 auto result=target;result+=L".tmp."+std::to_wstring(pid)+L"."+std::to_wstring(++serial);return result;
}
// Compare corruption evidence in fixed-size blocks. Do not trust only size or timestamps,
// and do not allocate an unbounded buffer just because the primary is oversized.
bool same_file_contents(const fs::path& first,const fs::path& second,bool& same,std::string& error){
 same=false;std::error_code ec;auto first_size=fs::file_size(first,ec);if(ec){error="Cannot inspect preserved profile: "+ec.message();return false;}auto second_size=fs::file_size(second,ec);if(ec){error="Cannot inspect preserved profile: "+ec.message();return false;}if(first_size!=second_size)return true;
 std::ifstream a(first,std::ios::binary),b(second,std::ios::binary);if(!a||!b){error="Cannot open profile corruption evidence";return false;}
 std::array<char,64*1024> left{},right{};
 for(;;){a.read(left.data(),std::streamsize(left.size()));b.read(right.data(),std::streamsize(right.size()));if(a.bad()||b.bad()){error="Cannot finish reading profile corruption evidence";return false;}auto n=a.gcount();if(n!=b.gcount()||!std::equal(left.begin(),left.begin()+n,right.begin()))return true;if(a.eof()&&b.eof()){same=true;return true;}if(a.fail()||b.fail()){error="Cannot finish comparing profile corruption evidence";return false;}}
}
bool preserve_large_primary(const fs::path& primary,const fs::path& archive,std::string& error){
 std::error_code ec;if(!fs::copy_file(primary,archive,fs::copy_options::none,ec)){error="Cannot preserve oversized profile: "+ec.message();return false;}
#ifdef _WIN32
 HANDLE copy=CreateFileW(archive.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(copy==INVALID_HANDLE_VALUE){error="Cannot open preserved oversized profile for flushing";return false;}bool flushed=FlushFileBuffers(copy)!=0;if(!CloseHandle(copy))flushed=false;
#else
 int copy=::open(archive.c_str(),O_WRONLY);if(copy<0){error="Cannot open preserved oversized profile for flushing";return false;}bool flushed=::fsync(copy)==0;if(::close(copy)!=0)flushed=false;
#endif
 if(!flushed){error="Cannot flush preserved oversized profile";return false;}bool same=false;if(!same_file_contents(primary,archive,same,error))return false;if(!same){error="Oversized profile changed while preserving it; reload before saving";return false;}return true;
}
bool write_new_file(const fs::path& p,const std::string& bytes,std::string& error){
#ifdef _WIN32
 HANDLE f=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE){error="Cannot create save temporary file ("+std::to_string(GetLastError())+")";return false;}
 DWORD written=0;bool ok=WriteFile(f,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size();if(ok)ok=FlushFileBuffers(f)!=0;DWORD code=GetLastError();if(!CloseHandle(f))ok=false;if(!ok)error="Cannot flush save temporary file ("+std::to_string(code)+")";return ok;
#else
 int f=::open(p.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);if(f<0){error="Cannot create save temporary file";return false;}size_t done=0;bool ok=true;while(done<bytes.size()){auto n=::write(f,bytes.data()+done,bytes.size()-done);if(n<=0){ok=false;break;}done+=size_t(n);}if(ok)ok=::fsync(f)==0;if(::close(f)!=0)ok=false;if(!ok)error="Cannot flush save temporary file";return ok;
#endif
}
bool replace_atomic(const fs::path& temp,const fs::path& target,std::string& error){
#ifdef _WIN32
 if(MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;error="Cannot atomically replace save ("+std::to_string(GetLastError())+")";return false;
#else
 std::error_code ec;fs::rename(temp,target,ec);if(ec){error="Cannot atomically replace save: "+ec.message();return false;}
 int fd=::open(target.parent_path().c_str(),O_RDONLY|O_DIRECTORY);if(fd>=0){::fsync(fd);::close(fd);}return true;
#endif
}
bool write_atomic(const fs::path& target,const std::string& bytes,std::string& error){auto temp=temporary_path(target);if(!write_new_file(temp,bytes,error)){std::error_code ec;fs::remove(temp,ec);return false;}if(!replace_atomic(temp,target,error)){std::error_code ec;fs::remove(temp,ec);return false;}return true;}
ProgressionResult fail(std::string text){ProgressionResult r;r.message=std::move(text);return r;}
}
// The ownership lease outlives every load/commit transaction. A second process can
// inspect the profile but cannot close or replace the running adventure.
class ProfileWriteLease { public: explicit ProfileWriteLease(const fs::path& path):lock_(path){} bool good()const{return lock_.good();} private: FileLock lock_; };
ProgressionStore::~ProgressionStore()=default;
DailySpec make_daily_spec(const std::string& date){DailySpec spec;spec.date=date;uint32_t key=date_key(date);if(!key){spec.error="Daily date must be a valid YYYY-MM-DD calendar date";return spec;}uint32_t hash=2166136261u;for(unsigned char c:std::string("MapleRGL.daily.v1:")+date){hash^=c;hash*=16777619u;}spec.seed=hash;spec.character=CharacterClass(hash%3);spec.setup.mode=RunMode::Daily;spec.setup.difficulty=Difficulty::Normal;spec.setup.daily_key=key;spec.setup.daily_variant=int((hash>>8)%3);spec.setup.daily_character=spec.character;spec.setup.enable_loot=true;spec.setup.unlocked_relic_mask=(1u<<relic_count)-1;spec.setup.starting_relic_mask=(1u<<spec.setup.daily_variant)|(1u<<(3+3*int(spec.character)));spec.setup.equipment={EquipmentId(4+int(spec.character)),EquipmentId::ScoutCloak,EquipmentId::MagnetPendant};Game preview;preview.start(spec.character,spec.seed,{},spec.setup);spec.rules=preview.rules_description();spec.valid=true;return spec;}
const std::array<AchievementDefinition,achievement_count>& achievement_definitions(){return achievement_catalog;}
std::string achievement_reward_text(AchievementId id){int i=int(id);if(i<0||i>=achievement_count)return {};const auto& d=achievement_catalog[i];std::string result;auto add=[&](const std::string& text){if(!result.empty())result+="、";result+=text;};if(d.equipment_reward!=EquipmentId::None)add(std::string("装备 ")+equipment_definitions()[int(d.equipment_reward)].name);for(const auto& relic:relic_definitions())if(d.relic_reward_mask&(1u<<int(relic.id)))add(std::string("遗物 ")+relic.name);if(d.challenge_reward)add("挑战资格");if(d.crystal_reward)add(std::to_string(d.crystal_reward)+" 结晶");return result;}
std::string equipment_unlock_requirement(EquipmentId id){int i=int(id);if(i==0)return "不装备";if(i<0||i>=equipment_count)return "未知装备";if(equipment_definitions()[i].initially_owned)return "初始拥有";for(const auto& d:achievement_catalog)if(d.equipment_reward==id)return std::string(d.name)+"："+d.description;return "尚无解锁任务";}
std::string relic_unlock_requirement(RelicId id){int i=int(id);if(i<0||i>=relic_count)return "未知遗物";if(initial_relic_mask()&(1u<<i))return "初始可获得";for(const auto& d:achievement_catalog)if(d.relic_reward_mask&(1u<<i))return std::string(d.name)+"："+d.description;return "尚无解锁任务";}
const std::array<TalentDefinition,talent_count>& talent_definitions(){return definitions;}
const char* talent_route_name(int route){static const char* names[]={"勇气","守护","智慧"};return route>=0&&route<3?names[route]:"未知";}
const char* run_outcome_name(RunOutcome o){switch(o){case RunOutcome::Victory:return "胜利";case RunOutcome::Defeat:return "战败";case RunOutcome::Abandoned:return "主动撤离";case RunOutcome::Interrupted:return "冒险中断";}return "未知";}
RunReport make_run_report(const Game& game,RunOutcome outcome,uint32_t seed){RunReport r;r.run_gear=game.run_gear;r.resonance_directions=game.resonance_directions;r.adventurer=true;r.upgrade_choices=game.metrics.upgrade_choices;r.bonus_choices=game.metrics.bonus_choices;r.scrolls=game.scrolls;r.timing=game.timing;r.content=game.metrics.content;r.outcome=outcome;r.character=game.character;r.seed=seed;r.elapsed=game.total_elapsed();r.kills=game.kills;r.elite_kills=game.metrics.enemy_kills_by_kind[int(EnemyKind::Elite)];r.events_succeeded=game.metrics.events_succeeded;r.level=game.player.level;r.boss_defeated=game.boss_defeated;r.damage_dealt=game.metrics.other_damage;for(double d:game.metrics.skill_damage)r.damage_dealt+=d;r.resonance=game.metrics.resonance;for(double d:r.resonance.damage)r.damage_dealt+=d;r.damage_taken=game.metrics.damage_taken;r.xp_acquired=game.metrics.xp_acquired;r.mode=game.run_mode();r.chapters_cleared=game.metrics.chapters_cleared;r.bosses_defeated=game.metrics.bosses_defeated;r.skills=game.skills;r.passives=game.passives;r.skill_damage=game.metrics.skill_damage;r.enemy_kills=game.metrics.enemy_kills_by_kind;r.dash_uses=game.metrics.dash_uses;r.items_collected=game.metrics.items_collected;r.rerolls_used=game.metrics.upgrade_rerolls_used;r.death_protections_used=game.metrics.death_protections_used;r.equipment=game.run_setup().equipment;r.owned_relic_mask=game.owned_relic_mask();r.detailed_metrics=true;r.difficulty=game.run_setup().difficulty;r.daily_key=game.run_setup().daily_key;r.daily_variant=game.run_setup().daily_variant;r.enemy_seen_mask=game.metrics.enemy_seen_mask;r.endless_cycle=game.metrics.endless_cycle;r.consumed_skills=game.consumed_skills;r.fusions_created=game.metrics.fusions_created;r.first_fusion_time=game.metrics.first_fusion_time;r.synergy_triggers=game.metrics.synergy_triggers;for(int i=0;i<skill_count;++i)if(game.evolved(Skill(i)))++r.evolved_count;return r;}
int settlement_crystals(const RunReport& r){
 if(r.mode==RunMode::Daily)return 0;
 if(!std::isfinite(r.elapsed)||r.elapsed<60||r.kills<20||r.outcome==RunOutcome::Interrupted)return 0;
 // No flat participation award. Time alone and repeated instant departures yield nothing.
 const bool campaign=r.mode==RunMode::Campaign,endless=r.mode==RunMode::Endless;int scale=campaign?3:(endless?6:1);
 int value=std::min(std::max(r.kills,0)/25,40*scale)+std::min(std::max(r.elite_kills,0),10*scale)*3+std::min(std::max(r.events_succeeded,0),10*scale)*2+int(std::min(r.elapsed/120.f,15.f*scale));
 if(campaign){value+=std::clamp(r.bosses_defeated,0,3)*25+std::clamp(r.chapters_cleared,0,3)*10;}else if(endless){value+=std::clamp(r.bosses_defeated,0,12)*25;}else{if(r.boss_defeated)value+=25;if(r.outcome==RunOutcome::Victory&&r.boss_defeated)value+=10;}
 if(r.outcome==RunOutcome::Abandoned)value/=2;return value;
}
std::filesystem::path default_profile_directory(){
#ifdef _WIN32
 PWSTR local=nullptr;HRESULT hr=SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DONT_VERIFY,nullptr,&local);if(SUCCEEDED(hr)&&local){fs::path p(local);CoTaskMemFree(local);if(p.is_absolute())return p/L"MapleRGL";}else if(local)CoTaskMemFree(local);
 DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);if(size){std::wstring value(size,L'\0');DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),size);if(length&&length<size){value.resize(length);fs::path p(value);if(p.is_absolute())return p/L"MapleRGL";}}
 throw std::runtime_error("Windows LocalAppData is unavailable; refusing to save beside the executable");
#else
 const char* xdg=std::getenv("XDG_DATA_HOME");if(xdg&&fs::path(xdg).is_absolute())return fs::path(xdg)/"MapleRGL";const char* home=std::getenv("HOME");if(home&&fs::path(home).is_absolute())return fs::path(home)/".local"/"share"/"MapleRGL";throw std::runtime_error("User data directory is unavailable");
#endif
}
namespace {
std::string log_profile_failure(const fs::path& directory,const char* operation,const std::string& reason){
 // Diagnostics never contain save bytes or an absolute user path and never touch save/backup files.
 try{
  if(directory.empty())return " Error log unavailable: profile directory is unknown.";
  std::error_code ec;const auto logs=directory/L"logs";fs::create_directories(logs,ec);
  if(ec)return " Error log unavailable: cannot create logs directory.";
  std::string message=reason;const auto excerpt=message.find("last read:");if(excerpt!=std::string::npos)message.erase(excerpt);
  static const std::regex windows_path(R"(([A-Za-z]:[\\/]|\\\\)[^\r\n]*)");
  static const std::regex absolute_path(R"((^|[\s\[])/[^\r\n]*)");
  message=std::regex_replace(message,windows_path,"[path]");message=std::regex_replace(message,absolute_path,"$1[path]");
  std::replace(message.begin(),message.end(),'\n',' ');std::replace(message.begin(),message.end(),'\r',' ');
  if(message.size()>640)message.resize(640);
  std::ofstream log(logs/L"profile-errors.log",std::ios::app);if(!log)return " Error log unavailable: cannot open log file.";
  log<<now_seconds()<<" UTC-epoch | "<<operation<<" | "<<message<<'\n';log.flush();
  return log?std::string():" Error log unavailable: write failed.";
 }catch(const std::exception&){return " Error log unavailable: logging failed.";}
}
}
ProgressionStore::ProgressionStore(fs::path directory):directory_(std::move(directory)),profile_(fresh_profile()){
 try{if(directory_.empty())directory_=default_profile_directory();directory_=fs::absolute(directory_).lexically_normal();load();}catch(const std::exception& e){read_only_=true;load_state_=ProfileLoadState::Unavailable;last_error_=std::string(e.what())+log_profile_failure(directory_,"load",e.what());}
}
ProgressionResult ProgressionStore::load(){
 read_only_=false;recovered_=false;last_error_.clear();profile_=fresh_profile();document_=json::object();baseline_bytes_.clear();baseline_exists_=false;oversized_baseline_.clear();
 auto failed=[&](std::string reason){last_error_=reason+log_profile_failure(directory_,"load",reason);return fail(last_error_);};
 auto unavailable=[&](const std::string& message){read_only_=true;load_state_=ProfileLoadState::Unavailable;last_error_=message;return failed(message);};
 if(directory_.empty())return unavailable("Profile directory is unavailable");
 try{
  std::error_code ec;fs::create_directories(directory_,ec);if(ec)return unavailable("Cannot create profile directory: "+ec.message());
  if(!writer_lease_){auto lease=std::make_unique<ProfileWriteLease>(directory_/L"profile.writer.lock");if(lease->good())writer_lease_=std::move(lease);}
  auto loaded=[&](ProgressionResult result){if(!writer_lease_){read_only_=true;last_error_="Another game owns this profile. Close that game, then reload to enable saving.";result.message=last_error_;}return result;};
  FileLock lock(directory_/L"profile.lock");if(!lock.good())return unavailable("Another game is using the profile; retry after it finishes saving");
  std::string error;bool oversized=false;if(!read_file(profile_path(),baseline_bytes_,baseline_exists_,error,&oversized)&&!oversized)return unavailable(error);
  if(baseline_exists_&&!oversized){
   try{auto j=json::parse(baseline_bytes_);auto version=schema_version(j);if(version>profile_schema_version){read_only_=true;load_state_=ProfileLoadState::FutureVersion;last_error_="This profile was created by a newer game version. Saving is disabled.";return failed(last_error_);}profile_=parse_profile(j);document_=std::move(j);load_state_=version<profile_schema_version?ProfileLoadState::Migrated:ProfileLoadState::Loaded;ProgressionResult r;r.ok=true;if(version<profile_schema_version)r.message="Older profile loaded; it will migrate on the next successful save.";return loaded(r);}catch(const UnsupportedSkillContent& e){read_only_=true;load_state_=ProfileLoadState::FutureVersion;last_error_=std::string(e.what())+". Saving is disabled.";return failed(last_error_);}catch(const std::exception& e){error=e.what();}
  }
  std::string backup;bool has_backup=false;std::string backup_error;
  if(!read_file(directory_/L"profile.json.bak",backup,has_backup,backup_error))return unavailable("Cannot inspect backup: "+backup_error);
  if(has_backup){try{auto j=json::parse(backup);if(schema_version(j)>profile_schema_version){read_only_=true;load_state_=ProfileLoadState::FutureVersion;last_error_="The backup belongs to a newer game version. Saving is disabled.";return failed(last_error_);}profile_=parse_profile(j);document_=std::move(j);if(oversized&&writer_lease_){auto archive=temporary_path(directory_/L"profile.json.corrupt");if(!preserve_large_primary(profile_path(),archive,backup_error))return unavailable(backup_error);oversized_baseline_=std::move(archive);}recovered_=true;load_state_=ProfileLoadState::RecoveredBackup;last_error_="Recovered the previous valid save from backup. The damaged primary will be preserved.";last_error_+=log_profile_failure(directory_,"load-primary",error.empty()?"Primary profile exceeds the supported size.":error);ProgressionResult r;r.ok=true;r.message=last_error_;return loaded(r);}catch(const UnsupportedSkillContent& e){read_only_=true;load_state_=ProfileLoadState::FutureVersion;last_error_=std::string(e.what())+". Saving is disabled.";return failed(last_error_);}catch(const std::exception& e){backup_error=e.what();}}
  if(baseline_exists_||has_backup){read_only_=true;load_state_=ProfileLoadState::Corrupt;last_error_="No valid profile or backup was found. Existing files are preserved; saving is disabled. "+error+" "+backup_error;return failed(last_error_);}
  load_state_=ProfileLoadState::NewProfile;ProgressionResult r;r.ok=true;return loaded(r);
 }catch(const std::exception& e){return unavailable(e.what());}
}
int ProgressionStore::talent_rank(TalentId talent)const{int i=int(talent);return i>=0&&i<talent_count?profile_.talent_ranks[i]:0;}
int ProgressionStore::talent_cost(TalentId talent)const{int i=int(talent);if(i<0||i>=talent_count)return -1;const auto& d=definitions[i];int rank=talent_rank(talent);return rank>=d.max_rank?-1:d.base_cost+d.cost_step*rank;}
RunBonuses ProgressionStore::run_bonuses()const{RunBonuses b;b.damage_bonus=.02f*talent_rank(TalentId::Damage);b.crit_bonus=.02f*talent_rank(TalentId::Crit);b.health_bonus=4.f*talent_rank(TalentId::Health);b.move_speed_bonus=.02f*talent_rank(TalentId::MoveSpeed);b.xp_bonus=.02f*talent_rank(TalentId::XP);b.cooldown_reduction=.02f*talent_rank(TalentId::Cooldown);b.upgrade_rerolls=talent_rank(TalentId::Reroll);b.death_protections=talent_rank(TalentId::DeathProtection);b.first_elite_reward=talent_rank(TalentId::FirstEliteBonus);return b;}
bool ProgressionStore::owns_equipment(EquipmentId id)const{int i=int(id);return i>0&&i<equipment_count&&profile_.owned_equipment[i];}
bool ProgressionStore::relic_unlocked(RelicId id)const{int i=int(id);return i>=0&&i<relic_count&&(profile_.unlocked_relic_mask&(1u<<i))!=0;}
EquipmentId ProgressionStore::equipped(EquipmentSlot slot)const{int i=int(slot);return i>=0&&i<equipment_slot_count?profile_.equipped[i]:EquipmentId::None;}
ProgressionResult ProgressionStore::equip(EquipmentSlot slot,EquipmentId id,CharacterClass character,EquipmentId expected){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);int i=int(slot);if(i<0||i>=equipment_slot_count||!valid_class(character))return fail("Unknown equipment slot or class");if(profile_.active_run)return fail("Equipment can only change in camp after settlement");if(profile_.equipped[i]!=expected)return fail("Equipped item changed; refresh before trying again");if(id!=EquipmentId::None&&(!owns_equipment(id)||!equipment_allowed(id,slot,character)))return fail("Equipment is locked, in the wrong slot, or unavailable to this class");Profile next=profile_;next.equipped[i]=id;return commit(std::move(next));
}
RunSetup ProgressionStore::run_setup(CharacterClass character,RunMode mode,Difficulty difficulty)const{RunSetup setup;setup.mode=mode;setup.difficulty=difficulty;setup.enable_loot=true;setup.unlocked_relic_mask=profile_.unlocked_relic_mask;setup.equipment=profile_.equipped;for(int i=0;i<equipment_slot_count;++i)if(setup.equipment[i]!=EquipmentId::None&&(!owns_equipment(setup.equipment[i])||!equipment_allowed(setup.equipment[i],EquipmentSlot(i),character)))setup.equipment[i]=EquipmentId::None;return setup;}
bool ProgressionStore::mode_unlocked(RunMode mode)const{return int(mode)>=0&&int(mode)<4;}
std::string ProgressionStore::mode_unlock_requirement(RunMode mode)const{return mode_unlocked(mode)?"默认开放":"未知模式";}
bool ProgressionStore::difficulty_unlocked(Difficulty difficulty)const{switch(difficulty){case Difficulty::Normal:return true;case Difficulty::Hard:return profile_.challenge_eligible;case Difficulty::Nightmare:return profile_.nightmare_eligible;}return false;}
std::string ProgressionStore::difficulty_unlock_requirement(Difficulty difficulty)const{switch(difficulty){case Difficulty::Normal:return "默认开放";case Difficulty::Hard:return "完成一次三章远征";case Difficulty::Nightmare:return "以困难难度赢得一局非每日冒险";}return "未知难度";}
RunBest ProgressionStore::run_best(CharacterClass character,Difficulty difficulty,RunMode mode)const{int i=best_index(character,difficulty,mode);return i>=0?profile_.bests[i]:RunBest{};}
std::optional<DailyRecord> ProgressionStore::daily_record(const std::string& date)const{for(const auto& record:profile_.daily_records)if(record.date==date)return record;return {};}
std::array<BestiaryEntry,enemy_kind_count> ProgressionStore::bestiary()const{std::array<BestiaryEntry,enemy_kind_count> result{};for(int i=0;i<enemy_kind_count;++i)result[i]={EnemyKind(i),(profile_.enemy_seen_mask&(1u<<i))!=0,profile_.achievement_stats.enemy_kills[i]};return result;}
std::array<AchievementStatus,achievement_count> ProgressionStore::achievement_status()const{std::array<AchievementStatus,achievement_count> out{};for(const auto& d:achievement_catalog)out[int(d.id)]={d.id,std::min(d.target,achievement_value(profile_.achievement_stats,d.metric)),d.target,profile_.achievement_completed[int(d.id)]};return out;}
ProgressionResult ProgressionStore::commit(Profile candidate){
 auto failed=[&](std::string reason){last_error_=reason+log_profile_failure(directory_,"save",reason);return fail(last_error_);};
 if(read_only_||!writer_lease_)return failed(last_error_.empty()?"The profile is read-only":last_error_);
 try{
  FileLock lock(directory_/L"profile.lock");if(!lock.good())return failed(last_error_="Another game is saving this profile; retry later");
  std::string current,error;bool exists=false;
  if(!oversized_baseline_.empty()){bool same=false;if(!same_file_contents(profile_path(),oversized_baseline_,same,error))return failed(last_error_=error);if(!same)return failed(last_error_="Profile changed in another game. Reload the profile before trying again.");exists=true;}
  else{if(!read_file(profile_path(),current,exists,error))return failed(last_error_=error);if(exists!=baseline_exists_||current!=baseline_bytes_)return failed(last_error_="Profile changed in another game. Reload the profile before trying again.");}
  if(candidate.revision>=serial_limit)return failed(last_error_="Profile revision limit reached");candidate.revision++;
  json next_document=serialize_profile(candidate,document_);std::string bytes=next_document.dump(2)+"\n";if(bytes.size()>max_profile_bytes)return failed(last_error_="Profile exceeds the safe size limit");
  // Never rotate a corrupt primary over the only valid backup.
  if(exists){
   if(recovered_){if(oversized_baseline_.empty()){auto archive=temporary_path(directory_/L"profile.json.corrupt");if(!write_new_file(archive,current,error))return failed(last_error_="Cannot preserve damaged profile: "+error);}}
   else if(!write_atomic(directory_/L"profile.json.bak",current,error))return failed(last_error_="Cannot save profile backup: "+error);
  }
  if(!write_atomic(profile_path(),bytes,error)){
   // If the OS reported an ambiguous replacement result, accept only byte-for-byte evidence.
   std::string observed,read_error;bool observed_exists=false;
   if(!read_file(profile_path(),observed,observed_exists,read_error)||!observed_exists||observed!=bytes)return failed(last_error_=error);
  }
  profile_=std::move(candidate);document_=std::move(next_document);baseline_bytes_=std::move(bytes);baseline_exists_=true;oversized_baseline_.clear();recovered_=false;load_state_=ProfileLoadState::Loaded;last_error_.clear();ProgressionResult r;r.ok=true;return r;
 }catch(const std::exception& e){return failed(last_error_=e.what());}
}
ProgressionResult ProgressionStore::begin_run(CharacterClass character,uint32_t seed,RunMode mode,Difficulty difficulty){
 if(!valid_class(character)||!mode_unlocked(mode))return fail("Unknown character or run mode");if(mode==RunMode::Daily)return fail("Use the fixed local date to start a daily challenge");if(!difficulty_unlocked(difficulty))return fail(difficulty_unlock_requirement(difficulty));return begin_snapshot(character,seed,run_setup(character,mode,difficulty),run_bonuses(),{});
}
ProgressionResult ProgressionStore::begin_daily(const std::string& date){auto spec=make_daily_spec(date);if(!spec.valid)return fail(spec.error);return begin_snapshot(spec.character,spec.seed,spec.setup,spec.bonuses,date);}
ProgressionResult ProgressionStore::begin_snapshot(CharacterClass character,uint32_t seed,RunSetup setup,RunBonuses bonuses,const std::string& daily_date){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);if(profile_.next_run_serial>=serial_limit)return fail("Run serial limit reached");
 Profile next=profile_;if(next.active_run){RunReport abandoned=next.active_run->claimed_report;abandoned.outcome=RunOutcome::Interrupted;abandoned.character=next.active_run->character;abandoned.seed=next.active_run->seed;abandoned.mode=next.active_run->mode;abandoned.difficulty=next.active_run->setup.difficulty;abandoned.daily_key=next.active_run->setup.daily_key;abandoned.daily_variant=next.active_run->setup.daily_variant;record_progress(next,abandoned,true);append_receipt(next,{next.active_run->id,next.active_run->claimed_crystals+next.active_run->achievement_crystals,abandoned,now_seconds()});next.closed_through_serial=next.active_run->id;next.records.runs++;next.records.class_runs[int(abandoned.character)]++;next.records.total_elapsed+=abandoned.elapsed;next.records.total_kills+=abandoned.kills;next.records.longest_run=std::max(next.records.longest_run,abandoned.elapsed);next.records.best_level=std::max(next.records.best_level,abandoned.level);}
 const uint64_t id=next.next_run_serial++;next.active_run=ActiveRun{id,character,seed,now_seconds()};next.active_run->mode=setup.mode;next.active_run->bonuses=bonuses;next.active_run->setup=setup;next.active_run->daily_date=daily_date;next.chapter_checkpoint=nullptr;if(setup.mode==RunMode::Daily)get_daily(next,daily_date).attempts++;auto result=commit(std::move(next));if(result.ok)result.run_id=id;return result;
}
ProgressionResult ProgressionStore::claim_chapter(uint64_t run_id,int chapter_index,const RunReport& report,const RunBuild& build){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);
 if(run_id==0||chapter_index<0||chapter_index>1)return fail("Only the first two campaign boundaries can be claimed");
 if(run_id<=profile_.closed_through_serial){ProgressionResult r;r.ok=true;r.already_claimed=true;r.run_id=run_id;return r;}
 if(!profile_.active_run||profile_.active_run->id!=run_id||profile_.active_run->mode!=RunMode::Campaign)return fail("No matching active campaign");const auto& active=*profile_.active_run;
 if(chapter_index<active.claimed_chapters){ProgressionResult r;r.ok=true;r.already_claimed=true;r.run_id=run_id;return r;}
 if(chapter_index!=active.claimed_chapters)return fail("Campaign chapter claims must be in order");
 try{
  RunBuild checked=parse_build(build_json(build));RunReport input=parse_report(report_json(report));if(input.seed==0)input.seed=active.seed;RunReport actual=build_report(checked);
  if(checked.chapter_index!=chapter_index||checked.intermission_choice!=-1||checked.character!=active.character||checked.seed!=active.seed||!same_bonuses(checked.bonuses,active.bonuses)||setup_json(build_setup(checked))!=setup_json(active.setup)||!same_progress(input,actual)||actual.chapters_cleared!=chapter_index+1||!progress_at_least(actual,active.claimed_report))return fail("Chapter report and boundary build do not match the saved adventure");
  if(chapter_index>0){RunBuild previous=checked_checkpoint(profile_.chapter_checkpoint,active);if(previous.intermission_choice<0)return fail("The previous chapter supply has not been selected");}
  const int earned=campaign_entitlement(actual);if(earned<active.claimed_crystals)return fail("Cumulative chapter rewards went backwards");const int award=earned-active.claimed_crystals;if(profile_.crystals>currency_limit-award)return fail("Crystal balance limit reached");
  Profile next=profile_;auto achievements=account_achievements(next,actual,false);record_progress(next,actual,false);if(next.crystals>currency_limit-award-achievements.crystals)return fail("Crystal balance limit reached");next.crystals+=award+achievements.crystals;next.records.total_crystals+=award+achievements.crystals;next.active_run->claimed_chapters=chapter_index+1;next.active_run->claimed_crystals=earned;next.active_run->claimed_report=actual;next.chapter_checkpoint=checkpoint_json(run_id,checked);auto result=commit(std::move(next));if(result.ok){result.run_id=run_id;result.progress_crystals_awarded=award;result.achievement_crystals_awarded=achievements.crystals;result.crystals_awarded=award+achievements.crystals;result.new_achievements=std::move(achievements.completed);}return result;
 }catch(const std::exception& e){return fail(e.what());}
}
bool ProgressionStore::checkpoint_build(RunBuild& build)const{if(!profile_.active_run||profile_.chapter_checkpoint.is_null())return false;try{auto checked=checked_checkpoint(profile_.chapter_checkpoint,*profile_.active_run);build=std::move(checked);return true;}catch(const std::exception&){return false;}}
ProgressionResult ProgressionStore::update_checkpoint_timing(uint64_t run_id,const RunTiming& observed){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);
 if(!profile_.active_run||profile_.active_run->id!=run_id)return fail("No matching chapter timing record");
 RunBuild build;if(!checkpoint_build(build))return fail("No valid saved chapter boundary");
 try{RunTiming incoming=read_timing(json{{"timing",timing_json(observed)}});RunTiming merged=build.timing;merged.speed=incoming.speed;merged.combat_seconds=std::max(merged.combat_seconds,incoming.combat_seconds);merged.used_speeds|=incoming.used_speeds;merged.recorded|=incoming.recorded;merged.effective_real_seconds=std::max(merged.effective_real_seconds,incoming.effective_real_seconds);merged.total_real_seconds=std::max(merged.total_real_seconds,incoming.total_real_seconds);
  if(timing_json(merged)==timing_json(build.timing)){ProgressionResult result;result.ok=true;result.run_id=run_id;return result;}
  build.timing=merged;Profile next=profile_;next.chapter_checkpoint=checkpoint_json(run_id,build);next.active_run->claimed_report.timing=merged;auto result=commit(std::move(next));if(result.ok)result.run_id=run_id;return result;
 }catch(const std::exception& error){return fail(error.what());}
}
ProgressionResult ProgressionStore::choose_chapter_supply(uint64_t run_id,int choice){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);
 if(choice<0||choice>2||!profile_.active_run||profile_.active_run->id!=run_id)return fail("No matching chapter supply selection");
 RunBuild build;if(!checkpoint_build(build))return fail("No valid saved chapter boundary");if(build.intermission_choice>=0){if(build.intermission_choice!=choice)return fail("This chapter supply has already been selected");ProgressionResult r;r.ok=true;r.already_claimed=true;r.run_id=run_id;return r;}
 Game staged;if(!staged.restore_build(build)||!staged.apply_chapter_reward(choice)||!staged.export_build(build))return fail("Cannot apply this chapter supply");Profile next=profile_;next.chapter_checkpoint=checkpoint_json(run_id,build);next.active_run->claimed_report=build_report(build);auto result=commit(std::move(next));if(result.ok)result.run_id=run_id;return result;
}
ProgressionResult ProgressionStore::settle(uint64_t run_id,const RunReport& report){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);
 if(run_id==0)return fail("Missing run identifier");if(run_id<=profile_.closed_through_serial){ProgressionResult r;r.ok=true;r.already_claimed=true;r.run_id=run_id;r.message="This run has already been closed; no additional crystals were awarded.";return r;}
 if(!profile_.active_run||profile_.active_run->id!=run_id)return fail("This run is not the saved active adventure");const auto& active=*profile_.active_run;
 RunReport checked;try{checked=parse_report(report_json(report));}catch(const std::exception& e){return fail(e.what());}
 if(checked.character!=active.character||(checked.seed!=0&&checked.seed!=active.seed)||checked.mode!=active.mode||checked.difficulty!=active.setup.difficulty||checked.daily_key!=active.setup.daily_key||checked.daily_variant!=active.setup.daily_variant)return fail("Run report does not match the saved adventure");checked.seed=active.seed;if(checked.detailed_metrics){if(checked.equipment!=active.setup.equipment||(checked.owned_relic_mask&~active.setup.unlocked_relic_mask)!=0||(checked.owned_relic_mask&active.setup.starting_relic_mask)!=active.setup.starting_relic_mask)return fail("Run loadout does not match its starting snapshot");for(int i=0;i<relic_count;++i)if((checked.owned_relic_mask&(1u<<i))&&!relic_allowed(RelicId(i),checked.character))return fail("Run contains an unavailable class relic");}
 int award=0;if(active.mode==RunMode::Campaign){if(checked.outcome==RunOutcome::Interrupted){checked=active.claimed_report;checked.character=active.character;checked.seed=active.seed;checked.mode=active.mode;checked.difficulty=active.setup.difficulty;checked.daily_key=active.setup.daily_key;checked.daily_variant=active.setup.daily_variant;checked.outcome=RunOutcome::Interrupted;}else{if(!progress_at_least(checked,active.claimed_report)||checked.chapters_cleared>active.claimed_chapters+1||checked.bosses_defeated>checked.chapters_cleared+(checked.chapters_cleared<3?1:0))return fail("Campaign progress does not match its chapter receipts");if(checked.outcome==RunOutcome::Victory&&(checked.chapters_cleared!=3||active.claimed_chapters!=2))return fail("Campaign victory requires all three chapters");award=std::max(0,campaign_entitlement(checked)-active.claimed_crystals);if(checked.outcome==RunOutcome::Abandoned)award/=2;}}
 else award=settlement_crystals(checked);if(profile_.crystals>currency_limit-award)return fail("Crystal balance limit reached");Profile next=profile_;AchievementAwards achievements;int daily_award=0;try{achievements=account_achievements(next,checked,true);daily_award=record_progress(next,checked,true);}catch(const std::exception& e){return fail(e.what());}if(next.crystals>currency_limit-award-achievements.crystals-daily_award)return fail("Crystal balance limit reached");const int total_received=active.claimed_crystals+award+next.active_run->achievement_crystals+daily_award;
 next.crystals+=award+achievements.crystals+daily_award;next.closed_through_serial=run_id;next.active_run.reset();next.chapter_checkpoint=nullptr;append_receipt(next,{run_id,total_received,checked,now_seconds()});next.records.runs++;next.records.class_runs[int(checked.character)]++;next.records.total_elapsed+=checked.elapsed;if(checked.outcome==RunOutcome::Victory)next.records.victories++;next.records.total_kills+=uint64_t(checked.kills);next.records.total_crystals+=uint64_t(award+achievements.crystals+daily_award);next.records.longest_run=std::max(next.records.longest_run,checked.elapsed);next.records.best_level=std::max(next.records.best_level,checked.level);
 auto result=commit(std::move(next));if(result.ok){result.run_id=run_id;result.progress_crystals_awarded=award;result.achievement_crystals_awarded=achievements.crystals;result.daily_crystals_awarded=daily_award;result.crystals_awarded=award+achievements.crystals+daily_award;result.new_achievements=std::move(achievements.completed);}return result;
}
ProgressionResult ProgressionStore::purchase(TalentId talent,int expected_rank){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);
 int i=int(talent);if(i<0||i>=talent_count)return fail("Unknown talent");if(profile_.active_run)return fail("Talents can only be purchased in camp, after settlement");if(expected_rank!=talent_rank(talent))return fail("The talent rank changed; refresh before buying again");int cost=talent_cost(talent);if(cost<0)return fail("This talent has reached its rank cap");if(profile_.crystals<cost)return fail("Not enough crystals");Profile next=profile_;next.crystals-=cost;next.talent_ranks[i]++;return commit(std::move(next));
}
ProgressionResult ProgressionStore::save_settings(const ProfileSettings& settings){
 if(settings.game_speed!=1&&settings.game_speed!=2)return fail("Unsupported game speed");
 if(!std::isfinite(settings.flash_strength)||settings.flash_strength<0||settings.flash_strength>1||!std::isfinite(settings.ui_scale)||settings.ui_scale<.85f||settings.ui_scale>1.25f||(settings.guide_seen&~127u)||(settings.boss_hints_seen&~((1u<<boss_profile_count)-1)))return fail("Invalid display or guidance setting");
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);if(!std::isfinite(settings.ui_volume)||settings.ui_volume<0||settings.ui_volume>1||!std::isfinite(settings.master_volume)||settings.master_volume<0||settings.master_volume>1||!std::isfinite(settings.sound_volume)||settings.sound_volume<0||settings.sound_volume>1||!std::isfinite(settings.music_volume)||settings.music_volume<0||settings.music_volume>1||!std::isfinite(settings.zoom)||settings.zoom<.25f||settings.zoom>4||!std::isfinite(settings.friendly_effect_alpha)||settings.friendly_effect_alpha<.15f||settings.friendly_effect_alpha>1||!std::isfinite(settings.shake_strength)||settings.shake_strength<0||settings.shake_strength>1||!std::isfinite(settings.damage_number_density)||settings.damage_number_density<0||settings.damage_number_density>1)return fail("Invalid sound volume or zoom");Profile next=profile_;next.settings=settings;return commit(std::move(next));}
ProgressionResult ProgressionStore::save_chapter_checkpoint(uint64_t,const json&){
 if(read_only_||!writer_lease_)return fail(last_error_.empty()?"The profile is read-only":last_error_);
 return fail("Chapter snapshots may only change through claim_chapter or choose_chapter_supply");
}
}

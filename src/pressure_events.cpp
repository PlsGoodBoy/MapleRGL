#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace maple {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr float tau=2*pi;
float unit(std::mt19937& rng){return float(rng()>>8)*(1.f/16777216.f);}
float between(std::mt19937& rng,float low,float high){return low+(high-low)*unit(rng);}
bool output_denial(PressureKind kind){return kind==PressureKind::LastWords||kind==PressureKind::Disarm||kind==PressureKind::ThinAir||kind==PressureKind::Distraction;}
bool output_zone(DangerKind kind){return kind==DangerKind::Smoke||kind==DangerKind::NaturalOrder||kind==DangerKind::Regression;}
bool major_zone(DangerKind kind){return output_zone(kind)||kind==DangerKind::Distortion||kind==DangerKind::Altar;}
bool direct_attack(Skill skill){
 // Pure setup/defence skills cannot serve as the remaining reliable attack.
 return skill!=Skill::BattleFocus&&skill!=Skill::Bulwark&&skill!=Skill::HunterMark&&skill!=Skill::Decoy&&skill!=Skill::GravityWell;
}
const PressureDefinition* definition(PressureKind kind){for(const auto& value:pressure_definitions())if(value.kind==kind)return &value;return nullptr;}
const char* direction_name(float angle){float x=std::cos(angle),y=std::sin(angle);return std::abs(x)>std::abs(y)?(x>0?"东侧":"西侧"):(y>0?"南侧":"北侧");}
std::vector<std::pair<uint32_t,uint32_t>> link_pairs(const std::vector<Enemy>& enemies,Vec2 player){
 std::vector<size_t> eligible;for(size_t i=0;i<enemies.size();++i)if(enemies[i].hp>0&&enemies[i].kind!=EnemyKind::Boss&&enemies[i].kind!=EnemyKind::Minion&&length(enemies[i].pos-player)<650)eligible.push_back(i);
 std::sort(eligible.begin(),eligible.end(),[&](size_t a,size_t b){return length(enemies[a].pos-player)<length(enemies[b].pos-player);});
 if(eligible.size()>24)eligible.resize(24);
 std::vector<bool> used(eligible.size(),false);std::vector<std::pair<uint32_t,uint32_t>> pairs;
 for(size_t i=0;i<eligible.size()&&pairs.size()<3;++i){if(used[i])continue;size_t best=eligible.size();float score=10000;
  for(size_t j=i+1;j<eligible.size();++j){if(used[j])continue;const auto& a=enemies[eligible[i]];const auto& b=enemies[eligible[j]];float distance=length(a.pos-b.pos);if(distance>270)continue;float value=distance+(a.kind==b.kind?160.f:0.f);if(value<score){score=value;best=j;}}
  if(best<eligible.size()){used[i]=used[best]=true;pairs.push_back({enemies[eligible[i]].id,enemies[eligible[best]].id});}
 }
 return pairs;
}
}

const std::array<PressureDefinition,20>& pressure_definitions(){
 static const std::array<PressureDefinition,20> definitions={{
  {PressureKind::DarkTide,"dark_tide","黑暗狂潮","普通怪与精英生命、移速、伤害和生成预算均为3倍；首领不叠加。",40,1.1f,false},
  {PressureKind::Invasion,"invasion","天外来敌","三批混合精英从边缘入场；结束只停止增援，存活精英仍会追击。",35,1.f,false},
  {PressureKind::LastWords,"last_words","遗言","封印一个攻击技能；冷却暂停，投射物自然结束，其余攻击仍可使用。",18,1.f,false},
  {PressureKind::Disarm,"disarm","卸甲","装备和正面战斗Buff暂停贡献；实例与真实成长保留，Buff仍正常计时。",18,1.f,false},
  {PressureKind::Frenzy,"frenzy","狂躁","普通怪和精英免疫外部强制位移；伤害与独立控制效果仍正常。",30,1.f,true},
  {PressureKind::Exhaustion,"exhaustion","精疲力竭","普通移动速度为65%；冲刺位移不变，负面移动下限为参考速度35%。",25,1.f,true},
  {PressureKind::ThinAir,"thin_air","灵气稀薄","施放与充能恢复时间为1.6倍；保留当前恢复比例，不重复减慢多段攻击。",25,1.f,true},
  {PressureKind::Distraction,"distraction","心猿意马","合法施法有25%概率失败并消耗冷却；同系列失败后下一次合法施法成功。",20,1.f,true},
  {PressureKind::Hunt,"hunt","猎杀印记","猎杀者分批从预告方向追来；可直接击杀，结束后存活者继续留场。",30,1.f,true},
  {PressureKind::FireRain,"fire_rain","天火审判","近期位置与附近落下延迟火雨；圈出现后不再追踪，移动离开落点。",25,1.f,true},
  {PressureKind::DeathGrudge,"death_grudge","亡者余恨","部分敌人死亡会留下有预警的爆炸；同一次死亡不重复触发两份爆炸。",30,1.f,true},
  {PressureKind::BloodCurse,"blood_curse","鲜血诅咒","实际治疗降低至40%；不改变当前生命，也不影响上限比例映射。",25,1.f,true},
  {PressureKind::Undying,"undying","不灭军团","部分普通敌人首次死亡后预告复苏一次；不重复掉落、不递归复苏。",35,1.f,true},
  {PressureKind::Altar,"altar","黑暗祭坛","摧毁祭坛即可移除范围内敌人的20%减伤；事件结束撤去祭坛。",35,1.f,true},
  {PressureKind::Encirclement,"encirclement","绝境围猎","三批敌人沿开放圆弧推进；标记方向始终保留缺口，朝缺口突破。",30,1.f,false},
  {PressureKind::Shackles,"shackles","沉重枷锁","冲刺恢复时间为2倍；保留恢复比例，移动距离和普通技能不变。",25,1.f,true},
  {PressureKind::StandPunishment,"stand_punishment","停步即罚","停留在小范围超过2秒会锁定脚下落点，1.6秒后爆炸；离开即可规避。",30,1.f,true},
  {PressureKind::SoulLink,"soul_link","灵魂锁链","少量敌人结对获得35%减伤；击败一端或拉开320距离即可断链。",30,1.f,true},
  {PressureKind::ShadowTrial,"shadow_trial","暗影试炼","两名职业暗影分批入场，分别使用两种有预告的攻击；不会复制玩家构筑。",35,1.f,false},
  {PressureKind::MapleDisorder,"maple_disorder","枫叶失序","常规吸取范围为40%；接触拾取、掉落价值与明确的全场磁铁仍然有效。",25,1.f,true}
 }};
 return definitions;
}

size_t Game::enemy_capacity() const {
 const size_t base=config.max_enemies*size_t(encounter_pace());
 return pressure_active(PressureKind::DarkTide)?base*3:base;
}
void Game::apply_dark_tide(Enemy& enemy){
 const float wanted=enemy.kind!=EnemyKind::Boss&&!enemy.boss_owner&&pressure_active(PressureKind::DarkTide)?3.f:1.f;
 const float previous=enemy.abilities.dark_scale>0?enemy.abilities.dark_scale:1.f;
 if(previous==wanted)return;
 const float health_ratio=enemy.max_hp>0?std::clamp(enemy.hp/enemy.max_hp,0.f,1.f):0.f;
 const float ratio=wanted/previous;
 enemy.max_hp*=ratio;enemy.hp=enemy.max_hp*health_ratio;enemy.speed*=ratio;enemy.damage*=ratio;
 enemy.abilities.dark_scale=wanted;
}
void Game::reset_pressure(){
 if(pressure_event.kind!=PressureKind::None&&!pressure_metrics.sequence.empty())pressure_metrics.sequence.back().ended=total_elapsed();
 pressure_event={};for(auto& enemy:enemies)apply_dark_tide(enemy);
 danger_zones.clear();soul_links.clear();reviving_enemies_.clear();
 const uint32_t chapter=uint32_t(std::max(0,chapter_index_));
 pressure_rng_.seed(run_seed_^0x58e1d2a7u^(chapter*0x9e3779b9u));
 ability_rng_.seed(run_seed_^0x3c6ef372u^(chapter*0x85ebca6bu));
 next_pressure_source_=1;last_pressure_=previous_pressure_=PressureKind::None;last_sealed_=Skill::Count;
 last_output_denial_end_=-100;next_pressure_in_=between(pressure_rng_,config.pressure_first_min,config.pressure_first_max);
 next_map_zone_in_=between(ability_rng_,28.f,42.f);
 cast_failed_.fill(false);cast_failure_flash_.fill(0);gear_suppressed_=buff_suppressed_=regressed_=false;
}
bool Game::pressure_legal(PressureKind kind) const {
 const auto* def=definition(kind);
 if(!config.enable_pressure||!def||state!=State::Playing||player.hp<=0||pressure_event.kind!=PressureKind::None)return false;
 if(boss_pressure_busy()&&(output_denial(kind)||kind==PressureKind::Altar||kind==PressureKind::Invasion||kind==PressureKind::DarkTide))return false;
 const bool boss_alive=std::any_of(enemies.begin(),enemies.end(),[](const Enemy& enemy){return enemy.kind==EnemyKind::Boss&&enemy.hp>0;});
 if(!def->boss_allowed){
  if(boss_alive)return false;
  if(elapsed+def->duration+4>=next_boss_time())return false;
 }
 if(output_denial(kind)&&total_elapsed()-last_output_denial_end_<30)return false;
 size_t major_count=0;bool has_output_zone=false,natural=false;
 for(const auto& zone:danger_zones)if(zone.age<zone.duration){if(major_zone(zone.kind))++major_count;has_output_zone|=output_zone(zone.kind);natural|=zone.kind==DangerKind::NaturalOrder;}
 if(has_output_zone&&(kind==PressureKind::DarkTide||kind==PressureKind::LastWords||kind==PressureKind::Disarm||kind==PressureKind::ThinAir||kind==PressureKind::Distraction))return false;
 if(natural&&kind==PressureKind::BloodCurse)return false;
 if(kind==PressureKind::LastWords){
  int attacks=0;for(int i=0;i<skill_count;++i)if(skills[i]>0&&effective_skill_level(Skill(i))>0&&skill_allowed(Skill(i))&&direct_attack(Skill(i)))++attacks;
  if(attacks<2)return false;
 }
 if(kind==PressureKind::Disarm){
  const bool gear=!run_gear.empty()||std::any_of(equipment_.begin(),equipment_.end(),[](EquipmentId id){return id!=EquipmentId::None;});
  if(!gear&&fury_remaining<=0&&shield_remaining<=0&&power_remaining<=0&&relic_haste_remaining_<=0&&guard_charge<=0)return false;
 }
 if(kind==PressureKind::Altar&&(major_count>=std::min(size_t(2),config.max_major_zones)||danger_zones.size()>=config.max_danger_zones))return false;
 if(kind==PressureKind::SoulLink&&link_pairs(enemies,player.pos).size()<2)return false;
 if(kind==PressureKind::Invasion||kind==PressureKind::Hunt||kind==PressureKind::Encirclement||kind==PressureKind::ShadowTrial){
  if(enemies.size()>=enemy_capacity()||enemy_capacity()-enemies.size()<4)return false;
  if(special_event.kind!=SpecialEventKind::None)return false;
 }
 return true;
}
bool Game::begin_pressure_event(PressureKind kind){
 if(!pressure_legal(kind))return false;
 const auto& def=*definition(kind);PressureEvent event;event.kind=kind;event.id=next_pressure_source_++;event.started=total_elapsed();event.remaining=def.duration;event.anchor=player.pos;event.tick=2;
 event.escape_angle=between(pressure_rng_,0.f,tau);
 if(kind==PressureKind::LastWords){
  std::vector<Skill> candidates;for(int i=0;i<skill_count;++i)if(skills[i]>0&&effective_skill_level(Skill(i))>0&&skill_allowed(Skill(i))&&direct_attack(Skill(i)))candidates.push_back(Skill(i));
  if(candidates.size()<2)return false;
  if(candidates.size()>1)candidates.erase(std::remove(candidates.begin(),candidates.end(),last_sealed_),candidates.end());
  event.sealed=candidates[pressure_rng_()%candidates.size()];last_sealed_=event.sealed;
 }
 pressure_event=event;pressure_metrics.sequence.push_back({kind,event.started,0,0});
 if(kind==PressureKind::DarkTide)for(auto& enemy:enemies)apply_dark_tide(enemy);
 if(kind==PressureKind::Frenzy)for(auto& enemy:enemies)if(enemy.kind!=EnemyKind::Boss)enemy.velocity={};
 if(kind==PressureKind::Altar){
  for(int i=0;i<2;++i){float angle=event.escape_angle+(i==0?-.9f:.9f);DangerZone altar;altar.kind=DangerKind::Altar;altar.source_id=event.id;altar.pos=player.pos+Vec2{std::cos(angle),std::sin(angle)}*220;resolve_scene_collision(altar.pos,24);altar.radius=150;altar.warning=1.3f;altar.duration=def.duration;altar.hp=altar.max_hp=(140+elapsed*.65f+chapter_index_*90)*(1+.2f*int(difficulty_));if(!add_danger_zone(altar))break;}
 }
 if(kind==PressureKind::SoulLink){for(const auto& pair:link_pairs(enemies,player.pos))soul_links.push_back({pair.first,pair.second,event.id,320,false});pressure_event.waves=1;}
 update_pressure_sources();
 std::string message=std::string(def.name)+"："+def.hint;
 if(kind==PressureKind::LastWords)message=std::string(def.name)+"："+skill_name(event.sealed)+" 暂时封印，其他攻击仍可使用。";
 if(kind==PressureKind::Invasion||kind==PressureKind::Hunt||kind==PressureKind::ShadowTrial)message+=std::string(" 首批来自")+direction_name(event.escape_angle)+"，2秒后入场。";
 if(kind==PressureKind::Encirclement)message+=std::string(" 缺口位于")+direction_name(event.escape_angle)+"。";
 notify(message);add_event(EventKind::SpecialEvent,player.pos);return true;
}
void Game::end_pressure_event(){
 if(pressure_event.kind==PressureKind::None)return;
 const PressureKind ended=pressure_event.kind;const uint32_t source=pressure_event.id;
 if(!pressure_metrics.sequence.empty())pressure_metrics.sequence.back().ended=total_elapsed();
 if(output_denial(ended))last_output_denial_end_=total_elapsed();
 previous_pressure_=last_pressure_;last_pressure_=ended;pressure_event={};
 if(ended==PressureKind::DarkTide)for(auto& enemy:enemies)apply_dark_tide(enemy);
 // Existing warning explosions and announced revivals complete normally.
 danger_zones.erase(std::remove_if(danger_zones.begin(),danger_zones.end(),[&](const DangerZone& zone){return zone.kind==DangerKind::Altar&&zone.source_id==source;}),danger_zones.end());
 soul_links.erase(std::remove_if(soul_links.begin(),soul_links.end(),[&](const SoulLink& link){return link.source_id==source;}),soul_links.end());
 next_pressure_in_=between(pressure_rng_,config.pressure_gap_min,config.pressure_gap_max);
 update_pressure_sources();
 const auto* def=definition(ended);if(def)notify(std::string(def->name)+"已结束；基础怪潮继续，存活敌人留场。");
}
void Game::pressure_spawn(EnemyKind kind,Vec2 offset,bool elite,int shadow){
 ++pressure_metrics.requested_spawns;
 if(enemies.size()>=enemy_capacity()){++pressure_metrics.capacity_limited;return;}
 elite=elite&&elite_population()<elite_capacity();
 const size_t before=enemies.size();spawn(kind,true);
 if(enemies.size()==before){++pressure_metrics.capacity_limited;return;}
 auto& enemy=enemies.back();enemy.pos=safe_enemy_position(player.pos+offset,enemy.radius,180);
 enemy.abilities.elite=elite||enemy.kind==EnemyKind::Elite;enemy.abilities.shadow_class=shadow;
 enemy.abilities.scroll_reward=enemy.abilities.elite&&!pressure_event.scroll_reward_assigned;
 if(enemy.abilities.scroll_reward)pressure_event.scroll_reward_assigned=true;
 if(elite&&enemy.kind!=EnemyKind::Elite){const float health=shadow>=0?6.f:4.f;enemy.hp*=health;enemy.max_hp*=health;enemy.damage*=1.35f;}
 if(enemy.abilities.elite&&enemy.abilities.mask==0)assign_enemy_abilities(enemy);
 if(enemy.abilities.elite&&(enemy.abilities.mask&(1u<<int(EnemyAbility::Refraction))))enemy.abilities.refraction=std::max(enemy.abilities.refraction,4);
 enemy.attack_clock=std::max(enemy.attack_clock,1.6f);enemy.attack_cooldown=std::max(enemy.attack_cooldown,1.6f);enemy.abilities.field_cooldown=std::max(enemy.abilities.field_cooldown,3.f);
 ++pressure_metrics.actual_spawns;pressure_metrics.max_enemies=std::max(pressure_metrics.max_enemies,enemies.size());
}
void Game::update_pressure(float dt,float combat_dt){
 if(!config.enable_pressure||state!=State::Playing||player.hp<=0||!std::isfinite(dt)||dt<=0)return;
 if(pressure_event.kind==PressureKind::None){
  next_pressure_in_-=dt;if(next_pressure_in_>0)return;
  std::vector<std::pair<PressureKind,float>> candidates;float total=0;
  for(const auto& def:pressure_definitions())if(pressure_legal(def.kind)){
   float weight=def.weight;if(def.kind==last_pressure_)weight*=.08f;else if(def.kind==previous_pressure_)weight*=.3f;
   if(def.kind==PressureKind::DarkTide||def.kind==PressureKind::Invasion||def.kind==PressureKind::ShadowTrial)weight*=1+.12f*int(difficulty_);
   candidates.push_back({def.kind,weight});total+=weight;
  }
  if(candidates.empty()){next_pressure_in_=3;return;}
  float draw=unit(pressure_rng_)*total;PressureKind selected=candidates.back().first;
  for(const auto& candidate:candidates){if(draw<candidate.second){selected=candidate.first;break;}draw-=candidate.second;}
  if(!begin_pressure_event(selected))next_pressure_in_=3;
  return;
 }
 auto& event=pressure_event;const auto* def=definition(event.kind);
 if(def&&!def->boss_allowed&&std::any_of(enemies.begin(),enemies.end(),[](const Enemy& enemy){return enemy.kind==EnemyKind::Boss&&enemy.hp>0;})){end_pressure_event();return;}
 event.remaining=std::max(0.f,event.remaining-dt);if(event.remaining<=0){end_pressure_event();return;}
 event.tick-=dt;event.internal_cooldown=std::max(0.f,event.internal_cooldown-combat_dt);
 const float safe=std::max(360.f,config.spawn_radius)+40;
 const float damage=(14+region_index()*3)*(difficulty_==Difficulty::Hard?1.15f:difficulty_==Difficulty::Nightmare?1.3f:1.f);
 auto explosion=[&](Vec2 pos,float radius,float warning,float hit){DangerZone zone;zone.kind=DangerKind::Explosion;zone.source_id=event.id;zone.pos=pos;zone.radius=radius;zone.warning=warning;zone.duration=warning+.35f;zone.damage=hit;return add_danger_zone(zone);};
 switch(event.kind){
 case PressureKind::Invasion:
  if(event.tick<=0&&event.waves<3){const EnemyKind roles[]={EnemyKind::WindupCharger,EnemyKind::Ranger,EnemyKind::BlueMushroom,EnemyKind::Support,EnemyKind::GroundController};
   const int count=3+int(pressure_rng_()%3);for(int i=0;i<count;++i){float angle=event.escape_angle+(i-(count-1)*.5f)*.28f;pressure_spawn(roles[(event.waves+i)%5],Vec2{std::cos(angle),std::sin(angle)}*(safe+35*(i%2)),true);}
   ++event.waves;event.tick+=10;event.escape_angle+=2.2f;if(event.waves<3)notify(std::string("天外来敌：下一批精英来自")+direction_name(event.escape_angle)+"。");
  }break;
 case PressureKind::Hunt:
  if(event.tick<=0&&event.waves<3){for(int i=0;i<2;++i){float angle=event.escape_angle+(i==0?-.22f:.22f);pressure_spawn(i==0?EnemyKind::Pig:EnemyKind::WindupCharger,Vec2{std::cos(angle),std::sin(angle)}*safe,true);}++event.waves;event.tick+=10;event.escape_angle+=2.4f;if(event.waves<3)notify(std::string("猎杀者正在从")+direction_name(event.escape_angle)+"接近。");}break;
 case PressureKind::FireRain:
  if(event.tick<=0){event.tick+=2.8f;explosion(event.anchor,62,1.6f,damage);float angle=between(pressure_rng_,0.f,tau);explosion(player.pos+Vec2{std::cos(angle),std::sin(angle)}*125,56,1.9f,damage);event.anchor=player.pos;++event.waves;}break;
 case PressureKind::Encirclement:
  if(event.tick<=0&&event.waves<3){const float gap=std::max(.65f,config.director_config.escape_half_angle);for(int i=0;i<8;++i){float angle=event.escape_angle+gap+(tau-2*gap)*(i+.5f)/8;EnemyKind role=i%4==0?EnemyKind::BlueMushroom:i%3==0?EnemyKind::Pig:EnemyKind::Mushroom;pressure_spawn(role,Vec2{std::cos(angle),std::sin(angle)}*(safe+35*(i%2)));}++event.waves;event.tick+=9;}break;
 case PressureKind::StandPunishment:
  if(length(player.pos-event.anchor)>52){event.anchor=player.pos;event.stationary=0;}
  else event.stationary+=combat_dt;
  if(event.stationary>=2&&event.internal_cooldown<=0){if(explosion(player.pos,72,1.6f,damage*1.2f)){event.internal_cooldown=3;event.stationary=0;event.anchor=player.pos;++event.waves;}}break;
 case PressureKind::ShadowTrial:
  if(event.tick<=0&&event.waves<2){int profession=event.waves==0?int(character):(int(character)+1+int(pressure_rng_()%2))%3;EnemyKind role=profession==int(CharacterClass::Warrior)?EnemyKind::Pig:profession==int(CharacterClass::Mage)?EnemyKind::GroundController:EnemyKind::Ranger;float angle=event.escape_angle+event.waves*pi;pressure_spawn(role,Vec2{std::cos(angle),std::sin(angle)}*(safe+50),true,profession);++event.waves;event.tick+=12;if(event.waves==1)notify(std::string("第二名职业暗影将在")+direction_name(event.escape_angle+pi)+"入场。");}break;
 default:break; // Attribute, cast and death events use the shared source-aware entry points.
 }
 pressure_metrics.max_zones=std::max(pressure_metrics.max_zones,danger_zones.size());
}
} // namespace maple

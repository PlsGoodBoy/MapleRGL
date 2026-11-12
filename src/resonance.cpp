#include "game.hpp"
#include "adventure_content.hpp"
#include <limits>

namespace maple {
namespace {constexpr size_t none=std::numeric_limits<size_t>::max();}
const std::array<ResonanceDefinition,resonance_count>& resonance_definitions(){
 static const std::array<ResonanceDefinition,resonance_count> rules{{
  {Resonance::FrostRelay,"frost_relay","霜雷传导",Cold,Thunder,Skill::Frost,Skill::Lightning,.9f,"雷电命中另一技能留下的寒霜时，向附近最多3名寒霜目标传导；不消耗技能。"},
  {Resonance::PoisonIgnition,"poison_ignition","瘴炎引爆",Poison,Fire,Skill::PoisonMist,Skill::Zone,1.6f,"火焰命中达到2层的异源毒素，消耗毒层，在90范围引爆最多5名敌人。"},
  {Resonance::ArmorBreak,"armor_break","破甲重击",Armor,Heavy,Skill::DragonThrust,Skill::HeavyStrike,1.f,"重击消耗另一技能的破甲窗口，追加实际主击伤害的35%；遵守Boss防御。"},
  {Resonance::HuntAssist,"hunt_assist","猎手协攻",Mark,Companion,Skill::SilverEagle,Skill::Summon,1.2f,"召唤优先追猎另一技能标记的目标；合格命中触发一次有限协攻。"},
  {Resonance::DashFlame,"dash_flame","冲刺余焰",DashAttack,Fire,Skill::Charge,Skill::Zone,2.4f,"合法冲刺攻击沿实际路径留下最多4段火径，持续2.4秒；移动不受附加攻击限制。"},
  {Resonance::ShieldEcho,"shield_echo","圣盾回响",Shield,Counter,Skill::Bulwark,Skill::Retaliate,2.2f,"技能护盾实际吸收或抵挡攻击，积累12点回响后反击最多4名近敌；次生反击不回充。"},
  {Resonance::ColdHeat,"cold_heat","冷热迸裂",Cold,Fire,Skill::Frost,Skill::Zone,1.4f,"火焰命中另一技能的2层寒霜，消耗2层并冲击附近最多4名敌人。"},
  {Resonance::BattleFocus,"battle_focus","战意蓄势",Focus,MultiShot,Skill::BattleFocus,Skill::Arrow,.6f,"合格连射每次施法最多追加8斗气；满100后仍由斗气技能释放重击。"},
  {Resonance::FrostHunt,"frost_hunt","寒霜围猎",Cold,Rainfall,Skill::IceArrow,Skill::Rain,1.1f,"箭雨优先覆盖寒霜敌群；合格命中消耗1层寒霜，碎冰波及附近最多3名目标。"},
  {Resonance::ArcaneVolley,"arcane_volley","奥术蓄射",ArcaneField,Bolt,Skill::Rune,Skill::Homing,2.f,"身处有效法阵时，两次合法投射施法积蓄，下次合法投射施法追加一枚奥术追击；每次只积蓄一次。"},
  {Resonance::EarthResonance,"earth_resonance","地脉共振",GroundRift,Landing,Skill::EarthShatter,Skill::Meteor,2.f,"落点攻击优先覆盖有效地裂；命中区域内敌人时，追加一次有限脉冲，不移动Boss。"},
  {Resonance::HolyShelter,"holy_shelter","圣辉庇护",Shield,Holy,Skill::Bulwark,Skill::HolyLight,1.2f,"实际圣光伤害积累最多12点庇护，持续5秒。每次施法只贡献一次；仅统计实际吸收，不触发反击回充。"},
  {Resonance::ShadowHunt,"shadow_hunt","影袭追猎",Mark,Shadow,Skill::HunterMark,Skill::TripleStar,1.5f,"影系主攻击命中异源标记后，追加35%追击；每个根攻击最多一次。"},
  {Resonance::ToxicCoin,"toxic_coin","毒金连爆",Poison,CombatCoin,Skill::PoisonDart,Skill::CoinPrison,2.4f,"金币爆破消耗目标一层异源毒素，追加一次有限毒爆。"},
  {Resonance::BladeFistBeat,"blade_fist_beat","拳刃共振",Blade,Fist,Skill::SixBlade,Skill::Uppercut,1.2f,"刃与拳交替主命中时追加短距冲击；同一根攻击不重复积累。"},
  {Resonance::ThunderEnergy,"thunder_energy","雷海蓄能",Energy,Thunder,Skill::EnergyBlast,Skill::Lightning,1.4f,"合格雷击给能量爆破追加两次蓄能；未装备爆破时改为有限能量冲击。"},
  {Resonance::TurretCalibration,"turret_calibration","炮火校准",Mark,Turret,Skill::HunterMark,Skill::OctopusTurret,1.6f,"阵地主攻击处理异源标记时追加一次30%校准攻击。"},
  {Resonance::MachineLink,"machine_link","机械协同",Node,Machine,Skill::Engineer,Skill::Missiles,2.4f,"身处异源节点附近时，机械齐射首次合格命中追加45%支援。"},
  {Resonance::DragonBeat,"dragon_beat","龙魂节拍",Elemental,Dragon,Skill::DragonConcert,Skill::DragonBreath,2.f,"龙系主攻击按冰、火、雷三个阶段轮换一次附加状态或脉冲。"},
  {Resonance::RosePursuit,"rose_pursuit","玫瑰追击",Mark,Rose,Skill::HunterMark,Skill::RoseFinale,2.5f,"玫瑰命中异源标记目标时，追加一次80范围收尾，不追击其他次生攻击。"}
 }};return rules;
}
const std::array<ResonanceUpgrade,12>& resonance_upgrades(){
 static const std::array<ResonanceUpgrade,12> entries{{
 {Resonance::FrostRelay,1,"扩散","传导半径195，最多5名目标，每次38%；单体收益较低。"},{Resonance::FrostRelay,2,"聚雷","最多传导1名目标、80%伤害；没有其他寒霜目标时追加55%冲击。"},
 {Resonance::PoisonIgnition,1,"蔓延","爆炸半径130，最多7目标；毒层系数降为32%，偏覆盖。"},{Resonance::PoisonIgnition,2,"焚尽","爆炸半径60，最多3目标；毒层系数升至75%，偏集中爆发。"},
 {Resonance::ArmorBreak,1,"破阵","破甲消耗改为105范围最多4目标，各追加主击18%伤害。"},{Resonance::ArmorBreak,2,"裂甲","仅打击原目标；精英/Boss追加55%，普通目标追加30%。"},
 {Resonance::HuntAssist,1,"巡猎","召唤在多个标记目标间轮换；协攻向标记附近最多2名其他敌人各造成25%。"},{Resonance::HuntAssist,2,"集火","优先集中标记目标，协攻提高至65%；内部冷却延长至1.6秒。"},
 {Resonance::DashFlame,1,"长径","最多6段火径，持续3秒；每段系数降为26%，覆盖更密更久。"},{Resonance::DashFlame,2,"爆燃","最多2段短火径、系数22%；冲刺结束时75范围最多4目标受到110%集中爆发。"},
 {Resonance::ShieldEcho,1,"守护","回响提供6点庇护（上限12、5秒），反击降至45%、最多3目标。庇护不回充回响。"},{Resonance::ShieldEcho,2,"反攻","反击增至180%，半径95、最多2目标；不额外提供护盾。"}
 }};return entries;
}
const char* Game::resonance_direction_name(Resonance id) const {for(const auto& upgrade:resonance_upgrades())if(upgrade.id==id&&upgrade.direction==resonance_directions[int(id)])return upgrade.name;return "基础";}
uint32_t Game::skill_roles(Skill s){
 if(auto def=adventure_skill(s))return def->roles;
 switch(s){
 case Skill::Frost:case Skill::FrozenBarrage:return Cold;
 case Skill::IceLance:case Skill::IceArrow:return Cold|Bolt;
 case Skill::Lightning:case Skill::ThunderPrison:return Thunder;
 case Skill::FrostStorm:return Cold|Thunder;
 case Skill::ThunderLance:return Cold|Thunder|Bolt;
 case Skill::PoisonMist:case Skill::VenomVortex:case Skill::MiasmaDragon:return Poison;
 case Skill::PoisonFlare:return Poison|Fire;
 case Skill::Zone:case Skill::MoltenStar:return Fire;
 case Skill::Meteor:return Fire|Landing;
 case Skill::BlastArrow:return Fire|Bolt;
 case Skill::Summon:case Skill::FireBeast:case Skill::DragonPhoenix:return Fire|Companion;
 case Skill::HolyDragon:case Skill::HeavenDragon:return Companion|Holy;
 case Skill::HolyLight:return Holy;
 case Skill::Cleave:case Skill::DragonThrust:return Armor|Blade;
 case Skill::BladeWave:return Armor|Counter|Bolt;
 case Skill::HeavyStrike:case Skill::SiegeBolt:return Heavy;
 case Skill::Sniper:return Heavy|Bolt;
 case Skill::HunterMark:case Skill::SilverEagle:case Skill::EagleExecution:return Mark;
 case Skill::Charge:case Skill::DashBreaker:case Skill::MountainBreaker:return DashAttack;
 case Skill::Bulwark:case Skill::HolyShield:return Shield;
 case Skill::FrostArmor:return Cold|Shield;
 case Skill::Retaliate:return Counter;
 case Skill::EchoShield:case Skill::MirrorShield:return Shield|Counter;
 case Skill::Arrow:case Skill::Spread:case Skill::StormShot:case Skill::HunterVolley:case Skill::ThousandArrows:return MultiShot|Bolt;
 case Skill::Homing:case Skill::Pierce:case Skill::Ricochet:return Bolt;
 case Skill::BattleFocus:case Skill::BattleBlades:case Skill::SpiritJudgement:return Focus;
 case Skill::Rain:case Skill::PhoenixRain:return Landing|Rainfall;
 case Skill::Rune:return ArcaneField;
 case Skill::EarthShatter:return GroundRift;
 default:return 0;
 }
}

ResonanceStatus Game::resonance_status(Resonance id) const {
 const auto& rule=resonance_definitions()[int(id)];ResonanceStatus view;view.cooldown=resonance_cooldown_[int(id)];
 const auto& slots=skill_slots();
 for(Skill a:slots)if(a!=Skill::Count&&(skill_roles(a)&rule.first_role)){
  if(view.first==Skill::Count)view.first=a;
  for(Skill b:slots)if(b!=Skill::Count&&a!=b&&(skill_roles(b)&rule.second_role)){
   const bool usable=skill_status(a).usable()&&skill_status(b).usable();
   if(!view.equipped||usable){view.first=a;view.second=b;view.equipped=true;view.usable=usable;}if(usable)break;
  }if(view.usable)break;
 }
 if(view.second==Skill::Count)for(Skill s:slots)if(s!=Skill::Count&&s!=view.first&&(skill_roles(s)&rule.second_role)){view.second=s;break;}
 if(!view.equipped)view.hint=view.first==Skill::Count?std::string("需要 ")+skill_name(rule.first_example)+" 等能力":view.second==Skill::Count?std::string("还需 ")+skill_name(rule.second_example)+" 等另一技能":"需要两个不同技能";
 else if(!view.usable)view.hint="已装备 · 当前压制停用";
 else if(view.cooldown>0)view.hint="已激活 · 内部冷却";
 else view.hint="已激活 · 等待触发条件";
 return view;
}
Skill Game::resonance_partner(Resonance id,Skill source,bool second) const {
 const auto& rule=resonance_definitions()[int(id)];if(!skill_status(source).usable()||!(skill_roles(source)&(second?rule.second_role:rule.first_role)))return Skill::Count;
 for(Skill other:skill_slots())if(other!=Skill::Count&&other!=source&&(skill_roles(other)&(second?rule.first_role:rule.second_role))&&skill_status(other).usable())return other;
 return Skill::Count;
}
bool Game::begin_resonance(Resonance id){
 const int i=int(id);if(resonance_cooldown_[i]>0){++metrics.resonance.suppressed[i];return false;}
 resonance_cooldown_[i]=id==Resonance::HuntAssist&&resonance_directions[i]==2?1.6f:resonance_definitions()[i].cooldown;resonance_flash_[i]=1.f;++metrics.resonance.triggers[i];return true;
}
void Game::update_resonances(float dt){for(int i=0;i<resonance_count;++i){resonance_cooldown_[i]=std::max(0.f,resonance_cooldown_[i]-dt);resonance_flash_[i]=std::max(0.f,resonance_flash_[i]-dt);resonance_shield_time_[i]=std::max(0.f,resonance_shield_time_[i]-dt);if(resonance_shield_time_[i]==0)resonance_shields_[i]=0;}}
void Game::resonance_visual(Resonance id,Skill source,Vec2 from,Vec2 to,float radius){
 if(effects.size()+pending_resonance_effects_.size()>=config.max_effects){++metrics.resonance.suppressed[int(id)];return;}
 Effect e;e.kind=EffectKind::Resonance;e.source=source;e.pos=from;e.end=to;e.radius=radius;e.duration=.38f;e.proc_generated=true;e.resonance_id=int(id);e.root_attack_id=current_attack_id_;pending_resonance_effects_.push_back(e);
}
void Game::flush_resonance_effects(){for(auto& e:pending_resonance_effects_)add_effect(e);pending_resonance_effects_.clear();}
void Game::resonance_area(Resonance id,Skill source,Vec2 origin,float radius,float damage,int limit){
 int hits=0;for(auto target:nearby(origin,radius+50))if(length(enemies[target].pos-origin)<=radius+enemies[target].radius){if(hits++>=limit){++metrics.resonance.suppressed[int(id)];break;}hurt_enemy(target,damage,{},0,source,false,false,int(id));}
}
void Game::resonance_hit(size_t target,float actual,Skill source){
 auto& enemy=enemies[target];const uint32_t roles=skill_roles(source);
 for(int i=int(Resonance::ShadowHunt);i<resonance_count;++i){const auto id=Resonance(i);const auto& rule=resonance_definitions()[i];
  if(actual<=0||resonance_last_root_[i]==current_attack_id_||!resonance_status(id).usable)continue;
  const bool marked=enemy.mark_remaining>0&&enemy.mark_source!=source&&skill_status(enemy.mark_source).usable();bool eligible=(roles&rule.second_role)!=0;
  if(id==Resonance::ShadowHunt||id==Resonance::TurretCalibration||id==Resonance::RosePursuit)eligible&=marked;
  if(id==Resonance::ToxicCoin)eligible&=enemy.poison_layers>0&&enemy.poison_source!=source&&skill_status(enemy.poison_source).usable();
  if(id==Resonance::BladeFistBeat){int side=(roles&Fist)?2:(roles&Blade)?1:0;if(!side)continue;eligible=resonance_meter_[i]>0&&resonance_meter_[i]!=side;resonance_meter_[i]=float(side);}
  if(id==Resonance::MachineLink)eligible&=std::any_of(effects.begin(),effects.end(),[&](const Effect& e){return e.source!=source&&(skill_roles(e.source)&Node)&&skill_status(e.source).usable()&&length(e.pos-player.pos)<180;});
  if(!eligible||!begin_resonance(id))continue;resonance_last_root_[i]=current_attack_id_;
  switch(id){
  case Resonance::ToxicCoin:--enemy.poison_layers;resonance_area(id,source,enemy.pos,85,actual*.4f,4);break;
  case Resonance::BladeFistBeat:resonance_area(id,source,enemy.pos,70,actual*.32f,3);break;
  case Resonance::ThunderEnergy:if(skills[int(Skill::EnergyBlast)]){energy_hits_=std::min(8,energy_hits_+2);metrics.resonance.support[i]+=2;}else resonance_area(id,source,enemy.pos,70,actual*.25f,3);break;
  case Resonance::MachineLink:hurt_enemy(target,actual*.45f,{},0,source,false,false,i);break;
  case Resonance::DragonBeat:{int phase=int(resonance_meter_[i]++)%3;if(phase==0)apply_frost(enemy,2,source);else if(phase==1)apply_burn(enemy,actual*.18f,2,source);else resonance_area(id,source,enemy.pos,90,actual*.3f,4);break;}
  case Resonance::RosePursuit:resonance_area(id,source,enemy.pos,80,actual*.4f,4);break;
  default:hurt_enemy(target,actual*(id==Resonance::ShadowHunt?.35f:.3f),{},0,source,false,false,i);break;
  }resonance_visual(id,source,player.pos,enemy.pos,55);
 }
 if((roles&Thunder)&&enemy.frost_remaining>0&&enemy.frost_source!=source&&skill_status(enemy.frost_source).usable()&&(skill_roles(enemy.frost_source)&Cold)&&resonance_partner(Resonance::FrostRelay,source)!=Skill::Count&&begin_resonance(Resonance::FrostRelay)){
  const int direction=resonance_directions[int(Resonance::FrostRelay)],limit=direction==1?5:direction==2?1:3;const float ratio=direction==1?.38f:direction==2?.8f:.55f;
  int hits=0;for(auto n:nearby(enemy.pos,direction==1?195.f:150.f))if(n!=target&&enemies[n].frost_remaining>0){if(hits++>=limit){++metrics.resonance.suppressed[int(Resonance::FrostRelay)];break;}hurt_enemy(n,actual*ratio,{},0,source,false,false,int(Resonance::FrostRelay));resonance_visual(Resonance::FrostRelay,source,enemy.pos,enemies[n].pos,24);}
  if(!hits&&enemy.hp>0){hurt_enemy(target,actual*(direction==2?.55f:.2f),{},0,source,false,false,int(Resonance::FrostRelay));resonance_visual(Resonance::FrostRelay,source,enemy.pos,enemy.pos,36);}
 }
 if((roles&Fire)&&enemy.poison_remaining>0&&enemy.poison_layers>=2&&enemy.poison_source!=source&&skill_status(enemy.poison_source).usable()&&resonance_partner(Resonance::PoisonIgnition,source)!=Skill::Count&&begin_resonance(Resonance::PoisonIgnition)){
  const int layers=enemy.poison_layers;const float power=skill_power(enemy.poison_source);enemy.poison_layers=0;enemy.poison_remaining=0;
  const int direction=resonance_directions[int(Resonance::PoisonIgnition)];float radius=direction==1?130.f:direction==2?60.f:90.f;float ratio=direction==1?.32f:direction==2?.75f:.45f;
  resonance_area(Resonance::PoisonIgnition,source,enemy.pos,radius,actual*(direction==1?.25f:direction==2?.5f:.4f)+power*layers*ratio,direction==1?7:direction==2?3:5);resonance_visual(Resonance::PoisonIgnition,source,enemy.pos,enemy.pos,radius);
 }
 if((roles&Heavy)&&enemy.hp>0&&enemy.armor_break_remaining>0&&enemy.armor_source!=source&&skill_status(enemy.armor_source).usable()&&resonance_partner(Resonance::ArmorBreak,source)!=Skill::Count&&begin_resonance(Resonance::ArmorBreak)){
  enemy.armor_break_remaining=0;const int direction=resonance_directions[int(Resonance::ArmorBreak)];
  if(direction==1)resonance_area(Resonance::ArmorBreak,source,enemy.pos,105,actual*.18f,4);else hurt_enemy(target,actual*(direction==2?(enemy.kind==EnemyKind::Boss||enemy.kind==EnemyKind::Elite?.55f:.3f):.35f),{},0,source,false,false,int(Resonance::ArmorBreak));resonance_visual(Resonance::ArmorBreak,source,enemy.pos,enemy.pos,direction==1?105.f:45.f);
 }
 if((roles&Companion)&&enemy.hp>0&&enemy.mark_remaining>0&&enemy.mark_source!=source&&skill_status(enemy.mark_source).usable()&&resonance_partner(Resonance::HuntAssist,source)!=Skill::Count&&begin_resonance(Resonance::HuntAssist)){
  const int direction=resonance_directions[int(Resonance::HuntAssist)];if(direction==1){int count=0;for(auto n:nearby(enemy.pos,130))if(n!=target){if(count++>=2)break;hurt_enemy(n,actual*.25f,{},0,source,false,false,int(Resonance::HuntAssist));}}else hurt_enemy(target,actual*(direction==2?.65f:.4f),{},0,source,false,false,int(Resonance::HuntAssist));resonance_visual(Resonance::HuntAssist,source,player.pos,enemy.pos,28);
 }
 if((roles&Fire)&&enemy.frost_layers>=2&&enemy.frost_remaining>0&&enemy.frost_source!=source&&skill_status(enemy.frost_source).usable()&&resonance_partner(Resonance::ColdHeat,source)!=Skill::Count&&begin_resonance(Resonance::ColdHeat)){
  enemy.frost_layers-=2;if(!enemy.frost_layers)enemy.frost_remaining=0;resonance_area(Resonance::ColdHeat,source,enemy.pos,65,actual*.45f+skill_power(enemy.frost_source)*.4f,4);resonance_visual(Resonance::ColdHeat,source,enemy.pos,enemy.pos,65);
 }
 const auto focus=Resonance::BattleFocus;int focus_id=int(focus);
 if((roles&MultiShot)&&battle_focus<100&&current_attack_id_!=resonance_last_root_[focus_id]&&resonance_partner(focus,source)!=Skill::Count&&begin_resonance(focus)){
  resonance_last_root_[focus_id]=current_attack_id_;float gained=std::min(8.f,100-battle_focus);battle_focus+=gained;metrics.resonance.support[focus_id]+=gained;resonance_visual(focus,source,enemy.pos,player.pos,22);
 }
 if((source==Skill::Rain||source==Skill::PhoenixRain)&&enemy.frost_layers>0&&enemy.frost_remaining>0&&enemy.frost_source!=source&&skill_status(enemy.frost_source).usable()&&resonance_partner(Resonance::FrostHunt,source)!=Skill::Count&&begin_resonance(Resonance::FrostHunt)){
  if(--enemy.frost_layers==0)enemy.frost_remaining=0;resonance_area(Resonance::FrostHunt,source,enemy.pos,65,actual*.4f,3);resonance_visual(Resonance::FrostHunt,source,enemy.pos,enemy.pos,65);
 }
 const auto earth=Resonance::EarthResonance;
 if((roles&Landing)&&resonance_partner(earth,source)!=Skill::Count){
  for(const auto& field:effects)if(field.source!=source&&(skill_roles(field.source)&GroundRift)&&skill_status(field.source).usable()&&field.pattern==1&&field.age>=field.delay&&field.age<field.duration&&length(enemy.pos-field.pos)<=field.radius+enemy.radius){
   if(begin_resonance(earth)){resonance_area(earth,source,field.pos,70,actual*.45f,4);resonance_visual(earth,source,field.pos,field.pos,70);}break;
  }
 }
 const auto shelter=Resonance::HolyShelter;int shelter_id=int(shelter);
 if((roles&Holy)&&current_attack_id_!=resonance_last_root_[shelter_id]&&resonance_shields_[shelter_id]<12&&resonance_partner(shelter,source)!=Skill::Count&&begin_resonance(shelter)){
  resonance_last_root_[shelter_id]=current_attack_id_;resonance_add_shield(shelter,std::min(6.f,actual*.18f));resonance_visual(shelter,source,enemy.pos,player.pos,35);
 }

}
size_t Game::skill_role_target(Skill source,Vec2 origin,float range,uint32_t preferred){
 float reach=range;bool melee=false;
 switch(source){
 case Skill::Cleave:reach=(100+10*effective_skill_level(source))*area_multiplier+45;melee=true;break;
 case Skill::BattleFocus:reach=(145+8*effective_skill_level(source))*area_multiplier+45;melee=true;break;
 case Skill::SpearCombo:reach=(150+12*effective_skill_level(source))*area_multiplier+45;melee=true;break;
 case Skill::SixBlade:case Skill::Uppercut:case Skill::FrostCombo:case Skill::AsuraBlades:case Skill::BladeFist:reach=155*area_multiplier+45;melee=true;break;
 default:break;
 }
 range=std::min(range,reach);
 // Sustained sequences keep a legal identity, never a stored vector index or
 // an out-of-range coordinate. Invalid targets are replaced on this attack.
 size_t held=target_by_id(preferred,origin,range);if(held!=none)return held;
 if(melee){size_t best=none;float distance=range;for(size_t n:nearby(origin,range))if(enemy_targetable(enemies[n])){float gap=length(enemies[n].pos-origin);if(gap<distance){distance=gap;best=n;}}return best;}
 switch(source){
 case Skill::Sniper:case Skill::Assassinate:case Skill::Judgement:case Skill::SilverEagle:case Skill::GlacialSniper:case Skill::EagleExecution:case Skill::PrismShot:return priority_enemy(origin,range);
 default:break;
 }
 const uint32_t roles=skill_roles(source);
 const bool field=(roles&(ArcaneField|GroundRift|Rainfall|Landing))!=0||source==Skill::Zone||source==Skill::Grenade||source==Skill::Cannon||source==Skill::PoisonMist||source==Skill::MagicVortex;
 if(field){
  const auto candidates=nearby(origin,range);size_t selected=none;float best=-1e9f;
  // Sixteen sampled centers use the existing grid. Large waves do not turn
  // every area cast into an all-pairs crowd search.
  const size_t stride=std::max(size_t(1),(candidates.size()+15)/16);
  for(size_t i=0;i<candidates.size();i+=stride){size_t n=candidates[i];const auto& enemy=enemies[n];if(!enemy_targetable(enemy))continue;int count=0;
   for(size_t other:nearby(enemy.pos,100*area_multiplier))if(enemy_targetable(enemies[other]))++count;
   float score=std::min(count,12)*30.f-length(enemy.pos-origin)*.10f+(enemy.boss_owner?16.f:0.f);
   if(score>best){best=score;selected=n;}
  }
  if(selected!=none)return selected;
 }
 return nearest(origin,range);
}
size_t Game::resonance_target(Skill source,Vec2 origin,float range){
 if(resonance_partner(Resonance::HuntAssist,source)!=Skill::Count){size_t selected=none;float best=range;
  for(auto n:nearby(origin,range)){const auto& e=enemies[n];if(!enemy_targetable(e)||e.mark_remaining<=0||e.mark_source==source||!skill_status(e.mark_source).usable())continue;float distance=length(e.pos-origin);if(resonance_directions[int(Resonance::HuntAssist)]==1&&e.id==hunt_previous_target_[int(source)])distance+=range;if(distance<best){best=distance;selected=n;}}
  if(selected!=none){hunt_previous_target_[int(source)]=enemies[selected].id;return selected;}
 }
 if((source==Skill::Rain||source==Skill::PhoenixRain)&&resonance_partner(Resonance::FrostHunt,source)!=Skill::Count){size_t selected=none;float best=-1;
  for(auto n:nearby(origin,range))if(enemy_targetable(enemies[n])&&enemies[n].frost_remaining>0&&skill_status(enemies[n].frost_source).usable()){int cluster=0;for(auto other:nearby(enemies[n].pos,90))if(enemy_targetable(enemies[other])&&enemies[other].frost_remaining>0)++cluster;float score=cluster*100.f+enemies[n].frost_layers*5.f-length(enemies[n].pos-origin)*.1f;if(score>best){best=score;selected=n;}}
  if(selected!=none)return selected;
 }return skill_role_target(source,origin,range);
}
void Game::resonance_absorb(float amount,Skill source){
 const auto id=Resonance::ShieldEcho;Skill counter=resonance_partner(id,source,false);if(counter==Skill::Count||amount<=0)return;
 auto& meter=resonance_meter_[int(id)];meter=std::min(24.f,meter+std::min(amount,12.f));if(meter<12||nearest(player.pos,130)==none||!begin_resonance(id))return;
 meter-=12;current_attack_id_=next_attack_id_++;const int direction=resonance_directions[int(id)];if(direction==1)resonance_add_shield(id,6);const float radius=direction==1?110.f:direction==2?95.f:130.f;resonance_area(id,counter,player.pos,radius,skill_power(counter)*(direction==1?.45f:direction==2?1.8f:1.2f),direction==1?3:direction==2?2:4);resonance_visual(id,counter,player.pos,player.pos,radius);
}
void Game::resonance_cast(Skill source){
 const auto arcane=Resonance::ArcaneVolley;const int i=int(arcane);
 if((skill_roles(source)&Bolt)&&resonance_partner(arcane,source)!=Skill::Count){
  bool in_field=false;for(const auto& field:effects)if((skill_roles(field.source)&ArcaneField)&&field.source!=source&&skill_status(field.source).usable()&&field.age<field.duration&&length(player.pos-field.pos)<=field.radius){in_field=true;break;}
  if(in_field){auto& meter=resonance_meter_[i];if(meter<2)meter+=1;else{size_t target=nearest(player.pos,500);if(target!=none&&begin_resonance(arcane)){meter=0;Effect shot;shot.kind=EffectKind::Resonance;shot.source=source;shot.resonance_id=i;shot.pattern=2;shot.pos=player.pos;shot.end=enemies[target].pos;shot.target_id=enemies[target].id;shot.damage=skill_power(source)*.9f;shot.radius=18;shot.delay=.12f;shot.duration=.35f;shot.proc_generated=true;shot.root_attack_id=current_attack_id_;if(effects.size()+pending_resonance_effects_.size()<config.max_effects)pending_resonance_effects_.push_back(shot);else ++metrics.resonance.suppressed[i];}}}
 }
 if(!(skill_roles(source)&DashAttack))return;const auto id=Resonance::DashFlame;
 if(resonance_partner(id,source,false)==Skill::Count||!begin_resonance(id))return;
 resonance_dash_source_=source;resonance_dash_root_=current_attack_id_;resonance_trail_distance_=40;resonance_trail_count_=0;
}
void Game::resonance_dash(Vec2 from,Vec2 to,bool ended){
 const auto id=Resonance::DashFlame;Skill source=resonance_dash_source_;if(source==Skill::Count)return;
 if(resonance_partner(id,source,false)==Skill::Count){resonance_dash_source_=Skill::Count;return;}
 current_attack_id_=resonance_dash_root_;const int direction=resonance_directions[int(id)];resonance_trail_distance_+=length(to-from);
 if(resonance_trail_distance_>=(direction==1?18.f:30.f)&&resonance_trail_count_<(direction==1?6:direction==2?2:4)){resonance_trail_distance_=0;++resonance_trail_count_;
  if(effects.size()+pending_resonance_effects_.size()<config.max_effects){Effect e;e.kind=EffectKind::Resonance;e.resonance_id=int(id);e.pattern=1;e.source=source;e.pos=from;e.end=to;e.radius=30;e.damage=skill_power(source)*(direction==1?.26f:direction==2?.22f:.32f);e.duration=direction==1?3.f:direction==2?1.8f:2.4f;e.proc_generated=true;e.root_attack_id=current_attack_id_;pending_resonance_effects_.push_back(e);}else ++metrics.resonance.suppressed[int(id)];
 }if(ended){if(direction==2){resonance_area(id,source,to,75,skill_power(source)*1.1f,4);resonance_visual(id,source,to,to,75);}resonance_dash_source_=Skill::Count;}
}
Vec2 Game::resonance_landing(Skill source,Vec2 fallback){
 if(resonance_partner(Resonance::EarthResonance,source)==Skill::Count)return fallback;float best=450;Vec2 result=fallback;
 for(const auto& field:effects)if((skill_roles(field.source)&GroundRift)&&skill_status(field.source).usable()&&field.pattern==1&&field.age>=field.delay&&field.duration-field.age>.15f){float distance=length(field.pos-player.pos);if(distance<best&&nearest(field.pos,field.radius+50)!=none){best=distance;result=field.pos;}}return result;
}
void Game::resonance_add_shield(Resonance id,float amount){int i=int(id);resonance_shields_[i]=std::min(12.f,resonance_shields_[i]+amount);resonance_shield_time_[i]=5;}
float Game::resonance_shield() const {float total=0;if(buff_suppressed_)return total;for(int i=0;i<resonance_count;++i)if(resonance_shields_[i]>0&&resonance_status(Resonance(i)).usable)total+=resonance_shields_[i];return total;}
float Game::resonance_take_shield(float damage){
 if(buff_suppressed_)return damage;for(int i=0;i<resonance_count&&damage>0;++i)if(resonance_shields_[i]>0&&resonance_status(Resonance(i)).usable){float absorbed=std::min(damage,resonance_shields_[i]);damage-=absorbed;resonance_shields_[i]-=absorbed;metrics.resonance.shield[i]+=absorbed;if(absorbed>0)player.invulnerable=config.hit_invulnerability;}return damage;
}
std::string Game::resonance_change_preview(Skill removed,Skill learned,Skill second_removed) const {
 auto levels=skills;if(removed!=Skill::Count)levels[int(removed)]=0;if(second_removed!=Skill::Count)levels[int(second_removed)]=0;if(learned!=Skill::Count)levels[int(learned)]=1;
 auto qualifies=[&](const ResonanceDefinition& rule){for(int a=0;a<skill_count;++a)if(levels[a]&&(skill_roles(Skill(a))&rule.first_role))for(int b=0;b<skill_count;++b)if(a!=b&&levels[b]&&(skill_roles(Skill(b))&rule.second_role))return true;return false;};
 std::string result;for(const auto& rule:resonance_definitions()){bool before=resonance_status(rule.id).equipped,next=qualifies(rule);if(before==next)continue;if(!result.empty())result+='\n';result+=(next?"获得共鸣：":"失去共鸣：")+std::string(rule.name);}

 if(result.empty())result="当前共鸣资格不变。";return result;
}
}

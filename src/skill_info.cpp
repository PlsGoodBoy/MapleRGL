#include "game.hpp"
#include "adventure_content.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace maple {
const char* Game::skill_key(Skill skill) {
 if(auto def=adventure_skill(skill))return def->key;
 static const char* keys[]={"homing","arrow","nova","pierce","lightning","zone","summon","cleave","orbit","spread","rain","ricochet","frost","meteor","rune","charge","blade_wave","retaliate","star_return","frost_storm","dragon_armor","hunter_mark","arrow_trap","orbital_orb","gravity_well","heavy_strike","bulwark","phoenix_rain","hunter_volley","molten_star","arcane_ring","sword_cyclone","dash_breaker","flame_dance","feather_storm","star_bow","battle_focus","spear_combo","holy_shield","judgement","dragon_thrust","earth_shatter","poison_mist","ice_lance","holy_light","holy_dragon","fire_beast","magic_vortex","storm_shot","sniper","ice_arrow","blast_arrow","silver_eagle","decoy","battle_blades","dragon_formation","echo_shield","mountain_breaker","spirit_judgement","azure_rift","sanctuary","poison_flare","thunder_lance","heaven_dragon","inferno_array","venom_vortex","frost_fire","purity_well","thousand_arrows","eagle_execution","frozen_barrage","siege_bolt","trick_decoy","twin_hunt","prism_shot","thunder_prison","frost_armor","miasma_dragon","divine_verdict","frozen_dragon_spear","phoenix_assault","mirror_shield","dragon_breaker","fissure_arrows","spear_bolt","storm_arrow_stream","poison_hunting_ground","dragon_phoenix","glacial_sniper","flowing_fire"};
 const int i=int(skill);return i>=0&&i<int(std::size(keys))?keys[i]:"unknown";
}
int Game::active_skill_count() const {return int(std::count_if(skills.begin(),skills.end(),[](int level){return level>0;}));}
bool Game::can_gain_skill(Skill skill) const {int i=int(skill);return i>=0&&i<skill_count&&skill_allowed(skill)&&(is_fusion(skill)?skills[i]>0:can_learn_material(skill))&&skills[i]<skill_level_cap(skill);}
float Game::skill_power(Skill skill,int requested_level) const {
 int i=int(skill);if(i<0||i>=skill_count)return 0;
 int level=std::clamp(requested_level>0?requested_level:effective_skill_level(skill),0,5);if(level==0)return 0;
 static const int requirements[]={1,4,6,4,3,9,9,1,3,2,9,4,3,8,9,5,4,6};
 bool evo=i<18&&level>=5&&passives[requirements[i]]>=2;
 return (is_fusion(skill)?24+8.f*(level-1):10+4.f*(level-1))*damage_multiplier*gear_skill_bonus(skill)*(evo?1.25f:1.f);
}
float Game::skill_interval(Skill skill) const {
 if(auto def=adventure_skill(skill))return def->interval*cooldown_multiplier/std::max(.01f,attack_speed);
 static constexpr float seconds[]={.75f,.65f,2.2f,1.3f,2.f,3.7f,1.15f,.85f,3.f,1.7f,5.2f,1.9f,4.2f,6.5f,5.f,4.f,2.1f,2.5f,.95f,6.f,2.8f,5.5f,2.6f,4.5f,6.f,3.8f,5.5f,6.2f,2.6f,6.2f,3.2f,5.f,1.2f,5.5f,4.8f,4.2f,.2f,2.f,3.6f,4.5f,2.7f,4.2f,5.4f,2.2f,4.6f,5.f,5.f,5.4f,2.8f,4.8f,1.5f,2.6f,5.f,6.f,1.7f,4.2f,3.5f,1.4f,.5f,3.5f,6.f,6.f,3.2f,5.8f,5.8f,5.8f,6.f,5.8f,3.f,4.8f,5.8f,3.2f,6.5f,5.2f,4.6f,5.4f,5.f,4.8f,4.6f,3.8f,1.2f,3.8f,4.7f,5.3f,3.1f,3.f,6.f,6.f,4.8f,5.f};
 int i=int(skill);if(i<0||i>=int(std::size(seconds)))return 1;
 float factor=skill==Skill::Nova?.75f:((skill==Skill::Arrow||skill==Skill::Spread)&&evolved(skill))?.65f:1.f;
 const float interval=skill==Skill::DragonFormation?3.2f:skill==Skill::ThunderLance?3.4f:seconds[i];
 return interval*cooldown_multiplier/std::max(.01f,attack_speed)*factor;
}
float Game::skill_cooldown_remaining(Skill skill) const {int i=int(skill);return i>=0&&i<skill_count?std::max(0.f,cooldown_[i])*cast_recovery_factor():0;}
float Game::skill_cooldown_total(Skill skill) const {int i=int(skill);return (i>=0&&i<skill_count&&cooldown_period_[i]>0?cooldown_period_[i]:skill_interval(skill))*cast_recovery_factor();}
float Game::skill_active_remaining(Skill skill) const {if(skill==Skill::FrostArmor)return 0;float remaining=0;for(const auto& e:effects)if(!e.hostile&&e.source==skill&&e.damage>0)remaining=std::max(remaining,e.duration-e.age);return std::max(0.f,remaining);}
std::string Game::skill_parameters(Skill skill,int requested_level) const {
 int i=int(skill);if(i<0||i>=skill_count)return {};
 const int level=std::clamp(requested_level>0?requested_level:std::max(1,skills[i]),1,5);
 static const int requirements[]={1,4,6,4,3,9,9,1,3,2,9,4,3,8,9,5,4,6};
 const bool evo=i<18&&level>=5&&passives[requirements[i]]>=2;
 if(auto def=adventure_skill(skill)){std::ostringstream s;s<<def->description<<"\n单次基础威力 "<<skill_power(skill,level)<<" · 释放间隔 "<<skill_interval(skill)*cast_recovery_factor()<<" 秒\n真实等级上限5；融合材料必须各Lv.5。";return s.str();}
 if(skill>=Skill::BattleFocus)return advanced_skill_parameters(skill,level);
 const float base=skill_power(skill,level);
 float damage=base,radius=0,duration=0,tick=0;
 int bolts=1+(level-1)/2+extra_projectiles,targets=0;
 std::ostringstream out;out<<std::fixed<<std::setprecision(1);
 switch(skill){
 case Skill::HunterMark:out<<"标记附近最强敌人 "<<(5+level*.5f)*duration_multiplier<<"秒\n投射物优先追猎，命中追加25%伤害，最短间隔0.25秒\n释放间隔 "<<skill_interval(skill)*cast_recovery_factor()<<"秒";return out.str();
 case Skill::ArrowTrap:damage*=1.6f;radius=(48+level*5)*area_multiplier;duration=6*duration_multiplier;out<<"0.45秒布置后触发 · 同时最多3陷阱\n触发扇射 "<<std::min(10,3+level/2+extra_projectiles)<<" 箭，每箭 "<<base*.96f<<"伤害\n";break;
 case Skill::OrbitalOrb:damage*=.85f;bolts=std::min(5,2+level/2+extra_projectiles/2);radius=(72+level*3)*area_multiplier;out<<"每1.25秒各法球发弹 · 接触每0.3秒 "<<base*.35f<<"伤害\n";break;
 case Skill::GravityWell:damage*=.35f;radius=(155+level*9)*area_multiplier;duration=3.5f*duration_multiplier;tick=.5f;out<<"每秒牵引90距离 · 最多24目标 · 首领免疫牵引\n";break;
 case Skill::HeavyStrike:damage*=4.5f;radius=(145+level*10)*area_multiplier;out<<"锁定方向蓄势0.7秒 · 扇形半角0.85弧度\n消耗破甲时伤害+35%\n";break;
 case Skill::Bulwark:out<<"每次补至 "<<20+4*level<<"护盾点 · 当前 "<<guard_charge<<"\n吸收实际伤害后可准备反击或积累龙魂，间隔0.5秒\n补充间隔 "<<skill_interval(skill)*cast_recovery_factor()<<"秒";return out.str();
 case Skill::PhoenixRain:damage*=.65f;radius=(105+8*level)*area_multiplier;duration=2.5f*duration_multiplier;tick=.25f;out<<"凤凰标记0.6秒后箭雨\n随后燃区 "<<1.8f*duration_multiplier<<"秒，每0.45秒 "<<base*.35f<<"伤害\n离场灼烧1.2秒，每0.5秒 "<<base*.25f<<"伤害\n";break;
 case Skill::HunterVolley:damage*=1.1f;bolts=std::min(18,6+level+extra_projectiles);out<<"标记0.35秒后，每0.12秒发射1枚；目标死亡自动转移\n";break;
 case Skill::MoltenStar:damage*=.45f;radius=(100+8*level)*area_multiplier;duration=4.6f*duration_multiplier;tick=.4f;out<<"4次有效火域命中积满热量，0.65秒后陨落 "<<base*3.4f<<"伤害\n每场至多2次陨落\n";break;
 case Skill::ArcaneRing:damage*=1.6f;bolts=std::min(10,4+level+extra_projectiles/2);radius=(72+4*level)*area_multiplier;targets=2;out<<"聚球0.8秒后，每0.30秒释放2球并脉冲近身符文\n";break;
 case Skill::SwordCyclone:{const bool focus=fusion_form(skill)==1;damage*=focus?.65f:.85f;bolts=std::min(focus?3:6,3+level/2+extra_projectiles/2);radius=(75+5*level)*area_multiplier*(focus?.75f:1.f);duration=4.6f*duration_multiplier;tick=.22f;if(focus)out<<"归锋：每0.95秒三道窄剑气集中标记强敌，每道 "<<base*.9f<<"，各命中1目标\n";else out<<"散华：每1.1秒发出（首道追击强敌，其余散射） "<<std::min(8,4+level/2+extra_projectiles/2)<<"道剑气，每道 "<<base*1.15f<<"、穿透4目标\n";break;}
 case Skill::DashBreaker:out<<"仅主动冲刺触发，不强制移动\n路径宽 "<<26*area_multiplier<<" · 每敌 "<<base*1.3f<<"伤害一次\n终点半径 "<<(100+8*level)*area_multiplier<<" · 重斩 "<<base*2.5f<<"伤害\n技能冷却 "<<skill_interval(skill)*cast_recovery_factor()<<"秒";return out.str();
 case Skill::FlameDance:damage*=.8f;radius=(72+4*level)*area_multiplier;duration=4.4f*duration_multiplier;tick=.23f;out<<"移动18单位后每0.4秒最多留1段火径，同时最多6段，持续1.3秒\n";break;
 case Skill::FeatherStorm:damage*=.7f;radius=(72+4*level)*area_multiplier;duration=4.4f*duration_multiplier;tick=.23f;out<<"接触积蓄剑羽，每1.1秒向外散射3至9枚（"<<base*1.1f<<"伤害）\n";break;
 case Skill::StarBow:damage*=1.35f;bolts=std::min(6,4+level/2);radius=(72+4*level)*area_multiplier;out<<"固定轨道节点聚箭0.65秒，每0.28秒离轨追击；最多弹射 "<<2+level/2<<" 次\n";break;
 case Skill::StarReturn:damage*=1.65f;bolts=2+extra_projectiles/2;targets=8+level;out<<"回程最多跳转 "<<2+level<<" 次 · ";break;
 case Skill::FrostStorm:{const bool thunder=fusion_form(skill)==1;damage*=thunder?.4f:.65f;radius=(135+8*level)*area_multiplier*(thunder?.8f:1.18f);duration=5.2f*duration_multiplier;tick=thunder?.42f:.8f;out<<(thunder?"奔雷：减速较弱，传导更快":"凝霜：范围较大，减速更强")<<"\n每跳传导 "<<std::min(7,(thunder?4:3)+level/2)<<"目标（"<<base*(thunder?.75f:.95f)<<"伤害）；每"<<(thunder?3:2)<<"跳产生有限碎冰\n";break;}
 case Skill::DragonArmor:{const bool guard=fusion_form(skill)==0;damage*=5;radius=(155+12*level)*area_multiplier;out<<"100龙魂触发 · 当前 "<<dragon_soul<<"/100\n命中+"<<(guard?2:10)<<"（间隔0.25秒），吸收+"<<(guard?15:8)<<"（间隔0.5秒）\n咆哮后护盾 "<<(guard?std::min(2.f,1.8f*duration_multiplier):std::min(.9f,.65f*duration_multiplier))<<"秒 · 减伤10%\n";break;}
 case Skill::Arrow:targets=2+(equipped(EquipmentId::ReedBow)?1:0);break;
 case Skill::Pierce:targets=5+level;break;
 case Skill::Spread:damage*=.8f;bolts+=2;if(evo)bolts*=2;break;
 case Skill::Ricochet:bolts=1+extra_projectiles/2;targets=1+(2+level)*(evo?2:1);break;
 case Skill::BladeWave:damage*=2.2f;bolts=1+extra_projectiles/3+(evo?2:0);targets=8;break;
 case Skill::Nova:damage*=2.8f;radius=(95+level*10)*area_multiplier*1.15f;break;
 case Skill::Lightning:damage*=1.7f;targets=4+level+(evo?3:0);break;
 case Skill::Zone:damage*=.55f;radius=(70+level*8)*area_multiplier*(evo?1.2f:1.f);duration=(evo?6.f:3.5f)*duration_multiplier;tick=.45f;break;
 case Skill::Summon:damage*=1.8f;bolts=1;targets=evo?4:2;break;
 case Skill::Cleave:damage*=2;radius=(100+level*10)*area_multiplier;break;
 case Skill::Orbit:damage*=1.25f;bolts=std::min(8,1+(level>=3?1:0)+extra_projectiles+(evo?1:0));radius=(65+level*4)*area_multiplier;tick=.18f;break;
 case Skill::Rain:damage*=.55f;radius=(90+level*8)*area_multiplier;duration=(evo?5.f:2.5f)*duration_multiplier;tick=.28f;break;
 case Skill::Frost:out<<"命中留下2层寒霜 · ";damage*=1.65f;radius=(120+level*9)*area_multiplier;duration=(evo?3.f:2.f)*duration_multiplier;break;
 case Skill::Meteor:damage*=5;radius=(110+level*10)*area_multiplier;out<<"落点延迟 0.85秒 · ";break;
 case Skill::Rune:damage*=1.5f;radius=(95+level*7)*area_multiplier;duration=4*duration_multiplier;tick=evo?.55f:.85f;break;
 case Skill::Charge:out<<"仅玩家主动冲刺触发，不会自动移动\n沿真实路径每敌一次 "<<base*2.3f<<"伤害 · 路径宽 "<<30*area_multiplier<<"\n技能冷却 "<<skill_interval(skill)*cast_recovery_factor()<<"秒";return out.str();
 case Skill::Retaliate:damage*=2.8f;radius=(110+level*10)*area_multiplier;out<<"合格受击 / 护盾吸收后触发 · 减伤15%\n";break;
 default:break;
 }
 out<<"单次命中 "<<damage<<"（暴击前）";
 const bool projectile=skill==Skill::OrbitalOrb||skill==Skill::HunterVolley||skill==Skill::ArcaneRing||skill==Skill::SwordCyclone||skill==Skill::StarReturn||skill==Skill::Homing||skill==Skill::Arrow||skill==Skill::Pierce||skill==Skill::Spread||skill==Skill::Ricochet||skill==Skill::BladeWave||skill==Skill::Summon||skill==Skill::Orbit;
 if(projectile)out<<" · 数量 "<<bolts;
 if(targets)out<<" · 最多目标 "<<targets;
 if(radius>0)out<<"\n半径 "<<radius;
 if(duration>0)out<<" · 持续 "<<duration<<"秒";
 if(tick>0)out<<" · 每 "<<tick<<"秒命中";
 float interval=skill_interval(skill)*cast_recovery_factor();
 if((skill==Skill::Arrow||skill==Skill::Spread)&&evo&&!evolved(skill))interval*=.65f;
 out<<"\n释放间隔 "<<interval<<"秒";
 return out.str();
}
std::string Game::skill_upgrade_description(Skill skill) const {
 int i=int(skill);if(i<0||i>=skill_count)return {};
 if(skills[i]>=skill_level_cap(skill))return "已达最高等级。";
 return "Lv."+std::to_string(skills[i])+" → Lv."+std::to_string(skills[i]+1)+"\n"+skill_parameters(skill,skills[i]+1);
}
std::string Game::advanced_skill_parameters(Skill skill,int level) const {
 const float b=skill_power(skill,level),a=area_multiplier,d=duration_multiplier;
 std::ostringstream out;out<<std::fixed<<std::setprecision(1);
 switch(skill){
 case Skill::BattleFocus:out<<"有效攻击+12斗气，间隔0.18秒；当前 "<<battle_focus<<"/100\n满层蓄势0.2秒，前方半径 "<<(145+8*level)*a<<"、半角0.9，重斩 "<<b*3.6f;break;
 case Skill::SpearCombo:out<<"三拍直线突刺，每0.2秒一击；长度 "<<(150+12*level)*a<<"、半宽 "<<14*a<<"\n三击伤害 "<<b*.9f<<" / "<<b*1.05f<<" / "<<b*1.2f;break;
 case Skill::HolyShield:out<<"飞盾0.45秒后返回，往返各最多穿透5目标\n每击 "<<b*1.25f<<" · 半径 "<<16*a<<" · 最多拦截3枚敌弹";break;
 case Skill::Judgement:out<<"优先精英/远程，锁定后0.65秒落下\n半径 "<<(65+5*level)*a<<" · 裁决伤害 "<<b*3.5f;break;
 case Skill::DragonThrust:out<<"贯穿6目标 · 弹体半径 "<<22*a<<"\n命中 "<<b*2<<"，留下3秒破甲";break;
 case Skill::EarthShatter:out<<"前方4段地裂，间隔0.18秒展开\n每段半径 "<<38*a<<"，持续1.4秒，每0.45秒 "<<b*.6f;break;
 case Skill::PoisonMist:out<<"半径 "<<(105+7*level)*a<<"，持续 "<<4*d<<"秒；每0.65秒叠毒\n毒层最多3，每层每0.65秒 "<<b*.35f<<"，离场3秒消退\n原感染仅传播1次，传播毒不再传播";break;
 case Skill::IceLance:out<<"冰枪穿透5目标，每击 "<<b*1.4f<<"，寒霜2.3秒\n命中节点半径 "<<24*a<<"、持续1.4秒，每0.5秒 "<<b*.35f;break;
 case Skill::HolyLight:out<<"蓄能0.3秒，半径 "<<(110+8*level)*a<<"，脉冲 "<<b*1.8f<<"\n恢复2生命，圣光类恢复共享3秒间隔";break;
 case Skill::HolyDragon:out<<"圣龙随行，每1.25秒喷吐\n走廊长度 "<<230*a<<"、半宽 "<<18*a<<"，吐息伤害 "<<b*1.2f;break;
 case Skill::FireBeast:out<<"每1.4秒前方扇形吐焰，半径 "<<160*a<<"、半角0.65\n吐焰 "<<b*1.3f<<"，灼烧2秒，每0.5秒 "<<b*.3f;break;
 case Skill::MagicVortex:out<<"半径 "<<(140+8*level)*a<<"，牵引 "<<2.6f*d<<"秒，最多12敌人\n每0.45秒 "<<b*.25f<<"；结束爆发 "<<b*2.2f<<"，首领免疫牵引";break;
 case Skill::StormShot:out<<"每0.13秒一箭，共 "<<std::min(16,7+level+extra_projectiles)<<"箭，每4箭重新锁定\n每箭 "<<b*.65f;break;
 case Skill::Sniper:out<<"蓄势0.85秒，前0.5秒追踪，之后锁定方向\n重箭 "<<b*5<<"，穿透2目标，优先精英或远程";break;
 case Skill::IceArrow:out<<"箭矢 "<<b*1.2f<<"，穿透 "<<2+level/3<<"目标\n寒霜2.3秒，普通减速50%，首领减速20%";break;
 case Skill::BlastArrow:out<<"直击 "<<b*.8f<<"；命中爆炸半径 "<<(55+5*level)*a<<"，爆炸 "<<b*1.5f;break;
 case Skill::SilverEagle:out<<"银鹰环绕，每1.5秒标记强敌2秒并攻击\n每击 "<<b*1.45f;break;
 case Skill::Decoy:out<<"诱饵持续 "<<3.5f*d<<"秒，半径 "<<(160+8*level)*a<<"，最多8普通近战\n结束爆破半径 "<<70*a<<"，伤害 "<<b*1.4f;break;
 case Skill::BattleBlades:out<<"三次扇斩每0.4秒 "<<b*.85f<<"，命中每拍+25斗气\n100斗气后转为三次重斩，每0.2秒 "<<b*1.9f<<"\n半径 "<<(130+8*level)*a<<"、半角0.85，当前斗气 "<<battle_focus;break;
 case Skill::DragonFormation:out<<"0.2秒后三路贯阵：长 "<<320*a<<"、半宽 "<<30*a<<"，每路 "<<b*2.6f<<"\n0.55秒前方龙吼：半径 "<<180*a<<"，伤害 "<<b*4;break;
 case Skill::EchoShield:out<<"往返飞盾每击 "<<b*1.15f<<"，最多拦截3弹\n返回释放半径 "<<(90+7*level)*a<<"震波，基础 "<<b*1.8f<<"，每拦一弹增加 "<<b*.45f;break;
 case Skill::MountainBreaker:out<<"仅玩家冲刺触发；路径每敌一次 "<<b*1.2f<<"\n地裂延迟0.35秒，每0.4秒 "<<b*.65f<<"，最多8段\n终点半径 "<<(100+8*level)*a<<"，震地 "<<b*3;break;
 case Skill::SpiritJudgement:out<<"每0.6秒印记 "<<b*.35f<<"并积25斗气\n100斗气后0.6秒集中裁决 "<<b*4<<" + 每印记 "<<b*.4f<<"（最多4）\n半径 "<<(80+5*level)*a<<"，当前斗气 "<<battle_focus;break;
 case Skill::AzureRift:out<<"去程贯穿7敌，每击 "<<b*1.7f<<"；0.6秒后返程 "<<b*1.35f<<"\n最多4个裂空爆点，半径 "<<38*a<<"，每点 "<<b*.65f;break;
 case Skill::Sanctuary:out<<"飞盾每击 "<<b<<"，最多挡2弹；返身前建立圣域\n半径 "<<(125+6*level)*a<<"、持续 "<<3.4f*d<<"秒\n每0.75秒锁定一次审判，延迟0.4秒、半径 "<<45*a<<"、伤害 "<<b*1.5f;break;
 case Skill::PoisonFlare:{const bool consume=fusion_form(skill)==1;out<<"毒雾半径 "<<(130+7*level)*a*(consume?.75f:1.18f)<<"，持续 "<<5.2f*d<<"秒\n";if(consume)out<<"焚尽：积毒后集中引爆，落点半径 "<<85*a<<"，伤害 "<<b*1.8f<<" + 每层 "<<b*.8f<<"；消耗自身毒层、阻止传播";else out<<"蔓延：依次点燃3分区，各半径 "<<75*a<<"，伤害 "<<b*.9f<<" + 每层 "<<b*.2f<<"；仅消耗一层，保留有限传播";out<<"\n每场最多两轮，毒层最多3";break;}
 case Skill::ThunderLance:out<<"并行双冰枪，每枪直击 "<<b*1.05f<<"，各穿透5敌、最多3个雷电节点\n节点连线半宽 "<<18*a<<"、雷击 "<<b*1.1f<<"；末端碎冰 "<<b*.65f;break;
 case Skill::HeavenDragon:out<<"圣龙持续 "<<4.6f*d<<"秒，每1.4秒吐息，最多2条光廊\n长度 "<<260*a<<"、半宽 "<<22*a<<"；初击 "<<b*1.2f<<"，随后每0.6秒 "<<b*.55f;break;
 case Skill::InfernoArray:out<<"魔兽每0.9秒标记陨落，共3次；各延迟0.6秒\n陨星半径 "<<65*a<<"、伤害 "<<b*2.1f<<"；燃区每0.4秒 "<<b*.35f;break;
 case Skill::VenomVortex:out<<"半径 "<<(150+6*level)*a<<"，持续 "<<4*d<<"秒，最多12目标\n每0.5秒积毒；结束毒爆 "<<b*.8f<<" + 每层 "<<b*.65f<<"（最多3层）";break;
 case Skill::FrostFire:out<<"双域各半径 "<<(90+4*level)*a<<"，持续 "<<4.8f*d<<"秒，每1.3秒交换冰火\n每0.4秒冰 "<<b*.4f<<" / 火 "<<b*.55f<<"；实际跨界冲击 "<<b*.65f<<"\n每敌间隔1秒，每跳最多3次冲击，各至多3目标";break;
 case Skill::PurityWell:out<<"星井半径 "<<(145+6*level)*a<<"、持续 "<<4*d<<"秒，最多12牵引\n聚怪0.9秒后每0.75秒光脉冲 "<<b<<"\n域内恢复2生命，圣光类恢复共享3秒间隔";break;
 case Skill::ThousandArrows:out<<"每0.22秒交替集中3箭/扇射 "<<std::min(8,5+level/2+extra_projectiles/2)<<"箭，共6轮\n每箭 "<<b*.65f<<"，穿透2目标";break;
 case Skill::EagleExecution:out<<"银鹰标记弱点，0.95秒第一重箭 "<<b*5<<"\n1.35秒第二重箭 "<<b*2<<"，均穿透3目标";break;
 case Skill::FrozenBarrage:out<<"固定半径 "<<(130+7*level)*a<<"，4轮冰箭，每轮最多6目标，每箭 "<<b*.5f<<"\n2.25秒时最多8名三层寒霜目标碎冰 "<<b*1.8f<<"，各溅射最多3敌";break;
 case Skill::SiegeBolt:out<<"重箭 "<<b*1.5f<<"，贯穿8目标\n路径最多4个延迟爆点，半径 "<<48*a<<"，伤害 "<<b*1.25f;break;
 case Skill::TrickDecoy:out<<"诱饵聚敌 "<<2.5f*d<<"秒，最多8普通近战；随后部署4陷阱\n0.35秒布置后接触触发，有效2.5秒；爆破半径 "<<75*a<<"、伤害 "<<b*2.2f;break;
 case Skill::TwinHunt:out<<"双兽持续 "<<4.6f*d<<"秒\n";if(fusion_form(skill)==1)out<<"猎首：围绕强敌的紧凑轨道，每1.2秒两只同时攻击同一目标\n每只 "<<b*1.15f<<"，各命中1目标";else out<<"巡空：两只兽灵分散巡猎，每0.85秒交替选敌\n每击 "<<b*1.35f<<"，穿透2敌，优先选择不同目标";break;
 case Skill::PrismShot:out<<"蓄势0.8秒重箭 "<<b*4<<"，最多折射3次\n优先精英和远程威胁，不重复命中同一目标";break;
 case Skill::ThunderPrison:out<<"三道剑气，每0.65秒一发，每击 "<<b*1.15f<<"，贯穿4目标\n命中建立最多4雷印，持续2.3秒；后续有效攻击每0.6秒至多触发一次电网\n连线半宽 "<<14*a<<"，电网 "<<b*.9f;break;
 case Skill::FrostArmor:out<<"实际受击/吸收积30寒霜，间隔0.5秒，60释放冰环\n每5秒补至 "<<8+2*level<<"护盾点；冰环半径 "<<(115+6*level)*a<<"，伤害 "<<b*2.7f<<"\n寒霜2.4秒、反击护盾0.4秒";break;
 case Skill::MiasmaDragon:out<<"锁定扇形半角0.58、长度 "<<(255+10*level)*a<<"\n前2.5秒每0.45秒吐息 "<<b*.4f<<"并积毒；结束消耗毒层\n至多12个感染目标毒爆 "<<b*.8f<<" + 每层 "<<b*.35f;break;
 case Skill::DivineVerdict:out<<"三次圣光脉冲，每0.75秒 "<<b*.65f<<"，半径 "<<(120+6*level)*a<<"\n积3印记后集中裁决，半径 "<<55*a<<"，伤害 "<<b*3.65f;break;
 case Skill::FrozenDragonSpear:out<<"六拍交替近刺/冰枪，间隔0.35秒；近刺长度 "<<175*a<<"、伤害 "<<b*.95f<<"\n冰枪 "<<b*1.2f<<"，末枪 "<<b*2.05f<<"，贯穿6目标；消耗寒霜追加45%碎冰";break;
 case Skill::PhoenixAssault:out<<"仅玩家实际冲刺触发，双凤凰沿路径前后夹击\n各自半径 "<<29*a<<"，每0.1秒 "<<b*.65f<<"，持续0.65秒";break;
 case Skill::MirrorShield:out<<"往返飞盾每击 "<<b*1.15f<<"，去程6/返程5目标\n最多拦截3敌弹，每次折射一枚追击箭（"<<b*1.3225f<<"）；折射箭不继续折射";break;
 case Skill::DragonBreaker:out<<"0.25秒龙吼标记，半径 "<<110*a<<"、伤害 "<<b*.6f<<"\n0.85秒重型龙箭 "<<b*4.3f<<"，贯穿9目标";break;
 case Skill::FissureArrows:out<<"地裂长 "<<330*a<<"、半宽 "<<28*a<<"，0.5秒后 "<<b*.85f<<"\n沿途依次五段箭雨，每段半径 "<<48*a<<"、延迟0.32秒、伤害 "<<b*1.35f;break;
 case Skill::SpearBolt:out<<"六拍交替近刺/贯穿箭，间隔0.3秒\n近刺长 "<<170*a<<"、半宽 "<<18*a<<"、伤害 "<<b*.95f<<"\n贯穿箭 "<<b*1.2f<<"，末击 "<<b*2.05f<<"，穿透6敌";break;
 case Skill::StormArrowStream:out<<"每0.13秒一箭，共10箭，每箭 "<<b*.72f<<"\n每第三箭带雷电，命中传导最多3敌，每次 "<<b*.576f<<"；传导不再触发联动";break;
 case Skill::PoisonHuntingGround:out<<"毒沼草人半径 "<<(105+5*level)*a<<"，持续 "<<5*d<<"秒，至多诱引8普通近战\n每0.5秒给最多12敌积毒；结束消耗自身毒层，爆发 "<<b*1.2f<<" + 每层 "<<b*.5f;break;
 case Skill::DragonPhoenix:if(fusion_form(skill)==1)out<<"焚星：双兽锁定同一强敌，四次交替俯冲，间隔1.05秒\n俯冲长 "<<80*a<<"、半宽 "<<20*a<<"、伤害 "<<b*1.7f<<"；落点半径 "<<48*a<<"、爆发 "<<b*1.105f;
  else out<<"巡天：双兽四次交替横纵扫场，间隔1.05秒\n路径长 "<<320*a<<"、半宽 "<<32*a<<"、伤害 "<<b*1.5f<<"；交汇半径 "<<75*a<<"、冲击 "<<b*2.4f;break;
 case Skill::GlacialSniper:out<<"蓄势1.05秒，冰枪 "<<b*3.6f<<"，贯穿最多10目标\n到主要目标产生一次冰晶：半径 "<<75*a<<"、伤害 "<<b*1.98f<<"、寒霜2秒";break;
 case Skill::FlowingFire:out<<"火域半径 "<<(110+6*level)*a<<"，持续3.6秒，每0.6秒 "<<b*.32f<<"\n三枚爆炸箭每0.85秒射出；火点同时最多6个，每点 "<<b*.9f<<"，只允许一次60%次级传播";break;
 default:return {};
 }
 out<<"\n释放间隔 "<<skill_interval(skill)*cast_recovery_factor()<<"秒";return out.str();
}
}

#include "game.hpp"
#include "battle_art.hpp"
#include "boss_combat.hpp"
#include "adventure_content.hpp"
#include "encounter_pool.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <iomanip>
namespace maple {
namespace {
constexpr float pi=3.14159265f;
constexpr size_t none=std::numeric_limits<size_t>::max();
float sq(Vec2 v){return v.x*v.x+v.y*v.y;}
int si(Skill s){return static_cast<int>(s);}
int64_t cell(int x,int y){return static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(x))<<32)|static_cast<uint32_t>(y));}
float segment_distance(Vec2 p,Vec2 a,Vec2 b){Vec2 d=b-a;float t=std::clamp(((p-a).x*d.x+(p-a).y*d.y)/std::max(.001f,sq(d)),0.f,1.f);return length(p-(a+d*t));}
constexpr int evo_passive[]={1,4,6,4,3,9,9,1,3,2,9,4,3,8,9,5,4,6};

}
float length(Vec2 v){return std::sqrt(sq(v));}
Vec2 normalized(Vec2 v){float l=length(v);return l>.0001f?v*(1/l):Vec2{};}
Game::Game(Config c):config(c),director_(c.director_config),rng_(1){}
RunSetup Game::run_setup() const {RunSetup setup;setup.mode=run_mode_;setup.enable_loot=loot_enabled_;setup.unlocked_relic_mask=unlocked_relic_mask_;setup.equipment=equipment_;setup.difficulty=difficulty_;setup.daily_key=daily_key_;setup.daily_variant=daily_variant_;setup.daily_character=daily_character_;setup.starting_relic_mask=starting_relic_mask_;return setup;}
const char* Game::mode_name(RunMode mode){static const char* names[]={"经典生存","三章远征","无尽挑战","每日挑战"};int i=int(mode);return i>=0&&i<4?names[i]:"未知模式";}
const char* Game::difficulty_name(Difficulty value){static const char* names[]={"普通","困难","噩梦"};int i=int(value);return i>=0&&i<3?names[i]:"未知难度";}
const char* Game::enemy_name(EnemyKind kind){static const char* names[]={"绿水灵","蘑菇","蓝蘑菇","野猪","精英","首领","远射猎手","分裂怪","支援怪","召唤仆从","地面控制者","蓄势冲锋者"};int i=int(kind);return i>=0&&i<enemy_kind_count?names[i]:"未知敌人";}
const char* Game::enemy_description(EnemyKind kind){static const char* desc[]={"缓慢接近，接触造成伤害。","基础近战追击，常成群出现。","高生命近战敌人，移动较慢。","接近后预警冲锋；横向躲开冲锋线。","交替冲锋、召唤与护盾；必须看清预警。","红圈爆破、直线冲锋和放射弹幕；高难度追加双重爆破与交叉裂地。","保持距离，预警后发射敌方弹丸。","被击败时分裂出两个高速仆从。","周期强化附近同伴，靠近时会后退。","高速低生命敌人，常由召唤或分裂产生。","锁定地面后延迟形成危险区域，及时离开预警圈。","蓄势锁定冲锋终点，冲出后有明显收招空隙。"};int i=int(kind);return i>=0&&i<enemy_kind_count?desc[i]:"";}
std::string Game::rules_description() const {
 std::string text=std::string(mode_name(run_mode_))+" · "+difficulty_name(difficulty_)+"。";
 if(run_mode_==RunMode::Endless)text+="原生存目标后继续，每个周期都有首领机会，同时最多一个首领；击败首领不会结束。";
 else if(run_mode_==RunMode::Campaign)text+="每章保留原生存目标，章间保留构筑与天赋剩余次数。";
 else text+="存活至关卡终点获胜；首领击破单独记录，不提前结束。";
 if(difficulty_==Difficulty::Hard)text+="精英提前30%，敌方出招更快，预警略缩短，治疗为80%；首领增加双重爆破。";
 if(difficulty_==Difficulty::Nightmare)text+="精英提前55%，特殊敌人更早出现，敌方出招更快，治疗为60%；首领增加双重爆破与交叉裂地。";
 if(run_mode_==RunMode::Daily){text+="固定起始方案、装备和遗物，永久天赋不生效。";if(daily_variant_==0)text+="疾风规则：敌移速+15%，冲刺冷却-25%。";else if(daily_variant_==1)text+="匮乏规则：治疗减半，玩家移速+10%。";else text+="潮汐规则：技能冷却-15%，环境危险更频繁。";}
 return text;
}
float Game::healing_multiplier() const {return (difficulty_==Difficulty::Hard?.8f:difficulty_==Difficulty::Nightmare?.6f:1.f)*(run_mode_==RunMode::Daily&&daily_variant_==1?.5f:1.f)*(pressure_active(PressureKind::BloodCurse)?.4f:1.f);}
float Game::heal_player(float amount){if(!std::isfinite(amount)||amount<=0)return 0;float before=player.hp;player.hp=std::min(player.max_hp,player.hp+amount*healing_multiplier());return std::max(0.f,player.hp-before);}
float Game::enemy_attack_multiplier() const {return difficulty_==Difficulty::Hard?.88f:difficulty_==Difficulty::Nightmare?.75f:1.f;}
const std::array<RelicDefinition,relic_count>& relic_definitions(){
 static const std::array<RelicDefinition,relic_count> entries={{
 {RelicId::LifeSeed,"life_seed","生命种子","击败敌人恢复2生命，每3秒最多一次；无法复活。",-1,1,true},
 {RelicId::DashWard,"dash_ward","疾行护符","冲刺获得0.5秒护盾，每3秒最多一次。",-1,1,true},
 {RelicId::ThunderToken,"thunder_token","雷鸣刻印","每12次击杀向附近至多3个敌人释放20伤害雷击，至少间隔2秒。",-1,2,true},
 {RelicId::SplitArrow,"split_arrow","分枝箭羽","精准箭每4秒追加两枚侧翼箭，各造成45%伤害。",0,1,true},
 {RelicId::BarbedQuiver,"barbed_quiver","荆棘箭袋","箭术命中使目标减速40%持续1秒，每0.3秒最多一次。",0,2,false},
 {RelicId::WindFeather,"wind_feather","风行羽毛","冲刺立即装填精准箭，并使穿透箭接近就绪；每4秒一次。",0,3,false},
 {RelicId::FrostBell,"frost_bell","冰霜铃","施法使140范围敌人减速60%持续0.8秒，每4秒一次。",1,1,true},
 {RelicId::EmberCore,"ember_core","余烬核心","法术命中附加2秒灼烧，每跳5伤害；每秒一次，不递归触发。",1,2,false},
 {RelicId::EchoRune,"echo_rune","回响符文","施法在目标处留下延迟0.6秒的60范围回响，造成60%伤害；每6秒一次。",1,3,false},
 {RelicId::ThornBadge,"thorn_badge","荆棘徽章","实际受伤后对130范围内至多4个敌人反击12伤害，每3秒一次。",2,1,true},
 {RelicId::WarCry,"war_cry","战吼战旗","每8次击杀获得3秒20%攻速，至少间隔5秒。",2,2,false},
 {RelicId::IronWill,"iron_will","不屈之心","受伤后生命低于35%时恢复4生命并获得0.7秒护盾，每10秒一次；不能复活。",2,3,false}
 }};return entries;
}
const std::array<EquipmentDefinition,equipment_count>& equipment_definitions(){
 static const std::array<EquipmentDefinition,equipment_count> entries={{
 {EquipmentId::None,"none","未装备","",EquipmentSlot::Weapon,-1,0,false},
 {EquipmentId::TravelerKnife,"traveler_knife","旅人短刃","命中近身敌人时追加击退，每2秒一次。",EquipmentSlot::Weapon,-1,1,true},
 {EquipmentId::LeatherVest,"leather_vest","旧皮背心","每8秒可使一次有效命中的伤害降低30%。",EquipmentSlot::Armor,-1,1,true},
 {EquipmentId::AmberPendant,"amber_pendant","琥珀吊坠","红色药水的溢出治疗转为1秒护盾。",EquipmentSlot::Accessory,-1,1,true},
 {EquipmentId::ReedBow,"reed_bow","芦苇长弓","精准箭额外穿透一个敌人。",EquipmentSlot::Weapon,0,2,false},
 {EquipmentId::ApprenticeWand,"apprentice_wand","学徒魔杖","每第三次追踪魔弹额外发射一枚70%伤害法球。",EquipmentSlot::Weapon,1,2,false},
 {EquipmentId::IronBlade,"iron_blade","铁卫长刃","横扫斩的角度扩大30%，击退提升40%。",EquipmentSlot::Weapon,2,2,false},
 {EquipmentId::ScoutCloak,"scout_cloak","斥候斗篷","冲刺使130范围敌人减速40%持续1秒，每3秒一次。",EquipmentSlot::Armor,-1,2,false},
 {EquipmentId::ScholarRobe,"scholar_robe","贤者长袍","拾取道具获得0.45秒护盾，每3秒一次。",EquipmentSlot::Armor,-1,2,false},
 {EquipmentId::MagnetPendant,"magnet_pendant","磁力吊坠","每5秒将220范围的经验晶石向自身拉近80距离。",EquipmentSlot::Accessory,-1,2,false}
 }};return entries;
}
uint32_t initial_relic_mask(){uint32_t mask=0;for(const auto& def:relic_definitions())if(def.initially_unlocked)mask|=1u<<int(def.id);return mask;}
bool relic_allowed(RelicId id,CharacterClass cls){int i=int(id),c=int(cls);if(i<0||i>=relic_count||c<0||c>2)return false;return true;}
bool equipment_allowed(EquipmentId id,EquipmentSlot slot,CharacterClass cls){int i=int(id),c=int(cls),s=int(slot);if(i<0||i>=equipment_count||c<0||c>2||s<0||s>=equipment_slot_count)return false;if(id==EquipmentId::None)return true;const auto& def=equipment_definitions()[i];return def.slot==slot;}
bool Game::equipped(EquipmentId id) const {return !gear_suppressed_&&!regressed_&&id!=EquipmentId::None&&std::find(equipment_.begin(),equipment_.end(),id)!=equipment_.end();}
void Game::reset_loot_timers(){relic_cooldowns_.fill(0);equipment_cooldowns_.fill(0);equipment_cooldowns_[int(EquipmentId::MagnetPendant)]=5;relic_haste_remaining_=0;wand_casts_=0;thunder_pending_=false;}
void Game::update_loot(float dt){
 for(auto& cd:relic_cooldowns_)cd=std::max(0.f,cd-dt);for(auto& cd:equipment_cooldowns_)cd=std::max(0.f,cd-dt);relic_haste_remaining_=std::max(0.f,relic_haste_remaining_-dt);
 if(equipped(EquipmentId::MagnetPendant)&&equipment_cooldowns_[int(EquipmentId::MagnetPendant)]<=0){equipment_cooldowns_[int(EquipmentId::MagnetPendant)]=5;bool moved=false;for(auto& gem:gems){float distance=length(gem.pos-player.pos);if(distance>17&&distance<220){gem.pos+=normalized(player.pos-gem.pos)*std::min(80.f,distance);moved=true;}}if(moved)++metrics.equipment_procs[int(EquipmentId::MagnetPendant)];}
}
void Game::on_enemy_hit(size_t index,Skill source,Vec2 direction){
 if(index>=enemies.size()||enemies[index].hp<=0||!skill_attack_available(source,player.pos))return;auto& enemy=enemies[index];
 if(equipped(EquipmentId::TravelerKnife)&&length(enemy.pos-player.pos)<100&&equipment_cooldowns_[int(EquipmentId::TravelerKnife)]<=0){equipment_cooldowns_[int(EquipmentId::TravelerKnife)]=2;displace_enemy(enemy,normalized(direction)*80,true);++metrics.equipment_procs[int(EquipmentId::TravelerKnife)];}
 if(!control_immune(enemy)&&owns_relic(RelicId::BarbedQuiver)&&relic_cooldowns_[int(RelicId::BarbedQuiver)]<=0){relic_cooldowns_[int(RelicId::BarbedQuiver)]=.3f;enemy.slow_factor=enemy.slow_remaining>0?std::min(enemy.slow_factor,.6f):.6f;enemy.slow_remaining=std::max(enemy.slow_remaining,1.f);++metrics.relic_procs[int(RelicId::BarbedQuiver)];}
 if(owns_relic(RelicId::EmberCore)&&relic_cooldowns_[int(RelicId::EmberCore)]<=0){relic_cooldowns_[int(RelicId::EmberCore)]=1;enemy.burn_remaining=std::max(enemy.burn_remaining,2.f);enemy.burn_damage=std::max(enemy.burn_damage,5.f);enemy.burn_source=source;++metrics.relic_procs[int(RelicId::EmberCore)];}
}
void Game::on_player_hit(){
 on_cross_defense();
 if(!smoke_at(player.pos)&&owns_relic(RelicId::ThornBadge)&&relic_cooldowns_[int(RelicId::ThornBadge)]<=0){int hits=0;for(size_t i=0;i<enemies.size()&&hits<4;++i)if(enemies[i].hp>0&&length(enemies[i].pos-player.pos)<130){hurt_enemy(i,12,enemies[i].pos-player.pos,.3f,Skill::Count,false,false);++hits;}if(hits){relic_cooldowns_[int(RelicId::ThornBadge)]=3;++metrics.relic_procs[int(RelicId::ThornBadge)];}}
 if(player.hp>0&&player.hp<player.max_hp*.35f&&owns_relic(RelicId::IronWill)&&relic_cooldowns_[int(RelicId::IronWill)]<=0){relic_cooldowns_[int(RelicId::IronWill)]=10;heal_player(4);shield_remaining=std::max(shield_remaining,.7f);++metrics.relic_procs[int(RelicId::IronWill)];}
}
void Game::on_kill(){
 if(player.hp>0&&player.hp<player.max_hp&&owns_relic(RelicId::LifeSeed)&&relic_cooldowns_[int(RelicId::LifeSeed)]<=0){relic_cooldowns_[int(RelicId::LifeSeed)]=3;heal_player(2);++metrics.relic_procs[int(RelicId::LifeSeed)];}
 if(owns_relic(RelicId::ThunderToken)&&kills%12==0&&relic_cooldowns_[int(RelicId::ThunderToken)]<=0){relic_cooldowns_[int(RelicId::ThunderToken)]=2;thunder_pending_=true;}
 if(owns_relic(RelicId::WarCry)&&kills%8==0&&relic_cooldowns_[int(RelicId::WarCry)]<=0){relic_cooldowns_[int(RelicId::WarCry)]=5;relic_haste_remaining_=3;++metrics.relic_procs[int(RelicId::WarCry)];}
}
void Game::on_dash(){
 on_fusion_dash();on_cross_dash();
 if(owns_relic(RelicId::DashWard)&&relic_cooldowns_[int(RelicId::DashWard)]<=0){relic_cooldowns_[int(RelicId::DashWard)]=3;shield_remaining=std::max(shield_remaining,.5f);++metrics.relic_procs[int(RelicId::DashWard)];}
 if(!skill_sealed(Skill::Arrow)&&!skill_sealed(Skill::Pierce)&&owns_relic(RelicId::WindFeather)&&relic_cooldowns_[int(RelicId::WindFeather)]<=0){relic_cooldowns_[int(RelicId::WindFeather)]=4;cooldown_[si(Skill::Arrow)]=0;cooldown_[si(Skill::Pierce)]=std::min(cooldown_[si(Skill::Pierce)],.1f);++metrics.relic_procs[int(RelicId::WindFeather)];}
 if(equipped(EquipmentId::ScoutCloak)&&equipment_cooldowns_[int(EquipmentId::ScoutCloak)]<=0){bool hit=false;for(auto& e:enemies)if(e.hp>0&&!control_immune(e)&&length(e.pos-player.pos)<130){e.slow_factor=e.slow_remaining>0?std::min(e.slow_factor,.6f):.6f;e.slow_remaining=std::max(e.slow_remaining,1.f);hit=true;}if(hit){equipment_cooldowns_[int(EquipmentId::ScoutCloak)]=3;++metrics.equipment_procs[int(EquipmentId::ScoutCloak)];}}
}
void Game::on_skill_cast(Skill skill,float damage,Vec2 direction){
 emit_skill_visual(FxStage::Cast,skill,player.pos,player.pos+direction*55,26);
 resonance_cast(skill);
 auto extra=[&](Vec2 dir,float multiplier){if(projectiles.size()>=config.max_projectiles)return;Projectile p;p.pos=player.pos;p.velocity=dir*(skill==Skill::Homing?330.f:430.f);p.skill=skill;p.damage=damage*multiplier;p.proc_generated=true;add_projectile(p);};
 if(skill==Skill::Arrow&&owns_relic(RelicId::SplitArrow)&&relic_cooldowns_[int(RelicId::SplitArrow)]<=0){relic_cooldowns_[int(RelicId::SplitArrow)]=4;float angle=std::atan2(direction.y,direction.x);for(float delta:{-.22f,.22f})extra({std::cos(angle+delta),std::sin(angle+delta)},.45f);++metrics.relic_procs[int(RelicId::SplitArrow)];}
 if(skill==Skill::Homing&&equipped(EquipmentId::ApprenticeWand)&&++wand_casts_%3==0){extra(direction,.7f);++metrics.equipment_procs[int(EquipmentId::ApprenticeWand)];}
 if(owns_relic(RelicId::FrostBell)&&relic_cooldowns_[int(RelicId::FrostBell)]<=0){relic_cooldowns_[int(RelicId::FrostBell)]=4;bool hit=false;for(auto& e:enemies)if(e.hp>0&&!control_immune(e)&&length(e.pos-player.pos)<140){e.frost_remaining=std::max(e.frost_remaining,.8f);float frost_factor=e.kind==EnemyKind::Boss?.8f:.4f;e.slow_factor=e.slow_remaining>0?std::min(e.slow_factor,frost_factor):frost_factor;e.slow_remaining=std::max(e.slow_remaining,.8f);hit=true;}if(hit)++metrics.relic_procs[int(RelicId::FrostBell)];}
 if(owns_relic(RelicId::EchoRune)&&relic_cooldowns_[int(RelicId::EchoRune)]<=0){size_t target=nearest(player.pos,450);if(target!=none&&effects.size()<config.max_effects){relic_cooldowns_[int(RelicId::EchoRune)]=6;Effect echo;echo.kind=EffectKind::Rune;echo.pos=enemies[target].pos;echo.radius=60;echo.delay=.6f;echo.duration=.95f;echo.damage=damage*.6f;echo.source=skill;echo.proc_generated=true;add_effect(echo);++metrics.relic_procs[int(RelicId::EchoRune)];}}
}
bool Game::generate_relic_choices(){
 if(!loot_enabled_||pending_relic_rewards_<=0)return false;std::vector<RelicId> available;for(int i=0;i<relic_count;++i)if((unlocked_relic_mask_&(1u<<i))&&!owns_relic(RelicId(i))&&relic_allowed(RelicId(i),character))available.push_back(RelicId(i));
 if(available.empty()){pending_relic_rewards_=0;relic_selection_=false;return false;}
 choices.clear();while(choices.size()<3&&!available.empty()){size_t index=rng_()%available.size();RelicId id=available[index];const auto& def=relic_definitions()[int(id)];UpgradeChoice c{UpgradeKind::Relic,Skill::Count,def.name,def.description};c.relic=id;choices.push_back(c);available.erase(available.begin()+index);}
 const UpgradeChoice fallback[]={{UpgradeKind::Overcharge,Skill::Count,"强攻补给","本次冒险基础伤害 +3.5%。"},{UpgradeKind::Vitality,Skill::Count,"韧性补给","本次冒险生命上限 +8，并恢复20生命。"}};for(const auto& c:fallback)if(choices.size()<3)choices.push_back(c);
 relic_selection_=true;bonus_selection_=false;upgrade_picks_taken=upgrade_resets_remaining=0;state=State::Upgrade;return true;
}
void Game::continue_choices(){if(pending_levels_>0){relic_selection_=bonus_selection_=false;generate_choices();}else if(pending_skill_rewards_>0){relic_selection_=false;bonus_selection_=true;generate_choices();}else if(pending_relic_rewards_>0&&generate_relic_choices()){}else if(chapter_finish_pending_)finish_chapter();else state=State::Playing;}
const char* Game::skill_name(Skill s){if(auto def=adventure_skill(s))return def->name;static const char* n[]={"追踪魔弹","精准射击","龙咆哮","穿透箭","连锁闪电","火焰领域","凤凰","劈空斩","旋风剑刃","多重箭","箭雨","弹射箭","冰咆哮","陨石","魔法阵","冲锋斩","剑气","防御反击","回旋贯星","霜雷领域","龙魂战甲","猎手标记","箭矢陷阱","环绕法球","牵引魔井","蓄势重斩","战意壁垒","焚天箭雨","追猎箭阵","熔星灾变","奥术星环","万剑回旋","破阵连斩","烈焰剑舞","剑羽风暴","星轨魔弓","斗气聚合","枪连击","圣盾回旋","圣剑审判","龙魂突刺","地裂重击","毒雾","冰枪","圣光","圣龙召唤","火魔兽","魔力漩涡","暴风连射","狙击","寒冰箭","爆炸箭","银鹰","诱敌草人","斗气狂刃","龙魂贯阵","回响圣盾","破阵山河","斗气圣裁","苍龙裂空","圣域裁决","瘴炎连爆","雷霆冰枪","天国龙辉","炎魔陨阵","蚀骨旋涡","霜火双界","净化星井","千矢风暴","鹰眼绝杀","冰封箭幕","破城轰矢","诡雷草人","双翼巡猎","折光狙杀","雷霆剑狱","霜甲反击","瘴龙吐息","神圣裁决","极寒龙枪","凤翼突袭","镜盾回射","龙吼破军","裂地箭阵","枪弩连阵","雷暴箭流","毒沼猎场","龙凤巡天","极寒狙击","流火连爆"};return si(s)>=0&&si(s)<skill_count?n[si(s)]:"未知技能";}
const char* Game::skill_description(Skill s){if(auto def=adventure_skill(s))return def->description;static const char* n[]={"自动追踪附近目标的魔法弹。","高速直线射击，命中后可继续穿透。","以自身为中心爆发并强力击退敌人。","贯穿整条直线上的多个敌人。","在不同敌人之间传导，链条不重复命中。","在敌群脚下持续灼烧，离开后灼烧仍持续。","环绕的凤凰定时发射火羽，飞行与玩家独立。","朝目标挥出前方扇形斩击，背后无判定。","剑刃持续环绕，接触敌人时造成伤害。","同时发射扇形箭矢，覆盖宽广射线。","锁定区域连续落箭，敌人移出可避开。","命中后转向未命中的附近敌人。","冰环伤害并减速敌人，打断普通敌人冲锋；首领保留较弱寒霜。","瞄准强敌，预告落点后延迟爆炸。","在脚下部署固定魔法阵，蓄能后脉冲攻击。","仅由玩家冲刺触发，沿实际移动路径斩击，不会自动移动。","宽幅剑气沿直线飞行，贯穿并击退。","实际受击或护盾吸收后准备范围反击，受技能冷却约束；拥有时减伤15%。","贯穿后回旋，回程在有限目标间跳转。","寒霜领域周期传导雷电，对寒霜目标追加受限碎冰。","攻击或护盾吸收积累龙魂；满100层自动咆哮并获得护盾。受到伤害降低10%。","标记附近强敌，箭矢和凤凰优先追猎；命中标记目标受限追加伤害。","在身后部署短暂机关，敌人靠近时爆发并射出扇形箭矢。","法球持续绕身旋转，接触敌人并周期发射追踪弹。","优先在已有持续区域中心形成牵引，首领免疫外部牵引。","锁定前方扇形蓄势0.7秒后重斩，消耗破甲形成额外爆发。","周期补充有限护盾点数，实际吸收伤害可准备反击或积累龙魂。","凤凰标记区域，箭雨集中落下后留下燃烧地带。","标记强敌后分批追踪齐射，目标死亡时剩余箭矢重新选敌。","火域积累热量，最多引发两次延迟陨落和地面爆发。","魔弹在星环中积蓄，再分批脱离轨道追踪敌人。","跟随自身的剑环周期发射外扩剑气。","由玩家冲刺触发，沿实际冲刺轨迹追加斩击并在终点重斩；不会强制移动。","旋转火刃沿移动路线留下数量受限的短暂火径。","剑刃命中积蓄剑羽，按节拍向外散射。","固定法阵节点使魔箭绕轨转向后追击，并进行有限弹射。","有效攻击积累斗气，满100后自动释放前方重斩。","锁定方向按三拍连刺，后两击略强。","飞盾有限穿透，返身时重新命中；最多拦截3枚敌方弹丸。","优先精英或远程威胁，锁定落点后延迟审判。","龙形直线贯穿，对命中目标留下短暂破甲。","沿前方依次生成四段短暂震地伤害区。","雾内逐步叠毒，三层毒可有限传播一次，传播不会继续递归。","冰枪贯穿后留下短暂冰节点，使路径敌人受到寒霜。","短暂蓄能后发出伤害脉冲；生命恢复独立限流。","圣龙随行，定期朝目标喷出窄幅吐息。","火魔兽随行，前方扇形吐焰并留下短暂灼烧。","固定漩涡有限牵引，结束时爆发；首领免疫牵引。","按固定节拍连续射击，每四箭重新选定目标。","蓄势后发出重型箭矢，优先精英与远程威胁。","直线冰箭标记寒霜并短暂减速。","箭矢碰撞后爆炸，伤害归属爆炸箭。","银鹰环绕并标记强敌，随后发出追猎攻击。","短暂吸引有限普通近战敌人，结束时爆破。","连续斩击积累斗气，达到阈值后转入三次重斩。","锁定直线贯穿，随后在枪尖处爆发龙吼。","飞盾拦截弹丸积能，返身时将能量释放为震荡波。","仅由玩家实际冲刺触发，沿途留下延迟地裂，终点震地。","斗气化为连续审判印记，满层后集中裁决强敌。","苍龙剑气贯穿后回返，沿命中位置留下有限裂空爆发。","飞盾落点形成短暂圣域，周期锁定域内敌人降下审判。","毒雾积累后依次点燃三个分区，有限连爆后重新积毒。","冰枪布置有限节点，雷电沿节点间的真实路径传导。","圣龙吐息留下光辉走廊，光脉冲沿走廊继续攻击。","火魔兽标记猎物，连续三颗陨星围绕锁定目标落下。","牵引过程中积毒，结束时按最多三层毒进行有限爆破。","相邻寒霜与火焰区域交替，实际跨过边界触发受限冲击。","固定星井聚敌后周期圣光脉冲，恢复效果独立限流。","集中三箭与宽扇形箭轮流发射，形成明确射击节拍。","银鹰先标记弱点，再用两次重型射击集中打击。","分批冰箭累积寒霜，高层目标随后碎冰，数量受限。","贯穿路径留下最多四个爆破标记，按次序引爆。","草人短时聚敌，结束后展开四个有时限的爆炸陷阱。","两只兽灵以相反轨道巡猎，交替选择不同目标攻击。","重箭蓄势射出，在少量未命中威胁之间折射。","剑气留下最多四个雷印，后续有效攻击触发短暂节点电网。","实际防御吸收或受击积蓄寒霜，满层释放反击冰环。","锁定扇区吐出毒雾，停留积毒后引发一次有限毒爆。","圣光脉冲积累审判印记，最后集中裁决主要威胁。","近身连刺与远程冰枪交替，末击贯穿并消耗寒霜碎冰。","仅玩家实际冲刺触发，双凤凰沿真实路径前后夹击。","回旋盾有限拦截敌弹，并将其转为一次追击折射。","龙吼标记主要威胁，随后发出重型龙形贯穿。","地裂标出直线攻击带，五段箭雨依次沿裂隙追击。","近身突刺与远程贯穿交替，形成内外两段攻击节拍。","连续射击每三箭积满电荷，雷箭命中后进行有限传导。","草人附近持续积毒，结束时按毒层进行有限爆发。","圣龙与凤凰交替扫场或集中俯冲，交汇位置形成光焰冲击。","蓄势冰枪贯穿并在主要目标处产生一次冰晶碎裂。","火域中的爆炸箭留下有限火点，每个火点只允许一次次级传播。"};return si(s)>=0&&si(s)<skill_count?n[si(s)]:"";}
const char* Game::evolution_name(Skill s){static const char* n[]={"星群追猎","暴风连射","龙魂觉醒","回旋贯穿","雷霆风暴","永恒烈焰","不灭凤凰","裂地斩","剑刃风暴","箭矢风暴","天穹箭雨","无尽弹射","永冻环","双子陨星","回响法阵","疾风冲锋","交叉剑气","不屈战魂"};return si(s)>=0&&si(s)<18?n[si(s)]:"";}
const char* Game::evolution_requirement(Skill s){static const char* n[]={"技能5级 + 伤害2级：魔弹爆炸溅射。","技能5级 + 投射物2级：穿透并提高连射频率。","技能5级 + 生命2级：获得护盾并产生延迟龙魂冲击。","技能5级 + 投射物2级：箭矢折返，可再次命中。","技能5级 + 范围2级：链条落点产生延迟雷暴。","技能5级 + 持续2级：领域扩大且灼烧持续更久。","技能5级 + 持续2级：火羽落点留下灼烧区域。","技能5级 + 伤害2级：扇形扩大并震退敌人。","技能5级 + 范围2级：剑刃吸引敌人并加速旋转。","技能5级 + 攻速2级：双向覆盖并提升发射频率。","技能5级 + 持续2级：箭雨减速且覆盖时间翻倍。","技能5级 + 投射物2级：弹射次数翻倍且命中不衰减。","技能5级 + 范围2级：冰环短暂冻结敌人。","技能5级 + 冷却2级：首颗陨石后追加第二次爆炸。","技能5级 + 持续2级：法阵随玩家移动并加快脉冲。","技能5级 + 移速2级：冲锋路径更长并触发回旋斩。","技能5级 + 投射物2级：额外发射交叉剑气。","技能5级 + 生命2级：反击吸血并刷新短暂护盾。"};return si(s)>=0&&si(s)<18?n[si(s)]:"";}
const char* Game::passive_name(UpgradeKind k){static const char* n[]={"技能","伤害强化","攻击速度","范围强化","额外投射物","移动速度","生命强化","磁力拾取","冷却缩减","持续时间","暴击几率","暴击伤害","应急补给","极限强攻","永久韧性","遗物","技能融合","秘卷研习"};int i=int(k);return i>=0&&i<int(UpgradeKind::Count)?n[i]:"未知强化";}
const char* Game::special_event_name(SpecialEventKind k){static const char* n[]={"平静时刻","守卫宝箱","精英悬赏","鲜血祝福","神秘商人","蘑菇暴动","精英围猎","幸运喷泉","时空裂隙"};return n[std::clamp(int(k),0,8)];}
const char* Game::item_name(ItemKind k){static const char* n[]={"红色药水","磁力晶石","狂热符石","无敌护盾","清场炸弹","双倍经验","力量卷轴","冷却重置","随机宝箱"};return n[std::clamp(int(k),0,8)];}
bool Game::skill_allowed(Skill skill)const{
 const int id=int(skill);if(id<0||id>=skill_count)return false;
 for(const auto& recipe:fusion_recipes())if(recipe.result==skill)return recipe.released;
 return true;
}
bool Game::evolved(Skill s)const{return si(s)>=0&&si(s)<18&&skill_allowed(s)&&effective_skill_level(s)>=5&&passives[evo_passive[si(s)]]>=2;}
void Game::start(CharacterClass cls,uint32_t seed,RunBonuses bonuses,RunSetup setup){
 run_mode_=int(setup.mode)>=0&&int(setup.mode)<=3?setup.mode:RunMode::Classic;difficulty_=int(setup.difficulty)>=0&&int(setup.difficulty)<=2?setup.difficulty:Difficulty::Normal;daily_key_=setup.daily_key;daily_variant_=std::clamp(setup.daily_variant,0,2);daily_character_=int(setup.daily_character)>=0&&int(setup.daily_character)<=2?setup.daily_character:CharacterClass::Archer;starting_relic_mask_=0;last_boss_cycle_=-1;
 if((config.duration==600&&config.boss_time==510)||(config.duration==1200&&config.boss_time==1110)){config.duration=run_mode_==RunMode::Classic?1200.f:600.f;config.boss_time=run_mode_==RunMode::Classic?1110.f:510.f;}
 run_gear.clear();gear_drops.clear();gear_shield_seconds_=0;gear_dirty_=true;gear_cooldowns_.fill(0);gear_roots_.fill(0);
 if(run_mode_==RunMode::Daily){cls=daily_character_;bonuses={};}
 loot_enabled_=setup.enable_loot;unlocked_relic_mask_=(setup.unlocked_relic_mask&((1u<<relic_count)-1))|initial_relic_mask();owned_relic_mask_=0;relic_selection_=false;pending_relic_rewards_=0;
 for(int i=0;i<equipment_slot_count;++i)equipment_[i]=equipment_allowed(setup.equipment[i],EquipmentSlot(i),cls)?setup.equipment[i]:EquipmentId::None;reset_loot_timers();
 run_seed_=seed;chapter_index_=0;intermission_choice_=-1;completed_elapsed_=next_chapter_shield_seconds_=0;hazard_clock_=12;chapter_finish_pending_=false;
 auto bounded=[](float value,float cap){return std::isfinite(value)?std::clamp(value,0.f,cap):0.f;};
 bonuses.damage_bonus=bounded(bonuses.damage_bonus,.10f);bonuses.crit_bonus=bounded(bonuses.crit_bonus,.06f);
 bonuses.health_bonus=bounded(bonuses.health_bonus,20.f);bonuses.move_speed_bonus=bounded(bonuses.move_speed_bonus,.06f);
 bonuses.xp_bonus=bounded(bonuses.xp_bonus,.10f);bonuses.cooldown_reduction=bounded(bonuses.cooldown_reduction,.06f);
 bonuses.upgrade_rerolls=std::clamp(bonuses.upgrade_rerolls,0,2);bonuses.death_protections=std::clamp(bonuses.death_protections,0,1);bonuses.first_elite_reward=std::clamp(bonuses.first_elite_reward,0,1);
 run_bonuses_=bonuses;upgrade_rerolls_remaining=bonuses.upgrade_rerolls;death_protections_remaining=bonuses.death_protections;first_elite_reward_claimed=false;
 ordered_skills_.fill(Skill::Count);cached_build_levels_.fill(-1);character=cls;rng_.seed(seed);state=State::Playing;player={};elapsed=combat_elapsed=0;kills=0;boss_spawned=boss_defeated=false;boss_encounters_spawned_=0;
 resonance_directions.fill(0);resonance_last_root_.fill(0);resonance_shields_.fill(0);resonance_shield_time_.fill(0);hunt_previous_target_.fill(0);resonance_cooldown_.fill(0);resonance_flash_.fill(0);resonance_meter_.fill(0);pending_resonance_effects_.clear();resonance_dash_source_=Skill::Count;next_attack_id_=1;current_attack_id_=0;skills.fill(0);adventure_phase_.fill(0);adventure_root_=0;shadow_ready_=0;adventure_dashing_=false;combat_coins_=energy_hits_=bat_hits_=0;consumed_skills=0;dragon_soul=guard_charge=battle_focus=0;synergy_cooldown_.fill(0);synergy_flash.fill(0);fusion_dash_active_=false;fusion_dash_hits_.clear();fusion_dash_trail_=0;soul_hit_cooldown_=soul_absorb_cooldown_=focus_hit_cooldown_=holy_support_cooldown_=0;fusion_dash_source_=Skill::Count;passives.fill(0);cooldown_.fill(.3f);cooldown_period_.fill(0);announced_evolutions_.fill(false);metrics={};metrics.character=cls;scrolls={};pending_study={};timing={};timing.rules_version=2;encounter_plan={};prepare_encounter_plan();
 if(run_mode_==RunMode::Daily){for(int i=0;i<relic_count;++i)if((setup.starting_relic_mask&(1u<<i))&&(unlocked_relic_mask_&(1u<<i))&&relic_allowed(RelicId(i),cls)){owned_relic_mask_|=1u<<i;++metrics.relics_collected;}starting_relic_mask_=owned_relic_mask_;}
 enemies.clear();projectiles.clear();gems.clear();effects.clear();hostile_impacts.clear();damage_numbers.clear();choices.clear();events.clear();grid_.clear();items.clear();special_event={};
 next_event_in=config.first_event_time;fury_remaining=magnet_remaining=notice_remaining=shield_remaining=double_xp_remaining=power_remaining=rift_remaining=curse_remaining=0;
 notice_text.clear();spawn_clock_=elite_clock_=dash_remaining_=0;next_id_=1;pending_levels_=pending_skill_rewards_=upgrade_picks_taken=upgrade_resets_remaining=0;bonus_selection_=false;coins=overcharge_levels=vitality_levels=0;retaliation_pending_=false;broadphase_candidates=simulation_steps=0;
 skills[si(cls==CharacterClass::Archer?Skill::Arrow:cls==CharacterClass::Mage?Skill::Homing:Skill::Cleave)]=1;
 combat_diagnostics={};pressure_metrics={};reset_pressure();reset_combat_scene();refresh_stats();player.hp=player.max_hp;for(int i=0;i<4;i++)spawn(EnemyKind::Slime);
}
const char* Game::chapter_name(int index){static const char* names[]={"射手村","魔法密林","勇士部落"};return index>=0&&index<3?names[index]:"未知章节";}
const char* Game::chapter_objective(int index){static const char* objectives[]={"基础近战为主，逐步引入冲锋与远射；本章主题提高权重，首领从当前合法池选定。","分裂、支援与地面控制形成配合；本章主题提高权重，首领从当前合法池选定。","重装敌人、蓄势冲锋和精英交替施压；本章主题提高权重，首领从当前合法池选定。"};return index>=0&&index<3?objectives[index]:"";}
int Game::chapter_reward_skill() const {for(int i=0;i<skill_count;++i)if(skill_allowed(Skill(i))&&skills[i]>0&&skills[i]<5)return i;return -1;}
std::string Game::chapter_reward_description(int index) const {
 if(index==0)return "坚韧补给：本次冒险韧性 +1（生命上限 +8），恢复25%最大生命。";
 if(index==1){int target=chapter_reward_skill();return target>=0?std::string("技艺补给：")+skill_name(Skill(target))+" +1（"+std::to_string(skills[target])+" → "+std::to_string(skills[target]+1)+"）。":"技艺补给：已学技能全部满级，本次冒险极限强攻 +1（基础伤害 +3.5%）。";}
 if(index==2)return "行军补给：获得15局内金币，下一章开始获得8秒护盾。";
 return "";
}
void Game::clear_battlefield(){
 skill_visuals.clear();visual_peak=visual_merged=visual_dropped=0;
 gear_drops.clear();gear_shield_seconds_=0;gear_cooldowns_.fill(0);gear_roots_.fill(0);
 resonance_cooldown_.fill(0);resonance_meter_.fill(0);resonance_flash_.fill(0);resonance_shields_.fill(0);resonance_shield_time_.fill(0);resonance_last_root_.fill(0);pending_resonance_effects_.clear();resonance_dash_source_=Skill::Count;
 reset_pressure();reset_combat_scene();
 pending_relic_rewards_=0;relic_selection_=false;reset_loot_timers();
 enemies.clear();projectiles.clear();gems.clear();effects.clear();damage_numbers.clear();choices.clear();events.clear();grid_.clear();items.clear();special_event={};
 synergy_cooldown_.fill(0);synergy_flash.fill(0);fusion_dash_active_=false;fusion_dash_hits_.clear();fusion_dash_trail_=0;soul_hit_cooldown_=soul_absorb_cooldown_=focus_hit_cooldown_=holy_support_cooldown_=0;fusion_dash_source_=Skill::Count;cooldown_.fill(.3f);cooldown_period_.fill(0);spawn_clock_=elite_clock_=dash_remaining_=0;next_id_=1;pending_levels_=pending_skill_rewards_=upgrade_picks_taken=upgrade_resets_remaining=0;bonus_selection_=false;retaliation_pending_=false;chapter_finish_pending_=false;
 fury_remaining=magnet_remaining=notice_remaining=shield_remaining=double_xp_remaining=power_remaining=rift_remaining=curse_remaining=0;notice_text.clear();next_event_in=config.first_event_time;hazard_clock_=12;
 player.pos={};player.dash_cooldown=player.invulnerable=player.animation_time=0;player.facing_left=player.moving=false;dash_direction_={1,0};
}
void Game::finish_chapter(){
 end_pressure_event();danger_zones.clear();soul_links.clear();reviving_enemies_.clear();update_pressure_sources();refresh_stats();
 if(run_mode_!=RunMode::Campaign){chapter_finish_pending_=false;state=State::Victory;add_event(EventKind::Victory,player.pos);return;}
 chapter_finish_pending_=false;metrics.chapters_cleared=chapter_index_+1;intermission_choice_=-1;
 if(chapter_index_>=2){state=State::Victory;add_event(EventKind::Victory,player.pos);notify("三章远征完成！构筑与战绩已记录。");}
 else{state=State::ChapterComplete;notify(std::string(chapter_name(chapter_index_))+"通关：选择一次补给后，保留构筑前往下一章。");}
}
bool Game::export_build(RunBuild& out) const {
 if(run_mode_!=RunMode::Campaign||state!=State::ChapterComplete||chapter_index_<0||chapter_index_>=2||pending_levels_!=0||pending_relic_rewards_!=0||relic_selection_||!scrolls.offers.empty()||pending_study.active)return false;
 RunBuild b;b.run_gear=run_gear;b.resonance_directions=resonance_directions;b.scrolls=scrolls;b.encounter_plan=encounter_plan;b.timing=timing;std::ostringstream random_state;random_state<<rng_;b.rng_state=random_state.str();b.consumed_skills=consumed_skills;b.dragon_soul=dragon_soul;b.guard_charge=guard_charge;b.battle_focus=battle_focus;b.chapter_index=chapter_index_;b.intermission_choice=intermission_choice_;b.seed=run_seed_;b.character=character;b.completed_elapsed=total_elapsed();b.hp=player.hp;b.xp=player.xp;b.level=player.level;b.kills=kills;b.coins=coins;b.overcharge_levels=overcharge_levels;b.vitality_levels=vitality_levels;b.skills=skills;b.passives=passives;b.bonuses=run_bonuses_;b.upgrade_rerolls_remaining=upgrade_rerolls_remaining;b.death_protections_remaining=death_protections_remaining;b.first_elite_reward_claimed=first_elite_reward_claimed;b.boss_defeated=boss_defeated;b.next_chapter_shield_seconds=next_chapter_shield_seconds_;b.metrics=metrics;b.enable_loot=loot_enabled_;b.unlocked_relic_mask=unlocked_relic_mask_;b.owned_relic_mask=owned_relic_mask_;b.equipment=equipment_;b.difficulty=difficulty_;b.daily_key=daily_key_;b.daily_variant=daily_variant_;b.daily_character=daily_character_;b.starting_relic_mask=starting_relic_mask_;
 Game checked(config);if(!checked.restore_build(b))return false;out=std::move(b);return true;
}
bool Game::restore_build(const RunBuild& b){
 auto bounded=[](double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
 if(b.version!=1||b.chapter_index<0||b.chapter_index>=2||b.intermission_choice<-1||b.intermission_choice>2||int(b.character)<0||int(b.character)>2||!bounded(b.completed_elapsed,.000001,1e8)||b.level<1||b.level>10000||b.kills<0||b.kills>100000000||b.coins<0||b.coins>100000000||b.overcharge_levels<0||b.overcharge_levels>100000||b.vitality_levels<0||b.vitality_levels>100000)return false;
 if(int(b.difficulty)<0||int(b.difficulty)>2||b.daily_variant<0||b.daily_variant>2||int(b.daily_character)<0||int(b.daily_character)>2||b.starting_relic_mask!=0||b.metrics.endless_cycle!=0||(b.metrics.enemy_seen_mask&~((1u<<enemy_kind_count)-1)))return false;
 uint32_t valid_mask=(1u<<relic_count)-1,unlocked=b.unlocked_relic_mask|initial_relic_mask();
 if((b.unlocked_relic_mask&~valid_mask)||(b.owned_relic_mask&~unlocked)||(!b.enable_loot&&b.owned_relic_mask))return false;
 int owned_count=0;for(int i=0;i<relic_count;++i){if(b.owned_relic_mask&(1u<<i)){if(!relic_allowed(RelicId(i),b.character))return false;++owned_count;}if(b.metrics.relic_procs[i]<0||b.metrics.relic_procs[i]>100000000||(b.metrics.relic_procs[i]>0&&!(b.owned_relic_mask&(1u<<i))))return false;}
 if(b.metrics.relics_collected!=owned_count)return false;
 for(int i=0;i<equipment_slot_count;++i)if(!equipment_allowed(b.equipment[i],EquipmentSlot(i),b.character))return false;
 for(int i=0;i<equipment_count;++i)if(b.metrics.equipment_procs[i]<0||b.metrics.equipment_procs[i]>100000000||(b.metrics.equipment_procs[i]>0&&(i==0||std::find(b.equipment.begin(),b.equipment.end(),EquipmentId(i))==b.equipment.end())))return false;
 const auto& bonus=b.bonuses;
 if(!bounded(bonus.damage_bonus,0,.100001)||!bounded(bonus.crit_bonus,0,.060001)||!bounded(bonus.health_bonus,0,20)||!bounded(bonus.move_speed_bonus,0,.060001)||!bounded(bonus.xp_bonus,0,.100001)||!bounded(bonus.cooldown_reduction,0,.060001)||bonus.upgrade_rerolls<0||bonus.upgrade_rerolls>2||bonus.death_protections<0||bonus.death_protections>1||bonus.first_elite_reward<0||bonus.first_elite_reward>1)return false;
 const auto& m=b.metrics;
 if(m.character!=b.character||m.chapters_cleared!=b.chapter_index+1||m.bosses_defeated<0||m.bosses_defeated>b.chapter_index+1||m.bosses_defeated!=m.enemy_kills_by_kind[int(EnemyKind::Boss)]||(b.boss_defeated&&m.bosses_defeated==0))return false;
 int64_t measured_kills=0;for(int count:m.enemy_kills_by_kind){if(count<0)return false;measured_kills+=count;}if(measured_kills!=b.kills)return false;
 if(b.upgrade_rerolls_remaining<0||m.upgrade_rerolls_used<0||m.upgrade_rerolls_used>2||b.upgrade_rerolls_remaining!=bonus.upgrade_rerolls-m.upgrade_rerolls_used||b.death_protections_remaining<0||m.death_protections_used<0||m.death_protections_used>1||b.death_protections_remaining!=bonus.death_protections-m.death_protections_used||m.first_elite_rewards<0||m.first_elite_rewards>bonus.first_elite_reward||b.first_elite_reward_claimed!=(m.first_elite_rewards==1))return false;
 if(!bounded(b.next_chapter_shield_seconds,0,8)||b.next_chapter_shield_seconds!=(b.intermission_choice==2?8.f:0.f))return false;
 for(double value:m.skill_damage)if(!bounded(value,0,1e16))return false;for(double value:m.xp_by_minute)if(!bounded(value,0,1e16))return false;
 for(double value:{m.other_damage,m.damage_taken,m.altar_health_spent,m.xp_acquired,m.boss_damage_taken,m.environment_damage_taken})if(!bounded(value,0,1e16))return false;
 for(int value:{m.dash_uses,m.events_started,m.events_succeeded,m.events_failed,m.items_collected,m.boss_attacks,m.hazards_spawned,m.environment_hits})if(value<0||value>100000000)return false;
 if(!bounded(m.boss_spawn_time,-1,b.completed_elapsed+.1)||!bounded(m.boss_defeat_time,-1,b.completed_elapsed+.1))return false;
 Game restored(config);if(restored.config.duration==1200){restored.config.duration=600;restored.config.boss_time=510;}restored.run_gear=b.run_gear;restored.gear_dirty_=true;restored.character=b.character;restored.run_mode_=RunMode::Campaign;restored.run_seed_=b.seed;restored.chapter_index_=b.chapter_index;restored.intermission_choice_=b.intermission_choice;restored.completed_elapsed_=b.completed_elapsed;restored.run_bonuses_=b.bonuses;restored.difficulty_=b.difficulty;restored.daily_key_=b.daily_key;restored.daily_variant_=b.daily_variant;restored.daily_character_=b.daily_character;restored.starting_relic_mask_=b.starting_relic_mask;restored.loot_enabled_=b.enable_loot;restored.unlocked_relic_mask_=unlocked;restored.owned_relic_mask_=b.owned_relic_mask;restored.equipment_=b.equipment;restored.reset_loot_timers();restored.scrolls=b.scrolls;restored.skills=b.skills;
 if(!restored.validate_scroll_state()||!b.scrolls.offers.empty())return false;
 for(int i=0;i<skill_count;++i)if(b.skills[i]<0||b.skills[i]>5||(b.skills[i]>0&&!restored.skill_allowed(Skill(i))))return false;
 for(int i=0;i<passive_count;++i)if(b.passives[i]<0||b.passives[i]>8||(i==0&&b.passives[i]!=0))return false;
 restored.resonance_directions=b.resonance_directions;restored.skills=b.skills;restored.consumed_skills=b.consumed_skills;restored.dragon_soul=b.dragon_soul;restored.guard_charge=b.guard_charge;restored.battle_focus=b.battle_focus;
 if(!bounded(b.battle_focus,0,100)||(b.battle_focus>0&&b.skills[int(Skill::BattleFocus)]==0&&b.skills[int(Skill::BattleBlades)]==0&&b.skills[int(Skill::SpiritJudgement)]==0))return false;
 const int guard_limit=std::max(b.skills[int(Skill::Bulwark)]>0?20+4*b.skills[int(Skill::Bulwark)]:0,b.skills[int(Skill::FrostArmor)]>0?8+2*b.skills[int(Skill::FrostArmor)]:0);
 if(!bounded(b.guard_charge,0,guard_limit))return false;
 for(int count:b.metrics.synergy_triggers)if(count<0||count>100000000)return false;
 if((b.consumed_skills&~material_skill_mask())!=0||!bounded(b.dragon_soul,0,100)||b.metrics.fusions_created<0||b.metrics.fusions_created>1000000||!bounded(b.metrics.first_fusion_time,-1,b.completed_elapsed))return false;
 uint64_t expected_consumed=0;int fused=0;for(int i=0;i<skill_count;++i)if(b.skills[i]>0&&is_fusion(Skill(i))){++fused;for(const auto& r:fusion_recipes())if(r.result==Skill(i)){for(Skill source:r.sources)if(int(source)<64)expected_consumed|=uint64_t(1)<<int(source);break;}}
 if((expected_consumed&b.consumed_skills)!=expected_consumed||fused>b.metrics.fusions_created||(b.metrics.fusions_created==0)!=(b.metrics.first_fusion_time<0)||(b.dragon_soul>0&&b.skills[int(Skill::DragonArmor)]==0))return false;
 for(const auto& recipe:fusion_recipes())if(b.skills[int(recipe.result)]>0&&std::none_of(restored.scrolls.completed.begin(),restored.scrolls.completed.end(),[&](const ScrollOffer& offer){return offer.result==recipe.result;}))restored.scrolls.completed.push_back({recipe.result,0});
 restored.encounter_plan=b.encounter_plan;restored.timing=b.timing;
 if((b.timing.speed!=1&&b.timing.speed!=2)||(b.timing.used_speeds&~3u)||!bounded(b.timing.effective_real_seconds,0,1e9)||!bounded(b.timing.total_real_seconds,b.timing.effective_real_seconds,1e9))return false;
 if(b.encounter_plan.version!=1||b.encounter_plan.epoch<-1||b.encounter_plan.region<0||b.encounter_plan.region>5||b.encounter_plan.boss<-1||b.encounter_plan.boss>=boss_profile_count||b.encounter_plan.roster.size()>32)return false;
 for(int visual:b.encounter_plan.roster)if(visual<0||visual>=enemy_visual_count)return false;
 restored.rng_.seed(b.seed^(0x9e3779b9u*uint32_t(b.chapter_index+1)));if(!b.rng_state.empty()){std::istringstream random_state(b.rng_state);if(!(random_state>>restored.rng_))return false;}
 if(restored.encounter_plan.epoch<0)restored.prepare_encounter_plan();
 restored.passives=b.passives;restored.overcharge_levels=b.overcharge_levels;restored.vitality_levels=b.vitality_levels;restored.player.level=b.level;
 float late=std::max(0.f,float(b.level-12));restored.player.xp_next=b.level==1?8.f:8+b.level*3.2f+.45f*late*late;restored.refresh_stats();
 if(!bounded(b.hp,.000001,restored.player.max_hp+25.001)||!bounded(b.xp,0,restored.player.xp_next)||b.xp>=restored.player.xp_next)return false;
 restored.player.hp=std::min(b.hp,restored.player.max_hp);restored.player.xp=b.xp;restored.kills=b.kills;restored.coins=b.coins;restored.upgrade_rerolls_remaining=b.upgrade_rerolls_remaining;restored.death_protections_remaining=b.death_protections_remaining;restored.first_elite_reward_claimed=b.first_elite_reward_claimed;restored.next_chapter_shield_seconds_=b.next_chapter_shield_seconds;restored.metrics=b.metrics;restored.boss_spawned=b.boss_defeated;restored.boss_defeated=b.boss_defeated;
 for(int i=0;i<skill_count;++i)restored.announced_evolutions_[i]=restored.evolved(Skill(i));restored.state=State::ChapterComplete;restored.notify("已恢复章节间记录：本章构筑与已选补给保留。");*this=std::move(restored);return true;
}
bool Game::apply_chapter_reward(int index){
 if(run_mode_!=RunMode::Campaign||state!=State::ChapterComplete||chapter_index_>=2||intermission_choice_!=-1||index<0||index>2)return false;
 if(index==0){++vitality_levels;refresh_stats();heal_player(player.max_hp*.25f);}
 else if(index==1){int target=chapter_reward_skill();if(target>=0)++skills[target];else ++overcharge_levels;refresh_stats();for(int i=0;i<skill_count;++i)announced_evolutions_[i]=evolved(Skill(i));}
 else{coins+=15;next_chapter_shield_seconds_=8;}
 intermission_choice_=index;notify("章节补给已领取。构筑保留，可以继续下一章。");auto_fuse_skills();return true;
}
bool Game::advance_chapter(){
 if(run_mode_!=RunMode::Campaign||state!=State::ChapterComplete||chapter_index_>=2||intermission_choice_<0)return false;
 completed_elapsed_=total_elapsed();elapsed=combat_elapsed=0;++chapter_index_;intermission_choice_=-1;boss_spawned=boss_defeated=false;boss_encounters_spawned_=0;
 float starting_shield=next_chapter_shield_seconds_;next_chapter_shield_seconds_=0;clear_battlefield();
 rng_.seed(run_seed_^(0x9e3779b9u*uint32_t(chapter_index_+1)));prepare_encounter_plan();refresh_stats();shield_remaining=starting_shield;state=State::Playing;
 for(int i=0;i<6+chapter_index_*2;++i)spawn(chapter_index_==1?(i%2?EnemyKind::Ranger:EnemyKind::Slime):(i%2?EnemyKind::BlueMushroom:EnemyKind::Pig));
 notify(std::string("进入")+chapter_name(chapter_index_)+"：等级、技能、生命和本次冒险天赋次数均保留。");auto_fuse_skills();return true;
}
int Game::boss_pattern(const Enemy& enemy) const {
 if(difficulty_==Difficulty::Hard){static const int patterns[]={0,1,2,3};return patterns[enemy.phase%4];}
 if(difficulty_==Difficulty::Nightmare){static const int patterns[]={0,2,3,1,4};return patterns[enemy.phase%5];}
 if(run_mode_==RunMode::Classic||run_mode_==RunMode::Daily)return enemy.phase%3;
 static const int patterns[3][4]={{0,0,1,2},{2,0,2,1},{1,0,1,2}};return patterns[region_index()][enemy.phase%4];
}
float Game::boss_interval() const {float base=run_mode_==RunMode::Campaign?(chapter_index_==0?5.f:chapter_index_==1?4.3f:3.7f):4.6f;return base*enemy_attack_multiplier()*(run_mode_==RunMode::Endless?std::max(.68f,1-.04f*std::min(metrics.endless_cycle,8)):1.f);}
void Game::update_environment(float dt){
 if(run_mode_==RunMode::Classic&&difficulty_==Difficulty::Normal)return;hazard_clock_-=dt;if(hazard_clock_>0)return;
 int region=region_index();float density=(run_mode_==RunMode::Endless?.75f:1.f)*(difficulty_==Difficulty::Hard?.85f:difficulty_==Difficulty::Nightmare?.7f:1.f)*(run_mode_==RunMode::Daily&&daily_variant_==2?.7f:1.f);
 hazard_clock_=(region==0?20.f:region==1?14.f:11.f)*density;
 const int count=region==0?1:region==1?3:2;
 for(int i=0;i<count&&effects.size()<config.max_effects;++i){Effect e;e.kind=EffectKind::Warning;e.hostile=e.environment=true;e.enemy_kind=EnemyKind::Slime;e.pos=player.pos;e.radius=region==0?62.f:region==1?48.f:40.f;e.delay=region==0?1.6f:1.3f;e.damage=(9.f+region*4)*(difficulty_==Difficulty::Hard?1.15f:difficulty_==Difficulty::Nightmare?1.3f:1.f);
  if(region==1){float angle=2*pi*i/3+elapsed*.1f;e.pos+=Vec2{std::cos(angle),std::sin(angle)}*70;}
  if(region==2&&i==1){e.pattern=1;e.radius=23;float angle=random(0,2*pi);Vec2 axis{std::cos(angle)*180,std::sin(angle)*180};e.pos=player.pos-axis;e.end=player.pos+axis;e.delay=1.7f;}
  e.delay=std::max(.8f,e.delay*(difficulty_==Difficulty::Hard?.9f:difficulty_==Difficulty::Nightmare?.8f:1.f));e.duration=e.delay+.3f;add_effect(e);++metrics.hazards_spawned;
 }
}
int Game::scheduled_boss_count() const {
 return run_mode_==RunMode::Classic&&config.boss_time>config.duration*2.f/3.f&&config.boss_time<config.duration?(config.duration>=1200?4:3):1;
}
float Game::next_boss_time() const {
 if(run_mode_==RunMode::Endless){const float duration=std::max(.05f,config.duration);return metrics.endless_cycle*duration+std::min(std::max(0.f,config.boss_time),duration*.95f);}
 if(boss_encounters_spawned_>=scheduled_boss_count())return std::numeric_limits<float>::infinity();
 if(scheduled_boss_count()==4&&boss_encounters_spawned_<3){static const float fractions[]={.20f,.46f,.73f};return config.duration*fractions[boss_encounters_spawned_];}
 if(scheduled_boss_count()==3&&boss_encounters_spawned_<2)return config.duration*(boss_encounters_spawned_+1)/3.f;
 return config.boss_time;
}
void Game::record_boss_spawn(Enemy& enemy){
 enemy.final_boss=boss_encounters_spawned_>=scheduled_boss_count()-1;++boss_encounters_spawned_;
 boss_spawned=true;if(enemy.final_boss)boss_defeated=false;last_boss_cycle_=metrics.endless_cycle;metrics.boss_spawn_time=total_elapsed();add_event(EventKind::Boss,enemy.pos);
 notify((scheduled_boss_count()>1?"第 "+std::to_string(boss_encounters_spawned_)+" / "+std::to_string(scheduled_boss_count())+" 名 Boss：":"Boss 登场：")+std::string(boss_profile_name(enemy.boss_profile))+"，留意招式预警！");
}
void Game::update_director(float dt,bool boss_due,bool boss_alive){
 DirectorInput in;in.elapsed=run_mode_==RunMode::Endless?std::fmod(elapsed,std::max(.05f,config.duration)):elapsed;in.chapter_duration=config.duration;in.chapter=region_index();in.difficulty=int(difficulty_);in.hp_ratio=player.hp/std::max(1.f,player.max_hp);in.enemy_count=enemies.size();in.max_enemies=enemy_capacity()-std::min(enemy_capacity(),reviving_enemies_.size());in.spawn_budget_multiplier=pressure_active(PressureKind::DarkTide)?3:1;if(boss_due)in.max_enemies=std::max(in.max_enemies,enemies.size()+1);if(pressure_active(PressureKind::Encirclement)){in.reserved_escape_angle=pressure_event.escape_angle;in.reserved_escape_half_angle=std::max(.65f,config.director_config.escape_half_angle);}in.spawn_radius=config.spawn_radius;in.boss_due=boss_due;in.boss_alive=boss_alive;in.boss_defeated=boss_defeated;in.external_important_active=boss_pressure_busy()||special_event.kind!=SpecialEventKind::None||pressure_active(PressureKind::Invasion)||pressure_active(PressureKind::Encirclement)||pressure_active(PressureKind::ShadowTrial);
 int near_count=0;for(const auto& e:enemies)if(e.hp>0&&length(e.pos-player.pos)<180)++near_count;in.recent_pressure=std::clamp(float(near_count)/18.f+float(metrics.damage_taken-director_last_damage_)/std::max(1.f,player.max_hp)*4,0.f,1.f);director_last_damage_=metrics.damage_taken;
 in.move_x=dash_direction_.x;in.move_y=dash_direction_.y;director_state=director_.update(dt,in);pressure_metrics.normal_spawn_budget+=director_state.normal_spawn_budget;pressure_metrics.event_spawn_budget+=director_state.event_spawn_budget;pressure_metrics.requested_spawns+=director_state.requested_spawns;pressure_metrics.capacity_limited+=director_state.capacity_limited;
 if(director_state.announced){notify(std::string(EncounterDirector::encounter_name(director_state.kind))+": "+EncounterDirector::encounter_hint(director_state.kind));add_event(EventKind::SpecialEvent,player.pos);}
 if(director_state.started&&director_state.kind==EncounterKind::Crisis){for(int i=0;i<3;++i){float angle=director_state.escape_angle+pi+(i-1)*.65f;Effect hazard;hazard.kind=EffectKind::Warning;hazard.pos=player.pos+Vec2{std::cos(angle),std::sin(angle)}*145;hazard.radius=66;hazard.delay=1.65f+.18f*i;hazard.duration=hazard.delay+.25f;hazard.damage=14;hazard.hostile=hazard.environment=true;add_effect(hazard);++metrics.hazards_spawned;}}
 for(const auto& request:director_state.spawns){EnemyKind kind=EnemyKind::Slime;switch(request.role){
  case DirectorEnemyRole::Common:kind=region_index()==0?EnemyKind::Mushroom:region_index()==1?EnemyKind::Slime:EnemyKind::BlueMushroom;break;
  case DirectorEnemyRole::Swarm:kind=region_index()==1||(elapsed>config.duration*.12f&&rng_()%4==0)?EnemyKind::Splitter:EnemyKind::Mushroom;break;
  case DirectorEnemyRole::Ground:kind=EnemyKind::GroundController;break;case DirectorEnemyRole::Charger:kind=EnemyKind::WindupCharger;break;case DirectorEnemyRole::Fast:kind=EnemyKind::Pig;break;case DirectorEnemyRole::Armored:kind=EnemyKind::BlueMushroom;break;case DirectorEnemyRole::Ranged:kind=EnemyKind::Ranger;break;case DirectorEnemyRole::Support:kind=EnemyKind::Support;break;case DirectorEnemyRole::Elite:kind=EnemyKind::Elite;break;case DirectorEnemyRole::Boss:kind=EnemyKind::Boss;break;
  }
  size_t before=enemies.size();spawn(kind);if(enemies.size()==before){++pressure_metrics.capacity_limited;continue;}++pressure_metrics.actual_spawns;auto& enemy=enemies.back();enemy.pos=safe_enemy_position(player.pos+Vec2{request.offset_x,request.offset_y},enemy.radius,std::max(180.f,config.director_config.min_safe_distance));enemy.speed*=request.speed_scale;enemy.hp*=request.health_scale;enemy.max_hp=enemy.hp;
  if(kind==EnemyKind::Boss)record_boss_spawn(enemy);
 }
}
const char* Game::scene_object_key(SceneObjectKind kind){static const char* keys[]={"prop_crate","prop_barrel","prop_mud","prop_heal"};int i=int(kind);return i>=0&&i<4?keys[i]:keys[0];}
const char* Game::scene_animation_key(const SceneObject& object){return object.state==SceneObjectState::Broken||object.state==SceneObjectState::Spent?"break":object.hit_flash>0||object.state==SceneObjectState::Armed?"hit":"idle";}
const char* Game::boss_stage_name(int stage){return stage>=3?"第三阶段 · 狂怒":stage==2?"第二阶段 · 援军":"第一阶段 · 追猎";}
void Game::reset_combat_scene(){combat_records.clear();pending_boss_nodes_.clear();previous_combat_states_=0;last_failed_record_=-10;scene_objects.clear();next_scene_id_=1;scene_spawn_clock_=0;director_=EncounterDirector(config.director_config);director_.reset(run_seed_,chapter_index_);director_state={};director_last_damage_=metrics.damage_taken;}
bool Game::scene_solid(const SceneObject& o) const {return (o.kind==SceneObjectKind::Crate||o.kind==SceneObjectKind::Barrel)&&(o.state==SceneObjectState::Active||o.state==SceneObjectState::Armed);}
float Game::scene_slow(Vec2 pos,float radius) const {if(!config.enable_scene_objects)return 1;for(const auto& o:scene_objects)if(o.kind==SceneObjectKind::Mud&&o.state==SceneObjectState::Active&&length(pos-o.pos)<o.radius+radius)return .55f;return 1;}
void Game::resolve_scene_collision(Vec2& pos,float radius){
 if(!config.enable_scene_objects)return;
 // Bounded circle separation, with an escape valve for malformed/overlapping layouts.
 for(int pass=0;pass<6;++pass){bool moved=false;for(const auto& o:scene_objects)if(scene_solid(o)){Vec2 delta=pos-o.pos;float distance=length(delta),limit=radius+o.radius;if(distance<limit){Vec2 axis=distance>.001f?delta*(1/distance):Vec2{1,0};pos=o.pos+axis*(limit+.1f);moved=true;++combat_diagnostics.collision_resolutions;}}if(!moved)return;}
 for(auto& o:scene_objects)if(scene_solid(o)&&length(pos-o.pos)<radius+o.radius){o.state=SceneObjectState::Broken;o.hp=0;o.countdown=.75f;++combat_diagnostics.escape_resolutions;}
}
void Game::hurt_scene_object(size_t index,float damage,Skill source){
 if(index>=scene_objects.size()||!std::isfinite(damage)||damage<=0)return;auto& o=scene_objects[index];if(!scene_solid(o))return;o.hit_flash=.16f;
 if(o.kind==SceneObjectKind::Barrel){if(o.state!=SceneObjectState::Armed){o.state=SceneObjectState::Armed;o.countdown=1.1f;o.source=source;}return;}
 float actual=std::min(o.hp,damage);o.hp=std::max(0.f,o.hp-damage);combat_diagnostics.object_damage+=actual;
 if(o.hp<=0){o.state=SceneObjectState::Broken;o.countdown=.75f;++combat_diagnostics.objects_destroyed;}
}
void Game::damage_scene_area(Vec2 pos,float radius,float damage,Skill source){if(source!=Skill::Count)damage_pressure_facilities(pos,radius,damage);if(!config.enable_scene_objects)return;for(size_t i=0;i<scene_objects.size();++i)if(scene_solid(scene_objects[i])&&length(scene_objects[i].pos-pos)<radius+scene_objects[i].radius)hurt_scene_object(i,damage,source);}
bool Game::scene_projectile_collision(Vec2 from,Vec2 to,float radius,float damage,Skill source){
 if(source!=Skill::Count)for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone.hp>0&&zone.age>=zone.warning&&zone.age<zone.duration&&segment_distance(zone.pos,from,to)<=24+radius){Vec2 center=zone.pos;damage_pressure_facilities(center,0,damage);return true;}
 if(!config.enable_scene_objects)return false;size_t hit=none;float best=std::numeric_limits<float>::max();
 for(size_t i=0;i<scene_objects.size();++i){const auto& o=scene_objects[i];if(!scene_solid(o)||segment_distance(o.pos,from,to)>o.radius+radius)continue;float distance=length(o.pos-from);if(distance<best){best=distance;hit=i;}}
 if(hit==none)return false;hurt_scene_object(hit,damage,source);return true;
}
void Game::populate_scene(){
 if(!config.enable_scene_objects)return;int wanted=scene_objects.empty()?8:4;
 for(int n=0;n<wanted&&scene_objects.size()<config.max_scene_objects;++n){SceneObject object;object.id=next_scene_id_++;object.kind=SceneObjectKind(n%4);object.radius=object.kind==SceneObjectKind::Mud?74.f:object.kind==SceneObjectKind::Barrel?20.f:25.f;object.hp=object.max_hp=object.kind==SceneObjectKind::Crate?36.f:1.f;object.lifetime=object.kind==SceneObjectKind::Mud?65.f:95.f;
  bool safe=false;for(int attempt=0;attempt<12;++attempt){float angle=random(0,2*pi);object.pos=player.pos+Vec2{std::cos(angle),std::sin(angle)}*random(240,570);safe=true;for(const auto& other:scene_objects)if(length(object.pos-other.pos)<object.radius+other.radius+60){safe=false;break;}if(safe)for(const auto& enemy:enemies)if(enemy.hp>0&&length(object.pos-enemy.pos)<object.radius+enemy.radius+36){safe=false;break;}if(safe)break;}
  if(safe){scene_objects.push_back(object);++combat_diagnostics.scene_objects_created;}
 }
}
void Game::update_scene_objects(float dt,bool interact){
 if(!config.enable_scene_objects)return;scene_spawn_clock_-=dt*encounter_pace();if(scene_spawn_clock_<=0){populate_scene();scene_spawn_clock_=18;}
 size_t use=none;float nearest_distance=80;if(interact)for(size_t i=0;i<scene_objects.size();++i){auto& o=scene_objects[i];if(o.state!=SceneObjectState::Active||o.kind==SceneObjectKind::Mud)continue;float distance=length(o.pos-player.pos);if(distance<nearest_distance){nearest_distance=distance;use=i;}}
 if(use!=none){auto& object=scene_objects[use];if(object.kind==SceneObjectKind::HealingWell){float restored=heal_player(30);if(restored>0){object.state=SceneObjectState::Spent;object.countdown=.75f;++combat_diagnostics.healing_uses;add_event(EventKind::ItemPickup,object.pos);}}else hurt_scene_object(use,24,Skill::Count);}
 for(auto& object:scene_objects){object.age+=dt;object.hit_flash=std::max(0.f,object.hit_flash-dt);if(object.state==SceneObjectState::Armed){object.countdown-=dt;if(object.countdown<=0){
    // A barrel is secondary damage. It cannot arm another barrel or proc a relic.
    object.state=SceneObjectState::Broken;object.countdown=1.6f;++combat_diagnostics.barrels_exploded;++combat_diagnostics.objects_destroyed;Vec2 origin=object.pos;float radius=118;
    for(auto index:nearby(origin,radius+55))if(length(enemies[index].pos-origin)<radius+enemies[index].radius)hurt_enemy(index,85,enemies[index].pos-origin,2,object.source,false,false);
    if(length(player.pos-origin)<radius+12)hurt_player(20,false,true);
    Effect blast;blast.kind=EffectKind::Nova;blast.pos=origin;blast.radius=radius;blast.duration=.45f;blast.environment=true;blast.proc_generated=true;add_effect(blast);
   }}else if(object.state==SceneObjectState::Broken||object.state==SceneObjectState::Spent)object.countdown-=dt;
 }
 scene_objects.erase(std::remove_if(scene_objects.begin(),scene_objects.end(),[&](const SceneObject& o){return ((o.state==SceneObjectState::Broken||o.state==SceneObjectState::Spent)&&o.countdown<=0)||o.age>o.lifetime||length(o.pos-player.pos)>config.despawn_radius;}),scene_objects.end());
}
void Game::reset_to_menu(){reset_pressure();state=State::Menu;events.clear();choices.clear();scene_objects.clear();director_state={};}
void Game::toggle_pause(){if(state==State::Playing)state=State::Paused;else if(state==State::Paused)state=State::Playing;}
float Game::random(float lo,float hi){return lo+(hi-lo)*float(static_cast<double>(rng_())/4294967296.0);}
void Game::add_projectile(Projectile p){p.gear_snapshot=(1+gear_stats_[0])*gear_skill_bonus(p.skill);if(!p.root_attack_id)p.root_attack_id=current_attack_id_;if(p.level_snapshot<=0){p.level_snapshot=effective_skill_level(p.skill);p.power_snapshot=skill_power(p.skill);}if(projectiles.size()<config.max_projectiles){projectiles.push_back(p);if(p.skill>=Skill::TripleStar)emit_skill_visual(FxStage::Cast,p.skill,p.pos,p.pos+normalized(p.velocity)*45,16);}}
void Game::add_effect(Effect e){e.gear_snapshot=(1+gear_stats_[0])*gear_skill_bonus(e.source);if(!e.root_attack_id)e.root_attack_id=current_attack_id_;if(e.source!=Skill::Count&&e.effective_level_snapshot==0)e.effective_level_snapshot=effective_skill_level(e.source);if(effects.size()<config.max_effects+(e.hostile&&e.enemy_kind==EnemyKind::Boss?24:0))effects.push_back(e);}
void Game::add_event(EventKind k,Vec2 p,Skill source){if(events.size()<64)events.push_back({k,p,0,false,source});}
void Game::refresh_stats(){
 update_pressure_sources();refresh_gear();
 damage_multiplier=(1+.15f*passives[1]+.035f*overcharge_levels)*(1+run_bonuses_.damage_bonus)*(!buff_suppressed_&&fury_remaining>0?1.4f:1.f)*(!buff_suppressed_&&power_remaining>0?1.5f:1.f);
 attack_speed=(1+.12f*passives[2])*(!buff_suppressed_&&fury_remaining>0?1.25f:1.f)*(!buff_suppressed_&&relic_haste_remaining_>0?1.2f:1.f);area_multiplier=1+.12f*passives[3];extra_projectiles=passives[4];
 pickup_radius=magnet_remaining>0&&!buff_suppressed_?1600.f:(65+22.f*passives[7])*(pressure_active(PressureKind::MapleDisorder)?config.maple_disorder_factor:1.f);player.speed=170.f*(1+.08f*passives[5])*(1+run_bonuses_.move_speed_bonus)*(run_mode_==RunMode::Daily&&daily_variant_==1?1.1f:1.f);
 player.max_hp=110.f+20*passives[6]+8*vitality_levels+run_bonuses_.health_bonus;
 cooldown_multiplier=1/(1+.08f*passives[8])*(1-run_bonuses_.cooldown_reduction)*(run_mode_==RunMode::Daily&&daily_variant_==2?.85f:1.f)*(!buff_suppressed_&&rift_remaining>0?.7f:1.f);duration_multiplier=1+.18f*passives[9];crit_chance=std::min(.7f,.10f+.035f*passives[10]+run_bonuses_.crit_bonus);crit_multiplier=1.7f+.18f*passives[11];
 damage_multiplier*=1+gear_stats_[0];player.max_hp+=gear_stats_[1];player.hp=std::min(player.hp,player.max_hp);player.speed*=1+gear_stats_[2];pickup_radius+=gear_stats_[3];cooldown_multiplier/=1+gear_stats_[5];const float gear_crit=crit_chance+gear_stats_[6];crit_chance=std::min(.85f,gear_crit);crit_multiplier+=gear_stats_[7]+gear_stats_[5]*.2f+std::max(0.f,gear_crit-.85f);area_multiplier+=gear_stats_[8];duration_multiplier+=gear_stats_[9];attack_speed+=gear_stats_[11];
}
std::string Game::passive_value(UpgradeKind k)const{
 std::ostringstream o;o<<std::fixed<<std::setprecision(0);switch(k){
 case UpgradeKind::Damage:o<<"伤害倍率 "<<std::setprecision(2)<<damage_multiplier<<"×";break;
 case UpgradeKind::AttackSpeed:o<<"攻击频率 "<<std::setprecision(2)<<attack_speed<<"×";break;
 case UpgradeKind::Area:o<<"范围 "<<std::setprecision(2)<<area_multiplier<<"×";break;
 case UpgradeKind::Projectiles:o<<"额外投射物 +"<<extra_projectiles;break;
 case UpgradeKind::MoveSpeed:o<<"移速 "<<player.speed;break;
 case UpgradeKind::Health:o<<"生命上限 "<<player.max_hp;break;
 case UpgradeKind::Magnet:o<<"拾取距离 "<<pickup_radius;break;
 case UpgradeKind::Cooldown:o<<"冷却时间 "<<cooldown_multiplier*100<<"%";break;
 case UpgradeKind::Duration:o<<"持续时间 "<<duration_multiplier*100<<"%";break;
 case UpgradeKind::CritChance:o<<"暴击率 "<<crit_chance*100<<"%";break;
 case UpgradeKind::CritDamage:o<<"暴击伤害 "<<crit_multiplier*100<<"%";break;
 default:break;}return o.str();
}
void Game::spawn(EnemyKind k,bool event_target){
 // Keep room for the rush's required targets; ordinary waves and summons must not take it.
 size_t reserved=reviving_enemies_.size()+(!event_target&&k!=EnemyKind::Boss&&special_event.kind==SpecialEventKind::MushroomRush?size_t(std::max(0,special_event.target-special_event.spawned)):0);
 if(k!=EnemyKind::Boss&&(enemies.size()+1>=enemy_capacity()||reserved>=enemy_capacity()-enemies.size()))return;k=budget_enemy_role(k,event_target);if(int(k)<0)return;float a=random(0,2*pi);Enemy e;e.id=next_id_++;e.kind=k;e.pos=player.pos+Vec2{std::cos(a),std::sin(a)}*random(config.spawn_radius,config.spawn_radius+100);
 switch(k){
 case EnemyKind::Slime:e.max_hp=12;e.speed=38;e.radius=16;e.damage=7;break;
 case EnemyKind::Mushroom:e.max_hp=22;e.speed=48;e.radius=18;e.damage=9;break;
 case EnemyKind::BlueMushroom:e.max_hp=65;e.speed=29;e.radius=22;e.damage=12;break;
 case EnemyKind::Pig:e.max_hp=18;e.speed=85;e.radius=15;e.damage=8;e.attack_clock=1;break;
 case EnemyKind::Elite:e.max_hp=180;e.speed=46;e.radius=30;e.damage=18;e.attack_clock=1;break;
 case EnemyKind::Boss:e.max_hp=9000;e.speed=32;e.radius=50;e.damage=26;e.attack_clock=2;break;
 case EnemyKind::Ranger:e.max_hp=28;e.speed=40;e.radius=17;e.damage=11;e.attack_clock=random(0,2);break;
 case EnemyKind::Splitter:e.max_hp=45;e.speed=43;e.radius=21;e.damage=10;break;
 case EnemyKind::Support:e.max_hp=42;e.speed=32;e.radius=20;e.damage=8;e.attack_clock=2;break;
 case EnemyKind::GroundController:e.max_hp=32;e.speed=30;e.radius=20;e.damage=9;e.attack_clock=1;break;
 case EnemyKind::WindupCharger:e.max_hp=40;e.speed=43;e.radius=20;e.damage=12;e.attack_clock=1;break;
 case EnemyKind::Minion:e.max_hp=7;e.speed=105;e.radius=10;e.damage=5;break;
 }
 if(k==EnemyKind::Boss){
  if(config.enable_director)e.max_hp*=std::clamp(config.director_boss_health_scale,.1f,2.f);
  e.boss_profile=next_boss_profile();if(int(e.boss_profile)<0){if(notice_remaining<=0)notify("当前资源池没有可用首领，未生成替代素材。");return;}e.boss_stage=1;e.attack_cooldown=2.6f;
  const auto& definition=boss_definition(e.boss_profile);e.radius=definition.radius;e.speed=definition.speed;e.max_hp*=definition.health_scale;
  int profile=int(e.boss_profile);if(profile>=0&&profile<32){encounter_plan.used_boss_mask|=1u<<profile;metrics.content.bosses_seen|=1u<<profile;++metrics.content.boss_encounters[profile];}
 }else{
  e.visual=select_enemy_visual(k,event_target);if(e.visual<0){if(notice_remaining<=0)notify("当前怪物资源池没有可用外观。");return;}if(k!=EnemyKind::Minion&&e.visual<64)metrics.content.enemies_seen|=uint64_t(1)<<e.visual;e.radius=k==EnemyKind::Minion?10.f:enemy_visual_radius(e.visual);
  e.pos=safe_enemy_position(e.pos,e.radius,std::max(180.f,config.spawn_radius));
  const bool first=(metrics.enemy_seen_mask&(1u<<int(k)))==0;
  if(first){const char* hint=k==EnemyKind::GroundController?"地面控制：预警圈出现后离开落点。":k==EnemyKind::WindupCharger?"蓄势冲锋：侧移避开整条冲锋线，收招时反击。":k==EnemyKind::Pig?"突进敌人：红线锁定后侧移躲开。":k==EnemyKind::Ranger?"远程敌人：射线预告后发射，障碍物可以挡弹。":k==EnemyKind::Splitter?"分裂敌人：击倒后会留下两只小型追兵。":k==EnemyKind::Support?"支援敌人：附近同伴获得短时加速，优先击败。":k==EnemyKind::BlueMushroom?"重装敌人：生命较高，绕行或集中火力。":k==EnemyKind::Elite?"精英敌人：轮流冲锋、召唤与护盾，留意预告。":nullptr;if(hint)notify(hint);}
 }
 float chapter_pressure=run_mode_==RunMode::Campaign?float(chapter_index_):0.f;float growth_time=run_mode_==RunMode::Endless?std::min(elapsed,3600.0):elapsed;float endless_pressure=run_mode_==RunMode::Endless?metrics.endless_cycle*.5f:0.f;
 e.max_hp*=(1+growth_time/160.f+chapter_pressure*4.f+endless_pressure)*(difficulty_==Difficulty::Hard?1.25f:difficulty_==Difficulty::Nightmare?1.55f:1.f);e.hp=e.max_hp;e.damage*=(1+growth_time/750.f+chapter_pressure*.22f+endless_pressure*.2f)*(difficulty_==Difficulty::Hard?1.15f:difficulty_==Difficulty::Nightmare?1.3f:1.f);e.speed*=(1+chapter_pressure*.06f)*(difficulty_==Difficulty::Hard?1.05f:difficulty_==Difficulty::Nightmare?1.1f:1.f)*(run_mode_==RunMode::Daily&&daily_variant_==0?1.15f:1.f);assign_enemy_abilities(e);apply_dark_tide(e);enemies.push_back(e);int kind=int(k);if(kind>=0&&kind<enemy_kind_count)metrics.enemy_seen_mask|=1u<<kind;
}
void Game::rebuild_grid(){grid_.clear();for(size_t i=0;i<enemies.size();++i)if(enemies[i].hp>0)grid_[cell(int(std::floor(enemies[i].pos.x/96)),int(std::floor(enemies[i].pos.y/96)))].push_back(i);}
std::vector<size_t> Game::nearby(Vec2 p,float r,size_t limit){
 std::vector<size_t> result;const int x0=int(std::floor((p.x-r)/96)),x1=int(std::floor((p.x+r)/96)),y0=int(std::floor((p.y-r)/96)),y1=int(std::floor((p.y+r)/96));
 auto collect=[&](int x,int y){if(x<x0||x>x1||y<y0||y>y1)return false;auto it=grid_.find(cell(x,y));if(it==grid_.end())return false;for(size_t n:it->second){++broadphase_candidates;if(n<enemies.size()&&enemies[n].hp>0&&sq(enemies[n].pos-p)<=r*r){result.push_back(n);if(limit&&result.size()>=limit)return true;}}return false;};
 // Damage queries keep their full, stable traversal. Movement only needs a
 // small local sample, so begin in the occupied cell and stop when it is full.
 if(!limit){for(int x=x0;x<=x1;++x)for(int y=y0;y<=y1;++y)collect(x,y);return result;}
 result.reserve(limit);const int cx=int(std::floor(p.x/96)),cy=int(std::floor(p.y/96));if(collect(cx,cy))return result;
 const int rings=std::max({cx-x0,x1-cx,cy-y0,y1-cy});
 for(int ring=1;ring<=rings;++ring){
  for(int x=cx-ring;x<=cx+ring;++x)if(collect(x,cy-ring)||collect(x,cy+ring))return result;
  for(int y=cy-ring+1;y<cy+ring;++y)if(collect(cx-ring,y)||collect(cx+ring,y))return result;
 }
 return result;
}
size_t Game::nearest(Vec2 p,float r){size_t best=none;float score=1e9f;for(size_t n:nearby(p,r)){const auto& enemy=enemies[n];if(!enemy_targetable(enemy))continue;bool marked=enemy.mark_remaining>0&&skill_status(enemy.mark_source).usable();float candidate=length(enemy.pos-p)-(marked?100.f:0.f);if(candidate<score){score=candidate;best=n;}}return best;}
void Game::hurt_enemy(size_t i,float dmg,Vec2 dir,float knockback,Skill source,bool can_crit,bool can_proc,int resonance_id){
 if(i>=enemies.size()||enemies[i].hp<=0||!std::isfinite(dmg)||dmg<=0)return;auto& e=enemies[i];if(e.kind==EnemyKind::Boss){if(e.boss_exposure>0&&e.windup<=0&&e.action_remaining<=0)dmg*=1.3f;else if((e.boss_profile==BossProfile::SnailKing||e.boss_profile==BossProfile::Golem||e.boss_profile==BossProfile::GiantCrab)&&e.windup>0){const Vec2 incoming=normalized(dir)*(-1);if(incoming.x*e.attack_direction.x+incoming.y*e.attack_direction.y>.3f)dmg*=.65f;}}bool crit=can_crit&&random(0,1)<crit_chance;if(crit)dmg*=crit_multiplier;dmg=resolve_ability_damage(e,dmg);
 float actual=std::min(e.hp,dmg);e.hp=std::max(0.f,e.hp-dmg);if(e.hp<=0&&(e.kind==EnemyKind::Boss||e.boss_owner)){for(auto& attack:effects)if(attack.hostile&&(attack.target_id==e.id||(e.kind==EnemyKind::Boss&&e.boss_attack_serial&&attack.root_attack_id==e.boss_attack_serial))&&attack.kind==EffectKind::Warning&&!attack.triggered){attack.age=attack.duration;attack.damage=attack.energy=0;attack.triggered=true;}}if(resonance_id>=0)metrics.resonance.damage[resonance_id]+=actual;else if(si(source)>=0&&si(source)<skill_count)metrics.skill_damage[si(source)]+=actual;else metrics.other_damage+=actual;
 e.hit_flash=.12f;displace_enemy(e,normalized(dir)*95.f*knockback,true);if(actual<=0)return;
 emit_skill_visual(FxStage::Impact,source,e.pos,e.pos+dir,e.radius,0,crit||e.kind==EnemyKind::Boss||e.kind==EnemyKind::Elite);
 // Coalesce rapid hits for display only; damage, source attribution and proc events stay per hit.
 auto number=std::find_if(damage_numbers.begin(),damage_numbers.end(),[&](const DamageNumber& n){return n.target_id==e.id&&n.age<.12f;});
 if(number!=damage_numbers.end()){number->value+=int(std::ceil(actual));number->critical=number->critical||crit;}
 else if(damage_numbers.size()<config.max_damage_numbers)damage_numbers.push_back({e.pos,int(std::ceil(actual)),0,crit,e.id});
 if(actual>0&&can_proc&&(source==Skill::Cleave||source==Skill::BladeWave)){e.armor_break_remaining=3;e.armor_source=source;}
 if(actual>0){++combat_diagnostics.actual_hits;if(crit)++combat_diagnostics.critical_hits;if(events.size()<64)events.push_back({EventKind::Hit,e.pos,actual,crit,source,e.id,e.kind,!can_proc,current_attack_id_,resonance_id});}if(can_proc){gear_hit(i,actual,source);if(actual>0&&skill_attack_available(Skill::DragonArmor,player.pos)&&source!=Skill::DragonArmor&&soul_hit_cooldown_<=0){dragon_soul=std::min(100.f,dragon_soul+(fusion_form(Skill::DragonArmor)==0?2.f:10.f));soul_hit_cooldown_=.25f;}if(actual>0){on_advanced_hit(source);on_adventure_hit(i,actual,source);}on_enemy_hit(i,source,dir);resonance_hit(i,actual,source);}
}
void Game::hurt_player(float damage,bool from_boss,bool from_environment,int boss_attack,const char* source){
 if(player.hp<=0||player.invulnerable>0||damage<=0)return;
 const std::string cause=source?std::string(source)+(boss_attack>=200?std::string(" · ")+boss_attack_name(boss_attack):""):from_boss?"首领攻击":from_environment?"地图危险":"敌人接触/弹道";
 damage/=1+gear_stats_[10];if(gear_shield_seconds_>0&&!equipment_suppressed()&&!buffs_suppressed()){record_combat(CombatRecordKind::Shield,damage,cause,boss_attack);return;}
 if(shield_remaining>0&&!buff_suppressed_){record_combat(CombatRecordKind::Shield,damage,cause,boss_attack);on_shield_absorb();return;}
 if(effective_skill_level(Skill::DragonArmor)>0)damage*=.9f;
 if(skill_allowed(Skill::Retaliate)&&effective_skill_level(Skill::Retaliate)>0)damage*=.85f;
 if(equipped(EquipmentId::LeatherVest)&&equipment_cooldowns_[int(EquipmentId::LeatherVest)]<=0){damage*=.7f;equipment_cooldowns_[int(EquipmentId::LeatherVest)]=8;++metrics.equipment_procs[int(EquipmentId::LeatherVest)];}
 if(!buff_suppressed_&&guard_charge>0&&(effective_skill_level(Skill::Bulwark)>0||effective_skill_level(Skill::FrostArmor)>0)){float absorbed=std::min(guard_charge,damage);guard_charge-=absorbed;damage-=absorbed;if(absorbed>0){record_combat(CombatRecordKind::Shield,absorbed,cause,boss_attack);resonance_absorb(absorbed,effective_skill_level(Skill::Bulwark)>0?Skill::Bulwark:Skill::FrostArmor);on_shield_absorb();player.invulnerable=config.hit_invulnerability;}if(damage<=0)return;}
 {float before=damage;damage=resonance_take_shield(damage);if(before>damage)record_combat(CombatRecordKind::Shield,before-damage,cause,boss_attack);}if(damage<=0)return;
 float actual=std::min(player.hp,damage);player.hp=std::max(0.f,player.hp-damage);metrics.damage_taken+=actual;if(pressure_event.kind!=PressureKind::None&&!pressure_metrics.sequence.empty())pressure_metrics.sequence.back().damage_taken+=actual;if(actual>0){pressure_metrics.death_cause=cause;record_combat(CombatRecordKind::Damage,actual,cause,boss_attack);}if(from_boss)metrics.boss_damage_taken+=actual;if(from_environment&&actual>0){++metrics.environment_hits;metrics.environment_damage_taken+=actual;}
 player.invulnerable=config.hit_invulnerability;retaliation_pending_=true;if(events.size()<64)events.push_back({EventKind::Hurt,player.pos,actual});
 if(player.hp<=0&&death_protections_remaining>0){
  --death_protections_remaining;++metrics.death_protections_used;player.hp=std::max(1.f,player.max_hp*.35f*healing_multiplier());
  player.invulnerable=std::max(player.invulnerable,3.f);shield_remaining=std::max(shield_remaining,3.f);
  // hurt_player can run while effects/enemies are being iterated: invalidate danger in place.
  for(auto& effect:effects)if(effect.hostile){effect.damage=0;effect.triggered=true;effect.age=effect.duration;}
  for(auto& enemy:enemies){enemy.windup=enemy.action_remaining=enemy.attack_clock=0;enemy.velocity={};
   Vec2 away=enemy.pos-player.pos;if(length(away)<180+enemy.radius){if(length(away)<.001f)away={1,0};displace_enemy(enemy,player.pos+normalized(away)*(180+enemy.radius)-enemy.pos);}}
  notify("守护天赋触发：免于死亡，恢复35%生命并获得3秒护盾！本局已用尽。");
 }
 on_player_hit();
}
void Game::cast(Skill skill){
 if(!cast_has_conditions(skill)||!try_skill_cast(skill))return;current_attack_id_=next_attack_id_++;
 int level=effective_skill_level(skill);bool evo=evolved(skill);float dmg=skill_power(skill);
 float range=420*area_multiplier;size_t target=resonance_target(skill,player.pos,range);const DangerZone* altar=altar_target(skill,player.pos,range,target);Vec2 point=altar?altar->pos:target==none?player.pos:enemies[target].pos;Vec2 dir=target==none&&!altar?Vec2{player.facing_left?-1.f:1.f,0}:normalized(point-player.pos);
 cooldown_[si(skill)]=skill_interval(skill);
 if(skill>=Skill::TripleStar){if(!cast_adventure(skill)){cooldown_[si(skill)]=.12f;return;}cooldown_period_[si(skill)]=cooldown_[si(skill)];on_skill_cast(skill,dmg,dir);add_event(EventKind::Shot,player.pos,skill);return;}
 if(skill>=Skill::ThunderPrison){if(!cast_cross_fusion(skill)){cooldown_[si(skill)]=.1f;return;}cooldown_period_[si(skill)]=cooldown_[si(skill)];on_skill_cast(skill,dmg,dir);add_event(EventKind::Shot,player.pos,skill);return;}
 if(skill>=Skill::BattleFocus){if(!cast_advanced(skill)){cooldown_[si(skill)]=.1f;return;}cooldown_period_[si(skill)]=cooldown_[si(skill)];on_skill_cast(skill,dmg,dir);add_event(EventKind::Shot,player.pos,skill);return;}
 if(is_fusion(skill)){if(!cast_fusion(skill)){cooldown_[si(skill)]=.05f;return;}cooldown_period_[si(skill)]=cooldown_[si(skill)];on_skill_cast(skill,dmg,dir);add_event(EventKind::Shot,player.pos,skill);return;}
 if(skill>=Skill::HunterMark&&skill<=Skill::Bulwark){if(!cast_expansion(skill)){cooldown_[si(skill)]=.1f;return;}cooldown_period_[si(skill)]=cooldown_[si(skill)];on_skill_cast(skill,dmg,dir);add_event(EventKind::Shot,player.pos,skill);return;}
 if(skill==Skill::Retaliate&&!retaliation_pending_){cooldown_[si(skill)]=.05f;return;}
 auto projectile=[&](Vec2 origin,Vec2 direction,float damage,float speed,int penetration,int bounces=0){if(projectiles.size()>=config.max_projectiles)return;Projectile p;p.pos=origin;p.velocity=direction*speed;p.skill=skill;p.damage=damage;p.target=altar||target==none?0:enemies[target].id;p.altar_target_id=altar?altar->id:0;p.pierce=penetration;p.bounces=bounces;p.evolved=evo;p.radius=skill==Skill::BladeWave?19.f:skill==Skill::Pierce?10.f:7.f;add_projectile(p);};
 auto field=[&](EffectKind kind,Vec2 pos,float radius,float duration,float damage,float delay=0){Effect e;e.kind=kind;e.pos=pos;e.radius=radius;e.duration=duration;e.damage=damage;e.delay=delay;e.source=skill;e.evolved=evo;add_effect(e);};
 switch(skill){
 case Skill::Homing:case Skill::Arrow:case Skill::Pierce:case Skill::Spread:case Skill::Ricochet:case Skill::BladeWave:{
  if(target==none&&!altar){cooldown_[si(skill)]=.1f;return;}
  int count=1+(level-1)/2+extra_projectiles;if(skill==Skill::Spread)count+=2;
  if(skill==Skill::Ricochet)count=1+extra_projectiles/2;if(skill==Skill::BladeWave)count=1+extra_projectiles/3+(evo?2:0);
  float spread=skill==Skill::Spread?.24f:skill==Skill::BladeWave?.38f:.14f;
  for(int j=0;j<count;++j){float a=std::atan2(dir.y,dir.x)+(j-(count-1)*.5f)*spread;Vec2 d{std::cos(a),std::sin(a)};int pierce=skill==Skill::Pierce?5+level:skill==Skill::BladeWave?8:skill==Skill::Arrow?2:1;if(skill==Skill::Arrow&&equipped(EquipmentId::ReedBow))++pierce;
   projectile(player.pos,d,dmg*(skill==Skill::BladeWave?2.2f:skill==Skill::Spread?.8f:1.f),skill==Skill::Homing?330.f:430.f,pierce,skill==Skill::Ricochet?(2+level)*(evo?2:1):0);
   if(skill==Skill::Spread&&evo)projectile(player.pos,d*(-1),dmg*.8f,430,1);
  }

  break;}
 case Skill::Nova:{float radius=(95+level*10)*area_multiplier*1.15f;field(EffectKind::Nova,player.pos,radius,1.44f,0);
  damage_scene_area(player.pos,radius,dmg*2.8f,skill);
  for(size_t n:nearby(player.pos,radius+50))if(length(enemies[n].pos-player.pos)<radius+enemies[n].radius)hurt_enemy(n,dmg*2.8f,enemies[n].pos-player.pos,1.8f,skill);
  if(evo){shield_remaining=std::max(shield_remaining,.65f*duration_multiplier);field(EffectKind::Rune,player.pos,radius*1.25f,1.0f,dmg*1.8f,.65f);}break;}
 case Skill::Lightning:{if(target==none&&!altar)return;Vec2 from=player.pos;std::vector<uint32_t> visited;
  if(altar){Effect fx;fx.kind=EffectKind::Lightning;fx.pos=from;fx.end=point;fx.radius=4;fx.duration=.25f;fx.source=skill;fx.evolved=evo;add_effect(fx);damage_pressure_facilities(point,0,dmg*1.7f);from=point;range=180*area_multiplier;}
  for(int j=altar?1:0;j<4+level+(evo?3:0);++j){size_t next=none;float best=range*range;for(size_t n:nearby(from,range)){auto& e=enemies[n];if(std::find(visited.begin(),visited.end(),e.id)!=visited.end())continue;float d=sq(e.pos-from)*(e.frost_remaining>0?.55f:1.f);if(d<best){best=d;next=n;}}
   if(next==none)break;Vec2 pos=enemies[next].pos;Effect fx;fx.kind=EffectKind::Lightning;fx.pos=from;fx.end=pos;fx.radius=4;fx.duration=.25f;fx.source=skill;fx.evolved=evo;add_effect(fx);visited.push_back(enemies[next].id);hurt_enemy(next,dmg*1.7f,pos-from,1,skill);

   if(evo)field(EffectKind::Rune,pos,42*area_multiplier,.9f,dmg*.65f,.5f);from=pos;range=180*area_multiplier;
  }break;}
 case Skill::Zone:field(EffectKind::Zone,point,(70+level*8)*area_multiplier*(evo?1.2f:1.f),(evo?6.f:3.5f)*duration_multiplier,dmg*.55f);break;
 case Skill::Summon:{float a=combat_elapsed*1.8f;Vec2 p=player.pos+Vec2{std::cos(a)*80,std::sin(a)*60};Effect bird;bird.kind=EffectKind::Summon;bird.source=skill;bird.pos=p;bird.radius=30;bird.duration=1;if(target!=none||altar){bird.visual_attack_age=0;bird.visual_aim=point-p;bird.end=point;Vec2 muzzle=battle_muzzle(skill,p,bird.visual_aim);projectile(muzzle,normalized(point-muzzle),dmg*1.8f,380,evo?4:2);}add_effect(bird);break;}
 case Skill::Cleave:{bool blade=equipped(EquipmentId::IronBlade);if(blade)++metrics.equipment_procs[int(EquipmentId::IronBlade)];float radius=(100+level*10)*area_multiplier;Effect e;e.kind=EffectKind::Cleave;e.pos=player.pos;e.end=player.pos+dir*radius;e.radius=radius;e.duration=.24f;e.source=skill;e.evolved=evo;e.angle=config.cleave_half_angle*(evo?1.5f:1.f)*(blade?1.3f:1.f);add_effect(e);
  if(config.enable_scene_objects)for(size_t index=0;index<scene_objects.size();++index){Vec2 delta=scene_objects[index].pos-player.pos;Vec2 axis=normalized(delta);if(length(delta)<radius+scene_objects[index].radius&&axis.x*dir.x+axis.y*dir.y>std::cos(config.cleave_half_angle*(evo?1.5f:1.f)*(blade?1.3f:1.f)))hurt_scene_object(index,dmg*2,skill);}
  for(const auto& zone:danger_zones)if(zone.kind==DangerKind::Altar&&zone.hp>0){Vec2 delta=zone.pos-player.pos,axis=normalized(delta);if(length(delta)<radius+24&&axis.x*dir.x+axis.y*dir.y>std::cos(e.angle))damage_pressure_facilities(zone.pos,0,dmg*2.f);}
  for(auto n:nearby(player.pos,radius+50)){Vec2 delta=enemies[n].pos-player.pos;Vec2 d=normalized(delta);if(length(delta)<radius+enemies[n].radius&&d.x*dir.x+d.y*dir.y>std::cos(config.cleave_half_angle*(evo?1.5f:1.f)*(blade?1.3f:1.f)))hurt_enemy(n,dmg*2.f,delta,(evo?2.5f:1.5f)*(blade?1.4f:1.f),skill);}break;}
 case Skill::Orbit:{effects.erase(std::remove_if(effects.begin(),effects.end(),[](const Effect&e){return e.kind==EffectKind::Orbit;}),effects.end());int blades=std::min(8,1+(level>=3?1:0)+extra_projectiles+(evo?1:0));for(int i=0;i<blades;++i){Effect e;e.kind=EffectKind::Orbit;e.pos=player.pos;e.radius=(65+level*4)*area_multiplier;e.duration=cooldown_[si(skill)]+.1f;e.damage=dmg*1.25f;e.angle=2*pi*i/blades;e.source=skill;e.evolved=evo;add_effect(e);}break;}
 case Skill::Rain:if(target==none&&!altar)return;field(EffectKind::Rain,altar?point:resonance_landing(skill,point),(90+level*8)*area_multiplier,(evo?5.f:2.5f)*duration_multiplier,dmg*.55f,.2f);break;
 case Skill::Frost:{float radius=(120+level*9)*area_multiplier;field(EffectKind::Frost,player.pos,radius,1.045f,0);damage_scene_area(player.pos,radius,dmg*1.65f,skill);for(auto n:nearby(player.pos,radius+50)){auto&e=enemies[n];if(length(e.pos-player.pos)>radius+e.radius)continue;hurt_enemy(n,dmg*1.65f,e.pos-player.pos,.2f,skill);apply_frost(e,(evo?3.f:2.f)*duration_multiplier,skill,2);if(control_immune(e))continue;e.slow_factor=e.kind==EnemyKind::Boss?.8f:evo?.08f:.4f;if(e.kind!=EnemyKind::Boss)e.action_remaining=0;}break;}
 case Skill::Meteor:{if(target==none&&!altar)return;if(!altar){for(auto n:nearby(player.pos,range))if(enemies[n].max_hp>enemies[target].max_hp)target=n;point=resonance_landing(skill,enemies[target].pos);}field(EffectKind::Meteor,point,(110+level*10)*area_multiplier,1.93f,dmg*5.f,.85f);if(evo)field(EffectKind::Meteor,point,(130+level*10)*area_multiplier,2.63f,dmg*3.5f,1.55f);break;}
 case Skill::Rune:field(EffectKind::Rune,player.pos,(95+level*7)*area_multiplier,4.f*duration_multiplier,dmg*1.5f,.6f);break;
 case Skill::Charge:return; // Damage is emitted only by the actual player-dash path.

 case Skill::Retaliate:{retaliation_pending_=false;float radius=(110+level*10)*area_multiplier;field(EffectKind::Retaliate,player.pos,radius,1.2f,0);damage_scene_area(player.pos,radius,dmg*2.8f,skill);int hits=0;for(auto n:nearby(player.pos,radius+50))if(length(enemies[n].pos-player.pos)<radius+enemies[n].radius){float hp_before=enemies[n].hp;hurt_enemy(n,dmg*2.8f,enemies[n].pos-player.pos,2.f,skill);if(enemies[n].hp<hp_before)++hits;}if(evo){heal_player(std::min(12.f,hits*2.f));shield_remaining=std::max(shield_remaining,.6f*duration_multiplier);}break;}
 default:break;
 }
 cooldown_period_[si(skill)]=cooldown_[si(skill)];
 if(skill==Skill::Arrow&&equipped(EquipmentId::ReedBow))++metrics.equipment_procs[int(EquipmentId::ReedBow)];on_skill_cast(skill,dmg,dir);add_event(EventKind::Shot,player.pos,skill);
}
void Game::update_enemies(float dt){
 // Spawns and the previous cleanup can change indices. Navigation needs the
 // current population; the later rebuild still serves post-movement combat.
 rebuild_grid();enemy_budget_boss_present_=false;active_enemy_attacks_.clear();
 for(size_t i=0;i<enemies.size();++i)if(enemies[i].hp>0){enemy_budget_boss_present_|=enemies[i].kind==EnemyKind::Boss;if(enemies[i].windup>0||enemies[i].action_remaining>0)active_enemy_attacks_.push_back(i);}
 std::vector<SummonRequest> summons;
 for(size_t i=0;i<enemies.size();++i){auto& e=enemies[i];e.mark_remaining=std::max(0.f,e.mark_remaining-dt);e.frost_remaining=std::max(0.f,e.frost_remaining-dt);e.armor_break_remaining=std::max(0.f,e.armor_break_remaining-dt);update_skill_status(i,dt);if(e.hp<=0)continue;int seen=int(e.kind);if(seen>=0&&seen<enemy_kind_count)metrics.enemy_seen_mask|=1u<<seen;
  e.age+=dt;e.hit_flash=std::max(0.f,e.hit_flash-dt);e.slow_remaining=std::max(0.f,e.slow_remaining-dt);e.shield_remaining=std::max(0.f,e.shield_remaining-dt);e.buff_remaining=std::max(0.f,e.buff_remaining-dt);
  if(e.burn_source!=Skill::Count&&!skill_allowed(e.burn_source))e.burn_remaining=0;
  if(e.burn_remaining>0){e.burn_remaining-=dt;e.burn_tick-=dt;if(e.burn_tick<=0){e.burn_tick=.5f;if(!skill_sealed(e.burn_source)&&effective_skill_level(e.burn_source)>0)hurt_enemy(i,e.burn_damage,{},0,e.burn_source,false,false);if(e.hp<=0)continue;}}
  if(e.boss_owner){update_boss_node(e,dt);continue;}
  if(update_enemy_abilities(e,dt))continue;
  Vec2 chase=e.lure_remaining>0?e.lure_point:player.pos;Vec2 dir=normalized(chase-e.pos);float dist=length(player.pos-e.pos);Vec2 facing=e.windup>0||e.action_remaining>0?e.attack_direction:dir;if(std::abs(facing.x)>.05f)e.facing_left=facing.x<0;
  if(config.enable_boss_phases&&e.kind==EnemyKind::Boss){update_phased_boss(e,dt,summons);continue;}
  if(update_extra_enemy(e,dt))continue;
  if(e.windup>0||e.action_remaining>0||e.recovery_remaining>0)e.attack_animation_time+=dt;
  e.recovery_remaining=std::max(0.f,e.recovery_remaining-dt);bool fixed_position=false;
  float speed=e.speed*scene_slow(e.pos,e.radius)*(e.slow_remaining>0?e.slow_factor:1.f)*(e.buff_remaining>0?1.3f:1.f)*(rift_remaining>0?1.25f:1.f);
  if(e.kind==EnemyKind::Ranger&&enemy_attack_visible(e)){if(dist<150)dir=dir*(-1);else if(dist<250)dir={};}
  if(e.kind!=EnemyKind::Boss)dir=cooperative_enemy_direction(e,dir);
  if(e.windup>0){e.windup=std::max(0.f,e.windup-dt);speed=0;fixed_position=true;
   if(e.windup<=0){
    e.recovery_remaining=.4f;
    if(e.kind==EnemyKind::Ranger){Effect shot;shot.kind=EffectKind::EnemyShot;shot.pos=e.attack_start;shot.velocity=normalized(e.attack_target-e.attack_start)*170;shot.radius=9;shot.duration=4;shot.damage=e.damage;shot.hostile=true;shot.target_id=e.id;shot.enemy_kind=e.kind;shot.enemy_visual=e.visual;add_effect(shot);}
    else if(e.kind==EnemyKind::Boss){int pattern=boss_pattern(e);++e.phase;if(pattern==1){e.action_remaining=.9f;}else if(pattern==2){for(int j=0;j<10;++j){float a=j*2*pi/10+.18f*e.phase;Effect shot;shot.kind=EffectKind::EnemyShot;shot.pos=e.pos;shot.velocity={std::cos(a)*150,std::sin(a)*150};shot.radius=10;shot.duration=5;shot.damage=e.damage*.8f;shot.hostile=true;shot.target_id=e.id;shot.enemy_kind=e.kind;add_effect(shot);}}else if(pattern==4){e.action_remaining=.45f;for(int j=0;j<4;++j){float angle=j*pi*.5f;Effect shot;shot.kind=EffectKind::EnemyShot;shot.pos=e.pos;shot.velocity={std::cos(angle)*175,std::sin(angle)*175};shot.radius=9;shot.duration=3;shot.damage=e.damage*.6f;shot.hostile=true;shot.target_id=e.id;shot.enemy_kind=e.kind;add_effect(shot);}}}
    else if(e.kind==EnemyKind::Elite){int pattern=e.phase++%3;if(pattern==0)e.action_remaining=e.action_duration;else if(pattern==1){for(int j=0;j<3;++j)summons.push_back({e.pos+Vec2{float((j-1)*35),25},false,e.visual});}else e.shield_remaining=2.3f;}
    else if(e.kind==EnemyKind::Pig)e.action_remaining=e.action_duration;
   }
  }else if(e.action_remaining<=0&&e.recovery_remaining<=0){
   e.attack_clock+=dt;float interval=e.kind==EnemyKind::Boss?boss_interval():e.kind==EnemyKind::Elite?4.2f:e.kind==EnemyKind::Ranger?3.2f:e.kind==EnemyKind::Support?4.f:e.kind==EnemyKind::Pig?3.8f:1e9f;if(e.kind!=EnemyKind::Boss)interval*=enemy_attack_multiplier();
   float attack_range=e.kind==EnemyKind::Ranger?360.f:e.kind==EnemyKind::Pig?320.f:e.kind==EnemyKind::Boss?560.f:340.f;
   if(e.attack_clock>=interval&&dist<attack_range&&enemy_attack_visible(e)&&effects.size()<config.max_effects&&((e.kind!=EnemyKind::Ranger&&e.kind!=EnemyKind::Pig&&e.kind!=EnemyKind::Elite)||can_start_enemy_attack(e))){e.attack_clock=0;e.attack_target=player.pos;e.attack_direction=normalized(player.pos-e.pos);e.attack_start=e.pos;e.attack_animation_time=0;
    if(e.kind==EnemyKind::Support){for(auto& ally:enemies)if(ally.hp>0&&!ally.boss_owner&&ally.kind!=EnemyKind::Boss&&length(ally.pos-e.pos)<170)ally.buff_remaining=2.5f;Effect pulse;pulse.kind=EffectKind::Warning;pulse.pos=e.pos;pulse.radius=170;pulse.duration=.6f;pulse.enemy_kind=e.kind;add_effect(pulse);}
    else{e.windup=e.kind==EnemyKind::Boss?(difficulty_==Difficulty::Hard?.95f:difficulty_==Difficulty::Nightmare?.8f:1.1f):(difficulty_==Difficulty::Hard?.6f:difficulty_==Difficulty::Nightmare?.55f:.7f);Effect warn;warn.kind=EffectKind::Warning;warn.pos=e.pos;warn.radius=e.radius+15;warn.duration=e.windup+.18f;warn.delay=e.windup;warn.hostile=true;warn.target_id=e.id;warn.enemy_kind=e.kind;warn.enemy_visual=e.visual;fixed_position=true;speed=0;
     if(e.kind==EnemyKind::Boss){++metrics.boss_attacks;int pattern=boss_pattern(e);if(pattern==0){warn.pos=e.attack_target;warn.radius=115;warn.damage=e.damage*1.25f;}else if(pattern==1){warn.end=e.pos+e.attack_direction*340;warn.radius=32;warn.pattern=1;}else if(pattern==2){warn.radius=80;warn.pattern=2;}
      else if(pattern==3){warn.pos=e.attack_target+Vec2{-65,0};warn.radius=72;warn.damage=e.damage*.8f;Effect second=warn;second.pos=e.attack_target+Vec2{65,0};second.delay+=.3f;second.duration=second.delay+.2f;add_effect(second);}
      else{warn.pattern=1;warn.radius=23;warn.damage=e.damage*.7f;warn.pos=e.attack_target+Vec2{-160,-160};warn.end=e.attack_target+Vec2{160,160};Effect cross=warn;cross.pos=e.attack_target+Vec2{-160,160};cross.end=e.attack_target+Vec2{160,-160};add_effect(cross);}}
     else if(e.kind==EnemyKind::Pig||(e.kind==EnemyKind::Elite&&e.phase%3==0)){e.action_duration=e.kind==EnemyKind::Pig?.72f:.76f;e.attack_end=enemy_attack_endpoint(e.pos,e.pos+e.attack_direction*230,e.radius);warn.pattern=1;warn.end=e.attack_end;warn.radius=e.radius+8;warn.duration+=e.action_duration;}
     else if(e.kind==EnemyKind::Ranger){warn.pattern=1;warn.end=e.attack_target;warn.radius=8;}
     else warn.radius=75;
     add_effect(warn);
    }
   }
  }
  if(e.action_remaining>0){e.action_remaining=std::max(0.f,e.action_remaining-dt);dir=e.attack_direction;if(e.kind!=EnemyKind::Boss&&e.action_duration>0){fixed_position=true;e.pos=e.attack_start+(e.attack_end-e.attack_start)*(1-e.action_remaining/e.action_duration);}else{fixed_position=false;speed=360.f*scene_slow(e.pos,e.radius)*(e.slow_remaining>0?e.slow_factor:1.f);}}
  if(!fixed_position){if(e.kind!=EnemyKind::Boss&&e.action_remaining<=0)move_ordinary_enemy(e,dir,speed,dt);else e.pos+=dir*(speed*dt)+e.velocity*dt;}e.velocity=e.velocity*std::max(0.f,1-dt*9);
  damage_scene_area(e.pos,e.radius,8*dt,Skill::Count);
  resolve_scene_collision(e.pos,e.radius);
  if(length(e.pos-player.pos)<e.radius+13){hurt_player(e.damage,e.kind==EnemyKind::Boss);if(player.invulnerable>0&&player.hp>0)player.pos+=normalized(player.pos-e.pos)*1.2f;}
 }
 for(auto p:summons){size_t before=enemies.size();spawn(EnemyKind::Minion);if(enemies.size()>before){enemies.back().pos=safe_enemy_position(p.pos,enemies.back().radius,130);if(p.visual>=0)enemies.back().visual=p.visual;if(enemies.back().visual>=0&&enemies.back().visual<64)metrics.content.enemies_seen|=uint64_t(1)<<enemies.back().visual;if(p.from_boss)++combat_diagnostics.boss_summons;}else ++pressure_metrics.capacity_limited;}
}
void Game::update_projectiles(float dt){
 for(auto& p:projectiles){p.visual_trail_clock-=dt;if(p.visual_trail_clock<=0){p.visual_trail_clock=.035f;for(int i=3;i>0;--i)p.visual_trail[i]=p.visual_trail[i-1];p.visual_trail[0]=p.pos;p.visual_trail_count=std::min(4,p.visual_trail_count+1);}current_attack_id_=p.root_attack_id;if(!skill_allowed(p.skill)){p.pierce=0;continue;}float gear_now=(1+gear_stats_[0])*gear_skill_bonus(p.skill);p.damage*=gear_now/p.gear_snapshot;p.power_snapshot*=gear_now/p.gear_snapshot;p.gear_snapshot=gear_now;p.age+=dt;update_adventure_projectile(p,dt);update_advanced_projectile(p,dt);update_cross_projectile(p,dt);if(p.pierce<=0||p.age>=p.lifetime)continue;
  if(((p.evolved&&p.skill==Skill::Pierce)||p.skill==Skill::StarReturn)&&!p.returned&&p.age>.65f){p.velocity=normalized(player.pos-p.pos)*430;p.returned=true;p.hit_ids.clear();p.pierce=std::max(p.pierce,5);p.lifetime=1.5f;}
  if(p.skill==Skill::Homing||p.skill==Skill::OrbitalOrb||p.skill==Skill::HunterVolley||p.skill==Skill::ArcaneRing||p.skill==Skill::StarBow){
   size_t target=none;float best=1e9f;int priority=-1;for(auto candidate:nearby(p.pos,600*area_multiplier)){const auto& enemy=enemies[candidate];if(!enemy_targetable(enemy)||std::find(p.hit_ids.begin(),p.hit_ids.end(),enemy.id)!=p.hit_ids.end())continue;int rank=(enemy.mark_remaining>0&&skill_status(enemy.mark_source).usable()?1:0)+(enemy.id==p.target?2:0);float score=sq(enemy.pos-p.pos);if(rank>priority||(rank==priority&&score<best)){target=candidate;best=score;priority=rank;}}
   float speed=p.skill==Skill::HunterVolley?410.f:p.skill==Skill::OrbitalOrb?350.f:330.f;
   const DangerZone* altar=altar_target(p.skill,p.pos,600*area_multiplier,target,p.altar_target_id);
   if(altar){p.target=0;p.altar_target_id=altar->id;p.velocity=normalized(p.velocity*.62f+normalized(altar->pos-p.pos)*speed*.38f)*speed;}
   else{p.altar_target_id=0;if(target!=none){p.target=enemies[target].id;p.velocity=normalized(p.velocity*.62f+normalized(enemies[target].pos-p.pos)*speed*.38f)*speed;}else p.target=0;}
  }
  Vec2 from=p.pos;p.pos=projectile_motion(p.pos,p.velocity,dt);
  if(scene_projectile_collision(from,p.pos,p.radius,p.damage,p.skill)){p.pierce=0;continue;}
  for(size_t n:nearby(p.pos,p.radius+55+length(p.pos-from))){auto& e=enemies[n];if(segment_distance(e.pos,from,p.pos)>e.radius+p.radius||std::find(p.hit_ids.begin(),p.hit_ids.end(),e.id)!=p.hit_ids.end())continue;
   Vec2 hit_pos=e.pos;hurt_enemy(n,p.damage,p.velocity,p.skill==Skill::BladeWave?2.f:1.f,p.skill,true,!p.proc_generated);p.hit_ids.push_back(e.id);on_adventure_projectile_hit(p,n);on_advanced_projectile_hit(p,n);on_cross_projectile_hit(p,n);
   if(!p.proc_generated&&e.mark_remaining>0&&skill_attack_available(Skill::HunterMark,player.pos)&&synergy_cooldown_[2]<=0&&e.hp>0){synergy_cooldown_[2]=.25f;record_synergy(2);hurt_enemy(n,p.damage*.25f,p.velocity,.2f,Skill::HunterMark,false,false);}
   Effect hit;hit.kind=EffectKind::Hit;hit.pos=p.pos;hit.radius=15;hit.duration=.18f;hit.source=p.skill;add_effect(hit);
   if(p.evolved&&p.skill==Skill::Homing)for(auto near:nearby(hit_pos,55*area_multiplier))if(near!=n)hurt_enemy(near,p.damage*.45f,enemies[near].pos-hit_pos,.2f,p.skill,false,false);
   if(p.evolved&&p.skill==Skill::Summon){Effect fire;fire.kind=EffectKind::Zone;fire.pos=hit_pos;fire.radius=48*area_multiplier;fire.duration=2*duration_multiplier;fire.damage=p.damage*.25f;fire.source=p.skill;fire.evolved=true;fire.proc_generated=true;add_effect(fire);}
   if((p.skill==Skill::Ricochet||p.skill==Skill::StarBow||p.skill==Skill::PrismShot||(p.skill==Skill::StarReturn&&p.returned))&&p.bounces>0){size_t next=none;float best=260*area_multiplier;for(auto q:nearby(hit_pos,best)){if(!enemy_targetable(enemies[q])||std::find(p.hit_ids.begin(),p.hit_ids.end(),enemies[q].id)!=p.hit_ids.end())continue;float d=length(enemies[q].pos-hit_pos);if(p.skill==Skill::PrismShot&&(enemies[q].boss_owner||enemies[q].kind==EnemyKind::Boss||enemies[q].kind==EnemyKind::Elite||enemies[q].kind==EnemyKind::Ranger))d*=.1f;if(d<best){best=d;next=q;}}
    if(next!=none){--p.bounces;p.velocity=normalized(enemies[next].pos-hit_pos)*430;p.target=enemies[next].id;p.damage*=p.evolved?1.f:.92f;p.pos=hit_pos;p.lifetime+=.12f;break;}
   }
   if(--p.pierce<=0)break;
  }
 }
 projectiles.erase(std::remove_if(projectiles.begin(),projectiles.end(),[](const Projectile&p){return p.age>=p.lifetime||p.pierce<=0;}),projectiles.end());
}
void Game::update_effects(float dt){
 for(auto& impact:hostile_impacts)impact.age+=dt;hostile_impacts.erase(std::remove_if(hostile_impacts.begin(),hostile_impacts.end(),[](const HostileImpact& i){return i.age>=.55f;}),hostile_impacts.end());
 std::vector<Effect> spawned;
 for(auto& e:effects){current_attack_id_=e.root_attack_id;e.age+=dt;e.visual_attack_age+=dt;
  if(e.age>=e.duration)continue;
  if(e.hostile&&e.enemy_kind==EnemyKind::Boss&&e.count>=200&&e.kind==EffectKind::Warning&&!e.triggered){
   auto source=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& target){return target.id==e.target_id&&target.hp>0;});
   bool active=source!=enemies.end();if(active&&source->boss_owner)active=std::any_of(enemies.begin(),enemies.end(),[&](const Enemy& boss){return boss.id==source->boss_owner&&boss.hp>0;});
   if(!active){e.age=e.duration;continue;}
  }
  if(e.hostile&&e.kind==EffectKind::Warning&&e.evolved&&!e.triggered&&e.age>=e.delay){e.kind=EffectKind::EnemyShot;e.damage=e.energy;e.duration=e.secondary_tick;e.age=0;e.delay=0;e.evolved=false;e.pattern=e.proc_generated?3:0;}
  if(e.hostile&&e.kind==EffectKind::EnemyShot&&e.pattern==3&&e.age>.25f&&e.age<1.15f){float speed=length(e.velocity);Vec2 heading=normalized(e.velocity),aim=normalized(player.pos-e.pos);float turn=std::atan2(heading.x*aim.y-heading.y*aim.x,heading.x*aim.x+heading.y*aim.y);float angle=std::clamp(turn,-1.05f*dt,1.05f*dt),c=std::cos(angle),sn=std::sin(angle);e.velocity={speed*(heading.x*c-heading.y*sn),speed*(heading.x*sn+heading.y*c)};}

  if(e.hostile){if(e.kind==EffectKind::EnemyShot){Vec2 from=e.pos;e.pos=projectile_motion(e.pos,e.velocity,dt);
    e.visual_trail_clock+=dt;if(e.visual_trail_clock>=.035f){e.visual_trail_clock=0;for(int n=3;n>0;--n)e.visual_trail[n]=e.visual_trail[n-1];e.visual_trail[0]=from;e.visual_trail_count=std::min(4,e.visual_trail_count+1);}
    ImpactResult result=ImpactResult::Expired;bool ended=e.age>=e.duration;
    if(scene_projectile_collision(from,e.pos,e.radius,e.damage,Skill::Count)){ended=true;result=ImpactResult::Wall;}
    else if(segment_distance(player.pos,from,e.pos)<e.radius+12){const float hp=player.hp;const bool immune=player.invulnerable>0;hurt_player(e.damage,e.enemy_kind==EnemyKind::Boss,e.environment,e.count,e.enemy_kind==EnemyKind::Boss?boss_profile_name(e.boss_profile):enemy_visual_name(e.enemy_visual));ended=true;result=player.hp<hp?ImpactResult::Damage:immune?ImpactResult::Immune:ImpactResult::Shield;}
    if(ended){e.age=e.duration;if(hostile_impacts.size()<48)hostile_impacts.push_back({e.pos,e.enemy_kind,e.boss_profile,e.enemy_visual,result,0,e.radius});}
   }
   else if(!e.triggered&&e.age>=e.delay){e.triggered=true;bool hit=hostile_shape_contains(e,player.pos);if(hit)hurt_player(e.damage,e.enemy_kind==EnemyKind::Boss,e.environment,e.count,e.enemy_kind==EnemyKind::Boss?boss_profile_name(e.boss_profile):enemy_visual_name(e.enemy_visual));}continue;}
  if(e.source!=Skill::Count&&!skill_allowed(e.source)){e.age=e.duration;continue;}
  if(!effect_attack_available(e)){e.tick=std::max(0.f,e.tick-dt);if(e.kind!=EffectKind::Adventure)e.secondary_tick=std::max(0.f,e.secondary_tick-dt);continue;}
  if(e.source!=Skill::Count&&e.effective_level_snapshot>0){int current=effective_skill_level(e.source);if(current!=e.effective_level_snapshot){auto base=[&](int level){return is_fusion(e.source)?24+8.f*(level-1):10+4.f*(level-1);};e.damage*=base(current)/base(e.effective_level_snapshot);e.effective_level_snapshot=current;}}
  {float gear_now=(1+gear_stats_[0])*gear_skill_bonus(e.source);e.damage*=gear_now/e.gear_snapshot;e.gear_snapshot=gear_now;}
  if(e.kind==EffectKind::Resonance){if(e.pattern==1){e.tick-=dt;if(e.tick<=0){e.tick=.5f;resonance_area(Resonance(e.resonance_id),e.source,(e.pos+e.end)*.5f,e.radius,e.damage,4);}}else if(e.pattern==2&&!e.triggered&&e.age>=e.delay){e.triggered=true;for(size_t target=0;target<enemies.size();++target)if(enemies[target].id==e.target_id&&enemies[target].hp>0){hurt_enemy(target,e.damage,{},0,e.source,false,false,e.resonance_id);break;}}continue;}
  if(update_adventure_effect(e,dt)||update_cross_effect(e,dt,spawned)||update_advanced_effect(e,dt,spawned)||update_fusion_effect(e,dt,spawned)||update_expansion_effect(e,dt,spawned))continue;
  if(e.kind==EffectKind::Orbit){e.pos=player.pos;float a=combat_elapsed*(e.evolved?5.2f:3.4f)+e.angle;e.end=player.pos+Vec2{std::cos(a),std::sin(a)}*e.radius;e.tick-=dt;
   if(e.tick<=0){e.tick=.18f;damage_scene_area(e.end,18,e.damage,e.source);for(auto n:nearby(e.end,48))if(length(enemies[n].pos-e.end)<enemies[n].radius+18)hurt_enemy(n,e.damage,enemies[n].pos-player.pos,.7f,e.source);if(e.evolved)for(auto n:nearby(player.pos,e.radius+70)){auto& foe=enemies[n];if(foe.kind!=EnemyKind::Boss)displace_enemy(foe,normalized(player.pos-foe.pos)*25,true);}}continue;}
  if(e.kind==EffectKind::Meteor){if(!e.triggered&&e.age>=e.delay){e.triggered=true;if(!e.proc_generated)damage_scene_area(e.pos,e.radius,e.damage,e.source);for(auto n:nearby(e.pos,e.radius+50))if(length(enemies[n].pos-e.pos)<e.radius+enemies[n].radius){hurt_enemy(n,e.damage,enemies[n].pos-e.pos,2,e.source,!e.proc_generated,!e.proc_generated);if(!e.proc_generated)trigger_burst_synergy(n,e.damage,e.source,spawned);}}continue;}
  if(e.kind==EffectKind::Zone||e.kind==EffectKind::Rain||e.kind==EffectKind::Rune){
   if(e.kind==EffectKind::Rune&&e.evolved&&e.source==Skill::Rune)e.pos=player.pos;
   if(e.age<e.delay)continue;e.tick-=dt;if(e.tick>0)continue;e.tick=e.kind==EffectKind::Rain?.28f:e.kind==EffectKind::Rune?(e.evolved?.55f:.85f):.45f;
   // Nova/Lightning/Charge echoes are one-shot delayed areas; regular runes pulse.
   if(e.kind==EffectKind::Rune&&e.source!=Skill::Rune){if(e.triggered)continue;e.triggered=true;}
   if(!e.proc_generated)damage_scene_area(e.pos,e.radius,e.damage,e.source);
   for(auto n:nearby(e.pos,e.radius+50)){auto& foe=enemies[n];if(length(foe.pos-e.pos)>e.radius+foe.radius)continue;hurt_enemy(n,e.damage,foe.pos-e.pos,e.kind==EffectKind::Zone?.05f:.35f,e.source,true,!e.proc_generated);
    if(e.kind==EffectKind::Zone){foe.burn_remaining=(e.evolved?2.8f:1.2f)*duration_multiplier;foe.burn_damage=e.damage*.45f;foe.burn_source=e.source;}
    if(e.kind==EffectKind::Rain&&e.evolved&&!control_immune(foe)){foe.slow_remaining=.6f;foe.slow_factor=.65f;}
   }
  }
 }
 for(const auto& e:effects)if(e.age>=e.duration&&!e.hostile&&e.source!=Skill::Count&&e.duration>.5f&&(skill_roles(e.source)&(Companion|Turret|Node)))emit_skill_visual(FxStage::End,e.source,e.pos,e.pos+e.visual_aim,30);
 effects.erase(std::remove_if(effects.begin(),effects.end(),[](const Effect&e){return e.age>=e.duration;}),effects.end());
 for(const auto& effect:spawned)add_effect(effect);
}
void Game::remove_dead(){
 std::vector<SummonRequest> splits;
 for(auto& e:enemies)if(e.hp<=0){
  if(e.boss_owner){
   for(auto& fx:effects)if(fx.hostile&&fx.target_id==e.id&&fx.kind==EffectKind::Warning&&!fx.triggered){fx.age=fx.duration;fx.damage=fx.energy=0;fx.triggered=true;}
   if(!e.abilities.reward_claimed){++combat_diagnostics.boss_nodes_destroyed;for(auto& owner:enemies)if(owner.id==e.boss_owner&&owner.hp>0){owner.boss_exposure=std::max(owner.boss_exposure,3.f);owner.shield_remaining=0;}
    Effect death;death.kind=EffectKind::Death;death.pos=e.pos;death.enemy_kind=e.kind;death.enemy_visual=e.visual;death.duration=enemy_death_duration(e.visual);add_effect(death);}
   e.abilities.reward_claimed=true;continue;
  }
  if(e.kind==EnemyKind::Boss){cancel_boss_attack(e);for(auto& n:enemies)if(n.boss_owner==e.id){n.hp=0;n.abilities.reward_claimed=true;}for(auto& z:danger_zones)if(z.owner_id==e.id)z.age=z.duration;}
  on_enemy_real_death(e);if(e.abilities.reward_claimed)continue;const bool elite=e.kind==EnemyKind::Elite||e.abilities.elite;e.abilities.reward_claimed=true;drop_gear(e);if(e.kind==EnemyKind::Boss&&int(e.boss_profile)>=0&&int(e.boss_profile)<32)++metrics.content.boss_kills[int(e.boss_profile)];else if(e.visual>=0&&e.visual<64)++metrics.content.enemy_kills[e.visual];++kills;int kind=elite?int(EnemyKind::Elite):int(e.kind);if(kind>=0&&kind<enemy_kind_count)++metrics.enemy_kills_by_kind[kind];coins+=elite?8:e.kind==EnemyKind::Boss?30:1;on_kill();if(loot_enabled_&&elite&&(metrics.enemy_kills_by_kind[int(EnemyKind::Elite)]==1||metrics.enemy_kills_by_kind[int(EnemyKind::Elite)]%4==0))pending_relic_rewards_=std::min(2,pending_relic_rewards_+1);
  if(elite&&run_bonuses_.first_elite_reward>0&&!first_elite_reward_claimed){first_elite_reward_claimed=true;++metrics.first_elite_rewards;coins+=10;gain_xp(5);notify("勇气天赋：首次击败精英，额外获得10金币与5基础经验！");}
  float v=e.kind==EnemyKind::Boss?150:elite?25:e.kind==EnemyKind::Minion?.6f:e.kind==EnemyKind::BlueMushroom?3:e.kind==EnemyKind::Support?3:1.2f+.3f*std::min(3,int(e.kind));
  v*=config.kill_xp_multiplier;
  if(gems.size()<config.max_gems)gems.push_back({e.pos,v,0});else if(!gems.empty())gems.front().value+=v;else gain_xp(v);
  Effect death;death.kind=EffectKind::Death;death.pos=e.pos;death.radius=e.radius;death.duration=e.kind==EnemyKind::Boss?boss_death_duration(e.boss_profile):enemy_death_duration(e.visual);death.enemy_kind=e.kind;death.boss_profile=e.boss_profile;death.enemy_visual=e.visual;add_effect(death);add_event(EventKind::Kill,e.pos);
  if(special_event.kind==SpecialEventKind::MushroomRush&&std::find(special_event.guardians.begin(),special_event.guardians.end(),e.id)!=special_event.guardians.end())special_event.progress+=1;
  if(e.kind==EnemyKind::Splitter&&e.abilities.shadow_class<0){splits.push_back({e.pos+Vec2{-15,0},false,e.visual});splits.push_back({e.pos+Vec2{15,0},false,e.visual});}
  if(elite||random(0,1)<config.item_drop_chance){float roll=random(0,1);ItemKind item=roll<.3f?ItemKind::Health:roll<.48f?ItemKind::Magnet:roll<.65f?ItemKind::Fury:ItemKind(3+rng_()%6);drop_item(item,e.pos);}
  if(e.kind==EnemyKind::Boss){if(e.final_boss)boss_defeated=true;++metrics.bosses_defeated;metrics.boss_defeat_time=total_elapsed();}
 }
 enemies.erase(std::remove_if(enemies.begin(),enemies.end(),[&](const Enemy&e){return e.hp<=0||(e.kind!=EnemyKind::Boss&&std::find(special_event.guardians.begin(),special_event.guardians.end(),e.id)==special_event.guardians.end()&&sq(e.pos-player.pos)>config.despawn_radius*config.despawn_radius);}),enemies.end());
 for(auto p:splits){size_t before=enemies.size();spawn(EnemyKind::Minion);if(enemies.size()>before){enemies.back().pos=safe_enemy_position(p.pos,enemies.back().radius,110);enemies.back().visual=p.visual;if(p.visual>=0&&p.visual<64)metrics.content.enemies_seen|=uint64_t(1)<<p.visual;resolve_scene_collision(enemies.back().pos,enemies.back().radius);}}
 if(thunder_pending_){thunder_pending_=false;int hits=0;if(!smoke_at(player.pos))for(size_t i=0;i<enemies.size()&&hits<3;++i)if(enemies[i].hp>0&&length(enemies[i].pos-player.pos)<280){hurt_enemy(i,20,enemies[i].pos-player.pos,.3f,Skill::Count,false,false);++hits;}if(hits)++metrics.relic_procs[int(RelicId::ThunderToken)];}
}
void Game::gain_xp(float amount){
 amount*=1+gear_stats_[4];if(amount<=0)return;float value=amount*(!buff_suppressed_&&double_xp_remaining>0?2.f:1.f)*(1+run_bonuses_.xp_bonus);metrics.xp_acquired+=value;metrics.xp_by_minute[std::clamp(int(total_elapsed()/60),0,xp_bucket_count-1)]+=value;player.xp+=value;
 while(player.xp>=player.xp_next){player.xp-=player.xp_next;++player.level;++pending_levels_;float late=std::max(0.f,float(player.level-12));player.xp_next=8+player.level*3.2f+.45f*late*late;}
}
void Game::update(float dt,Input input){events.clear();if(state!=State::Playing||!std::isfinite(dt)||dt<=0)return;dt=std::min(dt,.25f);while(dt>0&&state==State::Playing){float slice=std::min(dt,1.f/60);step(slice,input);input.dash=input.interact=false;dt-=slice;}}
void Game::step(float dt,Input input){
 update_skill_visuals(dt);
 ++simulation_steps;update_resonances(dt);const float encounter_dt=dt*encounter_pace();elapsed+=encounter_dt;combat_elapsed+=dt;timing.combat_seconds+=dt;timing.used_speeds|=1u<<(encounter_pace()-1);update_pressure(encounter_dt,dt);update_danger_zones(dt);schedule_map_zone(encounter_dt);update_pressure_sources();
 {const uint32_t status=(smoke_at(player.pos)?1u:0u)|(equipment_suppressed()?2u:0u)|(regression_active()?4u:0u)|(pressure_active(PressureKind::LastWords)?8u:0u);
  if(status!=previous_combat_states_){record_combat(CombatRecordKind::Status,0,"压制状态变化");previous_combat_states_=status;}}
 combat_records.erase(std::remove_if(combat_records.begin(),combat_records.end(),[&](const CombatRecord& r){return combat_elapsed-r.time>8;}),combat_records.end());
 update_loot(dt);for(int i=0;i<synergy_count;++i){synergy_cooldown_[i]=std::max(0.f,synergy_cooldown_[i]-dt);synergy_flash[i]=std::max(0.f,synergy_flash[i]-dt);}soul_hit_cooldown_=std::max(0.f,soul_hit_cooldown_-dt);soul_absorb_cooldown_=std::max(0.f,soul_absorb_cooldown_-dt);focus_hit_cooldown_=std::max(0.f,focus_hit_cooldown_-dt);holy_support_cooldown_=std::max(0.f,holy_support_cooldown_-dt);
 if(run_mode_==RunMode::Endless){int cycle=std::clamp(int(elapsed/std::max(.05f,config.duration)),0,1000000);if(cycle>metrics.endless_cycle){metrics.endless_cycle=cycle;prepare_encounter_plan();notify("无尽第 "+std::to_string(cycle+1)+" 轮：构筑保留，威胁继续提升。");}}
 for(float* timer:{&fury_remaining,&magnet_remaining,&notice_remaining,&shield_remaining,&double_xp_remaining,&power_remaining,&rift_remaining,&curse_remaining})*timer=std::max(0.f,*timer-dt);
 refresh_stats();player.animation_time+=dt;player.invulnerable=std::max(0.f,player.invulnerable-dt);player.dash_cooldown=std::max(0.f,player.dash_cooldown-dt/dash_recovery_factor());
 Vec2 movement=normalized(input.move);player.moving=length(movement)>.1f;if(movement.x!=0)player.facing_left=movement.x<0;
 if(input.dash&&player.dash_cooldown<=0){++metrics.dash_uses;dash_direction_=player.moving?movement:Vec2{player.facing_left?-1.f:1.f,0};dash_remaining_=config.dash_duration*duration_multiplier;player.dash_cooldown=config.dash_cooldown*(run_mode_==RunMode::Daily&&daily_variant_==0?.75f:1.f);player.invulnerable=std::max(player.invulnerable,dash_remaining_);add_effect({EffectKind::Dash,player.pos,player.pos+dash_direction_*80,24,0,.25f});add_event(EventKind::Dash,player.pos);on_dash();}
 Vec2 before_move=player.pos;bool was_dashing=dash_remaining_>0;
 if(dash_remaining_>0){player.pos+=dash_direction_*(config.dash_speed*dt);dash_remaining_-=dt;}else player.pos+=movement*(player.speed*std::max(.35f,scene_slow(player.pos,13)*(curse_remaining>0?.8f:1.f)*(pressure_active(PressureKind::Exhaustion)?config.exhaustion_factor:1.f))*dt);
 resolve_scene_collision(player.pos,13);update_pressure_sources();refresh_stats();
 if(was_dashing){update_fusion_dash(before_move,player.pos,dash_remaining_<=0);update_cross_dash(before_move,player.pos,dash_remaining_<=0);adventure_dash(before_move,player.pos,dash_remaining_<=0);resonance_dash(before_move,player.pos,dash_remaining_<=0);}
 if(!config.enable_director){
 spawn_clock_-=encounter_dt;if(spawn_clock_<=0){int phase=std::clamp(int(elapsed/120)+(run_mode_==RunMode::Campaign&&chapter_index_>0?chapter_index_+1:0)+(run_mode_==RunMode::Endless?std::min(metrics.endless_cycle,3):0),0,4);const float intervals[]={.72f,.58f,.44f,.34f,.28f};const int counts[]={1,2,3,3,4};spawn_clock_+=intervals[phase];int normal=counts[phase],count=normal*(pressure_active(PressureKind::DarkTide)?3:1);pressure_metrics.normal_spawn_budget+=normal;pressure_metrics.event_spawn_budget+=count;pressure_metrics.requested_spawns+=count;size_t size_before=enemies.size();for(int j=0;j<count;++j){
   if(run_mode_==RunMode::Classic||run_mode_==RunMode::Daily){const EnemyKind pool[]={EnemyKind::Slime,EnemyKind::Mushroom,EnemyKind::Pig,EnemyKind::BlueMushroom,EnemyKind::Ranger,EnemyKind::Splitter,EnemyKind::Support};int available=std::min(7,2+int(elapsed/65)+(difficulty_==Difficulty::Hard?2:difficulty_==Difficulty::Nightmare?4:0));spawn(pool[rng_()%available]);}
   else{static const EnemyKind pools[3][8]={{EnemyKind::Mushroom,EnemyKind::Slime,EnemyKind::Mushroom,EnemyKind::Pig,EnemyKind::BlueMushroom,EnemyKind::Pig,EnemyKind::Splitter,EnemyKind::Mushroom},{EnemyKind::Slime,EnemyKind::Ranger,EnemyKind::Splitter,EnemyKind::Support,EnemyKind::Ranger,EnemyKind::Slime,EnemyKind::Splitter,EnemyKind::Support},{EnemyKind::Pig,EnemyKind::BlueMushroom,EnemyKind::Pig,EnemyKind::Ranger,EnemyKind::BlueMushroom,EnemyKind::Support,EnemyKind::Elite,EnemyKind::Pig}};int region=region_index();int available=run_mode_==RunMode::Campaign&&chapter_index_==0?std::min(8,2+int(elapsed/65)+(difficulty_==Difficulty::Hard?2:difficulty_==Difficulty::Nightmare?4:0)):8;spawn(pools[region][rng_()%available]);}
  }pressure_metrics.actual_spawns+=enemies.size()-size_before;pressure_metrics.capacity_limited+=count-(enemies.size()-size_before);}
 elite_clock_+=encounter_dt;if(elite_clock_>=std::max(difficulty_==Difficulty::Normal&&run_mode_!=RunMode::Endless?1.f:.03f,config.elite_interval*(difficulty_==Difficulty::Hard?.7f:difficulty_==Difficulty::Nightmare?.45f:1.f)*(run_mode_==RunMode::Endless?.8f:1.f))){elite_clock_=0;spawn(EnemyKind::Elite);notify("精英出现：留意冲锋预警、召唤与护盾！");}
 }
 const float boss_due=next_boss_time();
 bool boss_ready=run_mode_==RunMode::Endless?(last_boss_cycle_<metrics.endless_cycle&&std::none_of(enemies.begin(),enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss&&e.hp>0;})):boss_encounters_spawned_<scheduled_boss_count()&&std::none_of(enemies.begin(),enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss&&e.hp>0;});
 if(!config.enable_director&&boss_ready&&elapsed>=boss_due){size_t before=enemies.size();spawn(EnemyKind::Boss);if(enemies.size()>before)record_boss_spawn(enemies.back());}
 if(config.enable_director){bool alive=std::any_of(enemies.begin(),enemies.end(),[](const Enemy& enemy){return enemy.kind==EnemyKind::Boss&&enemy.hp>0;});update_director(encounter_dt,boss_ready&&elapsed>=boss_due,alive);}
 if(!config.enable_director||!director_state.suppress_environment)update_environment(encounter_dt);update_enemies(dt);spawn_boss_nodes();resolve_scene_collision(player.pos,13);rebuild_grid();update_scene_objects(dt,input.interact);
 // Every equipped implemented skill uses the same combat-time cast path.
 for(int i=0;i<skill_count;++i){cast_failure_flash_[i]=std::max(0.f,cast_failure_flash_[i]-dt);float period=skill_interval(Skill(i));if(cooldown_period_[i]>0)cooldown_[i]*=period/cooldown_period_[i];cooldown_period_[i]=period;if(!skill_sealed(Skill(i)))cooldown_[i]=std::max(0.f,cooldown_[i]-dt/cast_recovery_factor());if(effective_skill_level(Skill(i))>0&&Skill(i)!=Skill::DashBreaker&&Skill(i)!=Skill::MountainBreaker&&Skill(i)!=Skill::Charge&&Skill(i)!=Skill::PhoenixAssault&&!(i>=90&&(skill_roles(Skill(i))&DashAttack))&&skill_allowed(Skill(i))&&cooldown_[i]<=0)cast(Skill(i));}
 update_projectiles(dt);update_effects(dt);flush_resonance_effects();update_gear(dt);
 for(auto& d:damage_numbers){d.age+=dt;d.pos.y-=30*dt;}damage_numbers.erase(std::remove_if(damage_numbers.begin(),damage_numbers.end(),[](const DamageNumber&d){return d.age>.75f;}),damage_numbers.end());
 remove_dead();
 for(auto& g:gems){g.age+=dt;float dist=length(player.pos-g.pos);if(dist<pickup_radius)g.pos+=normalized(player.pos-g.pos)*std::min(dist,(magnet_remaining>0?700.f:250.f)*dt);if(length(player.pos-g.pos)<17){gain_xp(g.value);g.value=0;}}
 gems.erase(std::remove_if(gems.begin(),gems.end(),[](const Gem&g){return g.value<=0;}),gems.end());
 update_content(dt);
 pressure_metrics.max_enemies=std::max(pressure_metrics.max_enemies,enemies.size());pressure_metrics.max_projectiles=std::max(pressure_metrics.max_projectiles,projectiles.size());pressure_metrics.max_zones=std::max(pressure_metrics.max_zones,danger_zones.size());
 if(player.hp<=0){player.hp=0;end_pressure_event();danger_zones.clear();soul_links.clear();reviving_enemies_.clear();update_pressure_sources();refresh_stats();state=State::Defeat;add_event(EventKind::Defeat,player.pos);}
 else if(run_mode_!=RunMode::Endless&&elapsed>=config.duration){
  elapsed=config.duration;
  if(run_mode_==RunMode::Classic&&!loot_enabled_){end_pressure_event();danger_zones.clear();soul_links.clear();update_pressure_sources();refresh_stats();state=State::Victory;if(!boss_defeated)add_event(EventKind::Victory,player.pos);}
  else{chapter_finish_pending_=true;remove_dead();for(auto& gem:gems)gain_xp(gem.value);gems.clear();continue_choices();}
 }else if(pending_levels_>0||pending_skill_rewards_>0||pending_relic_rewards_>0)continue_choices();
}
void Game::notify(const std::string& text){notice_text=text;notice_remaining=4;}
void Game::drop_item(ItemKind kind,Vec2 pos){if(items.size()<config.max_items)items.push_back({kind,pos,0});}
void Game::collect_item(ItemKind kind){
 ++metrics.items_collected;float hp_before=player.hp;switch(kind){
 case ItemKind::Health:heal_player(40);notify("拾取红色药水：恢复40生命！");break;
 case ItemKind::Magnet:magnet_remaining=config.magnet_duration;notify("磁力晶石：远距离拾取经验！");break;
 case ItemKind::Fury:fury_remaining=config.fury_duration;notify("狂热符石：伤害+40%，攻速+25%！");break;
 case ItemKind::Shield:shield_remaining=std::max(shield_remaining,5.f*duration_multiplier);notify("无敌护盾："+std::to_string(int(std::ceil(5.f*duration_multiplier)))+"秒内免疫伤害！");break;
 case ItemKind::Bomb:for(size_t i=0;i<enemies.size();++i)if(length(enemies[i].pos-player.pos)<480)hurt_enemy(i,enemies[i].kind==EnemyKind::Boss?350.f:900.f,enemies[i].pos-player.pos,3,Skill::Count,false);notify("清场炸弹：附近敌人受到爆炸伤害！");add_effect({EffectKind::Nova,player.pos,{},480,0,.8f});break;
 case ItemKind::DoubleXP:double_xp_remaining=20;notify("双倍经验：20秒内拾取经验翻倍！");break;
 case ItemKind::Power:power_remaining=12;notify("力量卷轴：12秒伤害+50%！");break;
 case ItemKind::Reset:cooldown_.fill(0);player.dash_cooldown=0;notify("冷却重置：全部技能与冲刺立即就绪！");break;
 case ItemKind::Chest:++pending_skill_rewards_;notify("宝箱强化：从五项候选中选择一项，满槽技能可以替换。");gain_xp(20);break;
 default:break;
 }
 if(kind==ItemKind::Health&&equipped(EquipmentId::AmberPendant)&&hp_before+40*healing_multiplier()>player.max_hp){shield_remaining=std::max(shield_remaining,1.f);++metrics.equipment_procs[int(EquipmentId::AmberPendant)];}
 if(equipped(EquipmentId::ScholarRobe)&&equipment_cooldowns_[int(EquipmentId::ScholarRobe)]<=0){equipment_cooldowns_[int(EquipmentId::ScholarRobe)]=3;shield_remaining=std::max(shield_remaining,.45f);++metrics.equipment_procs[int(EquipmentId::ScholarRobe)];}
 refresh_stats();add_event(EventKind::ItemPickup,player.pos);
}
std::string Game::event_objective()const{
 if(special_event.scroll_challenge)return character==CharacterClass::Warrior?"勇士试炼：击破包围的重装守卫，靠近残卷领取路线":character==CharacterClass::Mage?"魔法试炼：清理分散的控场守卫，靠近古书领取路线":"猎手试炼：移动清理远程守卫，靠近笔记领取路线";
 switch(special_event.kind){
 case SpecialEventKind::GuardedChest:return "击败守卫后靠近宝箱领取奖励";
 case SpecialEventKind::BountyElite:return "追猎标记精英，击败后奖励掉落";
 case SpecialEventKind::Blessing:return "靠近祭坛消耗"+std::to_string(int(std::ceil(config.blessing_health_cost)))+"生命，获得狂热与磁力";
 case SpecialEventKind::Merchant:return "靠近自动消费25金币，获得随机稀有道具（现有"+std::to_string(coins)+"）";
 case SpecialEventKind::MushroomRush:return "击败标记蘑菇 "+std::to_string(int(special_event.progress))+"/"+std::to_string(special_event.target);
 case SpecialEventKind::EliteHunt:return "围猎剩余精英 "+std::to_string(special_event.guardians.size())+"，击败全部获取宝箱";
 case SpecialEventKind::Fountain:return "靠近喷泉：随机治疗+40经验，或减速诅咒+80经验";
 case SpecialEventKind::Rift:return "圈内累计守住8秒（"+std::to_string(int(special_event.progress))+"/8），敌速+25%，技能冷却-30%";
 default:return "探索、击败敌人并收集经验";
 }
}
void Game::begin_special_event(bool scroll_challenge){
 if(special_event.kind!=SpecialEventKind::None)return;SpecialEvent ev;
 if(scroll_challenge){ev.kind=SpecialEventKind::GuardedChest;ev.scroll_challenge=true;}
 else if(run_mode_==RunMode::Classic||run_mode_==RunMode::Daily)ev.kind=SpecialEventKind(1+rng_()%std::clamp(config.enabled_events,1,8));
 else{static const SpecialEventKind pools[3][8]={{SpecialEventKind::MushroomRush,SpecialEventKind::GuardedChest,SpecialEventKind::MushroomRush,SpecialEventKind::Merchant,SpecialEventKind::BountyElite,SpecialEventKind::GuardedChest,SpecialEventKind::Blessing,SpecialEventKind::Fountain},{SpecialEventKind::Rift,SpecialEventKind::Fountain,SpecialEventKind::Rift,SpecialEventKind::Blessing,SpecialEventKind::Fountain,SpecialEventKind::GuardedChest,SpecialEventKind::Merchant,SpecialEventKind::BountyElite},{SpecialEventKind::EliteHunt,SpecialEventKind::BountyElite,SpecialEventKind::EliteHunt,SpecialEventKind::BountyElite,SpecialEventKind::Merchant,SpecialEventKind::Blessing,SpecialEventKind::Rift,SpecialEventKind::GuardedChest}};std::vector<SpecialEventKind> enabled;for(auto kind:pools[region_index()])if(int(kind)<=std::clamp(config.enabled_events,1,8))enabled.push_back(kind);ev.kind=enabled[rng_()%enabled.size()];}
 if(ev.kind==SpecialEventKind::MushroomRush&&!(config.available_enemy_mask&((1u<<1)|(1u<<2))))ev.kind=SpecialEventKind::GuardedChest;
 float a=random(0,2*pi);ev.pos=player.pos+Vec2{std::cos(a),std::sin(a)}*220;ev.remaining=config.event_duration;
 if(ev.kind==SpecialEventKind::GuardedChest||ev.kind==SpecialEventKind::BountyElite||ev.kind==SpecialEventKind::EliteHunt){int count=ev.kind==SpecialEventKind::BountyElite?1:3;if(enemies.size()+count>enemy_capacity()){next_event_in=10;return;}
  for(int i=0;i<count;++i){size_t before=enemies.size();EnemyKind role=EnemyKind::Elite;if(scroll_challenge)role=character==CharacterClass::Warrior?EnemyKind::BlueMushroom:character==CharacterClass::Mage?(i==0?EnemyKind::GroundController:EnemyKind::Ranger):EnemyKind::Ranger;spawn(role,true);if(enemies.size()>before){auto& e=enemies.back();float angle=2*pi*i/count;float spacing=scroll_challenge?(character==CharacterClass::Warrior?85.f:165.f):65.f;e.pos=safe_enemy_position(ev.pos+Vec2{std::cos(angle),std::sin(angle)}*spacing,e.radius,180);if(scroll_challenge){e.max_hp*=1.8f;e.damage*=.8f;e.attack_clock=.5f+i*.65f;}else e.max_hp*=ev.kind==SpecialEventKind::GuardedChest?.45f:ev.kind==SpecialEventKind::BountyElite?1.4f:.65f;e.hp=e.max_hp;ev.guardians.push_back(e.id);}}
 }
 if(ev.kind==SpecialEventKind::MushroomRush){ev.target=12;if(enemies.size()>enemy_capacity()||size_t(ev.target)>enemy_capacity()-enemies.size()){next_event_in=10;return;}ev.remaining=std::max(25.f,config.event_duration);}
 if(ev.kind==SpecialEventKind::Rift){ev.target=8;ev.remaining=std::min(35.f,config.event_duration);}
 special_event=ev;if(ev.scroll_challenge)scrolls.challenge_offered=true;++metrics.events_started;notify(std::string(special_event_name(ev.kind))+"出现！"+event_objective());add_event(EventKind::SpecialEvent,ev.pos);
}
void Game::finish_special_event(bool success){
 if(special_event.kind==SpecialEventKind::None)return;auto kind=special_event.kind;Vec2 pos=special_event.pos;
 if(kind==SpecialEventKind::Rift)rift_remaining=0;
 if(success){++metrics.events_succeeded;if(loot_enabled_)pending_relic_rewards_=std::min(2,pending_relic_rewards_+1);
  if(kind==SpecialEventKind::Blessing){float cost=std::min(std::max(0.f,player.hp-1.f),config.blessing_health_cost);player.hp-=cost;metrics.altar_health_spent+=cost;record_combat(CombatRecordKind::HealthCost,cost,"祭坛献血");fury_remaining=config.fury_duration*1.5f;magnet_remaining=config.magnet_duration;notify("献出"+std::to_string(int(std::ceil(cost)))+"生命，获得强化狂热与磁力祝福！");}
  else if(kind==SpecialEventKind::Merchant){coins-=25;ItemKind reward=ItemKind(3+rng_()%6);collect_item(reward);notify(std::string("商人交易完成：25金币换取")+item_name(reward));}
  else if(kind==SpecialEventKind::Fountain){if(random(0,1)<.65f){heal_player(50);gain_xp(40);notify("幸运喷泉：恢复50生命，获得40基础经验！");}else{curse_remaining=18;gain_xp(80);notify("喷泉诅咒：18秒移速-20%，获得80基础经验！");}}
  else{float xp=kind==SpecialEventKind::GuardedChest?45.f:kind==SpecialEventKind::MushroomRush?65.f:kind==SpecialEventKind::Rift?85.f:70.f;if(gems.size()<config.max_gems)gems.push_back({pos,xp,0});else gain_xp(xp);
   drop_item(ItemKind::Health,pos+Vec2{-20,0});drop_item(ItemKind::Fury,pos+Vec2{20,0});drop_item(ItemKind::Magnet,pos+Vec2{0,20});if(kind==SpecialEventKind::EliteHunt||kind==SpecialEventKind::Rift)drop_item(ItemKind::Chest,pos);
   notify(std::string(special_event_name(kind))+"完成！经验与补给已掉落。");
  }
  add_event(EventKind::EventComplete,pos);
 }else{++metrics.events_failed;notify(std::string(special_event_name(kind))+"超时，挑战结束。下一次机会即将到来。");}
 refresh_stats();special_event={};next_event_in=random(std::max(.1f,config.event_interval_min),std::max(.1f,std::max(config.event_interval_min,config.event_interval_max)))*(run_mode_==RunMode::Endless?.75f:1.f)*(difficulty_==Difficulty::Hard?.85f:difficulty_==Difficulty::Nightmare?.7f:1.f);
}
void Game::update_content(float dt){
 for(auto& item:items){item.age+=dt;if(length(item.pos-player.pos)<25){collect_item(item.kind);item.age=config.item_lifetime+1;}}
 items.erase(std::remove_if(items.begin(),items.end(),[&](const ItemDrop&i){return i.age>config.item_lifetime;}),items.end());
 if(player.hp<=0||state!=State::Playing)return;
 if(special_event.kind==SpecialEventKind::None){next_event_in-=dt*encounter_pace();float window=run_mode_==RunMode::Endless?next_boss_time():std::min(next_boss_time(),config.duration);bool clear_boss=std::none_of(enemies.begin(),enemies.end(),[](const Enemy& e){return e.kind==EnemyKind::Boss&&e.hp>0;});bool can_event=elapsed+config.event_duration<window&&clear_boss&&(!config.enable_director||director_state.allow_special_events);if(next_event_in<=0&&can_event)begin_special_event();return;}
 special_event.remaining-=dt*encounter_pace();if(special_event.remaining<=0){finish_special_event(false);return;}
 float dist=length(player.pos-special_event.pos);auto kind=special_event.kind;
 if(kind==SpecialEventKind::Blessing){if(dist<35){if(player.hp>config.blessing_health_cost)finish_special_event(true);else if(notice_remaining<=0)notify("鲜血祝福需要多于"+std::to_string(int(std::ceil(config.blessing_health_cost)))+"生命，请先恢复生命。");}return;}
 if(kind==SpecialEventKind::Merchant){if(dist<35){if(coins>=25)finish_special_event(true);else if(notice_remaining<=0)notify("金币不足：击败敌人获取金币，商人需要25金币。");}return;}
 if(kind==SpecialEventKind::Fountain){if(dist<35)finish_special_event(true);return;}
 if(kind==SpecialEventKind::Rift){if(dist<95){special_event.activated=true;special_event.progress+=dt;rift_remaining=special_event.remaining/encounter_pace();}else special_event.progress=std::max(0.f,special_event.progress-dt*.4f);if(special_event.progress>=8)finish_special_event(true);return;}
 if(kind==SpecialEventKind::MushroomRush){special_event.tick-=dt*encounter_pace();if(special_event.tick<=0&&special_event.spawned<20){special_event.tick=2.f;for(int j=0;j<4&&special_event.spawned<20;++j){size_t before=enemies.size();spawn(EnemyKind::Mushroom,true);if(enemies.size()>before){float a=random(0,2*pi);auto& e=enemies.back();e.pos=safe_enemy_position(special_event.pos+Vec2{std::cos(a),std::sin(a)}*160,e.radius,180);e.max_hp*=.65f;e.hp=e.max_hp;special_event.guardians.push_back(e.id);++special_event.spawned;}}}if(special_event.progress>=special_event.target){finish_special_event(true);return;}}
 special_event.guardians.erase(std::remove_if(special_event.guardians.begin(),special_event.guardians.end(),[&](uint32_t id){return std::none_of(enemies.begin(),enemies.end(),[&](const Enemy&e){return e.id==id&&e.hp>0;});}),special_event.guardians.end());
 if(kind==SpecialEventKind::BountyElite)for(auto& e:enemies)if(std::find(special_event.guardians.begin(),special_event.guardians.end(),e.id)!=special_event.guardians.end())special_event.pos=e.pos;
 if(kind!=SpecialEventKind::MushroomRush&&special_event.guardians.empty()&&(kind!=SpecialEventKind::GuardedChest||dist<42))finish_special_event(true);
}
}




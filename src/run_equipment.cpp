#include "game.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
namespace maple {
const std::vector<GearDefinition>& gear_definitions(){
 static const std::vector<GearDefinition> defs={
 {"maple_badge","枫叶徽记","通用伤害；重复份数按平方根累计。",0,1,0,0,.035f},
 {"adventurer_cape","冒险披风","生命上限；拾取和压制恢复不会免费补满生命。",1,1,1,0,8},
 {"swift_boots","轻灵短靴","提高移动速度。",2,1,2,0,.025f},
 {"scavenger_belt","拾荒腰带","扩大拾取距离。",2,1,3,0,9},
 {"scholar_glasses","学者眼镜","增加经验收益，不改变地面经验数量。",2,1,4,0,.035f},
 {"clock_charm","怀表挂饰","缩短冷却，采用倒数曲线；继续拾取同时提高暴击伤害。",2,1,5,0,.035f},
 {"lucky_glove","幸运手套","提高暴击率；超过有效概率的收益转入暴击伤害。",0,1,6,0,.018f},
 {"keen_ring","锋锐指环","提高暴击伤害。",0,1,7,0,.06f},
 {"wide_lens","广角镜片","扩大技能范围。",0,1,8,0,.03f},
 {"lasting_ribbon","绵长缎带","延长持续效果。",1,1,9,0,.04f},
 {"iron_plate","铁甲扣","减少实际受伤，按倒数曲线计算。",1,1,10,0,.035f},
 {"rhythm_watch","节拍腕表","提高攻击频率，重复收益递减。",0,1,11,0,.03f},
 {"shadow_bracer","暗影护腕","强化有实际影系行为的技能。",0,2,12,Shadow,.08f},
 {"tempered_blade","淬毒匕首","强化施毒技能。",0,2,12,Poison,.08f},
 {"hunting_ring","狩猎指环","强化标记技能的攻击。",0,2,12,Mark,.09f},
 {"pirate_magazine","海盗弹匣","提升枪炮单次质量，不无限增加实体。",0,2,12,Gun,.08f},
 {"energy_belt","能量腰带","强化拳法与蓄能攻击。",0,2,12,Fist|Energy,.08f},
 {"nautical_compass","航海罗盘","强化召唤与阵地的有效攻击。",0,2,12,Companion|Turret,.08f},
 {"machine_core","机械核心","强化机械攻击。",0,2,12,Machine,.10f},
 {"dragon_pendant","龙魂吊坠","强化龙形与元素阶段技能。",0,2,12,Dragon|Elemental,.08f},
 {"frost_earring","寒霜耳环","强化寒霜攻击。",0,2,12,Cold,.08f},
 {"thunder_glove","雷鸣手套","强化有限传导与雷电攻击。",0,2,12,Thunder,.08f},
 {"phantom_poker","幻影扑克","强化卡牌攻击。",0,2,12,Card,.10f},
 {"rose_brooch","玫瑰胸针","强化延迟落点与玫瑰爆发。",0,2,12,Rose|Landing,.08f},
 {"echo_bell","回响铃铛","主命中每3秒最多追加一次延迟追击；不复制次生链。",0,3,20,0,.25f},
 {"venom_vial","毒囊","主命中每2秒施加一次毒层；重复提高毒伤。",0,3,21,0,.18f},
 {"winter_seal","冬之封印","主命中每2秒留下寒霜；重复增加附加伤害。",1,3,22,0,.18f},
 {"ember_flask","余烬瓶","主命中每2秒灼烧一个目标。",0,3,23,0,.18f},
 {"breaker_spike","破甲钉","主命中每3秒留下破甲窗口并追加受限穿刺。",0,3,24,0,.22f},
 {"hunter_seal","猎手结印","对已标记目标每2秒进行一次集中追击。",0,3,25,0,.35f},
 {"shock_ring","震荡指环","主命中每4秒在目标周围释放一次小范围冲击。",0,3,26,0,.3f},
 {"execution_badge","处决徽章","低于35%生命的敌人每3秒遭受一次额外攻击；不按最大生命扣血。",0,3,27,0,.55f},
 {"dash_ward","疾行护符","冲刺结束获得短暂护盾；频率固定，重复提高附加冲击。",1,3,28,0,.3f},
 {"life_thread","生命丝线","合格主命中每5秒有限回复；重复提高回复量。",1,3,29,0,1.f},
 {"storm_coil","风暴线圈","主命中每4秒向另一个邻近目标传导一次。",0,4,30,0,.45f},
 {"fortress_key","要塞密钥","阵地或机械主命中每3秒触发局部支援炮击。",0,4,31,0,.5f},
 {"twin_sigil","双生印记","合格投射主命中每3秒从两侧追加衰减夹击。",0,4,32,0,.28f},
 {"dragon_heart","龙心结晶","元素或龙系主命中每4秒在目标附近形成有限冰焰脉冲。",0,4,33,0,.4f},
 {"oath_medal","守誓勋章","近身主命中每5秒获得0.5秒护盾，并追加一次反攻。",1,4,34,0,.35f},
 {"convergence_mark","汇流枫印","三源及以上融合主命中每4秒追加一次收束爆发。",0,4,35,0,.5f}
 };return defs;
}
const GearDefinition* gear_definition(const std::string& id){for(const auto& d:gear_definitions())if(id==d.id)return &d;return nullptr;}
float gear_contribution(uint64_t count,int level,float amount){return float(std::sqrt(double(count)))*std::max(0,level)*amount;}
std::string Game::gear_description(const GearStack& stack) const {
 const auto* def=gear_definition(stack.id);if(!def)return "未识别的装备";
 const bool suppressed=equipment_suppressed();const int level=suppressed?0:std::max(0,stack.level-(regression_active()?1:0));
 const double root=std::sqrt(double(stack.count)),current=root*level*def->amount;
 // This form retains the small marginal gain without count + 1 overflowing.
 const double delta=double(level)*def->amount/(std::sqrt(double(stack.count)+1.)+root);
 std::ostringstream out;out<<std::fixed<<std::setprecision(2)<<"数量 "<<stack.count<<" · 效果等级 "<<level<<" / "<<stack.level<<"\n"<<def->description<<"\n";
 if(suppressed)out<<"当前装备失效；份数保留，恢复后按原数量贡献。\n";
 else if(level!=stack.level)out<<"退化仅降低有效等级，不扣除数量。\n";
 if(def->stat==1||def->stat==3||def->stat==29)out<<"本组累计 +"<<current<<"；下一件 +"<<delta;
 else out<<"本组累计 +"<<current*100<<"%；下一件 +"<<delta*100<<"%";
 if(def->stat==5)out<<"\n全部装备冷却系数 "<<1./(1.+gear_stats_[5])<<" → "<<1./(1.+gear_stats_[5]+delta)<<"；额外暴击伤害 +"<<delta*20<<"%。";
 else if(def->stat==6){const double probability=std::min(delta,std::max(0.,.85-double(crit_chance)));out<<"\n下一件实际暴击率 +"<<probability*100<<"%，超出85%的部分转为暴击伤害 +"<<(delta-probability)*100<<"%。";}
 else if(def->stat==10)out<<"\n全部装备受伤系数 "<<1./(1.+gear_stats_[10])<<" → "<<1./(1.+gear_stats_[10]+delta)<<"。";
 if(def->stat>=20)out<<"\n触发频率固定；重复份数提高效果量，不增加监听器或递归触发。";
 std::string supported;int count=0;
 for(int i=0;i<skill_count;++i)if(skills[i]){const auto roles=skill_roles(Skill(i));bool valid=def->stat!=12||(roles&def->role)!=0;
  if(def->stat==31)valid=(roles&(Machine|Turret))!=0;else if(def->stat==32)valid=(roles&Bolt)!=0;else if(def->stat==33)valid=(roles&(Dragon|Elemental))!=0;else if(def->stat==35)valid=fusion_tier(Skill(i))>=3;
  if(valid){if(count++)supported+="、";supported+=skill_name(Skill(i));}
 }
 if(!supported.empty())out<<"\n当前支持："<<supported;else if(def->stat==12||def->stat>=31)out<<"\n当前没有符合条件的已装备技能。";
 return out.str();
}
uint64_t Game::gear_count() const {uint64_t count=0;for(const auto& stack:run_gear)count+=stack.count;return count;}
void Game::gain_gear(const std::string& id,int level,uint64_t count){
 if(!gear_definition(id)||count==0)return;level=std::clamp(level,1,3);
 auto found=std::find_if(run_gear.begin(),run_gear.end(),[&](const GearStack& s){return s.id==id&&s.level==level;});if(found==run_gear.end())run_gear.push_back({id,level,count});else found->count+=count;
 gear_dirty_=true;refresh_stats();notify(std::string("获得 ")+gear_definition(id)->name+" · 装备总数 "+std::to_string(gear_count())+(equipment_suppressed()?"（当前受压制）":""));
}
void Game::refresh_gear(){
 const bool suppressed=equipment_suppressed(),regression=regression_active();if(!gear_dirty_&&suppressed==gear_cache_suppressed_&&regression==gear_cache_regressed_)return;
 gear_dirty_=false;gear_cache_suppressed_=suppressed;gear_cache_regressed_=regression;gear_stats_.fill(0);gear_roles_.fill(0);gear_mechanisms_.fill(0);
 if(suppressed)return;
 for(const auto& stack:run_gear){const auto* def=gear_definition(stack.id);if(!def)continue;float value=gear_contribution(stack.count,stack.level-(regression?1:0),def->amount);if(def->stat<12)gear_stats_[def->stat]+=value;else if(def->stat==12){for(int bit=0;bit<32;++bit)if(def->role&(uint32_t(1)<<bit))gear_roles_[bit]+=value;}else gear_mechanisms_[def->stat-20]+=value;}
}
float Game::gear_skill_bonus(Skill skill) const {float bonus=0;uint32_t roles=skill_roles(skill);for(int bit=0;bit<32;++bit)if(roles&(uint32_t(1)<<bit))bonus=std::max(bonus,gear_roles_[bit]);return 1+bonus;}
void Game::update_gear(float dt){
 gear_shield_seconds_=std::max(0.f,gear_shield_seconds_-dt);
 for(auto& cooldown:gear_cooldowns_)cooldown=std::max(0.f,cooldown-dt);
 for(auto it=gear_drops.begin();it!=gear_drops.end();){if(length(it->pos-player.pos)<pickup_radius){gain_gear(it->id,it->level,it->count);it=gear_drops.erase(it);}else ++it;}
}
void Game::drop_gear(const Enemy& enemy){
 const bool boss=enemy.kind==EnemyKind::Boss,elite=enemy.kind==EnemyKind::Elite||enemy.abilities.elite;
 const bool early=gear_count()==0&&gear_drops.empty()&&elapsed>=50;
 if(!boss&&!elite&&!early&&random(0,1)>.0018f)return;
 const int rarity=boss?4:elite?3:elapsed>=600?2:1;
 std::vector<const GearDefinition*> pool;for(const auto& d:gear_definitions())if(d.rarity<=rarity)pool.push_back(&d);
 const auto* chosen=pool[rng_()%pool.size()];Vec2 position=enemy.pos;resolve_scene_collision(position,12);gear_drops.push_back({chosen->id,position,boss?2:1,1});
}
void Game::gear_hit(size_t target,float actual,Skill source){
 if(equipment_suppressed()||actual<=0||target>=enemies.size())return;const auto roles=skill_roles(source);auto& enemy=enemies[target];
 for(int i=0;i<16;++i){float value=gear_mechanisms_[i];if(value<=0||gear_cooldowns_[i]>0||gear_roots_[i]==current_attack_id_)continue;
  if(i==5&&enemy.mark_remaining<=0)continue;if(i==7&&enemy.hp>enemy.max_hp*.35f)continue;if(i==8)continue;if(i==11&&!(roles&(Machine|Turret)))continue;if(i==12&&!(roles&Bolt))continue;if(i==13&&!(roles&(Dragon|Elemental)))continue;if(i==14&&length(enemy.pos-player.pos)>150)continue;if(i==15&&fusion_tier(source)<3)continue;
  static const float periods[]={3,2,2,2,3,2,4,3,2,5,4,3,3,4,5,4};gear_cooldowns_[i]=periods[i];gear_roots_[i]=current_attack_id_;
  if(i==1){apply_poison(target,actual*value,3,source);continue;}if(i==2)apply_frost(enemy,2,source);if(i==3){apply_burn(enemy,actual*value,3,source);continue;}if(i==4){enemy.armor_break_remaining=2.5f;enemy.armor_source=source;}if(i==9){heal_player(value);continue;}if(i==14)gear_shield_seconds_=std::max(gear_shield_seconds_,.5f);
  Effect effect;effect.kind=EffectKind::Adventure;effect.source=source;effect.pattern=(i==6||i==11||i==13||i==15)?2:1;effect.target_id=enemy.id;effect.pos=player.pos;effect.end=enemy.pos;effect.radius=i==11?85.f:65.f;effect.damage=actual*value;effect.delay=i==11?.65f:.2f;effect.duration=effect.delay+.35f;effect.proc_generated=true;effect.root_attack_id=current_attack_id_;effect.resonance_id=-2;
  if(i==10){size_t other=priority_enemy(enemy.pos,160,enemy.id);if(other==size_t(-1))continue;effect.target_id=enemies[other].id;effect.end=enemies[other].pos;}
  if(i==12){effect.damage*=.75f;effect.pos=enemy.pos+Vec2{-65,0};pending_resonance_effects_.push_back(effect);effect.pos=enemy.pos+Vec2{65,0};}
  pending_resonance_effects_.push_back(effect);
 }
}
}

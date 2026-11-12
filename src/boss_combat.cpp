#include "boss_combat.hpp"
#include "encounter_pool.hpp"
#include <cmath>

namespace maple {
namespace {
constexpr float pi=3.14159265359f;
Vec2 rotate(Vec2 v,float angle){return {v.x*std::cos(angle)-v.y*std::sin(angle),v.x*std::sin(angle)+v.y*std::cos(angle)};}
float segment_distance(Vec2 p,Vec2 a,Vec2 b){Vec2 v=b-a;float n=v.x*v.x+v.y*v.y;float t=n>0?std::clamp(((p-a).x*v.x+(p-a).y*v.y)/n,0.f,1.f):0;return length(p-(a+v*t));}
}

const BossCombatDefinition& boss_combat_definition(BossProfile profile){
 static const std::array<BossCombatDefinition,boss_profile_count> definitions={{
  {{{{"crown_leap","王冠重跃",1.15f,2.2f},{"spore_garden","孢子花园",1.1f,1.5f},{"royal_roll","王者滚击",1.2f,2.1f}}},"孢子仔存活时，滚击落点追加孢子爆发。","先清孢子仔；跳跃锁定落点后向侧面移动。","孢子仔",1,.68f,.32f},
  {{{{"branch_fork","分叉根须",1.25f,1.8f},{"seed_guard","种子守卫",1.2f,1.7f},{"growth_ring","生长缺环",1.45f,2.2f}}},"木妖守卫向生长环补射种子；击毁会暴露本体。","朝环的缺口离开，靠近木妖可让自动攻击优先处理它。","木妖守卫",6,.70f,.35f},
  {{{{"slime_drop","弹跳重压",1.1f,2.f},{"slime_spray","水灵喷涌",1.15f,1.6f},{"slime_merge","分裂回流",1.25f,2.f}}},"分裂水灵回到本体时提供短时减伤；击毁可阻止合流。","水灵回流前拦截，短时减伤结束仍可直接击杀本体。","回流水灵",0,.60f,.28f},
  {{{{"shell_spiral","蜗壳螺旋",1.35f,2.1f},{"slime_return","黏液折返",1.2f,2.5f},{"shell_spikes","背壳棘刺",1.1f,2.1f}}},"蓄力时正面甲壳减伤，收壳后出现破绽；黏液线会再次脉冲。","避开黏液回流线，绕至侧后方输出。","",-1,.62f,.30f},
  {{{{"double_fist","左右重拳",1.4f,2.8f},{"fault_fissure","断层地裂",1.5f,2.2f},{"rock_collapse","落岩围阵",1.4f,2.6f}}},"两拳依次锁定同一轴线，完整收拳后本体承伤提高。","横移躲开拳路，在收拳和落岩间隙贴身反击。","",-1,.72f,.34f},
  {{{{"dark_fire_fan","暗焰扇翼",1.1f,1.5f},{"shadow_dive","暗影俯冲",1.25f,2.4f},{"dark_pillars","交错暗柱",1.35f,2.f}}},"俯冲沿锁定路线执行，身后的暗焰延迟爆发；落地后暴露。","先侧移离开影线，再避开留下的暗焰回击。","",-1,.65f,.25f},
  {{{{"snow_bowling","巨型滚雪",1.3f,1.8f},{"ice_wave","冰震接力",1.25f,2.2f},{"wolf_stomp","驮狼踏雪",1.2f,2.5f}}},"冰震沿已锁定方向逐段推进；踩踏雪痕会再次震动。","横穿波列的侧面，不沿预警落点后退。","",-1,.66f,.30f},
  {{{{"broadside","舷侧齐射",1.4f,1.7f},{"cross_cannon","交错炮列",1.3f,2.1f},{"powder_crew","火药炮组",1.2f,2.f}}},"炮组存活时从两翼补射；炮组爆破前可击毁并暴露船长。","优先击毁近处炮组，从炮列留白处通过。","火药炮组",29,.70f,.33f},
  {{{{"clock_hand","报时横扫",1.5f,2.f},{"tick_bombs","滴答连爆",1.35f,2.f},{"clock_nodes","钟摆校时",1.3f,2.5f}}},"钟摆节点决定下一次扫线数量；报时后的钟域只减慢弹道。","红色线才是伤害；击毁大立钟减少下一轮扫线。","钟摆节点",43,.75f,.38f},
  {{{{"sea_breath","苍海吐息",1.4f,2.2f},{"tidal_rush","潮汐掠冲",1.25f,2.f},{"bubble_spouts","水泡涌泉",1.4f,2.3f}}},"吹泡泡鱼布置额外水柱；节点有限且可击毁。","先打掉挡住通道的吹泡泡鱼，再从吐息侧面靠近。","涌泉水泡鱼",28,.64f,.29f},
  {{{{"spine_fork","三叉毒刺",1.2f,1.8f},{"sand_plow","沙丘犁地",1.3f,2.3f},{"desert_bloom","沙漠花圃",1.2f,2.2f}}},"花圃存活时向冲锋终点补充毒刺；击毁停止补射。","冲锋锁定后侧移，清除靠近自己的食人花。","沙漠食人花",58,.65f,.31f},
  {{{{"hex_triangles","三角诅咒",1.3f,1.9f},{"ghost_bananas","幽灵追猎",1.2f,1.8f},{"ghost_procession","幽灵仪式",1.4f,2.4f}}},"追猎弹只在短时间内缓慢转向；幽灵使下一轮追猎增多。","诱导弹道转弯后反向走，击毁幽灵减少后续追猎。","仪式幽灵",14,.68f,.32f},
  {{{{"pincer_gate","双钳合门",1.35f,2.5f},{"crab_sidestep","横步钩击",1.2f,2.f},{"shore_guards","潮岸蟹群",1.3f,2.1f}}},"双钳留有中央窄路，青螃蟹从两侧追加钳击。","从合门的中缝穿过，绕开正面甲壳。","潮岸青螃蟹",57,.72f,.35f},
  {{{{"doll_missiles","十字追踪弹",1.35f,1.8f},{"doll_barrage","扫射阵列",1.4f,2.f},{"doll_overheat","核心过热",1.6f,4.f}}},"连续武器循环后核心过热，短时间承受额外伤害。","扫射逐列爆发；在过热收招窗口集中输出。","",-1,.60f,.25f},
  {{{{"zeno_lattice","异星栅格",1.5f,2.f},{"zeno_nodes","卫兵充能",1.3f,2.3f},{"zeno_links","节点连射",1.4f,2.4f}}},"外星卫兵是独立能量节点；摧毁后对应连射立即停止。","靠近一侧节点打开通路，不必清完全部节点才能打本体。","充能卫兵",59,.70f,.30f},
  {{{{"panda_clap","熊掌震荡",1.35f,2.3f},{"panda_sweep","竹风横扫",1.4f,2.1f},{"panda_shoulder","踏步靠山",1.25f,2.7f}}},"熊掌先近后远，靠山锁线并在结束后露出长破绽。","不要贴着拳路后退，横移后追击收招。","",-1,.67f,.33f},
  {{{{"fox_flames","九尾狐火",1.25f,1.9f},{"fox_ambush","残影扑杀",1.35f,2.5f},{"fox_decoys","妖狐幻阵",1.3f,2.2f}}},"幻灵存活会补射狐火；扑杀的第二击固定延后。","别立刻回到旧落点，击毁幻灵削减火力。","狐火幻灵",38,.62f,.28f},
  {{{{"eliza_notes","精灵音阶",1.2f,1.9f},{"eliza_thorns","荆棘花环",1.4f,2.4f},{"eliza_garden","花妖守护",1.35f,2.1f}}},"花妖补射音符；生长环始终保留一个可移动缺口。","清除最近的花妖并从花环缺口离开。","花妖",58,.70f,.35f},
  {{{{"chimera_venom","毒息残迹",1.4f,2.f},{"chimera_claws","三重裂爪",1.3f,2.5f},{"chimera_vent","超载喷射",1.6f,3.4f}}},"毒息在锁定落点残留脉冲；喷射后核心短暂暴露。","离开毒迹，绕至喷射侧翼等待核心冷却。","",-1,.64f,.30f},
  {{{{"dodo_rings","时间涟漪",1.45f,2.3f},{"dodo_current","时间回流",1.4f,2.f},{"dodo_eyes","时眼召集",1.3f,2.3f}}},"时眼存活会补充回流弹；波环缺口在下一阶段错位。","先看缺口方向，打掉附近时眼减少弹道交叉。","时间之眼",50,.74f,.40f},
  {{{{"lily_charge","玄冰角冲",1.3f,2.5f},{"lily_starfall","星霜坠落",1.5f,2.2f},{"lily_guards","霜卫镜阵",1.4f,2.1f}}},"霜卫存活会在角冲后追加冰线；角冲固定终点。","侧移出角冲通道，清理一侧霜卫打开冰线缺口。","霜卫雪人",39,.66f,.30f},
  {{{{"lyka_horns","月角犁击",1.4f,2.5f},{"lyka_quake","月轮震地",1.45f,2.2f},{"lyka_howl","月影召唤",1.35f,2.1f}}},"月影怪猫存活会扩展下一次犁击的侧路攻击。","先杀近处怪猫，利用震地环的缺口切换方向。","月影怪猫",35,.68f,.32f},
  {{{{"manon_breath","熔火龙息",1.6f,2.6f},{"manon_crush","龙翼坠压",1.45f,2.8f},{"manon_gust","双翼焚风",1.4f,2.3f}}},"龙息只覆盖正面，坠压后暴露；焚风从两翼向外扫。","靠近侧后方，坠压结束后集中攻击。","",-1,.70f,.28f},
  {{{{"griffey_cross","十字飞羽",1.25f,1.8f},{"griffey_dive","鹰爪掠空",1.35f,2.5f},{"griffey_eye","风眼护卫",1.4f,2.3f}}},"幼狮护卫补充飞羽，风眼环留有缺口；掠空落地后暴露。","先清近处护卫，再沿风眼的缺口移动。","飞狮护卫",40,.65f,.30f},
 }};
 return definitions[std::clamp(int(profile),0,boss_profile_count-1)];
}

bool hostile_shape_contains(const Effect& e,Vec2 p,float body){
 if(e.pattern==1)return segment_distance(p,e.pos,e.end)<=e.radius+body;
 if(e.pattern==4){Vec2 delta=p-e.pos;float distance=length(delta);if(distance>e.radius+body)return false;if(distance<=body)return true;
  float angle=std::atan2(e.end.x*delta.y-e.end.y*delta.x,e.end.x*delta.x+e.end.y*delta.y);
  return std::abs(angle)<=e.angle+std::asin(std::min(1.f,body/distance));}
 return length(p-e.pos)<=e.radius+body;
}

void Game::record_combat(CombatRecordKind kind,float amount,const std::string& source,int attack,Skill skill){
 if(kind==CombatRecordKind::Shield)for(auto it=combat_records.rbegin();it!=combat_records.rend()&&combat_elapsed-it->time<.2f;++it)if(it->kind==kind&&it->source==source){it->amount+=amount;return;}
 if(combat_records.size()>=96)combat_records.erase(combat_records.begin());
 const uint32_t status=(smoke_at(player.pos)?1u:0u)|(equipment_suppressed()?2u:0u)|(regression_active()?4u:0u)|(pressure_active(PressureKind::LastWords)?8u:0u);
 combat_records.push_back({kind,combat_elapsed,amount,player.hp,player.pos,status,attack,skill,source});
}

void Game::cancel_boss_attack(Enemy& boss){
 for(auto& effect:effects)if(effect.hostile&&(effect.target_id==boss.id||(boss.boss_attack_serial&&effect.root_attack_id==boss.boss_attack_serial))&&effect.kind==EffectKind::Warning&&!effect.triggered){effect.age=effect.duration;effect.damage=effect.energy=0;effect.triggered=true;++combat_diagnostics.boss_attacks_cancelled;}
 for(auto& zone:danger_zones)if(boss.boss_attack_serial&&zone.root_attack_id==boss.boss_attack_serial&&zone.age<zone.warning)zone.age=zone.duration;
 boss.windup=boss.action_remaining=0;boss.attack_cooldown=1.f;boss.recovery_remaining=1.2f;boss.boss_contact_hit=false;
}

bool Game::boss_pressure_busy() const {
 for(const auto& boss:enemies)if(boss.kind==EnemyKind::Boss&&boss.hp>0&&(boss.windup>0||boss.action_remaining>0))return true;
 int fields=0;for(const auto& effect:effects)if(effect.hostile&&effect.enemy_kind==EnemyKind::Boss&&effect.kind==EffectKind::Warning&&effect.damage>0&&effect.age<effect.duration)++fields;
 return fields>=3;
}

void Game::spawn_boss_nodes(){
 for(auto& node:pending_boss_nodes_){
  if(enemies.size()>=enemy_capacity()){++pressure_metrics.capacity_limited;continue;}
  auto owner=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& e){return e.id==node.boss_owner&&e.hp>0;});
  if(owner==enemies.end())continue;
  node.id=next_id_++;node.pos=safe_enemy_position(node.pos,node.radius,110);enemies.push_back(node);++combat_diagnostics.boss_summons;
 }
 pending_boss_nodes_.clear();
}

void Game::update_boss_node(Enemy& node,float dt){
 auto owner=std::find_if(enemies.begin(),enemies.end(),[&](const Enemy& e){return e.id==node.boss_owner&&e.kind==EnemyKind::Boss&&e.hp>0;});
 if(owner==enemies.end()){node.hp=0;node.abilities.reward_claimed=true;return;}
 node.node_lifetime-=dt;
 if(node.boss_node==2){
  if(node.age>1.5f)node.pos+=normalized(owner->pos-node.pos)*(42*dt);
  if(length(node.pos-owner->pos)<owner->radius+node.radius){owner->shield_remaining=std::max(owner->shield_remaining,2.8f);node.node_lifetime=0;}
 }
 if(node.node_lifetime<=0){
  if(node.boss_profile==BossProfile::Pirate){Effect fx;fx.kind=EffectKind::Warning;fx.hostile=true;fx.enemy_kind=EnemyKind::Boss;fx.boss_profile=node.boss_profile;fx.pos=node.pos;fx.radius=66;fx.damage=owner->damage*.6f;fx.delay=1.2f;fx.duration=1.5f;fx.count=boss_move_id(node.boss_profile,2);fx.target_id=owner->id;fx.root_attack_id=next_attack_id_++;add_effect(fx);}
  node.hp=0;node.abilities.reward_claimed=true;++combat_diagnostics.boss_nodes_expired;
 }
}

void Game::update_phased_boss(Enemy& e,float dt,std::vector<SummonRequest>&){
 const auto& definition=boss_combat_definition(e.boss_profile);
 const int profile=int(e.boss_profile);
 const int stage=e.hp<=e.max_hp*definition.phase_three?3:e.hp<=e.max_hp*definition.phase_two?2:1;
 e.boss_exposure=std::max(0.f,e.boss_exposure-dt);
 if(stage>e.boss_stage){
  cancel_boss_attack(e);e.boss_stage=stage;e.hit_flash=.25f;++combat_diagnostics.boss_phase_changes;
  record_combat(CombatRecordKind::Phase,stage,std::string(boss_profile_name(e.boss_profile))+" · 阶段变化");add_event(EventKind::Boss,e.pos);
 }
 const Vec2 toward=normalized(player.pos-e.pos),side{-toward.y,toward.x};const float distance=length(player.pos-e.pos);
 if(e.windup>0||e.action_remaining>0||e.recovery_remaining>0)e.attack_animation_time+=dt;
 if(e.windup>0){
  e.windup=std::max(0.f,e.windup-dt);
  if(e.windup==0){e.action_remaining=e.action_duration;e.recovery_remaining=definition.moves[(e.boss_attack-200)%3].recovery;e.attack_cooldown=1.6f*enemy_attack_multiplier();++e.phase;}
  return;
 }
 if(e.action_remaining>0){
  Vec2 previous=e.pos;e.action_remaining=std::max(0.f,e.action_remaining-dt);
  if(e.action_step==1||e.action_step==2){
   e.pos=e.attack_start+(e.attack_end-e.attack_start)*std::clamp((e.action_duration-e.action_remaining)/std::max(.01f,e.action_tick),0.f,1.f);
   if(e.action_step==1&&!e.boss_contact_hit&&length(e.pos-previous)>.0001f&&segment_distance(player.pos,previous,e.pos)<e.radius+12){
    e.boss_contact_hit=true;hurt_player(e.damage,true,false,e.boss_attack,boss_profile_name(e.boss_profile));
   }
  }
  if(e.action_remaining==0&&(e.action_step>0||e.boss_profile==BossProfile::DollRobot||e.boss_profile==BossProfile::Chimera))e.boss_exposure=std::max(e.boss_exposure,e.recovery_remaining);
  return;
 }
 if(e.recovery_remaining>0){e.recovery_remaining=std::max(0.f,e.recovery_remaining-dt);return;}
 e.attack_cooldown=std::max(0.f,e.attack_cooldown-dt);
 if(e.attack_cooldown>0||distance>560||!enemy_attack_visible(e)||effects.size()>config.max_effects){
  Vec2 movement=toward;
  if((profile==1||profile==7||profile==8||profile==11||profile==14||profile==17||profile==19)&&distance<210)movement=toward*(-.45f);
  e.pos+=movement*(e.speed*scene_slow(e.pos,e.radius)*(e.slow_remaining>0?e.slow_factor:1.f)*dt);resolve_scene_collision(e.pos,e.radius);
  if(length(e.pos-player.pos)<e.radius+12)hurt_player(e.damage,true,false,-1,boss_profile_name(e.boss_profile));return;
 }
 const int first[]={0,2,1},second[]={2,0,1,0},last[]={1,2,0,1};
 const int move=stage==1?first[e.phase%3]:stage==2?second[e.phase%4]:last[e.phase%4];
 e.boss_attack=boss_move_id(e.boss_profile,move);e.boss_attack_serial=next_attack_id_++;e.boss_contact_hit=false;
 e.windup=definition.moves[move].windup;e.attack_animation_time=0;e.action_duration=.12f;e.action_step=0;
 e.attack_start=e.pos;e.attack_end=e.pos;e.attack_target=player.pos;e.attack_direction=toward;
 ++metrics.boss_attacks;metrics.content.boss_moves_seen[profile]|=1<<move;add_event(EventKind::BossAttack,e.pos);
 int nodes=0;for(const auto& node:enemies)if(node.hp>0&&node.boss_owner==e.id)++nodes;
 auto field=[&](Vec2 from,Vec2 end,float radius,float after,float factor,int shape=0,float angle=0.f,uint32_t owner=0){
  Effect fx;fx.kind=EffectKind::Warning;fx.pos=from;fx.end=end;fx.radius=radius;fx.pattern=shape;fx.angle=angle;
  fx.delay=e.windup+after;fx.duration=fx.delay+.32f;fx.damage=e.damage*factor;fx.hostile=true;fx.enemy_kind=EnemyKind::Boss;fx.boss_profile=e.boss_profile;fx.target_id=owner?owner:e.id;fx.count=e.boss_attack;fx.root_attack_id=e.boss_attack_serial;add_effect(fx);
  e.action_duration=std::max(e.action_duration,after+.08f);
 };
 auto disk=[&](Vec2 at,float radius,float after=0.f,float factor=.85f){field(at,{},radius,after,factor);};
 auto line=[&](Vec2 from,Vec2 to,float radius,float after=0.f,float factor=.85f){field(from,to,radius,after,factor,1);};
 auto cone=[&](Vec2 at,Vec2 axis,float radius,float half,float after=0.f,float factor=.85f){field(at,axis,radius,after,factor,4,half);};
 auto shot=[&](Vec2 from,Vec2 aim,float velocity,float radius,float after=0.f,bool tracking=false,float factor=.7f,uint32_t owner=0){
  Effect fx;fx.kind=EffectKind::Warning;fx.pos=from;fx.end=enemy_attack_endpoint(from,from+aim*480,radius);fx.radius=radius;fx.pattern=1;fx.delay=e.windup+after;fx.duration=fx.delay+.2f;fx.velocity=aim*velocity;fx.energy=e.damage*factor;fx.secondary_tick=std::max(.1f,length(fx.end-from)/velocity);
  fx.hostile=fx.evolved=true;fx.proc_generated=tracking;fx.enemy_kind=EnemyKind::Boss;fx.boss_profile=e.boss_profile;fx.target_id=owner?owner:e.id;fx.count=e.boss_attack;fx.root_attack_id=e.boss_attack_serial;add_effect(fx);e.action_duration=std::max(e.action_duration,after+.08f);
 };
 auto fan=[&](Vec2 from,Vec2 axis,int count,float spread,float velocity=175.f,float after=0.f,bool tracking=false){for(int i=0;i<count;++i)shot(from,rotate(axis,(float(i)/std::max(1,count-1)-.5f)*spread),velocity,10,after,tracking);};
 auto ring=[&](Vec2 center,float radius,float after,int gap){for(int i=0;i<8;++i)if(i!=gap&&i!=(gap+1)%8)disk(center+rotate({radius,0},i*pi/4),radius*.24f,after,.7f);};
 auto charge=[&](Vec2 end,float duration,bool leap=false){e.action_duration=duration;e.action_tick=duration;e.action_step=leap?2:1;e.attack_end=enemy_attack_endpoint(e.pos,end,e.radius);field(e.attack_start,e.attack_end,e.radius,0,0,1);if(leap)disk(e.attack_end,88,duration,1.05f);};
 auto create_nodes=[&](int count,bool returning=false){
  count=std::min(count,std::max(0,3-nodes));
  for(int i=0;i<count;++i){Enemy n;n.kind=EnemyKind::Minion;n.boss_owner=e.id;n.boss_profile=e.boss_profile;n.boss_node=returning?2:1;n.visual=definition.node_visual;n.radius=19;n.hp=n.max_hp=std::max(24.f,e.max_hp*.025f);n.damage=0;n.pos=e.pos+rotate(toward,(i%2?1.f:-1.f)*(1.1f+i*.22f))*160;n.node_lifetime=returning?10.f:e.boss_profile==BossProfile::Pirate?9.f:18.f;n.attack_cooldown=2;
   pending_boss_nodes_.push_back(n);
  }
 };
 auto node_volley=[&](float after,bool tracking=false){for(const auto& n:enemies)if(n.hp>0&&n.boss_owner==e.id)shot(n.pos,normalized(e.attack_target-n.pos),145,10,after,tracking,.6f,n.id);};
 const int gap=(e.phase+stage*2)%8;
 switch(e.boss_profile){
 case BossProfile::MushroomKing:
  if(move==0)charge(e.attack_target,.72f,true);
  else if(move==1){create_nodes(stage==1?2:3);disk(e.attack_target,64);if(stage>1)disk(e.attack_target+side*135,58,.6f);}
  else{charge(e.pos+toward*330,.82f);for(int i=0;i<std::max(1,nodes);++i)disk(e.attack_end+side*((i-(nodes-1)*.5f)*90),50,1.1f+i*.25f,.65f);}break;
 case BossProfile::ForestGuardian:
  if(move==0){for(int i=-1;i<=1;++i)line(e.pos, e.pos+rotate(toward,i*.45f)*330,21,.25f*(i+1),.75f);}
  else if(move==1){create_nodes(3);fan(e.pos,toward,3,1.2f,150);}
  else{ring(e.attack_target,155,0,gap);node_volley(.7f);if(stage==3)ring(e.attack_target,235,1.1f,(gap+1)%8);}break;
 case BossProfile::KingSlime:
  if(move==0){charge(e.attack_target,.65f,true);if(stage>1)ring(e.attack_end,150,1.2f,gap);}
  else if(move==1){fan(e.pos,toward,5,1.75f,145);if(stage==3)fan(e.pos,toward,4,1.2f,165,.65f);}
  else{create_nodes(3,true);ring(e.pos,110,0,gap);}break;
 case BossProfile::SnailKing:
  if(move==0){for(int i=0;i<3;++i)fan(e.pos,rotate(toward,i*.7f),stage+2,1.7f,125,i*.42f);}
  else if(move==1){for(int i=0;i<3;++i)line(e.pos,e.attack_target+toward*125,28,i*.85f,.6f);}
  else{cone(e.pos,toward*(-1),230,1.05f);if(stage>1)cone(e.pos,toward,170,.65f,.7f);}break;
 case BossProfile::Golem:
  if(move==0){line(e.pos+side*52,e.pos+toward*230+side*52,37);line(e.pos-side*52,e.pos+toward*260-side*52,37,.65f);e.boss_exposure=e.windup+.7f+definition.moves[move].recovery;}
  else if(move==1){for(int i=-1;i<=1;++i)line(e.pos,e.pos+rotate(toward,i*.5f)*380,23,(i+1)*.4f);}
  else{ring(e.attack_target,145,0,gap);disk(e.attack_target,58,.9f);if(stage>1)disk(e.attack_target+toward*120,55,1.5f);}break;
 case BossProfile::Balrog:
  if(move==0){fan(e.pos,toward,5,1.7f,190);if(stage>1)fan(e.pos,rotate(toward,.2f),4,1.5f,165,.7f);}
  else if(move==1){charge(e.pos+toward*400,.7f);line(e.attack_start,e.attack_end,27,1.1f,.7f);}
  else{for(int i=-1;i<=1;++i)disk(e.attack_target+side*(i*120.f)+toward*(i==0?60.f:-60.f),61,(i+1)*.4f);if(stage==3)line(e.attack_target-side*180,e.attack_target+side*180,20,1.5f);}break;
 case BossProfile::Snowman:
  if(move==0){for(int i=0;i<(stage==1?1:3);++i)shot(e.pos,rotate(toward,(i-(stage==1?0:1))*.32f),135,22);}
  else if(move==1){for(int i=0;i<3;++i)disk(e.attack_target+toward*(i*95.f),62,i*.6f,.8f);}
  else{for(int i=-1;i<=1;++i){Vec2 p=e.pos+toward*115+side*(i*100.f);disk(p,58,(i+1)*.45f);if(stage>1)disk(p,58,1.6f+(i+1)*.45f,.6f);}}break;
 case BossProfile::Pirate:
  if(move==0){for(int i=-1;i<=1;++i)if(i!=(stage+e.phase)%3-1)line(e.attack_target+side*(i*115.f)-toward*230,e.attack_target+side*(i*115.f)+toward*230,24);node_volley(.55f);}
  else if(move==1){for(int i=-1;i<=1;++i)shot(e.pos+side*(i*75.f),rotate(toward,i*.2f),240,13,(i+1)*.25f);if(stage>1)fan(e.pos,toward,3,.7f,180,.95f);}
  else{create_nodes(stage==1?2:3);disk(e.attack_target,65,0,.7f);}break;
 case BossProfile::Clockwork:
  if(move==0){int n=2+(nodes>0?1:0);for(int i=0;i<n;++i)line(e.pos,e.pos+rotate(toward,(i-(n-1)*.5f)*.65f)*330,19,i*.6f,.8f);}
  else if(move==1){for(int i=0;i<3;++i)disk(e.attack_target+side*((i-1)*115.f),56,i*.65f);DangerZone z;z.kind=DangerKind::Distortion;z.pos=e.attack_target;z.radius=115;z.warning=e.windup+1.5f;z.duration=z.warning+6;z.owner_id=e.id;z.root_attack_id=e.boss_attack_serial;z.from_boss=true;add_danger_zone(z);}
  else{create_nodes(2);ring(e.pos,175,0,gap);if(stage>1)node_volley(.8f);}break;
 case BossProfile::Leviathan:
  if(move==0){cone(e.pos,toward,300,.60f,0,1.0f);fan(e.pos,toward,3,1.5f,160,.7f);}
  else if(move==1){charge(e.pos+toward*395,.85f);for(int i=-1;i<=1;i+=2)disk(e.attack_end+side*(i*95.f),55,1.1f);}
  else{create_nodes(3);for(int i=-1;i<=1;++i)disk(e.attack_target+side*(i*130.f),53,(i+1)*.3f);node_volley(1.2f);}break;
 case BossProfile::Cactus:
  if(move==0){for(int i=-1;i<=1;++i)line(e.pos+rotate(toward,i*.65f)*40,e.pos+rotate(toward,i*.65f)*250,25);}
  else if(move==1){charge(e.pos+toward*340,.75f);node_volley(1.f);if(stage==3)disk(e.attack_end,65,1.2f);}
  else{create_nodes(3);ring(e.attack_target,145,0,gap);}break;
 case BossProfile::Faust:
  if(move==0){for(int i=0;i<3;++i)disk(e.attack_target+rotate({115,0},i*2*pi/3),54,i*.38f);if(stage==3)disk(e.attack_target,60,1.35f);}
  else if(move==1){fan(e.pos,toward,2+stage,.75f,125,0,true);node_volley(.5f,true);}
  else{create_nodes(3);line(e.attack_target-side*180,e.attack_target+side*180,24,0,.75f);}break;
 case BossProfile::GiantCrab:
  if(move==0){cone(e.pos+side*80,rotate(toward,.25f),220,.38f);cone(e.pos-side*80,rotate(toward,-.25f),220,.38f,.45f);}
  else if(move==1){charge(e.pos+side*((e.phase%2?1.f:-1.f)*180),.7f);fan(e.attack_end,normalized(e.attack_target-e.attack_end),3,1.15f,175,.9f);}
  else{create_nodes(2);node_volley(.2f);ring(e.pos,180,.7f,gap);}break;
 case BossProfile::DollRobot:
  if(move==0){for(int i=-1;i<=1;i+=2)fan(e.pos+side*(i*45.f),rotate(toward,i*.35f),2,.45f,155,0,true);}
  else if(move==1){for(int i=-2;i<=2;++i)if(i!=(e.phase%3)-1)line(e.pos+side*(i*65.f),e.pos+toward*390+side*(i*65.f),16,(i+2)*.22f,.6f);}
  else{disk(e.pos,150,0,1.1f);ring(e.pos,235,.9f,gap);e.boss_exposure=e.windup+5.2f;}break;
 case BossProfile::Zeno:
  if(move==0){for(int i=-1;i<=1;++i)if(i!=(gap%3)-1)line(e.attack_target+side*(i*125.f)-toward*220,e.attack_target+side*(i*125.f)+toward*220,21);}
  else if(move==1){create_nodes(3);fan(e.pos,toward,3,1.4f,170);}
  else{for(const auto& n:enemies)if(n.hp>0&&n.boss_owner==e.id)field(n.pos,e.attack_target+normalized(e.attack_target-n.pos)*100,23,.5f,.8f,1,0,n.id);line(e.pos,e.attack_target,25,0,.9f);if(stage==3)line(e.attack_target-side*180,e.attack_target+side*180,18,1.1f);}break;
 case BossProfile::TaeRoon:
  if(move==0){disk(e.pos+toward*75,95);ring(e.pos,205,.85f,gap);}
  else if(move==1){cone(e.pos,rotate(toward,-.45f),265,.65f);cone(e.pos,rotate(toward,.55f),265,.65f,.8f);}
  else{charge(e.pos+toward*310,.72f);disk(e.attack_end+side*75,60,1.05f);disk(e.attack_end-side*75,60,1.55f);}break;
 case BossProfile::NineTail:
  if(move==0){for(int i=-1;i<=1;++i)shot(e.pos+side*(i*65.f),rotate(toward,i*.45f),140,11,0,true);node_volley(.7f);}
  else if(move==1){charge(e.attack_target+toward*90,.65f);line(e.attack_target-side*160,e.attack_target+side*160,25,1.1f);if(stage>1)line(e.attack_target-toward*150,e.attack_target+toward*150,22,1.65f);}
  else{create_nodes(3);ring(e.attack_target,165,0,gap);}break;
 case BossProfile::Eliza:
  if(move==0){fan(e.pos,rotate(toward,-.4f),3,1.2f,150);fan(e.pos,rotate(toward,.4f),3,1.2f,180,.6f);node_volley(1.f);}
  else if(move==1){ring(e.attack_target,150,0,gap);if(stage>1)ring(e.attack_target,225,1.f,(gap+1)%8);}
  else{create_nodes(3);cone(e.pos,toward,220,.8f,.4f);}break;
 case BossProfile::Chimera:
  if(move==0){cone(e.pos,toward,275,.55f);for(int i=0;i<3;++i)disk(e.attack_target,70,.65f+i*.85f,.5f);}
  else if(move==1){for(int i=-1;i<=1;++i)line(e.pos+side*(i*65.f),e.pos+toward*240+side*(i*65.f),26,(i+1)*.35f);}
  else{line(e.pos,e.pos+toward*440,44,0,1.1f);if(stage>1)line(e.pos,e.pos+toward*440,30,.9f,.75f);e.boss_exposure=e.windup+4.5f;}break;
 case BossProfile::Dodo:
  if(move==0){ring(e.pos,145,0,gap);ring(e.pos,245,1.f,stage>1?(gap+2)%8:gap);}
  else if(move==1){for(int i=-1;i<=1;++i){shot(e.pos,rotate(toward,i*.5f),150,13);shot(e.attack_target+toward*230+side*(i*90.f),toward*(-1),130,11,1.f);}node_volley(1.6f);}
  else{create_nodes(3);cone(e.pos,toward,230,.75f);}break;
 case BossProfile::Lilynouch:
  if(move==0){charge(e.pos+toward*410,.8f);node_volley(1.15f);if(stage>1)line(e.attack_end-side*170,e.attack_end+side*170,23,1.4f);}
  else if(move==1){for(int i=0;i<3;++i)disk(e.attack_target+side*((i-1)*120.f)+toward*(i%2*70.f),62,i*.55f);}
  else{create_nodes(2);ring(e.attack_target,160,0,gap);}break;
 case BossProfile::Lyka:
  if(move==0){charge(e.pos+toward*350,.9f);for(int i=-1;i<=1;i+=2)line(e.attack_start+side*(i*95.f),e.attack_end+side*(i*95.f),nodes>0?25.f:15.f,1.1f,.65f);}
  else if(move==1){disk(e.pos,90);ring(e.pos,170,.65f,gap);if(stage>1)ring(e.pos,260,1.3f,gap);}
  else{create_nodes(3);fan(e.pos,toward,3,1.5f,160);}break;
 case BossProfile::Manon:
  if(move==0){cone(e.pos,toward,345,.58f,0,1.1f);for(int i=0;i<2;++i)line(e.pos+toward*65,e.pos+toward*310,35,.9f+i*.85f,.55f);}
  else if(move==1){charge(e.attack_target,.85f,true);ring(e.attack_end,175,1.3f,gap);}
  else{for(int sign:{-1,1})cone(e.pos,rotate(toward,sign*1.05f),290,.35f,sign<0?0:.7f);if(stage==3)disk(e.attack_target,55,1.4f);}break;
 case BossProfile::Griffey:
  if(move==0){for(int i=0;i<4;++i)fan(e.pos,rotate(toward,i*pi/2),stage==3?2:1,.3f,205);node_volley(.6f);}
  else if(move==1){charge(e.attack_target+toward*125,.75f);cone(e.attack_end,toward*(-1),180,.7f,1.1f);}
  else{create_nodes(2);ring(e.attack_target,175,0,gap);if(stage>1)ring(e.attack_target,250,1.f,gap);}break;
 }
 e.velocity={};
}
}

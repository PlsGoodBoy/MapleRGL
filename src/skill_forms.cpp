// Stable Expansion fusion-form metadata. Display and combat reads do not use RNG.
#include "game.hpp"

namespace maple {
namespace {
struct FormMetadata {
 Skill result;
 const char* keys[2];
 const char* names[2];
 const char* descriptions[2];
};

// Form IDs are stable: 0 is the first form/default for older completed fusions;
// 1 is its alternative. These are two choices, never rarity or power tiers.
constexpr FormMetadata forms[]={
 {Skill::FrostStorm,{"condensation","thunder"},{"凝霜","奔雷"},
  {"寒霜区域更大、减速更强；雷击节拍较慢，适合控制分散敌群。",
   "寒霜区域较小、控制较弱；雷击与传导更快，适合集中处理区域内的敌人。"}},
 {Skill::DragonArmor,{"guard","rage"},{"守御","狂怒"},
  {"抵挡积蓄15龙魂、主动命中积蓄2；咆哮护盾最长2秒，偏重反击与生存。",
   "主动命中积蓄10龙魂、抵挡积蓄8；咆哮护盾最长0.9秒，偏重持续进攻。"}},
 {Skill::SwordCyclone,{"scatter","focus"},{"散华","归锋"},
  {"环绕剑刃周期性散射剑气，首道校准强敌，其余覆盖四周；近身群体覆盖更广。",
   "固定三道剑气集中射向标记的强敌，近身覆盖减少、剑气只命中一个目标；适合持续压制主要目标。"}},
 {Skill::TwinHunt,{"patrol","headhunt"},{"巡空","猎首"},
  {"凤凰与银鹰优先选择不同目标，分散标记并交替射击；对单个目标的集中爆发较弱。",
   "双翼共同标记并追击同一强敌，同时射击；近身巡游范围和穿透能力降低。"}},
 {Skill::PoisonFlare,{"spread","consume"},{"蔓延","焚尽"},
  {"毒区更大，保留一次有限传播；分区引燃只消耗一层毒，单次毒爆威力较低。",
   "毒区较小、不会传播；中心引燃消耗全部自身毒层形成集中爆发，随后重新积累。"}},
 {Skill::DragonPhoenix,{"sweep","dive"},{"巡天","焚星"},
  {"圣龙与凤凰交替锁定战场中心扫过，交汇时产生光焰冲击；双兽轨迹持续可见。",
   "圣龙与凤凰围绕同一强敌集中俯冲并引爆落点；扫场路径与区域覆盖减少。"}},
};

const FormMetadata* metadata_for(Skill result){
 for(const auto& metadata:forms)if(metadata.result==result)return &metadata;
 return nullptr;
}
}

int Game::form_count(Skill result){return metadata_for(result)?2:1;}

int Game::fusion_form(Skill result) const {
 // Held scrolls and current offers never change an already equipped fusion.
 // Save/offer validation rejects invalid form IDs; this read remains safe for
 // an older completed fusion that has no explicit form record yet.
 for(const auto& completed:scrolls.completed)if(completed.result==result)
  return completed.form>=0&&completed.form<form_count(result)?completed.form:0;
 return 0;
}

const char* Game::scroll_form_key(Skill result,int form){
 const auto* metadata=metadata_for(result);
 if(!metadata)return form==0?"original":"unknown";
 return form>=0&&form<2?metadata->keys[form]:"unknown";
}

const char* Game::scroll_form_name(Skill result,int form){
 const auto* metadata=metadata_for(result);
 if(!metadata)return form==0?"原典":"未知形态";
 return form>=0&&form<2?metadata->names[form]:"未知形态";
}

const char* Game::scroll_form_description(Skill result,int form){
 const auto* metadata=metadata_for(result);
 if(!metadata)return form==0?"保留该配方的完整行为。":"此形态尚未开放。";
 return form>=0&&form<2?metadata->descriptions[form]:"此形态尚未开放。";
}
}

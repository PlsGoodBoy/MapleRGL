#include "adventure_content.hpp"
#include <set>

namespace maple {
const AdventureSkill* adventure_skill(Skill skill){
 static const AdventureSkill values[]={
#define X(id,s,key,name,desc,interval,roles,style,icon) {Skill::s,key,name,desc,interval,roles,style,icon},
#include "adventure_catalogue.inc"
#undef X
 };
 const int index=int(skill)-90;return index>=0&&index<int(std::size(values))?&values[index]:nullptr;
}
std::vector<Skill> fusion_inputs(const FusionRecipe& r){std::vector<Skill> result{r.first,r.second};if(r.third!=Skill::Count)result.push_back(r.third);if(r.fourth!=Skill::Count)result.push_back(r.fourth);return result;}
void append_adventure_recipes(std::vector<FusionRecipe>& recipes){
 using S=Skill;
 auto pair=[&](S a,S b,S out){recipes.push_back({a,b,out,CharacterClass::Archer,5});};
 pair(S::TripleStar,S::ShadowClone,S::ThousandStars);pair(S::ShadowBoomerang,S::PoisonDart,S::ToxicDance);
 pair(S::Assassinate,S::SmokeAssault,S::NightChase);pair(S::CoinBomb,S::ShadowStitch,S::CoinPrison);
 pair(S::SixBlade,S::Orbit,S::AsuraBlades);pair(S::ShadowBats,S::HolyLight,S::TwilightHunt);
 pair(S::DualGuns,S::Shotgun,S::GunFeast);pair(S::Penetrator,S::Sniper,S::ShipBreaker);
 pair(S::Grenade,S::Zone,S::BurningSea);pair(S::Uppercut,S::EnergyBlast,S::SoulUppercut);
 pair(S::SeaSerpent,S::Lightning,S::ThunderSerpent);pair(S::Cannon,S::OctopusTurret,S::DeepFleet);
 pair(S::TripleStar,S::Lightning,S::ThunderStars);pair(S::PoisonDart,S::Rain,S::PoisonRain);
 pair(S::ShadowClone,S::DualGuns,S::MirrorGuns);pair(S::ShadowStitch,S::Meteor,S::StarPrison);
 pair(S::CoinBomb,S::BlastArrow,S::GoldenFlame);pair(S::ShadowBats,S::Summon,S::GhostWings);
 pair(S::SmokeAssault,S::Charge,S::FlashRaid);pair(S::SixBlade,S::Uppercut,S::BladeFist);
 pair(S::GlacierSpear,S::SeaSerpent,S::IceSeaSpear);pair(S::DragonBreath,S::DragonConcert,S::ElementDragon);
 pair(S::Missiles,S::Engineer,S::SkySwarm);pair(S::PhantomCards,S::RoseFinale,S::PhantomFinale);
 pair(S::HunterMark,S::TripleStar,S::MarkedConstellation);pair(S::HunterMark,S::Missiles,S::GuidedSalvo);
 pair(S::ArrowTrap,S::Grenade,S::BreachMines);pair(S::ArrowTrap,S::ShadowStitch,S::SnareExecution);
 pair(S::OrbitalOrb,S::PhantomCards,S::CardOrbit);pair(S::OrbitalOrb,S::DragonConcert,S::ElementOrrery);
 pair(S::GravityWell,S::Shockwave,S::GravityBreaker);pair(S::GravityWell,S::Engineer,S::GravityFoundry);
 pair(S::HeavyStrike,S::Uppercut,S::TitanCombo);pair(S::HeavyStrike,S::GlacierSpear,S::GlacierHammer);
 pair(S::Bulwark,S::EnergyBlast,S::EnergyBastion);pair(S::Bulwark,S::SmokeAssault,S::ShadowBastion);
 pair(S::FrostCombo,S::SpearCombo,S::WinterMarch);pair(S::DragonThrust,S::SeaSerpent,S::TideBreaker);
 pair(S::Homing,S::Missiles,S::ArcaneBarrage);pair(S::Arrow,S::PhantomCards,S::CardVolley);pair(S::Penetrator,S::Shockwave,S::RiftCannon);
 // Each route explicitly lists the only predecessor results allowed to substitute its bases.
 // Generate a finite set of concrete disjoint input entries, never substitute runtime ancestry.
 auto route=[&](S out,std::vector<S> bases,std::vector<S> predecessors,int priority){
  std::vector<std::pair<S,std::vector<S>>> parts;for(S s:bases)parts.push_back({s,{s}});
  for(S s:predecessors){auto found=std::find_if(recipes.begin(),recipes.end(),[&](const FusionRecipe& r){return r.result==s;});if(found!=recipes.end())parts.push_back({s,found->sources});}
  std::vector<S> chosen,used;
  auto visit=[&](auto&& self,size_t begin)->void{
   if(used.size()==bases.size()){
    if(chosen.size()<2||chosen.size()>4)return;
    FusionRecipe r{chosen[0],chosen[1],out,CharacterClass::Archer,5};
    if(chosen.size()>2)r.third=chosen[2];if(chosen.size()>3)r.fourth=chosen[3];r.sources=bases;r.priority=priority;recipes.push_back(r);return;
   }
   for(size_t i=begin;i<parts.size();++i){const auto& part=parts[i];bool legal=!part.second.empty();for(S base:part.second)legal&=std::find(bases.begin(),bases.end(),base)!=bases.end()&&std::find(used.begin(),used.end(),base)==used.end();if(!legal)continue;
    size_t old=used.size();used.insert(used.end(),part.second.begin(),part.second.end());chosen.push_back(part.first);self(self,i+1);chosen.pop_back();used.resize(old);
   }
  };visit(visit,0);
 };
 for(auto& r:recipes)r.sources={r.first,r.second};
 route(S::ThunderShadow,{S::TripleStar,S::ShadowClone,S::Lightning},{S::ThousandStars,S::ThunderStars},100);
 route(S::BreachFireweb,{S::DualGuns,S::Shotgun,S::Grenade},{S::GunFeast},100);
 route(S::MechanicalSiege,{S::Missiles,S::Engineer,S::OctopusTurret},{S::SkySwarm},100);
 route(S::ColdPrison,{S::Frost,S::Lightning,S::MagicVortex},{S::FrostStorm},100);
 route(S::DragonOath,{S::Retaliate,S::HolyShield,S::Nova},{S::EchoShield,S::DragonArmor},100);
 route(S::FrostFlame,{S::Summon,S::Rain,S::IceArrow},{S::PhoenixRain,S::FrozenBarrage},100);
 route(S::HolyPhoenix,{S::HolyLight,S::HolyDragon,S::Summon},{S::HeavenDragon,S::DragonPhoenix},100);
 route(S::NightBanquet,{S::PhantomCards,S::RoseFinale,S::Assassinate},{S::PhantomFinale},100);
 route(S::VenomLock,{S::ShadowBoomerang,S::PoisonDart,S::ShadowStitch},{S::ToxicDance},100);
 route(S::TidalFist,{S::Uppercut,S::EnergyBlast,S::Shockwave},{S::SoulUppercut},100);
 route(S::PolarDragon,{S::GlacierSpear,S::SeaSerpent,S::FrostCombo},{S::IceSeaSpear},100);
 route(S::DragonOrrery,{S::DragonBreath,S::DragonConcert,S::OrbitalOrb},{S::ElementDragon,S::ElementOrrery},100);
 route(S::ToxicThunder,{S::TripleStar,S::ShadowClone,S::Lightning,S::PoisonDart},{S::ThousandStars,S::ThunderStars,S::ThunderShadow},100);
 route(S::PirateAssault,{S::DualGuns,S::Shotgun,S::Grenade,S::Cannon},{S::GunFeast,S::BreachFireweb},100);
 route(S::SeaFortress,{S::Missiles,S::Engineer,S::OctopusTurret,S::Cannon},{S::SkySwarm,S::DeepFleet,S::MechanicalSiege},100);
 route(S::StarfallPrison,{S::Frost,S::Lightning,S::MagicVortex,S::Meteor},{S::FrostStorm,S::ColdPrison},100);
 route(S::DragonFistArmor,{S::Retaliate,S::HolyShield,S::Nova,S::Uppercut},{S::EchoShield,S::DragonArmor,S::DragonOath},100);
 route(S::StarRain,{S::Summon,S::Rain,S::IceArrow,S::Meteor},{S::PhoenixRain,S::FrozenBarrage,S::FrostFlame},100);
 route(S::ElementSanctum,{S::HolyLight,S::HolyDragon,S::Summon,S::DragonConcert},{S::HeavenDragon,S::DragonPhoenix,S::HolyPhoenix},100);
 route(S::EternalMask,{S::PhantomCards,S::RoseFinale,S::Assassinate,S::ShadowBats},{S::PhantomFinale,S::NightBanquet},100);
 for(auto& r:recipes){r.material_level=5;r.id=Game::skill_key(r.result);for(S input:fusion_inputs(r))r.id+="_"+std::string(Game::skill_key(input));}
}
int fusion_rank(const FusionRecipe& recipe){return std::min(5,1+int(fusion_inputs(recipe).size()));}
const FusionRecipe* next_fusion(const std::array<int,skill_count>& levels){
 std::vector<const FusionRecipe*> candidates;
 for(const auto& r:fusion_recipes()){if(!r.released||levels[int(r.result)])continue;bool ready=true;for(Skill s:fusion_inputs(r))ready&=levels[int(s)]>=r.material_level;if(ready)candidates.push_back(&r);}
 if(candidates.empty())return nullptr;
 auto better=[](const FusionRecipe* a,const FusionRecipe* b){if(a->sources.size()!=b->sources.size())return a->sources.size()>b->sources.size();if(a->priority!=b->priority)return a->priority>b->priority;return a->id<b->id;};
 return *std::min_element(candidates.begin(),candidates.end(),better);
}
std::vector<const FusionRecipe*> fusion_plan(std::array<int,skill_count> levels){
 std::vector<const FusionRecipe*> plan;while(const auto* r=next_fusion(levels)){plan.push_back(r);for(Skill s:fusion_inputs(*r))levels[int(s)]=0;levels[int(r->result)]=fusion_rank(*r);}return plan;
}
}

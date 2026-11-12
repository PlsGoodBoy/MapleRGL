#include "game.hpp"
#include "encounter_pool.hpp"
#include "boss_combat.hpp"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace maple {
namespace {
constexpr float pi=3.14159265358979323846f;
Vec2 rotated(Vec2 direction,float angle){float c=std::cos(angle),s=std::sin(angle);return {direction.x*c-direction.y*s,direction.x*s+direction.y*c};}
}

const char* Game::boss_profile_name(BossProfile profile){return boss_definition(profile).name;}
const char* Game::boss_sprite_key(BossProfile profile){return boss_definition(profile).sprite;}
float Game::boss_render_scale(BossProfile profile){return boss_definition(profile).scale;}
float Game::boss_death_duration(BossProfile profile){return boss_definition(profile).death_duration;}
const char* Game::enemy_visual_key(int visual){return enemy_definition(visual).key;}
const char* Game::enemy_visual_name(int visual){return enemy_definition(visual).name;}
float Game::enemy_visual_scale(int visual){return enemy_definition(visual).scale;}
float Game::enemy_visual_radius(int visual){return enemy_definition(visual).radius;}
float Game::enemy_death_duration(int visual){return enemy_definition(visual).death_duration;}
const char* Game::boss_attack_name(int attack){
 if(attack>=200&&attack<200+boss_profile_count*3)return boss_combat_definition(BossProfile((attack-200)/3)).moves[(attack-200)%3].name;
 static const char* names[]={"孢子落点","直线冲锋","种子扇射","双重孢子","交叉根须","召唤护卫",
  "根须封路","双向种子","弹跳重压","水灵分裂","蜗壳扇射","螺旋弹幕","岩拳重击","地裂分叉","展翼飞扑","十字暗焰",
  "滚动雪球","连环冰震","直线炮击","侧移扇射","分区光束","时序网格","龙息扇面","掠空冲击","毒沙扇尾","沙漠突进","分散诅咒","幽灵追猎"};
 return attack>=0&&attack<int(sizeof(names)/sizeof(*names))?names[attack]:"蓄力攻击";
}

void Game::set_combat_view(float half_width,float half_height){
 if(std::isfinite(half_width)&&std::isfinite(half_height)&&half_width>0&&half_height>0){combat_half_width_=half_width;combat_half_height_=half_height;}
}
bool Game::enemy_attack_visible(const Enemy& e) const {
 Vec2 delta=e.pos-player.pos;
 // The source must already overlap the view before it can announce a ranged/charge attack.
 return std::abs(delta.x)<=combat_half_width_+e.radius*.5f&&std::abs(delta.y)<=combat_half_height_+e.radius*.5f;
}
Vec2 Game::enemy_attack_endpoint(Vec2 from,Vec2 wanted,float radius) const {
 if(!config.enable_scene_objects)return wanted;
 Vec2 delta=wanted-from;float travel=length(delta);if(travel<.001f)return from;
 Vec2 direction=delta*(1/travel);float allowed=travel;
 for(const auto& object:scene_objects)if(scene_solid(object)){
  Vec2 relative=object.pos-from;float along=relative.x*direction.x+relative.y*direction.y;
  float limit=object.radius+radius;float perpendicular=relative.x*relative.x+relative.y*relative.y-along*along;
  if(perpendicular>=limit*limit)continue;
  float reach=std::sqrt(std::max(0.f,limit*limit-perpendicular));
  if(along+reach<0)continue;
  allowed=std::min(allowed,std::max(0.f,along-reach-1.f));
 }
 return from+direction*allowed;
}

}

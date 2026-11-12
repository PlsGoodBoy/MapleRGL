#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace maple::ui {
inline constexpr ImU32 panel_color=IM_COL32(22,33,44,240),secondary=IM_COL32(34,51,66,240),border=IM_COL32(199,165,107,115);
inline constexpr ImU32 text=IM_COL32(244,234,219,255),muted=IM_COL32(183,193,200,255),warm=IM_COL32(199,165,107,255),accent=IM_COL32(232,154,74,255),ready=IM_COL32(108,197,210,255),danger=IM_COL32(232,117,117,255),fusion=IM_COL32(182,164,219,255);
inline ImVec4 color(ImU32 value){return ImGui::ColorConvertU32ToFloat4(value);}
inline void apply(){
 ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();s.WindowRounding=12;s.ChildRounding=8;s.FrameRounding=5;s.PopupRounding=8;s.WindowPadding={20,16};s.FramePadding={12,5};s.ItemSpacing={12,8};s.ItemInnerSpacing={8,4};s.ScrollbarSize=12;
 s.Colors[ImGuiCol_WindowBg]=color(panel_color);s.Colors[ImGuiCol_ChildBg]=color(secondary);s.Colors[ImGuiCol_PopupBg]=color(panel_color);s.Colors[ImGuiCol_Text]=color(text);s.Colors[ImGuiCol_TextDisabled]=color(muted);s.Colors[ImGuiCol_Border]=color(border);
 for(auto c:{ImGuiCol_Button,ImGuiCol_FrameBg,ImGuiCol_Header})s.Colors[c]=color(secondary);
 for(auto c:{ImGuiCol_ButtonHovered,ImGuiCol_FrameBgHovered,ImGuiCol_HeaderHovered})s.Colors[c]={.27f,.36f,.43f,1};
 for(auto c:{ImGuiCol_ButtonActive,ImGuiCol_FrameBgActive,ImGuiCol_HeaderActive})s.Colors[c]={.38f,.32f,.23f,1};
 s.Colors[ImGuiCol_CheckMark]=color(ready);s.Colors[ImGuiCol_SliderGrab]=color(accent);s.Colors[ImGuiCol_SliderGrabActive]=color(warm);s.Colors[ImGuiCol_TitleBg]=color(secondary);s.Colors[ImGuiCol_TitleBgActive]=color(secondary);s.Colors[ImGuiCol_ModalWindowDimBg]={.02f,.04f,.06f,.62f};
 s.FrameBorderSize=1;s.Colors[ImGuiCol_FrameBg]={.06f,.10f,.14f,1};s.Colors[ImGuiCol_Tab]=color(secondary);s.Colors[ImGuiCol_TabSelected]={.28f,.35f,.40f,1};s.Colors[ImGuiCol_TabHovered]={.35f,.40f,.43f,1};
}
inline void panel(ImDrawList* d,ImVec2 at,ImVec2 size){d->AddRectFilled(at,{at.x+size.x,at.y+size.y},panel_color,8);d->AddRect(at,{at.x+size.x,at.y+size.y},border,8);}
inline bool primary_button(const char* label,ImVec2 size={0,0}){ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.53f,.32f,.13f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.69f,.43f,.20f,1));bool pressed=ImGui::Button(label,size);ImGui::PopStyleColor(2);return pressed;}
inline void scroll_icon(ImDrawList* d,ImVec2 at,float edge,bool owned){ImU32 ink=owned?warm:muted;d->AddRectFilled({at.x+3,at.y+2},{at.x+edge-3,at.y+edge-2},secondary,2);d->AddRect({at.x+3,at.y+2},{at.x+edge-3,at.y+edge-2},ink,2);for(int i=0;i<2;++i)d->AddLine({at.x+6,at.y+6+i*4},{at.x+edge-6,at.y+6+i*4},ink);}
inline void levels(ImDrawList* d,ImVec2 at,float width,int actual,int effective,int cap){
 if(cap<=0)return;int groups=std::min(cap,10);float gap=3,edge=(width-gap*(groups-1))/groups;
 for(int i=0;i<groups;++i){int rank=1+i*cap/groups;ImVec2 a{at.x+i*(edge+gap),at.y},b{a.x+edge,a.y+7};bool owned=actual>=rank,active=effective>=rank;
  d->AddRectFilled(a,b,owned?(active?ready:IM_COL32(85,92,103,255)):secondary,1);d->AddRect(a,b,owned?ready:border,1);
  if(owned&&!active)d->AddLine({a.x,a.y+7},{b.x,a.y},danger,1.5f);
 }
}
}

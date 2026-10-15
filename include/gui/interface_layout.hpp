#pragma once
#include "imgui.h"
#include <algorithm>
namespace gui {
inline int panelPlacement=2;
inline bool topBarVisible=true, topBarHover=false;
inline float topBarHeight(){return std::max(44.f*ImGui::GetStyle().FontScaleMain,ImGui::GetFrameHeight()+8);}
inline float topInset(){return topBarVisible?topBarHeight():0.f;}
inline bool horizontalPanels(){return panelPlacement==0||panelPlacement==1||panelPlacement==3;}
// Preserve the native slider's keyboard, drag and focus behavior, with a line and circular thumb.
inline bool mediaSeek(const char* id,float* value,float duration) {
    const float s=ImGui::GetStyle().FontScaleMain;
    for(auto color:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive})
        ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,14*s);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    bool changed=ImGui::SliderFloat(id,value,0,std::max(1.f,duration),"",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput);
    ImGui::PopStyleVar(2);ImGui::PopStyleColor(5);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    const float left=a.x+2+7*s,right=b.x-2-7*s,y=(a.y+b.y)*.5f;
    const float x=left+(right-left)*std::clamp(*value/std::max(1.f,duration),0.f,1.f);
    auto* d=ImGui::GetWindowDrawList();
    d->AddLine(ImVec2(left,y),ImVec2(right,y),ImGui::GetColorU32(ImGuiCol_TextDisabled),2*s);
    d->AddLine(ImVec2(left,y),ImVec2(x,y),ImGui::GetColorU32(ImGuiCol_SliderGrabActive),3*s);
    d->AddCircleFilled(ImVec2(x,y),6*s,ImGui::GetColorU32(ImGuiCol_Text));
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Seek, %.0f of %.0f seconds",*value,duration);
    return changed;
}
}

#pragma once
#include "imgui.h"
#include "gui/interface_layout.hpp"
#include <algorithm>

namespace gui {
enum class PanelIcon { Telemetry, Charts, Controls, Map, Replay, Settings, Flow, Table };
inline int panelButtonIndex=0;
inline float panelSidebarWidth() { return panelPlacement==2?64.f*ImGui::GetStyle().FontScaleMain:0.f; }
inline void beginPanelSidebar(const char* id,int count) {
    const float s=ImGui::GetStyle().FontScaleMain,button=48*s,gap=6*s,pad=8*s;
    const bool horizontal=horizontalPanels();
    const auto display=ImGui::GetIO().DisplaySize;
    const float length=count*button+(count-1)*gap+2*pad;
    const ImVec2 size(horizontal?std::min(display.x,length):button+2*pad,
        horizontal?button+2*pad:std::min(std::max(1.f,display.y-topInset()),length));
    const float x=panelPlacement==0?(display.x-size.x)*.5f:panelPlacement>=3?display.x-size.x:0;
    const float y=panelPlacement==0?display.y-size.y:topInset();
    ImGui::SetNextWindowPos(ImVec2(std::max(0.f,x),std::max(0.f,y)));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(pad,pad));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(gap,gap));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin(id,nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar(3);
    panelButtonIndex=0;
}
inline void panelToggle(const char* label,PanelIcon icon,bool& open) {
    const float s=ImGui::GetStyle().FontScaleMain;
    if(panelButtonIndex++>0 && horizontalPanels())ImGui::SameLine(0,6*s);
    ImGui::PushID(label);
    const bool wasOpen=open;
    if(wasOpen) ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if(ImGui::Button("##toggle",ImVec2(48*s,48*s))) open=!open;
    if(open) { // A stripe also distinguishes the active state without relying on color.
        auto p=ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddRectFilled(p,ImVec2(p.x+3*s,p.y+48*s),ImGui::GetColorU32(ImGuiCol_Text));
    }
    if(wasOpen) ImGui::PopStyleColor();
    const auto origin=ImGui::GetItemRectMin();
    auto p=[&](float x,float y){return ImVec2(origin.x+(9+x*1.5f)*s,origin.y+(9+y*1.5f)*s);};
    auto* d=ImGui::GetWindowDrawList();const auto c=ImGui::GetColorU32(ImGuiCol_Text);
    auto line=[&](float x,float y,float a,float b){d->AddLine(p(x,y),p(a,b),c,1.5f*s);};
    switch(icon) {
    case PanelIcon::Telemetry: line(1,10,5,10);line(5,10,8,3);line(8,3,12,17);line(12,17,15,10);line(15,10,19,10);break;
    case PanelIcon::Charts: line(2,1,2,18);line(2,18,19,18);line(5,14,10,8);line(10,8,14,11);line(14,11,19,3);break;
    case PanelIcon::Controls: case PanelIcon::Settings:
        for(int i=0;i<3;++i){float y=4.f+i*6;line(1,y,19,y);d->AddCircleFilled(p(i==1?6.f:14.f,y),2.5f*s,c);}break;
    case PanelIcon::Map: d->AddCircle(p(10,10),9*s,c);line(1,10,19,10);d->AddEllipse(p(10,10),ImVec2(4*s,9*s),c);break;
    case PanelIcon::Replay: d->AddTriangle(p(5,2),p(18,10),p(5,18),c,1.5f*s);break;
    case PanelIcon::Flow: for(int i=0;i<3;++i){float y=3.f+i*6;line(1,y,9,y);line(9,y,19,y+4);}break;
    case PanelIcon::Table: d->AddRect(p(1,2),p(19,18),c);line(1,7,19,7);line(1,12,19,12);line(8,2,8,18);break;
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride)||(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible)) ImGui::SetTooltip("%s (%s)",label,open?"open":"closed");
    ImGui::PopID();
}
}

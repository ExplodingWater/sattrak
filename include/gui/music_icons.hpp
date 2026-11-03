#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace gui {
enum class MusicIcon { Previous, Play, Pause, Next };

// Lucide's original SVG paths, rendered as scalable, theme-colored outlines.
// Sources and upstream license: assets/icons/lucide/.
inline void drawMusicIcon(ImDrawList* draw, ImVec2 origin, float scale, ImU32 color, MusicIcon icon) {
    const float stroke=2*scale;
    auto p=[&](float x,float y){return ImVec2(origin.x+x*scale,origin.y+y*scale);};
    auto point=[&](float x,float y){draw->PathLineTo(p(x,y));};
    // All arcs in these upstream SVGs are small circular arcs of radius two.
    auto arc=[&](float x0,float y0,float x1,float y1,bool clockwise){
        const float dx=x1-x0,dy=y1-y0;
        const float chord2=dx*dx+dy*dy;
        const float radius2=std::max(4.f,chord2*.25f);
        const float factor=(clockwise?1.f:-1.f)*std::sqrt(std::max(0.f,(radius2-chord2*.25f)/chord2));
        const float cx=(x0+x1)*.5f-dy*factor,cy=(y0+y1)*.5f+dx*factor;
        const float start=std::atan2(y0-cy,x0-cx);
        float angle=std::atan2(y1-cy,x1-cx)-start;
        constexpr float tau=6.28318530718f;
        if(clockwise&&angle<0)angle+=tau;
        if(!clockwise&&angle>0)angle-=tau;
        const float radius=std::sqrt(radius2);
        for(int i=1;i<8;++i){const float a=start+angle*i/8;point(cx+radius*std::cos(a),cy+radius*std::sin(a));}
        point(x1,y1);
    };
    auto finish=[&]{draw->PathStroke(color,ImDrawFlags_Closed,stroke);};
    auto bar=[&](float x){
        draw->AddLine(p(x,4),p(x,20),color,stroke);
        draw->AddCircleFilled(p(x,4),scale,color);
        draw->AddCircleFilled(p(x,20),scale,color);
    };
    switch(icon) {
    case MusicIcon::Play:
        point(5,5);arc(5,5,8.008f,3.272f,true);
        point(20.005f,10.270f);arc(20.005f,10.270f,20.008f,13.728f,true);
        point(8.008f,20.728f);arc(8.008f,20.728f,5,19,true);finish();break;
    case MusicIcon::Pause:
        draw->AddRect(p(14,3),p(19,21),color,scale,0,stroke);
        draw->AddRect(p(5,3),p(10,21),color,scale,0,stroke);break;
    case MusicIcon::Previous:
        point(17.971f,4.285f);arc(17.971f,4.285f,21,6,true);
        point(21,18);arc(21,18,17.971f,19.715f,true);
        point(7.974f,13.717f);arc(7.974f,13.717f,7.971f,10.285f,true);
        finish();bar(3);break;
    case MusicIcon::Next:
        bar(21);
        point(6.029f,4.285f);arc(6.029f,4.285f,3,6,false);
        point(3,18);arc(3,18,6.029f,19.715f,false);
        point(16.026f,13.717f);arc(16.026f,13.717f,16.029f,10.285f,false);
        finish();break;
    }
}

inline bool musicIconButton(const char* id, MusicIcon icon) {
    const float scale=ImGui::GetStyle().FontScaleMain;
    const char* name=icon==MusicIcon::Previous?"Previous":icon==MusicIcon::Next?"Next":icon==MusicIcon::Play?"Play":"Pause";
    ImGui::PushID(id);
    const std::string label=std::string("      ")+name;
    const bool clicked=ImGui::Button(label.c_str(),ImVec2(0,28*scale));
    const auto min=ImGui::GetItemRectMin();
    // A 20px icon sits centered in the 28px control, leaving the word readable.
    drawMusicIcon(ImGui::GetWindowDrawList(),ImVec2(min.x+5*scale,min.y+4*scale),20.f/24*scale,
        ImGui::GetColorU32(ImGuiCol_Text),icon);
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride)||(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible))
        ImGui::SetTooltip("%s",icon==MusicIcon::Previous?"Previous music track":icon==MusicIcon::Next?"Next music track":icon==MusicIcon::Play?"Play music":"Pause music");
    ImGui::PopID();
    return clicked;
}
}

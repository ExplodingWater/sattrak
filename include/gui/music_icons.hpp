#pragma once
#include "imgui.h"

namespace gui {
enum class MusicIcon { Previous, Play, Pause, Next };
// Vector coordinates from Feather's 24px SVG icons; see assets/icons/feather/LICENSE.
inline bool musicIconButton(const char* id, MusicIcon icon) {
    const bool clicked=ImGui::Button(id,ImVec2(28,22));
    const auto min=ImGui::GetItemRectMin();
    auto p=[&](float x,float y){return ImVec2(min.x+5+x*.75f,min.y+2+y*.75f);};
    auto* draw=ImGui::GetWindowDrawList();
    const auto color=ImGui::GetColorU32(ImGuiCol_Text);
    auto line=[&](float x,float y,float a,float b){draw->AddLine(p(x,y),p(a,b),color,1.5f);};
    if(icon==MusicIcon::Pause) {
        draw->AddRect(p(6,4),p(10,20),color,0,0,1.5f);
        draw->AddRect(p(14,4),p(18,20),color,0,0,1.5f);
    } else if(icon==MusicIcon::Play) {
        draw->AddTriangle(p(5,3),p(19,12),p(5,21),color,1.5f);
    } else if(icon==MusicIcon::Previous) {
        draw->AddTriangle(p(19,20),p(9,12),p(19,4),color,1.5f);line(5,19,5,5);
    } else {
        draw->AddTriangle(p(5,4),p(15,12),p(5,20),color,1.5f);line(19,5,19,19);
    }
    return clicked;
}
}

#pragma once
#include "imgui.h"
#include <algorithm>

namespace gui::theme {
inline constexpr const char* names[] = {"Original (blue)", "Green", "Purple", "Yellow", "Black", "Red"};
inline constexpr int count = 6;
inline void apply(ImGuiStyle& style, const ImGuiStyle& original, int selected, bool highContrast) {
    selected = std::clamp(selected, 0, count-1);
    const ImVec4 accents[] = {{.40f,.83f,1,1},{.45f,1,.60f,1},{.82f,.65f,1,1},
                             {1,.88f,.35f,1},{.90f,.90f,.90f,1},{1,0,0,1}};
    const auto accent = accents[selected];
    auto tint = [&](float value) { return ImVec4(accent.x*value,accent.y*value,accent.z*value,1); };
    for (int i=0;i<ImGuiCol_COUNT;++i) {
        auto color=original.Colors[i];
        if (selected!=0) {
            const float hi=std::max({color.x,color.y,color.z});
            const float lo=std::min({color.x,color.y,color.z});
            if (hi-lo>.025f) { auto replacement=tint(hi); replacement.w=color.w; color=replacement; }
        }
        style.Colors[i]=color;
    }
    if (!highContrast) return;
    auto* c=style.Colors;
    c[ImGuiCol_Text]=ImVec4(1,1,1,1);
    c[ImGuiCol_TextDisabled]=ImVec4(.85f,.85f,.85f,1);
    for (auto slot : {ImGuiCol_WindowBg,ImGuiCol_ChildBg,ImGuiCol_PopupBg,ImGuiCol_ScrollbarBg,
                      ImGuiCol_TitleBg,ImGuiCol_TitleBgCollapsed,ImGuiCol_MenuBarBg,ImGuiCol_TableHeaderBg}) c[slot]=tint(.035f);
    for (auto slot : {ImGuiCol_FrameBg,ImGuiCol_Button,ImGuiCol_Header,ImGuiCol_Tab,
                      ImGuiCol_TabDimmed,ImGuiCol_TabDimmedSelected}) c[slot]=tint(.075f);
    for (auto slot : {ImGuiCol_FrameBgHovered,ImGuiCol_ButtonHovered,ImGuiCol_HeaderHovered,
                      ImGuiCol_TabHovered,ImGuiCol_TitleBgActive}) c[slot]=tint(.12f);
    for (auto slot : {ImGuiCol_FrameBgActive,ImGuiCol_ButtonActive,ImGuiCol_HeaderActive,
                      ImGuiCol_TabSelected}) c[slot]=tint(.18f);
    for (auto slot : {ImGuiCol_Border,ImGuiCol_Separator,ImGuiCol_SeparatorHovered,ImGuiCol_SeparatorActive,
                      ImGuiCol_CheckMark,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive,ImGuiCol_NavCursor,
                      ImGuiCol_ScrollbarGrab,ImGuiCol_ScrollbarGrabHovered,ImGuiCol_ScrollbarGrabActive,
                      ImGuiCol_ResizeGrip,ImGuiCol_ResizeGripHovered,ImGuiCol_ResizeGripActive,
                      ImGuiCol_TabSelectedOverline,ImGuiCol_TabDimmedSelectedOverline}) c[slot]=accent;
    c[ImGuiCol_TextSelectedBg]=tint(.22f);
    style.FrameBorderSize=1;
}
} // namespace gui::theme

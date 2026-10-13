#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace gui::accessibility {
// This is keyboard and visual access support, not an OS accessibility bridge.
inline bool panelKeyboardMode = false;
inline bool flightKeyboardFocus = false;

inline bool keyboardControlsAllowed(bool pointerOverScene) {
    const auto& io = ImGui::GetIO();
    return (flightKeyboardFocus || pointerOverScene) && !panelKeyboardMode &&
        !io.NavVisible && !io.WantTextInput && !io.AppFocusLost &&
        !io.KeyCtrl && !io.KeyAlt && !ImGui::IsAnyItemActive() &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

inline void itemDescription(const char* text) {
    if (ImGui::IsItemFocused() || ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline double luminance(const ImVec4& color) {
    auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
    return .2126 * linear(color.x) + .7152 * linear(color.y) + .0722 * linear(color.z);
}
inline double contrast(const ImVec4& a, const ImVec4& b) {
    const double x = luminance(a), y = luminance(b);
    return (std::max(x, y) + .05) / (std::min(x, y) + .05);
}

inline void applyContrastPalette() {
    auto& style = ImGui::GetStyle();
    auto* c = style.Colors;
    // Opaque surfaces make contrast independent of the bright Earth underneath.
    c[ImGuiCol_Text] = ImVec4(.95f,.97f,1,1);
    c[ImGuiCol_TextDisabled] = ImVec4(.72f,.79f,.86f,1);
    c[ImGuiCol_WindowBg] = ImVec4(.04f,.06f,.10f,1);
    c[ImGuiCol_ChildBg] = ImVec4(.04f,.06f,.10f,1);
    c[ImGuiCol_PopupBg] = ImVec4(.05f,.07f,.12f,1);
    c[ImGuiCol_FrameBg] = c[ImGuiCol_Button] = ImVec4(.08f,.12f,.18f,1);
    c[ImGuiCol_FrameBgHovered] = c[ImGuiCol_ButtonHovered] = ImVec4(.12f,.20f,.28f,1);
    c[ImGuiCol_FrameBgActive] = c[ImGuiCol_ButtonActive] = ImVec4(.12f,.25f,.34f,1);
    c[ImGuiCol_Header] = c[ImGuiCol_Tab] = ImVec4(.08f,.12f,.20f,1);
    c[ImGuiCol_HeaderHovered] = c[ImGuiCol_TabHovered] = ImVec4(.12f,.20f,.28f,1);
    c[ImGuiCol_HeaderActive] = c[ImGuiCol_TabSelected] = ImVec4(.12f,.25f,.34f,1);
    c[ImGuiCol_Border] = c[ImGuiCol_Separator] = ImVec4(.56f,.67f,.77f,1);
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = ImVec4(.40f,.83f,1,1);
    c[ImGuiCol_SliderGrabActive] = ImVec4(.70f,.92f,1,1);
    c[ImGuiCol_NavCursor] = ImVec4(1,.87f,.35f,1);
    c[ImGuiCol_ScrollbarBg] = ImVec4(.04f,.06f,.10f,1);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(.56f,.67f,.77f,1);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(.66f,.77f,.87f,1);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(.76f,.87f,.97f,1);
    style.FrameBorderSize = 1;
}
} // namespace gui::accessibility

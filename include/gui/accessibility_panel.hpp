#pragma once
#include "gui/accessibility.hpp"
#include "gui/action_bindings.hpp"
#include <fstream>
#include <iterator>
#include <string>

namespace gui {
class AccessibilityPanel {
public:
    bool visible = false;
    void open() { visible = true; focus = true; accessibility::panelKeyboardMode = true; }
    void focusFlight() {
        accessibility::panelKeyboardMode = false;
        accessibility::flightKeyboardFocus = true;
        visible = false;
        ImGui::SetWindowFocus(nullptr);
    }
    void keyboard(bool consoleOpen) {
        if (consoleOpen || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
        if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) {
            if (accessibility::panelKeyboardMode || ImGui::GetIO().NavVisible) focusFlight(); else open();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) accessibility::flightKeyboardFocus = false;
    }
    void draw(const std::string& description, int& studio, ActionBindings& actions) {
        if (!visible) return;
        const auto size = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(std::min(700.f,size.x-24),std::min(640.f,size.y-60)), ImGuiCond_FirstUseEver);
        if (focus) ImGui::SetNextWindowFocus();
        if (ImGui::Begin("Accessibility & help", &visible)) {
            if (focus) { ImGui::SetKeyboardFocusHere(); ImGui::SetNavCursorVisible(true); focus = false; }
            if (ImGui::Button("Return to flight controls (F10)")) focusFlight();
            ImGui::TextWrapped("Tab / Shift+Tab: next / previous control. Arrows: navigate or adjust. Enter / Space: activate. Escape: back out of a menu or edit. Ctrl+Tab: switch panels. F10: switch between this panel and flight controls.");
            ImGui::TextWrapped("While navigating controls, camera keys and action shortcuts are suspended. In flight focus, WASD moves, Q/E or C/Space moves vertically, arrows look around, and Page Up / Page Down zooms without the mouse.");
            if (ImGui::Button("Orbital studio")) studio = 0;
            ImGui::SameLine(); if (ImGui::Button("CFD studio")) studio = 1;
            if (ImGui::Button("Open Settings")) { studio = 0; actions.run("settings.toggle"); }
            ImGui::SeparatorText("Text alternative to the current visualization");
            if (snapshot.empty()) snapshot = description;
            if (ImGui::Button("Refresh text snapshot")) snapshot = description;
            ImGui::SameLine(); if (ImGui::Button("Copy snapshot")) ImGui::SetClipboardText(snapshot.c_str());
            ImGui::InputTextMultiline("##Snapshot", snapshot.data(), snapshot.size()+1,
                ImVec2(-1,ImGui::GetTextLineHeight()*7),ImGuiInputTextFlags_ReadOnly);
            ImGui::TextWrapped("The snapshot stays still while you read. Refresh it for current values. Full screen-reader navigation of this native interface is not yet supported. You can copy the text into your preferred accessible reader.");
            if (ImGui::CollapsingHeader("Terms of Use (draft)")) {
                if (terms.empty()) {
                    std::ifstream file("TERMS.md");
                    terms.assign(std::istreambuf_iterator<char>(file),{});
                    if (terms.empty()) terms = "TERMS.md was not found beside the application. See TERMS.md in the project repository.";
                }
                if (ImGui::Button("Copy terms")) ImGui::SetClipboardText(terms.c_str());
                ImGui::TextWrapped("%s",terms.c_str());
            }
        }
        ImGui::End();
        if (!visible) accessibility::panelKeyboardMode = false;
    }
private:
    bool focus = false;
    std::string snapshot, terms;
};
} // namespace gui

#pragma once
#include "gui/accessibility_panel.hpp"
#include "imgui_internal.h"
#include <iostream>

namespace gui {
struct AccessibilityChecks {
    static void input(int frame) {
        auto& io = ImGui::GetIO();
        if (frame==1 || frame==9) io.AddKeyEvent(ImGuiKey_F10,true);
        if (frame==2 || frame==10) io.AddKeyEvent(ImGuiKey_F10,false);
        if (frame==4) io.AddKeyEvent(ImGuiKey_Tab,true);
        if (frame==5) io.AddKeyEvent(ImGuiKey_Tab,false);
        if (frame==6) io.AddKeyEvent(ImGuiKey_Space,true);
        if (frame==7) io.AddKeyEvent(ImGuiKey_Space,false);
    }
    static bool check(int frame, const AccessibilityPanel& panel, int& studio) {
        bool okay = true;
        const auto fail = [&](bool pass,const char* name) { if(!pass){std::cerr << "Accessibility: " << name << '\n';okay=false;} };
        if (frame==0) {
            auto& io = ImGui::GetIO();
            const auto& style = ImGui::GetStyle();
            for (auto bg : {ImGuiCol_WindowBg,ImGuiCol_ChildBg,ImGuiCol_PopupBg,
                            ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,
                            ImGuiCol_Button,ImGuiCol_ButtonHovered,ImGuiCol_ButtonActive,
                            ImGuiCol_Header,ImGuiCol_HeaderHovered,ImGuiCol_HeaderActive}) {
                for (auto fg : {ImGuiCol_Text,ImGuiCol_TextDisabled})
                    fail(accessibility::contrast(style.Colors[fg],style.Colors[bg]) >= 4.5,"text contrast below 4.5:1");
                fail(style.Colors[bg].w==1,"contrast surface must be opaque");
                fail(accessibility::contrast(style.Colors[ImGuiCol_NavCursor],style.Colors[bg]) >= 3,"focus contrast below 3:1");
                fail(accessibility::contrast(style.Colors[ImGuiCol_Border],style.Colors[bg]) >= 3,"control border contrast below 3:1");
            }
            const bool nav = io.NavVisible, text = io.WantTextInput, lost = io.AppFocusLost;
            io.NavVisible=false;io.WantTextInput=false;io.AppFocusLost=false;
            accessibility::flightKeyboardFocus=true;
            fail(accessibility::keyboardControlsAllowed(false),"flight focus must work without hovering the scene");
            io.NavVisible=true;
            fail(!accessibility::keyboardControlsAllowed(true),"keyboard navigation must block camera keys");
            io.NavVisible=false;io.WantTextInput=true;
            fail(!accessibility::keyboardControlsAllowed(true),"text editing must block camera keys");
            io.NavVisible=nav;io.WantTextInput=text;io.AppFocusLost=lost;
            accessibility::flightKeyboardFocus=false;
            fail(!ActionBindings::allowed(ImGuiKey_F10),"F10 must remain available for focus recovery");
        }
        static ImGuiID firstFocus=0;
        if(frame==3) {
            fail(panel.visible && accessibility::panelKeyboardMode,"F10 must open help");
            firstFocus=GImGui->NavId;
            fail(firstFocus!=0,"help must give a control keyboard focus");
            fail(GImGui->NavCursorVisible,"keyboard focus must be visible");
        }
        if(frame==5) {
            fail(GImGui->NavId!=0 && GImGui->NavId!=firstFocus,"Tab must move focus");
            studio=1;
        }
        // The next control after Return to flight is Orbital studio. Space activates it.
        if(frame==8)fail(studio==0,"Space must activate the focused studio button");
        if(frame==11)fail(!panel.visible && accessibility::flightKeyboardFocus && !accessibility::panelKeyboardMode,"F10 must return to flight");
        return okay;
    }
};
} // namespace gui

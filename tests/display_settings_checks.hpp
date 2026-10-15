#pragma once
#include "gui/display_settings.hpp"
#include "gui/color_theme.hpp"
#include "gui/accessibility.hpp"
#include <iostream>
namespace gui {
struct DisplaySettingsChecks {
    static void scale(DisplaySettings& settings,int percent){settings.uiScale=percent;}
    static void confirm(DisplaySettings& settings){settings.confirming=true;settings.deadline=glfwGetTime()+15;settings.previous=settings.current;}
    static bool run(DisplaySettings& settings) {
        if(settings.monitors.empty())return false;
        auto initial=settings.current;
        std::string reason;bool okay=true;
        for(int theme=0;theme<gui::theme::count;++theme)for(bool high:{false,true}) {
            settings.colorTheme=theme;settings.highContrast=high;
            settings.uiScale=150;settings.update();
            const auto a=ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
            settings.uiScale=100;settings.update();
            const auto b=ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
            okay &= a.x==b.x && a.y==b.y && a.z==b.z;
            if(high)for(auto surface:{ImGuiCol_WindowBg,ImGuiCol_ChildBg,ImGuiCol_PopupBg,ImGuiCol_FrameBg,
                ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_Button,ImGuiCol_ButtonHovered,
                ImGuiCol_ButtonActive,ImGuiCol_Header,ImGuiCol_HeaderHovered,ImGuiCol_HeaderActive,
                ImGuiCol_TabSelected,ImGuiCol_TitleBgActive,ImGuiCol_MenuBarBg}) {
                const auto* colors=ImGui::GetStyle().Colors;
                okay &= colors[surface].w==1;
                okay &= accessibility::contrast(colors[ImGuiCol_Text],colors[surface])>=7;
                okay &= accessibility::contrast(colors[ImGuiCol_TextDisabled],colors[surface])>=7;
                okay &= accessibility::contrast(colors[ImGuiCol_NavCursor],colors[surface])>=3;
                okay &= accessibility::contrast(colors[ImGuiCol_Border],colors[surface])>=3;
            }
            if(theme==0 && !high)for(int i=0;i<ImGuiCol_COUNT;++i) {
                const auto x=ImGui::GetStyle().Colors[i],y=settings.baseStyle.Colors[i];
                okay &= x.x==y.x && x.y==y.y && x.z==y.z && x.w==y.w;
            }
        }
        settings.colorTheme=0;settings.highContrast=true;settings.update();
        settings.uiScale=150;settings.update();
        okay &= ImGui::GetStyle().FontScaleMain==1.5f;
        settings.uiScale=100;settings.update();
        okay &= ImGui::GetStyle().FontScaleMain==1.0f && ImGui::GetStyle().FramePadding.x==settings.baseStyle.FramePadding.x;
        const auto monitor=settings.monitors[settings.selected];
        settings.mode=2;settings.width=0;settings.height=720;
        okay &= !settings.valid(reason);
        settings.width=monitor.maxWidth+1;okay &= !settings.valid(reason);
        settings.width=800;settings.height=600;okay &= settings.valid(reason);
        settings.mode=0;settings.width=1;okay &= !settings.valid(reason);
        // Borderless uses desktop dimensions, independent of a stale custom input.
        settings.mode=1;settings.queued=true;settings.update();
        int width,height;glfwGetWindowSize(settings.window,&width,&height);
        okay &= settings.confirming && !glfwGetWindowMonitor(settings.window);
        okay &= !glfwGetWindowAttrib(settings.window,GLFW_DECORATED);
        okay &= width==monitor.width && height==monitor.height;
        settings.deadline=glfwGetTime()-1;settings.update();
        okay &= !settings.confirming && glfwGetWindowAttrib(settings.window,GLFW_DECORATED);
        // Exercise exclusive fullscreen at the existing desktop mode, avoiding a resolution switch.
        settings.mode=0;settings.width=monitor.width;settings.height=monitor.height;
        settings.queued=true;settings.update();
        okay &= settings.confirming && glfwGetWindowMonitor(settings.window)==monitor.handle;
        settings.reverting=true;settings.update();
        settings.mode=2;settings.width=800;settings.height=600;settings.queued=true;settings.update();
        glfwGetWindowSize(settings.window,&width,&height);
        okay &= settings.confirming && width==800 && height==600;
        settings.previous=initial;settings.previous.monitor=monitor.handle;settings.reverting=true;settings.update();
        std::cout<<(okay?"Twelve theme variants, scale retention, high contrast, display modes and timed revert passed.\n":"Display settings checks failed.\n");
        return okay;
    }
};
}

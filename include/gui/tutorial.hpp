#pragma once
#include "gui/tutorial_state.hpp"
#include "gui/adcs_view.hpp"
#include "gui/cfd_view.hpp"
#include "gui/developer_console.hpp"
#include "gui/accessibility_panel.hpp"
#include "gui/interface_layout.hpp"
#include "imgui_internal.h"
#include <array>
#include <iostream>

namespace gui {
// Complete the step type before initializing the constexpr table (Apple Clang).
struct TutorialStep { const char* title; const char* window; const char* text; int panel=-1; int category=-1; float scroll=0; };

class Tutorial {
public:
    using Step = TutorialStep;
    inline static constexpr Step steps[] = {
        {"Welcome to sattrak", "", "Explore Earth from orbit. This guided tour opens each part of the interface for you. The highlighted area is the subject of the current step. Use < and >, or Choose a step to jump anywhere. Skip tutorial is always available. The simulation is paused during the tour."},
        {"Your panel buttons", "Orbital panels", "These icons toggle Telemetry, Live charts, Flight director and camera, World map, Orbit replay and Settings. Hover an icon for its name. Settings > Video > Interface lets you place the strip at the bottom center or along either top edge, horizontally or vertically."},
        {"Top bar and music", "TopBar", "Help opens keyboard navigation and a text description of the scene. Simulation switches studios; Tools contains the shock explorer. The music controls provide previous, play/pause, next, a seek line and volume. On narrow screens they are inside Music. FPS reports rendering speed."},
        {"Hide or reveal the top bar", "Top bar handle", "Click this subtle arrow to collapse or expand the top bar. Settings > Video > Interface can also enable revealing the bar when you hover over this handle."},
        {"Telemetry", "GNC & Orbital Telemetry", "Read attitude estimates, tracking and visibility information here. The target elevation tells you whether Earth blocks the line of sight. These are simulation results, not live spacecraft telemetry.",0},
        {"Live charts", "Live Strip-Charts (ImPlot)", "Watch the plotted attitude and control values evolve over simulation time. Use the plot axes to inspect changing values. Pause from Flight director when you want to study a moment.",1},
        {"Flight director", "Flight Director & Camera", "Open Settings or orbit replay, pause/resume time, and inject a tumble to observe recovery. Earth rotation controls change the facing longitude; Auto-Spin advances it with time. These controls change the simulation when you use them after the tour.",2},
        {"Camera views and multiple views", "Flight Director & Camera", "Choose Satellite Orbit to follow the spacecraft, Free Orbit to orbit the scene, or Free Flycam for unrestricted flight. Other presets show chase, sensor, nadir, solar-wing, ground-station and orbital-plane perspectives. Multi View compares cameras; it can use separate desktop windows. Outside panels, WASD moves, Q/E moves vertically, right-drag or arrows looks, and the wheel zooms.",2,-1,.28f},
        {"Time, orbit and ground targets", "Flight Director & Camera", "Time Warp changes simulation speed; Spin Warp changes Earth's rotation. Altitude and Inclination set the demonstration circular orbit when available. The location cards choose a ground target for pointing and the ground camera. Restore edge layout reattaches dragged panels.",2,-1,.73f},
        {"World map", "2D Ground Track (ECEF)", "The map shows the spacecraft's ground track and selected target in Earth-fixed coordinates. Compare its path with the 3D view and check the target visibility message.",3},
        {"Import and replay a trajectory", "Recorded orbit replay", "Import a supported CSV or OEM trajectory, then use the replay controls to seek through time. CSV requires UTC and Earth-centered position/velocity in km and km/s, with a frame declaration. Invalid files show an error and preserve the loaded path. See the GitHub orbit-file guide for a complete example.",4},
        {"Record, compare and inspect passes", "Recorded orbit replay", "Record path captures your simulated trajectory. Stop recording before Save or Save As. Save your work before starting a new recording or exiting. Comparison paths let you inspect differences; ground-station coordinates and an elevation mask define pass calculations. Scroll this window after the tour to explore these sections.",4,-1,1.f},
        {"Settings: General", "Settings###Camera & simulation settings", "Restart this tutorial here at any time. Choose the spacecraft model, physical scale and motion settings. Master volume controls all audio; radio has its own level and mute. Satellite data controls choose offline use or optional catalog updates. Predictions depend on the age of the orbital elements.",5,0},
        {"Settings: Video", "Settings###Camera & simulation settings", "Adjust field of view, marker opacity, HUD panels, altimeters and trail visibility. Display settings choose monitor, resolution and window mode, with a timed confirmation to recover from an unsuitable mode. Interface controls set theme, contrast, UI scale, button placement and top-bar hover behavior.",5,1},
        {"Settings: Controls", "Settings###Camera & simulation settings", "Adjust camera movement, look and wheel sensitivity. Edit key bindings and ordered macros in this category; conflicting bindings are rejected. Defaults include P for pause, F2 for Settings, F9 for recording and Ctrl+S to save a stopped recording. Shortcuts are suspended while typing.",5,2},
        {"Settings: Advanced", "Settings###Camera & simulation settings", "Explore atmospheric entry previews, orbital dynamics, spacecraft properties, sensors and pointing controls. Read each control's hover explanation before changing it. The entry preview is illustrative and does not solve a full drag-driven trajectory, ablation or breakup.",5,3},
        {"Console", "Console###ConsoleLayoutV2", "Press backtick (`) to open or close the console after the tour. Type help for commands. Up/Down selects a suggestion; Tab completes it. Commands can adjust camera, simulation and audio settings. radio.play 01 plays the first bundled radio recording; radio.stop stops it."},
        {"Help and keyboard navigation", "Accessibility & help", "F10 opens help or returns focus to flight controls. Tab and Shift+Tab move between controls; Enter or Space activates them. Help includes a stable text snapshot you can refresh and copy. Full native screen-reader navigation is not yet supported."},
        {"Shock explorer: flowfield", "2D Compressible Supersonic Flowfield", "Find this optional tool under Simulation > Tools > Supersonic shock explorer. The flowfield visualizes a two-dimensional supersonic flow over a ramp. Its own icon strip toggles the flowfield, solver controls and verification benchmark."},
        {"Shock explorer: solver controls", "Supersonic CFD Controls", "Set inflow Mach number, ramp angle and solver parameters, choose a contour, or pause and reset the experiment. These are a separate simulation from the orbital studio. Return with Simulation > Orbital tracking."},
        {"Shock explorer: verification", "Analytical Rankine-Hugoniot Verification Benchmark", "Compare the numerical shock result with the analytical reference. Errors and unavailable reference solutions help identify unsuitable conditions. This is an exploration and verification tool, not a substitute for a validated engineering analysis."},
        {"Ready to explore", "Orbital panels", "You have reached the end. Finish tutorial restores the panels, studio and paused/running state you had before starting. You can return to any step now, or restart later from Settings > General > Restart guided tutorial. Hover controls for more detailed explanations."}
    };
    static constexpr int totalSteps = int(std::size(steps));
    explicit Tutorial(bool remember): remember_(remember), pending_(remember && !tutorialSeen(tutorialStatePath())) {}
    bool active() const { return active_; }
    int currentStep() const { return currentStep_; }
    void goTo(int step) { currentStep_=std::clamp(step,0,totalSteps-1); preparedStep_=-1; }
    void prepare(ADCSView& view, CFDView& cfd, DeveloperConsole& console, AccessibilityPanel& help, int& studio) {
        if ((pending_ || tutorialRequested) && !active_ && !view.m_askInternet &&
            !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)) {
            pending_=tutorialRequested=false;
            saved_={};
            std::copy(std::begin(view.m_panelVisible),std::end(view.m_panelVisible),saved_.panels.begin());
            saved_.settings=view.m_settingsVisible; saved_.replay=view.m_replayWindow;
            saved_.hud=view.m_hudVisible; saved_.paused=view.m_isPaused; saved_.category=view.m_settingsCategory;
            saved_.bar=topBarVisible; saved_.studio=studio; saved_.console=console.open; saved_.help=help.visible;
            saved_.flow=cfd.m_flowVisible; saved_.controls=cfd.m_controlsVisible; saved_.benchmark=cfd.m_benchmarkVisible;
            saved_.cfdPaused=cfd.m_isPaused;
            active_=tutorialRunning=true; goTo(0);
        }
        if (!active_) return;
        view.m_isPaused=true; cfd.m_isPaused=true;
        if (preparedStep_==currentStep_) return;
        preparedStep_=currentStep_;
        const auto& step=steps[currentStep_];
        studio=currentStep_>=18 && currentStep_<=20 ? 1 : 0;
        topBarVisible=true; view.m_hudVisible=true;
        std::fill(std::begin(view.m_panelVisible),std::end(view.m_panelVisible),false);
        view.m_settingsVisible=step.panel==5; view.m_openSettings=false; view.m_replayWindow=step.panel==4;
        if(step.panel>=0 && step.panel<4)view.m_panelVisible[step.panel]=true;
        if(step.category>=0)view.m_settingsCategory=step.category;
        console.open=currentStep_==16; console.focus=false; help.visible=currentStep_==17;
        cfd.m_flowVisible=currentStep_==18; cfd.m_controlsVisible=currentStep_==19; cfd.m_benchmarkVisible=currentStep_==20;
        if (auto* window=ImGui::FindWindowByName(step.window)) {
            ImGui::SetWindowCollapsed(window,false);
            const auto display=ImGui::GetIO().DisplaySize;
            ImGui::SetWindowPos(window,ImVec2(
                std::clamp(window->Pos.x,0.f,std::max(0.f,display.x-window->Size.x)),
                std::clamp(window->Pos.y,0.f,std::max(0.f,display.y-window->Size.y))));
            ImGui::SetScrollY(window,step.scroll*window->ScrollMax.y);
        }
        settleFrames_=0;
    }
    void finish(ADCSView& view, CFDView& cfd, DeveloperConsole& console, AccessibilityPanel& help, int& studio, bool completed) {
        if(!active_)return;
        std::copy(saved_.panels.begin(),saved_.panels.end(),std::begin(view.m_panelVisible));
        view.m_settingsVisible=saved_.settings; view.m_replayWindow=saved_.replay;
        view.m_hudVisible=saved_.hud; view.m_isPaused=saved_.paused; view.m_settingsCategory=saved_.category;
        topBarVisible=saved_.bar; studio=saved_.studio; console.open=saved_.console; help.visible=saved_.help;
        cfd.m_flowVisible=saved_.flow; cfd.m_controlsVisible=saved_.controls; cfd.m_benchmarkVisible=saved_.benchmark;
        cfd.m_isPaused=saved_.cfdPaused;
        active_=tutorialRunning=false;
        if(remember_ && !saveTutorialState(tutorialStatePath(),completed))
            std::cerr << "Could not save tutorial preference; the tour may appear on next launch.\n";
    }
    bool targetVisible() const { return targetVisible_; }
    void draw(ADCSView& view, CFDView& cfd, DeveloperConsole& console, AccessibilityPanel& help, int& studio) {
        if(!active_)return;
        const auto display=ImGui::GetIO().DisplaySize;
        if(display.x<100 || display.y<100)return;
        const float scale=ImGui::GetStyle().FontScaleMain;
        const auto& step=steps[currentStep_];
        ImVec2 a(display.x*.25f,display.y*.15f),b(display.x*.75f,display.y*.55f);
        targetVisible_=!step.window[0];
        if(auto* target=ImGui::FindWindowByName(step.window);target && target->Active) {
            targetVisible_=true;
            // Newly created windows may use a desktop-sized default position.
            // Fit them after their first Begin as well as when revisiting a step.
            const ImVec2 fitted(std::min(target->Size.x,display.x),std::min(target->Size.y,display.y));
            ImGui::SetWindowSize(target,fitted);
            ImGui::SetWindowPos(target,ImVec2(std::clamp(target->Pos.x,0.f,std::max(0.f,display.x-fitted.x)),
                std::clamp(target->Pos.y,0.f,std::max(0.f,display.y-fitted.y))));
            if(settleFrames_<3) { ImGui::SetWindowCollapsed(target,false); ImGui::SetScrollY(target,step.scroll*target->ScrollMax.y); }
            a=target->Pos; b=ImVec2(a.x+target->Size.x,a.y+target->Size.y);
        }
        ++settleFrames_;
        a.x=std::clamp(a.x,0.f,display.x); a.y=std::clamp(a.y,0.f,display.y);
        b.x=std::clamp(b.x,a.x,display.x); b.y=std::clamp(b.y,a.y,display.y);
        targetVisible_=targetVisible_ && b.x>a.x && b.y>a.y;
        const float margin=12*scale;
        const float cardWidth=std::min(440*scale,display.x-2*margin);
        const float cardHeight=std::min(350*scale,display.y*.48f);
        const bool targetOnRight=(a.x+b.x)*.5f>display.x*.55f;
        ImVec2 card(targetOnRight?margin:display.x-cardWidth-margin,display.y-cardHeight-margin);
        // If the viewport is too small to fit both, keep the readable card clear
        // and spotlight only the visible portion of the target above it.
        if(b.x>card.x && a.x<card.x+cardWidth && b.y>card.y && a.y<card.y+cardHeight)
            b.y=std::max(a.y,std::min(b.y,card.y-margin));
        ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(display);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
        ImGui::Begin("Guided tutorial overlay",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        // Capture the whole viewport, including the spotlight. Tour navigation is
        // read-only: clicks cannot accidentally modify covered simulation controls.
        ImGui::InvisibleButton("##tutorial_input_shield",display);
        auto* draw=ImGui::GetWindowDrawList(); const auto dim=IM_COL32(0,0,0,185);
        draw->AddRectFilled(ImVec2(0,0),ImVec2(display.x,a.y),dim);
        draw->AddRectFilled(ImVec2(0,b.y),display,dim);
        draw->AddRectFilled(ImVec2(0,a.y),ImVec2(a.x,b.y),dim);
        draw->AddRectFilled(ImVec2(b.x,a.y),ImVec2(display.x,b.y),dim);
        draw->AddRect(a,b,IM_COL32(230,235,240,255),2,0,2*scale);
        const ImVec2 anchor(std::clamp(card.x+cardWidth*.5f,a.x,b.x),b.y);
        draw->AddLine(anchor,ImVec2(card.x+cardWidth*.5f,card.y),IM_COL32(230,235,240,255),2*scale);
        draw->AddCircleFilled(anchor,4*scale,IM_COL32(230,235,240,255));
        ImGui::SetCursorPos(card);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.075f,.085f,.10f,1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16*scale,14*scale));
        ImGui::BeginChild("Tutorial information",ImVec2(cardWidth,cardHeight),ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);
        // PushFont size is unscaled; FontScaleMain already applies the user's UI scale.
        ImGui::PushFont(nullptr,18);
        ImGui::TextUnformatted("sattrak tutorial");
        ImGui::Separator();
        ImGui::BeginDisabled(currentStep_==0);
        if(ImGui::Button("<",ImVec2(36*scale,0)))goTo(currentStep_-1);
        ImGui::EndDisabled(); ImGui::SameLine();
        ImGui::Text("%d / %d",currentStep_+1,totalSteps); ImGui::SameLine();
        ImGui::BeginDisabled(currentStep_==totalSteps-1);
        if(ImGui::Button(">",ImVec2(36*scale,0)))goTo(currentStep_+1);
        ImGui::EndDisabled();
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##Choose a step", "Choose a step...")) {
            for(int i=0;i<totalSteps;++i)if(ImGui::Selectable(steps[i].title,currentStep_==i))goTo(i);
            ImGui::EndCombo();
        }
        const float footer=ImGui::GetFrameHeightWithSpacing()+8*scale;
        ImGui::BeginChild("Tutorial explanation",ImVec2(0,std::max(20.f,ImGui::GetContentRegionAvail().y-footer)));
        ImGui::TextWrapped("%s",step.title); ImGui::Spacing(); ImGui::TextWrapped("%s",step.text);
        ImGui::EndChild();
        if(ImGui::Button("Skip tutorial"))finish(view,cfd,console,help,studio,false);
        if(currentStep_==totalSteps-1) { ImGui::SameLine(); if(ImGui::Button("Finish tutorial"))finish(view,cfd,console,help,studio,true); }
        ImGui::PopFont();
        ImGui::EndChild(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
        ImGui::End(); ImGui::PopStyleVar(2);
    }
private:
    friend struct TutorialChecks;
    bool remember_=false,pending_=false,active_=false,targetVisible_=false;
    int currentStep_=0,preparedStep_=-1,settleFrames_=0;
    struct Saved {
        std::array<bool,4> panels{};
        bool settings=false,replay=false,hud=true,paused=false,bar=true,console=false,help=false;
        bool flow=false,controls=false,benchmark=false,cfdPaused=false;
        int category=0,studio=0;
    } saved_;
};
}

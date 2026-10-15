#include <iostream>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "imgui_internal.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot.h"

#include "gui/adcs_view.hpp"
#include "gui/cfd_view.hpp"
#include "gui/music_player.hpp"
#include "gui/developer_console.hpp"
#include "gui/music_icons.hpp"
#include "gui/panel_sidebar.hpp"
#include "gui/accessibility_panel.hpp"
#include "../../tests/accessibility_checks.hpp"
#include "../../tests/gui_view_checks.hpp"
#include "../../tests/display_settings_checks.hpp"
#include "../../tests/console_checks.hpp"
#include "../../tests/interaction_checks.hpp"

static bool exitDialogClosed=false;

void applyStealthAerospaceTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.HoverDelayNormal = 2.0f;
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.45f, 0.52f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.04f, 0.06f, 0.10f, 0.92f); // Stealth Deep Blue-Black
    colors[ImGuiCol_ChildBg]               = ImVec4(0.02f, 0.04f, 0.07f, 0.70f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.05f, 0.07f, 0.12f, 0.95f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.26f, 0.36f, 0.65f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.08f, 0.12f, 0.18f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.14f, 0.20f, 0.30f, 0.85f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.18f, 0.26f, 0.38f, 0.85f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.05f, 0.07f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.07f, 0.11f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.04f, 0.06f, 0.10f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.05f, 0.07f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.04f, 0.07f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.28f, 0.40f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.28f, 0.38f, 0.52f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.22f, 0.74f, 0.97f, 0.90f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.22f, 0.74f, 0.97f, 1.00f); // Cyan
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.22f, 0.74f, 0.97f, 0.90f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.35f, 0.82f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.10f, 0.15f, 0.24f, 0.85f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.16f, 0.25f, 0.38f, 0.90f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.22f, 0.74f, 0.97f, 0.80f);
    colors[ImGuiCol_Header]                = ImVec4(0.12f, 0.18f, 0.28f, 0.80f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.18f, 0.26f, 0.38f, 0.85f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.22f, 0.74f, 0.97f, 0.70f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.26f, 0.36f, 0.65f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.22f, 0.74f, 0.97f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.22f, 0.74f, 0.97f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.12f, 0.20f, 0.85f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.18f, 0.26f, 0.38f, 0.90f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.14f, 0.22f, 0.34f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.05f, 0.08f, 0.14f, 0.85f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.10f, 0.16f, 0.26f, 1.00f);
}

int main(int argc, char** argv) {
    const bool accessibilitySmoke = argc > 1 && std::strcmp(argv[1], "--accessibility-smoke-test") == 0;
    const bool multiSmoke = argc > 1 && std::strcmp(argv[1], "--multiview-smoke-test") == 0;
    const bool issDepthSmoke = argc > 1 && std::strcmp(argv[1], "--iss-depth-smoke-test") == 0;
    const bool interactionSmoke = argc > 1 && std::strcmp(argv[1], "--interaction-smoke-test") == 0;
    const bool modelSmoke = interactionSmoke || issDepthSmoke || (argc > 1 && std::strcmp(argv[1], "--models-smoke-test") == 0);
    const bool orbitSmoke = argc > 1 && std::strcmp(argv[1], "--orbit-smoke-test") == 0;
    const bool startupSmoke = argc > 1 && std::strcmp(argv[1], "--startup-smoke-test") == 0;
    const bool consoleSmoke = argc > 1 && std::strcmp(argv[1], "--console-smoke-test") == 0;
    const bool displaySmoke = argc > 1 && std::strcmp(argv[1], "--display-smoke-test") == 0;
    const bool catalogSmoke = argc > 1 && std::strcmp(argv[1], "--catalog-smoke-test") == 0;
    const bool interfaceSmoke = argc > 1 && std::strcmp(argv[1], "--interface-smoke-test") == 0;
    const bool smokeTest = interfaceSmoke || accessibilitySmoke || catalogSmoke || displaySmoke || consoleSmoke || startupSmoke || orbitSmoke || multiSmoke || modelSmoke || (argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0);
    if (!std::filesystem::exists("earth_texture.jpg")) {
        auto executableDir = std::filesystem::absolute(argv[0]).parent_path();
        if (std::filesystem::exists(executableDir / "earth_texture.jpg"))
            std::filesystem::current_path(executableDir);
    }
    int smokeFrame = 0;
    bool smokeFailed = false;
    std::cout << "========================================================\n";
    std::cout << "  sattrak                             \n";
    std::cout << "========================================================\n";

    if (!glfwInit()) {
        std::cerr << "CRITICAL ERROR: Failed to initialize GLFW.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    if (smokeTest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    int windowWidth = 1600;
    int windowHeight = 940;
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight,
        "sattrak", NULL, NULL);

    if (!window) {
        std::cerr << "CRITICAL ERROR: Failed to create GLFW window.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync for rock-solid 60 FPS

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "CRITICAL ERROR: Failed to initialize GLAD OpenGL loader.\n";
        return -1;
    }

    std::cout << "[INFO] OpenGL Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "[INFO] OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "[INFO] OpenGL Version:  " << glGetString(GL_VERSION) << "\n";

    // Setup Dear ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    if (smokeTest) io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigWindowsResizeFromEdges = true;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    applyStealthAerospaceTheme();
    io.Fonts->AddFontDefault();
    if (std::filesystem::exists("C:/Windows/Fonts/arial.ttf"))
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 20.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize Simulation Engines
    gui::ADCSView adcsView;
    gui::DisplaySettings displaySettings(window,!smokeTest);
    adcsView.attachDisplay(displaySettings);
    adcsView.configurePreferences(!smokeTest);
    if(displaySmoke)smokeFailed |= !gui::DisplaySettingsChecks::run(displaySettings);
    if (!adcsView.init()) {
        std::cerr << "Failed to initialize ADCS 3D View.\n";
        return 1;
    }
    glfwSetWindowUserPointer(window,&adcsView);
    if(!smokeTest){
        adcsView.initInternetChoice();
        adcsView.findRecoveryRecordings();
        glfwSetWindowCloseCallback(window,[](GLFWwindow* w){
#ifdef _WIN32
            auto* view=static_cast<gui::ADCSView*>(glfwGetWindowUserPointer(w));
            if(view)view->suspendAudio();
            const wchar_t* message=view&&(view->hasActivePathRecording()||view->hasUnsavedPathRecording())
                ? L"Your satellite path has not been saved. Exiting sattrak will discard it. Choose No to return and use Save or Save As. Confirm exit?"
                : L"Are you sure you want to exit?";
            glfwSetWindowShouldClose(w,MessageBoxW(glfwGetWin32Window(w),message,L"sattrak",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)==IDYES);
            exitDialogClosed=true;
#endif
        });
    }
    glfwSetKeyCallback(window,[](GLFWwindow* w,int key,int scancode,int action,int mods){
        ImGui_ImplGlfw_KeyCallback(w,key,scancode,action,mods);
        auto* view=static_cast<gui::ADCSView*>(glfwGetWindowUserPointer(w));
        if(view)view->secretKey(key,action,ImGui::GetIO().WantTextInput);
    });
    if (smokeTest) {
        adcsView.setCameraPreset(gui::CameraPreset::FreeOrbitArcball);
        smokeFailed |= !gui::ADCSViewChecks::run(adcsView);
    }

    gui::CFDView cfdView;
    if (!cfdView.init()) {
        std::cerr << "Failed to initialize CFD View.\n";
    }

    gui::MusicPlayer musicPlayer;
    gui::DeveloperConsole console;
    gui::AccessibilityPanel accessibilityPanel;
    console.attachActions(adcsView.actions());
    musicPlayer.init("assets/music");
    musicPlayer.initRadio("assets/radio");
    adcsView.attachAudio(musicPlayer);
    if (smokeTest) {
        musicPlayer.setVolumePercent(250);
        smokeFailed |= musicPlayer.volumePercent() != 200;
        musicPlayer.setVolumePercent(100);
        if(musicPlayer.hasTracks()) {
            musicPlayer.togglePause();
            smokeFailed |= !musicPlayer.isPlaying();
            musicPlayer.togglePause();
            smokeFailed |= musicPlayer.isPlaying();
        }
    }

    int activeStudioTab = 0; // 0: GNC & ADCS Studio, 1: Supersonic CFD Studio
    double lastTime = glfwGetTime();

    // Main Simulation & Rendering Loop
    while (!glfwWindowShouldClose(window)) {
        if (modelSmoke && smokeFrame == 0) glfwSetWindowSize(window, 1920, 1080);
        if(interactionSmoke){
            if(smokeFrame==4)glfwSetWindowSize(window,1000,900);
            if(smokeFrame==8){glfwSetWindowSize(window,1400,850);gui::DisplaySettingsChecks::scale(displaySettings,125);}
            if(smokeFrame==12){glfwSetWindowSize(window,900,1000);gui::DisplaySettingsChecks::scale(displaySettings,100);}
            if(smokeFrame==16)glfwSetWindowSize(window,1600,900);
        }
        if (smokeTest && !modelSmoke && smokeFrame == 4) glfwSetWindowSize(window, 900, 1100);
        if (smokeTest && !modelSmoke && smokeFrame == 8) glfwSetWindowSize(window, 2400, 800);
        glfwPollEvents();
        if(catalogSmoke&&smokeFrame==8)gui::DisplaySettingsChecks::scale(displaySettings,150);
        if(catalogSmoke&&smokeFrame==12)gui::DisplaySettingsChecks::scale(displaySettings,200);
        if(catalogSmoke&&smokeFrame==16)gui::DisplaySettingsChecks::scale(displaySettings,100);
        if(displaySmoke&&smokeFrame==18)gui::DisplaySettingsChecks::confirm(displaySettings);
        displaySettings.update();

        double currentTime = glfwGetTime();
        if(exitDialogClosed){lastTime=currentTime;exitDialogClosed=false;}
        if(glfwWindowShouldClose(window))break;
        float dt = (float)(currentTime - lastTime);
        lastTime = currentTime;
        // Preserve wall-clock time: 1:1 mode must not slow down on long frames.

        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

        // Start ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        if (smokeTest && !accessibilitySmoke) {
            io.AddMousePosEvent(smokeFrame < 3 ? io.DisplaySize.x-100 : io.DisplaySize.x/2, 100);
        }
        if(accessibilitySmoke) gui::AccessibilityChecks::input(smokeFrame);
        ImGui::NewFrame();
        if(interactionSmoke){
            if(smokeFrame==0)smokeFailed |= !gui::InteractionChecks::run(adcsView);
            smokeFailed |= !gui::InteractionChecks::keyboard(smokeFrame);
        }
        if(consoleSmoke){
            if(smokeFrame==0)smokeFailed |= !gui::ConsoleChecks::run(console);
            if(smokeFrame==0)smokeFailed |= !gui::ConsoleChecks::commands(console,musicPlayer,adcsView,displaySettings);
            if(smokeFrame==0)io.AddKeyEvent(ImGuiKey_GraveAccent,true);
            if(smokeFrame==1)io.AddKeyEvent(ImGuiKey_GraveAccent,false);
            if(smokeFrame==2)io.AddInputCharactersUTF8("ra");
            if(smokeFrame==3)io.AddKeyEvent(ImGuiKey_DownArrow,true);
            if(smokeFrame==4)io.AddKeyEvent(ImGuiKey_DownArrow,false);
            if(smokeFrame==5)io.AddKeyEvent(ImGuiKey_Tab,true);
            if(smokeFrame==6)io.AddKeyEvent(ImGuiKey_Tab,false);
            if(smokeFrame==7)io.AddKeyEvent(ImGuiKey_Enter,true);
            if(smokeFrame==8)io.AddKeyEvent(ImGuiKey_Enter,false);
            if(smokeFrame==10)io.AddInputCharactersUTF8("camera.fov 60");
            if(smokeFrame==12)io.AddKeyEvent(ImGuiKey_Enter,true);
            if(smokeFrame==13)io.AddKeyEvent(ImGuiKey_Enter,false);
            if(smokeFrame==15)smokeFailed |= !gui::ConsoleChecks::interaction(console,adcsView);
            if(smokeFrame==16)io.AddInputCharactersUTF8("ra");
        }
        if(smokeTest&&smokeFrame==0)smokeFailed |= !gui::ADCSViewChecks::checkDataFallback(adcsView);
        if(startupSmoke&&smokeFrame==0)gui::ADCSViewChecks::showStartupChoice(adcsView);
        musicPlayer.update(currentTime);
        console.draw(musicPlayer,adcsView,displaySettings);
        if(consoleSmoke&&smokeFrame==18)smokeFailed |= !gui::ConsoleChecks::suggestionsAfterCommands(console);
        accessibilityPanel.keyboard(console.isOpen());
        adcsView.actions().process(activeStudioTab!=0||console.isOpen());

        if(interfaceSmoke){gui::panelPlacement=smokeFrame/4;gui::topBarVisible=(smokeFrame%4)>=2;}
        // Top Command Bar (Tab Selector)
        const float uiScale=ImGui::GetStyle().FontScaleMain;
        const float logicalWidth=io.DisplaySize.x/uiScale;
        // The attached handle stays available when the top bar is collapsed.
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,gui::topInset()),ImGuiCond_Always,ImVec2(.5f,0));
        ImGui::SetNextWindowSize(ImVec2(72*uiScale,24*uiScale));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
        ImGui::Begin("Top bar handle",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,1*uiScale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.20f,.20f,.20f,.72f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.28f,.28f,.28f,.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.34f,.34f,.34f,.92f));
        const bool clicked=ImGui::Button("##top_bar_toggle",ImVec2(72*uiScale,24*uiScale));
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        const bool hovered=ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride);
        const auto handle=ImGui::GetItemRectMin();
        const float cx=handle.x+36*uiScale,cy=handle.y+12*uiScale;
        const float direction=gui::topBarVisible?-1.f:1.f;
        auto* handleDraw=ImGui::GetWindowDrawList();
        handleDraw->AddLine(ImVec2(cx-22*uiScale,cy-direction*3*uiScale),ImVec2(cx,cy+direction*4*uiScale),IM_COL32(235,235,235,255),2*uiScale);
        handleDraw->AddLine(ImVec2(cx,cy+direction*4*uiScale),ImVec2(cx+22*uiScale,cy-direction*3*uiScale),IM_COL32(235,235,235,255),2*uiScale);
        if(hovered)ImGui::SetTooltip(gui::topBarVisible?"Hide top bar":"Show top bar");
        if(clicked)gui::topBarVisible=!gui::topBarVisible;
        else if(hovered&&gui::topBarHover)gui::topBarVisible=true;
        ImGui::End();ImGui::PopStyleVar(2);
        if(gui::topBarVisible) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, gui::topBarHeight()));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(6*uiScale,12*uiScale));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,1*uiScale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
        ImGui::Begin("TopBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            // Measure each responsive layout and center the complete row, including FPS.
            static float rowWidths[2]={0,0};
            float& rowWidth=rowWidths[logicalWidth<1700?0:1];
            ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,
                (ImGui::GetWindowWidth()-rowWidth*uiScale)*.5f));
            const float rowStart=ImGui::GetCursorPosX();
            if(ImGui::MenuItem("Help", "F10")) accessibilityPanel.open();
            if(ImGui::BeginMenu("Simulation")) {
                if(ImGui::MenuItem("Orbital tracking",nullptr,activeStudioTab==0))activeStudioTab=0;
                if(ImGui::BeginMenu("Tools")) {
                    if(ImGui::MenuItem("Supersonic shock explorer",nullptr,activeStudioTab==1))activeStudioTab=1;
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            ImGui::Spacing();
            if(logicalWidth<1700) {
                if(ImGui::BeginMenu("Music")) {
                    ImGui::BeginDisabled(!musicPlayer.hasTracks());
                    if(gui::musicIconButton("##compact_previous",gui::MusicIcon::Previous))adcsView.actions().run("music.previous");ImGui::SameLine();
                    if(gui::musicIconButton("##compact_pause",musicPlayer.isPlaying()?gui::MusicIcon::Pause:gui::MusicIcon::Play))adcsView.actions().run("music.toggle");ImGui::SameLine();
                    if(gui::musicIconButton("##compact_next",gui::MusicIcon::Next))adcsView.actions().run("music.next");
                    float elapsed=float(musicPlayer.elapsedSeconds()),duration=float(musicPlayer.durationSeconds());
                    ImGui::Text("%d:%02d, -%d:%02d",int(elapsed)/60,int(elapsed)%60,int(std::max(0.0f,duration-elapsed))/60,int(std::max(0.0f,duration-elapsed))%60);
                    ImGui::SetNextItemWidth(280*uiScale);if(gui::mediaSeek("##compact_seek",&elapsed,duration))musicPlayer.seekSeconds(elapsed);
                    ImGui::EndDisabled();int volume=musicPlayer.volumePercent();ImGui::SetNextItemWidth(160*uiScale);
                    if(ImGui::SliderInt("Volume",&volume,0,200,"%d%%"))musicPlayer.setVolumePercent(volume);
                    ImGui::SetNextItemWidth(80*uiScale);if(ImGui::InputInt("Exact percent",&volume,0,0))musicPlayer.setVolumePercent(volume);
                    ImGui::EndMenu();
                }
            } else {
            ImGui::BeginDisabled(!musicPlayer.hasTracks());
            if (gui::musicIconButton("##music_previous",gui::MusicIcon::Previous)) adcsView.actions().run("music.previous");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                ImGui::SetTooltip("Restart the current song after three seconds, or skip to the previous song near its beginning.");
            ImGui::SameLine();
            if (gui::musicIconButton("##music_play_pause",musicPlayer.isPlaying()?gui::MusicIcon::Pause:gui::MusicIcon::Play)) adcsView.actions().run("music.toggle");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                ImGui::SetTooltip("Pause or continue music. Before automatic playback begins, this starts it immediately.");
            ImGui::SameLine();
            if (gui::musicIconButton("##music_next",gui::MusicIcon::Next)) adcsView.actions().run("music.next");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                ImGui::SetTooltip("Skip to the next song.");
            ImGui::EndDisabled();
            ImGui::SameLine();
            const double duration=musicPlayer.durationSeconds();
            float elapsed=float(musicPlayer.elapsedSeconds());
            const int seconds=int(elapsed);
            ImGui::Text("%d:%02d",seconds/60,seconds%60);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(240*uiScale);
            ImGui::BeginDisabled(duration<=0);
            if(gui::mediaSeek("##music_seek",&elapsed,float(duration)))musicPlayer.seekSeconds(elapsed);
            ImGui::EndDisabled();
            ImGui::SameLine();
            const int remaining=int(std::ceil(std::max(0.0,duration-elapsed)));
            ImGui::Text("-%d:%02d",remaining/60,remaining%60);
            ImGui::SameLine();
            if (logicalWidth >= 1500) {
                ImGui::TextDisabled("%.22s", musicPlayer.displayText().c_str());
                ImGui::SameLine();
            }
            ImGui::TextUnformatted("Volume");
            ImGui::SameLine();
            int musicVolume=musicPlayer.volumePercent();
            ImGui::SetNextItemWidth((logicalWidth < 1050 ? 52.0f : 72.0f)*uiScale);
            bool volumeChanged=ImGui::SliderInt("##music_volume_slider",&musicVolume,0,200,"%d%%",ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(46*uiScale);
            volumeChanged|=ImGui::InputInt("##music_volume_exact",&musicVolume,0,0,ImGuiInputTextFlags_CharsDecimal);
            musicVolume=std::clamp(musicVolume,0,200);
            if(volumeChanged)musicPlayer.setVolumePercent(musicVolume);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                ImGui::SetTooltip("Exact music volume percentage. Enter any whole number from 0 to 200; values above 100 amplify and may clip.");
            }

            {
                ImGui::SameLine();
                ImGui::TextDisabled("%.1f FPS, %.2f ms", io.Framerate, 1000.0f / io.Framerate);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                    ImGui::SetTooltip("Measured rendered frames per second and average frame time, including V-Sync waits.");
            }
            rowWidth=(ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x-rowStart)/uiScale;
            ImGui::EndMenuBar();
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
        } // Expanded top bar

        // Execute Active Simulation Studio
        if (activeStudioTab == 0) {
            // Render 3D OpenGL Scene (Textured Earth, Satellite, Muzzle Laser)
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Deep Space Black
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if(interactionSmoke)gui::InteractionChecks::prepare(adcsView,smokeFrame);
            else if(catalogSmoke)gui::ADCSViewChecks::prepareCatalogFrame(adcsView,smokeFrame);
            else if(orbitSmoke)gui::ADCSViewChecks::prepareOrbitFrame(adcsView,smokeFrame);
            else if(multiSmoke)gui::ADCSViewChecks::prepareMultiFrame(adcsView,smokeFrame);
            else if(issDepthSmoke)gui::ADCSViewChecks::prepareIssDepthFrame(adcsView,smokeFrame);
            else if(modelSmoke)gui::ADCSViewChecks::prepareModelFrame(adcsView,smokeFrame);
            else if (smokeTest && !accessibilitySmoke && !startupSmoke && !consoleSmoke) gui::ADCSViewChecks::prepareFrame(adcsView, smokeFrame);
            adcsView.update(dt);
            if (smokeTest && smokeFrame >= 8) {
                ImGui::SetScrollY(ImGui::FindWindowByName("Flight Director & Camera"), 650);
            }
            adcsView.drawUI(smokeTest);
            if (smokeTest && !accessibilitySmoke && !catalogSmoke && !displaySmoke && !consoleSmoke && !startupSmoke && !orbitSmoke && !modelSmoke && !multiSmoke) smokeFailed |= !gui::ADCSViewChecks::checkRouting(adcsView, smokeFrame);
            adcsView.renderViews(window,dt);
            adcsView.drawAltimeters();
        } else {
            adcsView.closeDesktopViews();
            adcsView.suspendAudio();
            // Render CFD Studio
            glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            cfdView.update(dt);
            cfdView.drawUI();
        }

        accessibilityPanel.draw(activeStudioTab==0 ? adcsView.textDescription() : cfdView.textDescription(), activeStudioTab, adcsView.actions());
        if(accessibilitySmoke) smokeFailed |= !gui::AccessibilityChecks::check(smokeFrame,accessibilityPanel,activeStudioTab);
        adcsView.drawDiscoveryAlert();
        displaySettings.drawConfirmation();
        if(!console.isOpen())adcsView.actions().drawNotice();

        // Render ImGui onto screen
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if(interfaceSmoke) {
            auto* strip=ImGui::FindWindowByName("Orbital panels");
            if(!strip || !(strip->Flags & ImGuiWindowFlags_NoBackground))smokeFailed=true;
            else {
                const bool horizontal=gui::horizontalPanels();
                const float expectedX=gui::panelPlacement==0?(io.DisplaySize.x-strip->Size.x)*.5f:
                    gui::panelPlacement>=3?io.DisplaySize.x-strip->Size.x:0;
                const float expectedY=gui::panelPlacement==0?io.DisplaySize.y-strip->Size.y:gui::topInset();
                if(std::abs(strip->Pos.x-expectedX)>1 || std::abs(strip->Pos.y-expectedY)>1 ||
                    (horizontal?strip->Size.x<=strip->Size.y:strip->Size.y<=strip->Size.x))smokeFailed=true;
            }
        }
        if (smokeTest) {
            const char* panels[] = {"GNC & Orbital Telemetry", "Live Strip-Charts (ImPlot)", "Flight Director & Camera", "2D Ground Track (ECEF)"};
            for (int i = 0; activeStudioTab == 0 && i < 4; ++i) {
                ImGuiWindow* panel = ImGui::FindWindowByName(panels[i]);
                if (!panel) { smokeFailed=true; continue; }
                float edge = i < 2 ? panel->Pos.x - gui::panelSidebarWidth() : io.DisplaySize.x - panel->Pos.x - panel->Size.x - (gui::panelPlacement==4?64*uiScale:0);
                if (std::abs(edge - 10.0f) > 1.0f || panel->Pos.y + panel->Size.y > io.DisplaySize.y + 1) {
                    std::cerr << "Panel bounds failed: " << panels[i] << "\n";
                    smokeFailed = true;
                }
            }
            if (smokeFrame == 3 || smokeFrame == 7 || smokeFrame == 11 || smokeFrame == 15 || smokeFrame == 19) {
                std::vector<unsigned char> pixels(windowWidth * windowHeight * 3);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                std::ofstream shot("smoke-" + std::to_string(smokeFrame) + ".ppm", std::ios::binary);
                shot << "P6\n" << windowWidth << " " << windowHeight << "\n255\n";
                for (int y = windowHeight-1; y >= 0; --y)
                    shot.write(reinterpret_cast<const char*>(pixels.data() + y*windowWidth*3), windowWidth*3);
            }
            if (glGetError() != GL_NO_ERROR) smokeFailed = true;
            if (++smokeFrame == 20) glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        glfwSwapBuffers(window);
    }

    // Cleanup
    adcsView.cleanup();
    cfdView.cleanup();

    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[INFO] GUI session terminated cleanly.\n";
    return smokeFailed ? 1 : 0;
}

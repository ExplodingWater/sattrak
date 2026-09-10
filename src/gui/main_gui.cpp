#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "imgui_internal.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot.h"

#include "gui/adcs_view.hpp"
#include "gui/cfd_view.hpp"
#include "../../tests/gui_view_checks.hpp"

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
    const bool smokeTest = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
    if (!std::filesystem::exists("earth_texture.jpg")) {
        auto executableDir = std::filesystem::absolute(argv[0]).parent_path();
        if (std::filesystem::exists(executableDir / "earth_texture.jpg"))
            std::filesystem::current_path(executableDir);
    }
    int smokeFrame = 0;
    bool smokeFailed = false;
    std::cout << "========================================================\n";
    std::cout << "  Dynamics & Supersonic CFD                             \n";
    std::cout << "========================================================\n";

    if (!glfwInit()) {
        std::cerr << "CRITICAL ERROR: Failed to initialize GLFW.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    if (smokeTest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    int windowWidth = 1600;
    int windowHeight = 940;
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight,
        "Dynamics & Supersonic CFD", NULL, NULL);

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    applyStealthAerospaceTheme();
    io.Fonts->AddFontDefault();
    if (std::filesystem::exists("C:/Windows/Fonts/arial.ttf"))
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 20.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize Simulation Engines
    gui::ADCSView adcsView;
    if (!adcsView.init()) {
        std::cerr << "Failed to initialize ADCS 3D View.\n";
        return 1;
    }
    if (smokeTest) {
        adcsView.setCameraPreset(gui::CameraPreset::FreeOrbitArcball);
        smokeFailed = !gui::ADCSViewChecks::run(adcsView);
    }

    gui::CFDView cfdView;
    if (!cfdView.init()) {
        std::cerr << "Failed to initialize CFD View.\n";
    }

    int activeStudioTab = 0; // 0: GNC & ADCS Studio, 1: Supersonic CFD Studio
    double lastTime = glfwGetTime();

    // Main Simulation & Rendering Loop
    while (!glfwWindowShouldClose(window)) {
        if (smokeTest && smokeFrame == 4) glfwSetWindowSize(window, 900, 1100);
        if (smokeTest && smokeFrame == 8) glfwSetWindowSize(window, 2400, 800);
        glfwPollEvents();

        double currentTime = glfwGetTime();
        float dt = (float)(currentTime - lastTime);
        lastTime = currentTime;
        // Preserve wall-clock time: 1:1 mode must not slow down on long frames.

        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

        // Start ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        if (smokeTest) {
            io.AddMousePosEvent(smokeFrame < 3 ? io.DisplaySize.x-100 : io.DisplaySize.x/2, 100);
        }
        ImGui::NewFrame();

        // Top Command Bar (Tab Selector)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 36));
        ImGui::Begin("TopBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "Simulation");
            ImGui::TextDisabled("|");

            if (ImGui::MenuItem(io.DisplaySize.x < 1200 ? " [1] ORBITAL GNC " : " [1] 3D ORBITAL GNC & SATELLITE TRACKING ", nullptr, activeStudioTab == 0)) {
                activeStudioTab = 0;
            }
            if (ImGui::MenuItem(io.DisplaySize.x < 1200 ? " [2] CFD " : " [2] SUPERSONIC CFD SHOCK EXPLORER ", nullptr, activeStudioTab == 1)) {
                activeStudioTab = 1;
            }

            ImGui::SameLine(ImGui::GetWindowWidth() - 160);
            ImGui::TextDisabled("%.1f FPS | %.2f ms", io.Framerate, 1000.0f / io.Framerate);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                ImGui::SetTooltip("Measured rendered frames per second and average frame time, including V-Sync waits.");
            ImGui::EndMenuBar();
        }
        ImGui::End();

        // Execute Active Simulation Studio
        if (activeStudioTab == 0) {
            // Render 3D OpenGL Scene (Textured Earth, Satellite, Muzzle Laser)
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Deep Space Black
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (smokeTest) gui::ADCSViewChecks::prepareFrame(adcsView, smokeFrame);
            adcsView.update(dt);
            if (smokeTest && smokeFrame >= 8) {
                ImGui::SetScrollY(ImGui::FindWindowByName("Flight Director & Camera"), 650);
            }
            adcsView.drawUI();
            if (smokeTest) smokeFailed |= !gui::ADCSViewChecks::checkRouting(adcsView, smokeFrame);
            adcsView.handleInput(window, dt);
            adcsView.render3D(windowWidth, windowHeight);
            adcsView.drawFlightOverlay();
        } else {
            adcsView.suspendAudio();
            // Render CFD Studio
            glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            cfdView.update(dt);
            cfdView.drawUI();
        }

        // Render ImGui onto screen
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (smokeTest) {
            const char* panels[] = {"GNC & Orbital Telemetry", "Live Strip-Charts (ImPlot)", "Flight Director & Camera", "2D Ground Track (ECEF)"};
            for (int i = 0; i < 4; ++i) {
                ImGuiWindow* panel = ImGui::FindWindowByName(panels[i]);
                float edge = i < 2 ? panel->Pos.x : io.DisplaySize.x - panel->Pos.x - panel->Size.x;
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


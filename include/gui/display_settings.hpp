#pragma once
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include "imgui.h"

namespace gui {
class DisplaySettings {
public:
    explicit DisplaySettings(GLFWwindow* window, bool remember=true);
    void draw();
    void update(); // Apply changes between frames, never while ImGui is rendering.
    void drawConfirmation();
private:
    friend class DeveloperConsole;
    friend struct ConsoleChecks;
    friend struct DisplaySettingsChecks;
    struct Monitor { GLFWmonitor* handle; int width,height,refresh; std::string name; int maxWidth,maxHeight; };
    struct State { int mode=2,width=1600,height=940,x=0,y=0; GLFWmonitor* monitor=nullptr; bool maximized=false; };
    GLFWwindow* window;
    std::vector<Monitor> monitors;
    State current,previous;
    int selected=0,mode=2,width=1600,height=940;
    bool remember,queued=false,reverting=false,confirming=false,restoringSaved=false;
    double deadline=0;
    std::string message;
    ImGuiStyle baseStyle;
    int uiScale=100,appliedScale=100;
    bool confirmationOpened=false;
    void refreshMonitors();
    bool valid(std::string& reason) const;
    bool apply(const State& state);
    void syncWindow();
    bool save() const;
};
}

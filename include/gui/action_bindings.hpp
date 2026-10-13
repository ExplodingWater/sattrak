#pragma once
#include "imgui.h"
#include <functional>
#include <string>
#include <vector>

namespace gui {
// One dispatch path for buttons, console commands, keyboard bindings and macros.
class ActionBindings {
public:
    struct Action { std::string id,label; ImGuiKeyChord binding; std::function<std::string()> invoke; };
    struct Macro { std::string name; ImGuiKeyChord binding=0; std::vector<std::string> steps; };
    std::vector<Action> actions;
    std::vector<Macro> macros;
    std::string status;
    void add(const char* id,const char* label,ImGuiKeyChord key,std::function<std::string()> invoke);
    bool contains(const std::string& id) const;
    bool run(const std::string& id);
    bool runMacro(const std::string& name);
    void process(bool blocked);
    void drawEditor();
    void drawNotice();
    void cancelCapture() { captureIndex=-1; }
    void configurePersistence(const std::string& path);
    bool assign(bool macro,size_t index,ImGuiKeyChord chord);
    static bool allowed(ImGuiKeyChord chord);
    static std::string keyLabel(ImGuiKeyChord chord);
    bool capturing() const { return captureIndex>=0; }
private:
    std::string file;
    std::string failure;
    double failureUntil=0;
    int captureIndex=-1;
    bool captureMacro=false;
    int captureFrame=0,selectedMacro=0,stepChoice=0;
    char newName[64]{};
    void save();
    void bindingButton(bool macro,int index);
};
}

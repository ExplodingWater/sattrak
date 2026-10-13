#include "gui/action_bindings.hpp"
#include "gui/accessibility.hpp"
#include "imgui_internal.h"
#include "../../external/nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

namespace gui {
void ActionBindings::add(const char* id,const char* label,ImGuiKeyChord key,std::function<std::string()> fn) {
    actions.push_back({id,label,key,std::move(fn)});
}
bool ActionBindings::contains(const std::string& id) const {
    return std::any_of(actions.begin(),actions.end(),[&](const Action& a){return a.id==id;});
}
bool ActionBindings::run(const std::string& id) {
    auto it=std::find_if(actions.begin(),actions.end(),[&](const Action& a){return a.id==id;});
    if(it==actions.end()){status="Unknown action: "+id;return false;}
    auto error=it->invoke();
    status=error.empty()?"Applied: "+it->label:error;
    if(!error.empty()){failure=error;failureUntil=ImGui::GetTime()+5;}
    return error.empty();
}
bool ActionBindings::allowed(ImGuiKeyChord chord) {
    if(!chord)return true;
    if(chord & ~(ImGuiMod_Mask_ | 0x7ff))return false;
    const auto key=ImGuiKey(chord & ~ImGuiMod_Mask_);
    if(chord==ImGuiKey_F10)return false; // Reserved for keyboard focus, including recovery from hidden HUDs.
    if((chord&ImGuiMod_Super)||(key==ImGuiKey_F4&&(chord&ImGuiMod_Alt)))return false;
    if(key<ImGuiKey_NamedKey_BEGIN||key>=ImGuiKey_GamepadStart)return false;
    // These keys already belong to flight, editing, navigation or the console.
    switch(key) {
    case ImGuiKey_GraveAccent:case ImGuiKey_Escape:case ImGuiKey_Tab:case ImGuiKey_Enter:
    case ImGuiKey_LeftCtrl:case ImGuiKey_RightCtrl:case ImGuiKey_LeftShift:case ImGuiKey_RightShift:
    case ImGuiKey_LeftAlt:case ImGuiKey_RightAlt:case ImGuiKey_LeftSuper:case ImGuiKey_RightSuper:
    case ImGuiKey_LeftArrow:case ImGuiKey_RightArrow:case ImGuiKey_UpArrow:case ImGuiKey_DownArrow:
    case ImGuiKey_PageUp:case ImGuiKey_PageDown:
    case ImGuiKey_Space:case ImGuiKey_W:case ImGuiKey_A:case ImGuiKey_S:case ImGuiKey_D:
    case ImGuiKey_Q:case ImGuiKey_E:case ImGuiKey_C:
        return key==ImGuiKey_S && chord==(ImGuiMod_Ctrl|ImGuiKey_S);
    default:return true;
    }
}
std::string ActionBindings::keyLabel(ImGuiKeyChord chord) {
    if(!chord)return "Unbound";
    std::string text;
    if(chord&ImGuiMod_Ctrl)text+="Ctrl+";
    if(chord&ImGuiMod_Shift)text+="Shift+";
    if(chord&ImGuiMod_Alt)text+="Alt+";
    if(chord&ImGuiMod_Super)text+="Super+";
    return text+ImGui::GetKeyName(ImGuiKey(chord&~ImGuiMod_Mask_));
}
bool ActionBindings::assign(bool macro,size_t index,ImGuiKeyChord chord) {
    if(index>=(macro?macros.size():actions.size())||!allowed(chord)){status="That key is reserved for flight, editing or the console.";return false;}
    if(chord) {
        for(size_t i=0;i<actions.size();++i)if((macro||i!=index)&&actions[i].binding==chord){status="Already assigned to "+actions[i].label;return false;}
        for(size_t i=0;i<macros.size();++i)if((!macro||i!=index)&&macros[i].binding==chord){status="Already assigned to macro: "+macros[i].name;return false;}
    }
    if(macro)macros[index].binding=chord;else actions[index].binding=chord;
    status="Binding saved.";save();return true;
}
void ActionBindings::configurePersistence(const std::string& path) {
    file=path;if(file.empty())return;
    std::ifstream in(file);if(!in)return;
    try {
        const auto json=nlohmann::json::parse(in);
        auto nextActions=actions;auto nextMacros=macros;
        for(auto& a:nextActions)if(json.at("bindings").contains(a.id))a.binding=json.at("bindings").at(a.id).get<int>();
        nextMacros.clear();
        for(const auto& m:json.at("macros")) {
            Macro item{m.at("name").get<std::string>(),m.at("key").get<int>(),m.at("steps").get<std::vector<std::string>>()};
            if(nextMacros.size()>=32||item.name.empty()||item.name.size()>63||item.steps.size()>32)throw std::runtime_error("Invalid macro size");
            for(const auto& step:item.steps)if(!contains(step))throw std::runtime_error("Unknown macro action");
            nextMacros.push_back(std::move(item));
        }
        std::set<ImGuiKeyChord> used;
        const auto check=[&](ImGuiKeyChord key){if(!allowed(key)||(key&&!used.insert(key).second))throw std::runtime_error("Invalid or conflicting binding");};
        for(const auto& a:nextActions)check(a.binding);
        for(const auto& m:nextMacros)check(m.binding);
        actions=std::move(nextActions);macros=std::move(nextMacros);
    }catch(const std::exception&){status="Could not load controls; using defaults.";}
}
void ActionBindings::save() {
    if(file.empty())return;
    nlohmann::json json;json["bindings"]=nlohmann::json::object();json["macros"]=nlohmann::json::array();
    for(const auto& a:actions)json["bindings"][a.id]=a.binding;
    for(const auto& m:macros)json["macros"].push_back({{"name",m.name},{"key",m.binding},{"steps",m.steps}});
    std::error_code ec;std::filesystem::create_directories(std::filesystem::path(file).parent_path(),ec);
    std::ofstream out(file);out<<json.dump(2);out.flush();
    if(!out)status="Controls could not be saved to disk.";
}
void ActionBindings::process(bool blocked) {
    if(blocked||ImGui::GetIO().AppFocusLost||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)) {
        captureIndex=-1;return;
    }
    if(captureIndex>=0) {
        if(ImGui::GetFrameCount()<=captureFrame)return;
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){captureIndex=-1;status="Binding cancelled.";return;}
        for(int k=ImGuiKey_NamedKey_BEGIN;k<ImGuiKey_GamepadStart;++k)if(ImGui::IsKeyPressed(ImGuiKey(k),false)) {
            if(k>=ImGuiKey_LeftCtrl&&k<=ImGuiKey_RightSuper)continue;
            if(assign(captureMacro,size_t(captureIndex),k|ImGui::GetIO().KeyMods))captureIndex=-1;
            return;
        }
        return;
    }
    const auto& io=ImGui::GetIO();
    if(io.WantTextInput||io.NavVisible||accessibility::panelKeyboardMode||ImGui::IsAnyItemActive())return;
    const auto pressed=[&](ImGuiKeyChord key){return key&&(key&ImGuiMod_Mask_)==io.KeyMods&&ImGui::IsKeyPressed(ImGuiKey(key&~ImGuiMod_Mask_),false);};
    for(const auto& a:actions)if(pressed(a.binding)){run(a.id);return;}
    for(const auto& m:macros)if(pressed(m.binding)){runMacro(m.name);return;}
}
bool ActionBindings::runMacro(const std::string& name) {
    const auto it=std::find_if(macros.begin(),macros.end(),[&](const Macro& m){return m.name==name;});
    if(it==macros.end()){status="Unknown macro: "+name;return false;}
    for(const auto& step:it->steps)if(!run(step))return false;
    status="Ran macro: "+name;return true;
}
void ActionBindings::bindingButton(bool macro,int index) {
    auto key=macro?macros[index].binding:actions[index].binding;
    if(ImGui::Button(captureIndex==index&&captureMacro==macro?"Press key...":keyLabel(key).c_str(),ImVec2(130*ImGui::GetStyle().FontScaleMain,0))) {
        captureIndex=index;captureMacro=macro;captureFrame=ImGui::GetFrameCount();status="Press a key combination. Escape cancels.";
    }
    ImGui::SameLine();if(ImGui::SmallButton("Clear")){assign(macro,index,0);captureIndex=-1;}
}
void ActionBindings::drawNotice() {
    if(failure.empty()||ImGui::GetTime()>failureUntil)return;
    const auto* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x+vp->Size.x*.5f,vp->Pos.y+45),ImGuiCond_Always,ImVec2(.5f,0));
    ImGui::SetNextWindowBgAlpha(.98f);
    if(ImGui::Begin("Action notice",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoInputs)){
        ImGui::PushTextWrapPos(std::min(500.f*ImGui::GetStyle().FontScaleMain,vp->Size.x-40));
        ImGui::TextUnformatted(failure.c_str());ImGui::PopTextWrapPos();
    }ImGui::End();
}
void ActionBindings::drawEditor() {
    ImGui::TextWrapped("%s",status.c_str());
    if(!ImGui::BeginTabBar("Control editors"))return;
    if(ImGui::BeginTabItem("Key bindings")) {
    ImGui::TextWrapped("Click a binding to change it. Conflicts are rejected. Shortcuts pause while typing, using a widget, or opening the console. WASD, Q/E, Space/C and arrows remain flight controls.");
    if(ImGui::BeginTable("Bindings",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Action",ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupColumn("Shortcut",ImGuiTableColumnFlags_WidthFixed,190*ImGui::GetStyle().FontScaleMain);
        for(int i=0;i<int(actions.size());++i){ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(actions[i].label.c_str());ImGui::TableNextColumn();bindingButton(false,i);ImGui::PopID();}
        ImGui::EndTable();
    }
    ImGui::EndTabItem();
    }
    if(ImGui::BeginTabItem("Macros")) {
    ImGui::TextWrapped("Run an ordered sequence of actions with one shortcut. Execution stops if an action cannot run in the current mode.");
    if(!macros.empty()) {
        selectedMacro=std::clamp(selectedMacro,0,int(macros.size())-1);
        if(ImGui::BeginCombo("Macro",macros[selectedMacro].name.c_str())){
            for(int i=0;i<int(macros.size());++i)if(ImGui::Selectable(macros[i].name.c_str(),i==selectedMacro)){selectedMacro=i;captureIndex=-1;}
            ImGui::EndCombo();
        }
        ImGui::PushID("MacroBinding");bindingButton(true,selectedMacro);ImGui::PopID();
        auto& steps=macros[selectedMacro].steps;
        for(int i=0;i<int(steps.size());++i){ImGui::PushID(1000+i);
            ImGui::BeginDisabled(i==0);if(ImGui::SmallButton("Up")){std::swap(steps[i],steps[i-1]);save();}ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled(i+1==int(steps.size()));if(ImGui::SmallButton("Down")){std::swap(steps[i],steps[i+1]);save();}ImGui::EndDisabled();ImGui::SameLine();
            if(ImGui::SmallButton("Remove")){steps.erase(steps.begin()+i);save();ImGui::PopID();break;}
            ImGui::SameLine();const auto it=std::find_if(actions.begin(),actions.end(),[&](const Action& a){return a.id==steps[i];});
            ImGui::TextWrapped("%d. %s",i+1,it==actions.end()?steps[i].c_str():it->label.c_str());ImGui::PopID();}
        if(ImGui::BeginCombo("Add action",actions[stepChoice].label.c_str())){
            for(int i=0;i<int(actions.size());++i)if(ImGui::Selectable(actions[i].label.c_str(),i==stepChoice))stepChoice=i;
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(steps.size()>=32);
        if(ImGui::Button("Append action")){steps.push_back(actions[stepChoice].id);save();}ImGui::EndDisabled();
        ImGui::SameLine();if(ImGui::Button("Run macro"))runMacro(macros[selectedMacro].name);
        ImGui::SameLine();if(ImGui::Button("Delete macro")){macros.erase(macros.begin()+selectedMacro);captureIndex=-1;save();}
    }
    ImGui::InputText("New macro name",newName,sizeof(newName));
    ImGui::BeginDisabled(!newName[0]||macros.size()>=32);
    if(ImGui::Button("Create macro")) {
        if(std::any_of(macros.begin(),macros.end(),[&](const Macro& m){return m.name==newName;}))status="A macro already has that name.";
        else {macros.push_back({newName,0,{}});newName[0]=0;selectedMacro=int(macros.size())-1;save();}
    }ImGui::EndDisabled();
    ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}
}

#include "gui/developer_console.hpp"
#include "gui/radio_commands.hpp"
#include "gui/adcs_view.hpp"
#include "gui/display_settings.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <cmath>
namespace gui {
namespace {
struct CommandInfo { const char* name; const char* value; const char* help; double minimum,maximum; };
constexpr CommandInfo commands[]={
    {"sim.speed","5","Time multiplier, 1..50; unavailable in 1:1 or replay",1,50},
    {"camera.speed","7.5","Free camera speed in km/s; 1:1 mode only",.001,1000},
    {"camera.sensitivity","0.0035","Mouse look in radians/pixel, 0.0002..0.012",.0002,.012},
    {"camera.fov","45","Vertical field of view in degrees, 10..120",10,120},
    {"orbit.altitude","650","Circular orbit altitude in km, 400..1500",400,1500},
    {"orbit.inclination","51.6","Circular orbit inclination in degrees, 0..98",0,98},
    {"audio.master","1","Master volume, 0..2 (1 = 100%)",0,2},
    {"ui.scale","100","UI scale in percent, integer 75..200",75,200},
    {"hud.spacecraft_marker","1","Spacecraft circle and label opacity, 0..1",0,1},
    {"satellite.select","","Select catalog satellite by NORAD ID (research mode unlocked)",1,999999999},
    {"expert.reset","","Reset all expert physics and sensor settings",0,0}
};
}
void DeveloperConsole::refreshSuggestions(const std::string& text){
    if(text==suggestionInput&&!suggestions.empty())return;
    suggestionInput=text;selected=0;scrollSuggestion=true;suggestions.clear();
    if(text.empty())return;
    const auto add=[&](const std::string& value,const std::string& label){suggestions.push_back({value,label});};
    if(text.find(' ')==std::string::npos){
        if(std::string("macro.run").starts_with(text))add("macro.run ","macro.run - Run a named macro");
        for(const char* name:{"radio.play","radio.stop","radio.mute","radio.unmute","radio.status","help","clear"})
            if(std::string(name).starts_with(text))add(std::string(name)+(std::string(name)=="radio.play"?" ":""),name);
        for(const auto& command:commands)if(std::string(command.name).starts_with(text))
            add(std::string(command.name)+((command.value[0]||std::string(command.name)=="satellite.select")?" ":""),std::string(command.name)+"  - "+command.help);
        if(actionBindings)for(const auto& action:actionBindings->actions)
            if(action.id.starts_with(text)&&std::none_of(std::begin(commands),std::end(commands),[&](const auto& c){return action.id==c.name;}))
                add(action.id,action.id+" - "+action.label);
        return;
    }
    if(actionBindings&&text.starts_with("macro.run ")) {
        for(const auto& macro:actionBindings->macros)if(macro.name.starts_with(text.substr(10)))add("macro.run "+macro.name,macro.name);
        return;
    }
    for(const auto& command:commands){
        const auto prefix=std::string(command.name)+" ";
        if(command.value[0]&&text.starts_with(prefix)&&std::string(command.value).starts_with(text.substr(prefix.size())))
            add(prefix+command.value,std::string(command.value)+"  (default) - "+command.help);
    }
    if(!text.starts_with("radio.play "))return;
    const auto last=text.find_last_of(' ');
    const auto prefix=text.substr(0,last+1),token=text.substr(last+1);
    if(prefix.ends_with("--volume ")){
        for(const char* value:{"0.1","0","0.5","1","2"})if(std::string(value).starts_with(token))
            add(prefix+value,std::string(value)+(std::string(value)=="0.1"?"  (default: 10%)":""));
    }else if(prefix.ends_with("--endat ")){
        for(const char* value:{"00:30","01:00","05:00"})if(std::string(value).starts_with(token))add(prefix+value,std::string(value)+"  (example; omit --endat for full recording)");
    }else if(last==10){
        std::error_code error;
        for(const auto& entry:std::filesystem::directory_iterator("assets/radio",error)){
            if(!entry.is_regular_file(error))continue;
            const auto extension=entry.path().extension().string();
            if(extension!=".mp3"&&extension!=".wav"&&extension!=".ogg"&&extension!=".flac")continue;
            const auto name=entry.path().filename().string();
            const auto id=name.substr(0,name.find(' '));
            if(id.starts_with(token))add(prefix+id+" ",name);
        }
        std::sort(suggestions.begin(),suggestions.end(),[](const auto& a,const auto& b){return a.label<b.label;});
    }else{
        for(const char* flag:{"--volume","--endat"})if(std::string(flag).starts_with(token)&&prefix.find(flag)==std::string::npos)
            add(prefix+flag+" ",std::string(flag)+(std::string(flag)=="--volume"?"  default: 0.1; range: 0..2":"  default: full recording; seconds or HH:MM:SS"));
    }
}
int DeveloperConsole::complete(ImGuiInputTextCallbackData* data){
    auto& self=*static_cast<DeveloperConsole*>(data->UserData);
    if(data->EventFlag==ImGuiInputTextFlags_CallbackCharFilter)return data->EventChar=='`'?1:0;
    self.refreshSuggestions(data->Buf);
    if(data->EventFlag==ImGuiInputTextFlags_CallbackHistory&&!self.suggestions.empty()){
        const int count=int(self.suggestions.size());
        self.selected=(self.selected+(data->EventKey==ImGuiKey_UpArrow?-1:1)+count)%count;
        self.scrollSuggestion=true;
    }
    if(data->EventFlag==ImGuiInputTextFlags_CallbackCompletion&&!self.suggestions.empty()){
        const auto value=self.suggestions[self.selected].replacement;
        data->DeleteChars(0,data->BufTextLen);data->InsertChars(0,value.c_str());
        self.refreshSuggestions(value);
    }
    return 0;
}
void DeveloperConsole::execute(MusicPlayer& player,ADCSView& view,DisplaySettings& display,const std::string& text){
    std::istringstream stream(text);std::string name,argument,extra;stream>>name;
    if(name=="macro.run") {
        std::getline(stream>>std::ws,argument);view.actions().runMacro(argument);
        lines.push_back("> "+text);lines.push_back(view.actions().status);return;
    }
    if(view.actions().contains(name)) {
        lines.push_back("> "+text);
        if(stream>>extra){lines.push_back("Error: unexpected argument.");return;}
        view.actions().run(name);lines.push_back(view.actions().status);return;
    }
    for(const auto& command:commands)if(name==command.name){
        lines.push_back("> "+text);
        const bool takesValue=command.value[0]||name=="satellite.select";
        double value=0;
        if(takesValue){
            if(!(stream>>argument)){lines.push_back(std::string(command.help)+(command.value[0]?std::string("; default: ")+command.value:"; no default satellite"));return;}
            try{size_t used=0;value=std::stod(argument,&used);if(used!=argument.size()||!std::isfinite(value)||value<command.minimum||value>command.maximum)throw std::invalid_argument("range");}
            catch(...){lines.push_back(std::string("Error: ")+command.help);return;}
        }
        if(stream>>extra){lines.push_back("Error: unexpected argument.");return;}
        if((name=="sim.speed"&&(view.m_realScale||view.m_replayActive))||
           ((name=="orbit.altitude"||name=="orbit.inclination")&&(view.m_replayActive||view.m_numericalOrbit||view.m_entryPreview))||
           (name=="camera.speed"&&!view.m_realScale)){
            lines.push_back("Error: this value is controlled by the current mode. "+std::string(command.help));return;
        }
        if((name=="ui.scale"||name=="satellite.select")&&std::floor(value)!=value){lines.push_back("Error: enter a whole number.");return;}
        if(name=="sim.speed")view.m_timeWarp=float(value);
        else if(name=="camera.speed")view.m_userSpeedKmS=float(value);
        else if(name=="camera.sensitivity")view.m_sensitivity=float(value);
        else if(name=="camera.fov")view.m_cameraFov=float(value);
        else if(name=="orbit.altitude")view.m_orbitAltitudeKm=value;
        else if(name=="orbit.inclination")view.m_orbitInclinationDeg=value;
        else if(name=="audio.master"){player.setMasterVolumePercent(int(std::round(value*100)));view.m_alarmAudio.setGain(player.masterVolumePercent()/100.f);}
        else if(name=="ui.scale")display.uiScale=int(value);
        else if(name=="hud.spacecraft_marker")view.setMarkerOpacity(float(value));
        else if(name=="expert.reset")view.resetExpertSettings();
        else if(name=="satellite.select"){
            if(!view.m_researchUnlocked){lines.push_back("Error: unlock research satellites first.");return;}
            const auto found=std::find_if(view.m_researchCatalog.satellites.begin(),view.m_researchCatalog.satellites.end(),[&](const auto& satellite){return satellite.norad==unsigned(value);});
            if(found==view.m_researchCatalog.satellites.end()){lines.push_back("Error: NORAD ID not in the loaded catalog.");return;}
            view.m_showResearch=true;view.m_selectedResearch=unsigned(value);
        }
        lines.push_back("Applied: "+name+(takesValue?" "+argument:""));return;
    }
    RadioCommand command;std::string message;lines.push_back("> "+text);
    if(!parseRadioCommand(text,command,message)){lines.push_back("Error: "+message);return;}
    if(command.name=="help")lines.push_back("radio.play 01 [--volume 0..2] [--endat seconds|MM:SS|HH:MM:SS]\nNumbers (01, 02, ...) or quoted filenames from assets/radio only.\nradio.stop | radio.mute | radio.unmute | radio.status | clear\nDefault volume: 0.1. Invalid/out-of-range end times are ignored.\nPlayback fades in over 5 seconds. Mute takes precedence.");
    else if(command.name=="clear")lines.clear();
    else if(command.name=="radio.play"){
        auto path=std::filesystem::u8path(command.file);player.playRadioFile(path,command.volume,command.endAt,message);lines.push_back(message);
    }else if(command.name=="radio.stop"){player.stopRadio();lines.push_back("Radio stopped. Next automatic clip in 15-30 minutes if enabled.");}
    else if(command.name=="radio.mute"){player.setRadioMuted(true);lines.push_back("Radio communications muted.");}
    else if(command.name=="radio.unmute"){player.setRadioMuted(false);lines.push_back("Radio communications unmuted.");}
    else if(command.name=="radio.status")lines.push_back(player.radioStatus()+(player.radioMuted()?" | MUTED":" | audible if playing"));
    if(command.name=="help")for(const auto& item:commands)lines.push_back(std::string(item.name)+" - "+item.help+(item.value[0]?std::string("; default: ")+item.value:""));
    if(command.name=="help") {
        for(const auto& action:view.actions().actions)lines.push_back(action.id+" - "+action.label);
        lines.push_back("macro.run <name> - Run a macro configured in Settings > Controls");
    }
}
void DeveloperConsole::draw(MusicPlayer& player,ADCSView& view,DisplaySettings& display){
    if(ImGui::IsKeyPressed(ImGuiKey_GraveAccent,false)){open=!open;focus=open;}
    if(!open)return;
    auto size=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(size,ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(.98f);
    // Keep safety/confirmation modals authoritative. Settings and replay are
    // regular windows, so the console can receive focus above either one.
    const bool blocked=ImGui::GetTopMostPopupModal()!=nullptr;
    if(focus&&!blocked)ImGui::SetNextWindowFocus();
    if(ImGui::Begin("Console###ConsoleLayoutV2",&open,ImGuiWindowFlags_NoFocusOnAppearing)){
        if(lines.size()>100)lines.erase(lines.begin(),lines.end()-100);
        refreshSuggestions(input);
        ImGui::BeginChild("Output",ImVec2(0,-ImGui::GetFrameHeightWithSpacing()),true);
        for(const auto& line:lines)ImGui::TextWrapped("%s",line.c_str());
        if(focus)ImGui::SetScrollHereY(1);ImGui::EndChild();
        if(focus&&!blocked){ImGui::SetKeyboardFocusHere();focus=false;}
        ImGui::SetNextItemWidth(-1);
        if(ImGui::InputText("##Command",input,sizeof(input),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackCompletion|ImGuiInputTextFlags_CallbackHistory|ImGuiInputTextFlags_CallbackEdit|ImGuiInputTextFlags_CallbackCharFilter,complete,this)){
            execute(player,view,display,input);input[0]=0;focus=true;
        }
        const auto inputMin=ImGui::GetItemRectMin(),inputMax=ImGui::GetItemRectMax();
        const bool typing=ImGui::IsItemActive()||ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        refreshSuggestions(input);
        if(!suggestions.empty()&&typing){
        const float height=std::min(size.y-24,ImGui::GetTextLineHeightWithSpacing()*std::min(5,int(suggestions.size()))+ImGui::GetStyle().WindowPadding.y*2);
        const float y=inputMax.y+height<=size.y?inputMax.y:std::max(0.f,inputMin.y-height);
        ImGui::SetNextWindowPos(ImVec2(inputMin.x,y));
        ImGui::SetNextWindowSize(ImVec2(inputMax.x-inputMin.x,height));
        ImGui::SetNextWindowBgAlpha(1);
        ImGui::Begin("##ConsoleSuggestions",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNavFocus);
        // Refocusing the input after Enter raises the console above this existing
        // window. Keep completions above it without stealing keyboard focus.
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        for(int i=0;i<int(suggestions.size());++i){
            if(ImGui::Selectable(suggestions[i].label.c_str(),i==selected)){
                const auto value=suggestions[i].replacement;
                const auto length=std::min(value.size(),sizeof(input)-1);
                std::copy_n(value.begin(),length,input);input[length]=0;focus=true;
            }
            if(i==selected&&scrollSuggestion)ImGui::SetScrollHereY();
        }
        scrollSuggestion=false;
        ImGui::End();
        }
    }ImGui::End();
}
}

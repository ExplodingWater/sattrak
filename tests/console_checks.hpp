#pragma once
#include "gui/developer_console.hpp"
#include "gui/adcs_view.hpp"
#include "gui/display_settings.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstring>
namespace gui {
struct ConsoleChecks {
    static bool commands(DeveloperConsole& console,MusicPlayer& player,ADCSView& view,DisplaySettings& display){
        const auto run=[&](const char* text){console.execute(player,view,display,text);};
        run("hud.spacecraft_marker 0.37");if(view.m_markerOpacity!=.37f)return false;
        run("hud.spacecraft_marker nan");run("hud.spacecraft_marker -1");run("hud.spacecraft_marker 2");
        if(view.m_markerOpacity!=.37f)return false;
        run("hud.spacecraft_marker 0");if(view.m_markerOpacity!=0)return false;
        run("hud.spacecraft_marker 1");
        run("sim.speed 12");if(view.m_timeWarp!=12)return false;
        run("sim.speed nan");if(view.m_timeWarp!=12)return false;
        run("sim.pause");if(!view.m_isPaused)return false;
        run("sim.resume");if(view.m_isPaused)return false;
        run("camera.sensitivity 0.006");if(view.m_sensitivity!=.006f)return false;
        run("camera.fov 70");if(view.m_cameraFov!=70)return false;
        run("camera.fov 200");if(view.m_cameraFov!=70)return false;
        run("orbit.altitude 700");run("orbit.inclination 60");
        if(view.m_orbitAltitudeKm!=700||view.m_orbitInclinationDeg!=60)return false;
        view.m_replayActive=true;run("orbit.altitude 800");view.m_replayActive=false;
        if(view.m_orbitAltitudeKm!=700)return false;
        view.m_realScale=true;run("camera.speed 10");run("sim.speed 20");view.m_realScale=false;
        if(view.m_userSpeedKmS!=10||view.m_timeWarp!=12)return false;
        run("audio.master 0.5");if(player.masterVolumePercent()!=50)return false;
        run("ui.scale 125");if(display.uiScale!=125)return false;
        run("trail.clear");if(view.m_trail.size()!=1)return false;
        run("expert.reset");if(view.m_numericalOrbit||view.m_sensorNoise)return false;
        view.m_researchUnlocked=true;run("satellite.select 25544");
        if(view.m_selectedResearch!=25544)return false;
        run("sim.speed 5");run("camera.fov 45");run("audio.master 1");run("ui.scale 100");
        console.lines.clear();
        view.m_settingsVisible=true;view.m_replayWindow=true;
        return true;
    }
    static bool interaction(const DeveloperConsole& console,const ADCSView& view){
        return view.m_settingsVisible&&view.m_replayWindow&&view.m_cameraFov==60&&
            std::find(console.lines.begin(),console.lines.end(),"> radio.stop")!=console.lines.end();
    }
    static bool suggestionsAfterCommands(const DeveloperConsole& console) {
        auto* input=ImGui::FindWindowByName("Console###ConsoleLayoutV2");
        auto* suggestions=ImGui::FindWindowByName("##ConsoleSuggestions");
        if(!input||!suggestions||!suggestions->Active||std::string(console.input)!="ra"||console.suggestions.empty())return false;
        int inputOrder=-1,suggestionOrder=-1;
        for(int i=0;i<GImGui->Windows.Size;++i){if(GImGui->Windows[i]==input)inputOrder=i;if(GImGui->Windows[i]==suggestions)suggestionOrder=i;}
        return suggestionOrder>inputOrder;
    }
    static bool run(DeveloperConsole& console) {
        char buffer[2048]="ra";
        ImGuiInputTextCallbackData data;
        data.UserData=&console;data.Buf=buffer;data.BufSize=sizeof(buffer);data.BufTextLen=2;
        data.CursorPos=data.SelectionStart=data.SelectionEnd=2;
        console.refreshSuggestions(buffer);
        if(console.suggestions.size()<5)return false;
        data.EventFlag=ImGuiInputTextFlags_CallbackHistory;data.EventKey=ImGuiKey_DownArrow;
        DeveloperConsole::complete(&data);
        if(console.suggestions[console.selected].replacement!="radio.stop")return false;
        console.refreshSuggestions("radio.play 01 --volume ");
        if(console.suggestions.empty()||console.suggestions[0].replacement!="radio.play 01 --volume 0.1")return false;
        console.refreshSuggestions("radio.play 01 --");
        if(console.suggestions.size()!=2)return false;
        console.refreshSuggestions("unknown");
        if(!console.suggestions.empty())return false;
        console.refreshSuggestions("");
        if(!console.suggestions.empty())return false;
        console.refreshSuggestions("camera.s");
        if(console.suggestions.size()!=2)return false;
        console.refreshSuggestions("camera.fov ");
        return console.suggestions.size()==1&&console.suggestions[0].replacement=="camera.fov 45";
    }
};
}

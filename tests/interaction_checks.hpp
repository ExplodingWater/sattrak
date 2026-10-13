#pragma once
#include "gui/adcs_view.hpp"
#include "imgui_internal.h"
#include <fstream>
#include <iostream>

namespace gui {
struct InteractionChecks {
    static bool run(ADCSView& view) {
        bool okay=true;
        const auto eye=view.m_cameraEye,sat=view.m_satPosECI;
        view.m_cameraEye={12,0,0};view.m_satPosECI={0,12+650.0*12/6378.137,0};
        okay &= std::abs(view.cameraAltitudeKm())<1e-9&&std::abs(view.satelliteAltitudeKm()-650)<1e-9;
        view.m_cameraEye={0,0,12-10.0*12/6378.137};okay &= std::abs(view.cameraAltitudeKm()+10)<1e-9;
        view.m_cameraEye=eye;view.m_satPosECI=sat;
        view.setMarkerOpacity(.37f);view.actions().run("hud.marker.toggle");okay &= view.m_markerOpacity==0;
        view.actions().run("hud.marker.toggle");okay &= view.m_markerOpacity==.37f;
        const bool paused=view.m_isPaused;
        view.actions().run("view.save");view.actions().runMacro("Inspect spacecraft");
        okay &= view.m_isPaused&&view.m_camPreset==CameraPreset::SatelliteOrbit&&view.m_cameraFov==45;
        view.actions().runMacro("Restore view");view.m_isPaused=paused;
        view.actions().runMacro("Clean view");okay &= !view.m_hudVisible&&view.m_markerOpacity==0&&!view.m_showTrail;
        view.actions().runMacro("Restore view");okay &= view.m_hudVisible&&view.m_markerOpacity==.37f;
        view.setMarkerOpacity(1);

        ActionBindings bindings;int value=0;
        const auto setup=[&](ActionBindings& b){
            b.add("one","One",ImGuiKey_F10,[&]{value=value*10+1;return std::string{};});
            b.add("two","Two",ImGuiKey_F11,[&]{value=value*10+2;return std::string{};});
            b.add("fail","Fail",0,[]{return std::string("Unavailable");});
            b.macros={{"ordered",0,{"one","two"}},{"stops",0,{"one","fail","two"}}};
        };
        setup(bindings);bindings.runMacro("ordered");okay &= value==12;
        value=0;okay &= !bindings.runMacro("stops")&&value==1;
        okay &= !bindings.assign(false,0,ImGuiKey_F11);
        okay &= !bindings.assign(false,0,ImGuiKey_Space)&&!bindings.assign(false,0,ImGuiKey_C);
        okay &= !bindings.assign(false,0,ImGuiMod_Alt|ImGuiKey_F4);
        okay &= bindings.assign(true,0,ImGuiMod_Ctrl|ImGuiKey_F10);
        const std::string path="local/interaction-controls-test.json";
        // Start from an invalid fixture, then write and reload an actual binding change.
        std::filesystem::create_directories("local");std::ofstream(path)<<"invalid-json";
        bindings.configurePersistence(path);okay &= bindings.assign(false,0,ImGuiKey_F12);
        ActionBindings loaded;setup(loaded);loaded.configurePersistence(path);
        okay &= loaded.actions[0].binding==ImGuiKey_F12&&loaded.macros[0].binding==(ImGuiMod_Ctrl|ImGuiKey_F10)&&loaded.macros[0].steps.size()==2;
        if(!okay)std::cerr<<"Action, macro, persistence or altitude checks failed\n";
        return okay;
    }
    static bool keyboard(int frame) {
        static ActionBindings bindings;static int calls=0;
        auto& io=ImGui::GetIO();
        if(frame==0){bindings.add("check","Check",ImGuiKey_F12,[]{++calls;return std::string{};});io.AddKeyEvent(ImGuiKey_F12,true);}
        if(frame==1){
            const bool text=io.WantTextInput;io.WantTextInput=true;bindings.process(false);io.WantTextInput=text;
            bindings.process(true);if(calls!=0)return false;
            bindings.process(false);if(calls!=1)return false;
        }
        if(frame==2){bindings.process(false);io.AddKeyEvent(ImGuiKey_F12,false);if(calls!=1)return false;}
        return true;
    }
    static void prepare(ADCSView& view,int frame) {
        view.m_isPaused=true;view.m_alarmMuted=true;view.m_settingsVisible=frame<16;
        view.m_settingsCategory=std::min(frame/4,3);
        if(frame==0){view.selectModel(5);view.setRealScale(true);view.m_camPreset=CameraPreset::SatelliteOrbit;view.m_satOrbitDistance=73;}
        if(frame==16)view.setMarkerOpacity(0);
    }
};
}

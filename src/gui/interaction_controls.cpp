#include "gui/adcs_view.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace gui {
std::string ADCSView::textDescription() const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << "3D Earth and simulated spacecraft. " << (m_isPaused ? "Paused." : "Running.")
        << "\nSimulation time: " << m_simTimeSec << " s. Spacecraft altitude: " << satelliteAltitudeKm() << " km."
        << "\nCamera altitude: " << cameraAltitudeKm() << " km."
        << "\nGround target: " << m_targetName << " (" << m_targetLatDeg << ", " << m_targetLonDeg << " degrees)."
        << "\nTarget " << (m_isTargetVisible ? "above the horizon" : "blocked by Earth")
        << ". Elevation: " << m_elevationDeg << " degrees. Range: " << m_slantRangeKm << " km."
        << "\nPointing error: " << m_slewErrorDeg << " degrees. Body rate: " << m_bodyRateNormDegS << " degrees/s."
        << "\nSpacecraft " << (m_isSatSunlit ? "sunlit" : "in Earth's shadow") << ".";
    if (!m_groundTrack2D.empty()) out << "\nGround-track position: latitude " << m_groundTrack2D.back().lat
        << ", longitude " << m_groundTrack2D.back().lon << " degrees.";
    if (m_multiView) out << "\nMultiple views active; camera altitude refers to View 1.";
    out << "\nIllustrative simulation, not live spacecraft telemetry.";
    return out.str();
}
void ADCSView::setMarkerOpacity(float opacity) {
    if(!std::isfinite(opacity))return;
    m_markerOpacity=std::clamp(opacity,0.f,1.f);
    if(m_markerOpacity>0)m_lastMarkerOpacity=m_markerOpacity;
    saveHudPreferences();
}
void ADCSView::saveHudPreferences() {
    if(!m_rememberPreferences)return;
    std::error_code ec;std::filesystem::create_directories("local",ec);
    std::ofstream out("local/hud_settings.txt");
    out<<m_markerOpacity<<' '<<m_lastMarkerOpacity<<' '<<m_showAltimeters<<'\n';
}
void ADCSView::configurePreferences(bool remember) {
    m_rememberPreferences=remember;
    if(remember) {
        std::ifstream in("local/hud_settings.txt");float opacity,last;int alt;
        if(in>>opacity>>last>>alt && std::isfinite(opacity)&&std::isfinite(last)) {
            m_markerOpacity=std::clamp(opacity,0.f,1.f);m_lastMarkerOpacity=std::clamp(last,.01f,1.f);m_showAltimeters=alt!=0;
        }
    }
    registerActions();
    m_actions.configurePersistence(remember?"local/controls.json":"");
}
void ADCSView::registerActions() {
    if(!m_actions.actions.empty())return;
    const auto add=[&](const char* id,const char* label,ImGuiKeyChord key,auto fn){m_actions.add(id,label,key,fn);};
    add("settings.toggle","Open / close Settings",ImGuiKey_F2,[this]{m_settingsVisible=!m_settingsVisible;return std::string{};});
    add("hud.toggle","Show / hide HUD panels",ImGuiKey_F1,[this]{m_hudVisible=!m_hudVisible;return std::string{};});
    add("hud.hide","Hide HUD panels",0,[this]{m_hudVisible=false;return std::string{};});
    add("hud.show","Show HUD panels",0,[this]{m_hudVisible=true;return std::string{};});
    add("hud.marker.toggle","Toggle spacecraft marker",ImGuiKey_M,[this]{setMarkerOpacity(m_markerOpacity>0?0:m_lastMarkerOpacity);return std::string{};});
    add("hud.marker.hide","Hide spacecraft marker",0,[this]{setMarkerOpacity(0);return std::string{};});
    add("hud.altimeters.toggle","Show / hide altimeters",ImGuiKey_F3,[this]{m_showAltimeters=!m_showAltimeters;saveHudPreferences();return std::string{};});
    add("sim.toggle","Pause / resume simulation",ImGuiKey_P,[this]{m_isPaused=!m_isPaused;return std::string{};});
    add("sim.pause","Pause simulation",0,[this]{m_isPaused=true;return std::string{};});
    add("sim.resume","Resume simulation",0,[this]{m_isPaused=false;return std::string{};});
    for(bool faster:{false,true})add(faster?"sim.faster":"sim.slower",faster?"Increase simulation speed":"Decrease simulation speed",faster?ImGuiKey_Equal:ImGuiKey_Minus,[this,faster]{
        if(m_realScale||m_replayActive)return std::string("Time multiplier is unavailable in 1:1 or replay.");
        m_timeWarp=std::clamp(m_timeWarp*(faster?2.f:.5f),1.f,50.f);return std::string{};
    });
    for(bool next:{false,true})add(next?"camera.next":"camera.previous",next?"Next camera":"Previous camera",next?ImGuiKey_V:ImGuiMod_Shift|ImGuiKey_V,[this,next]{
        if(m_multiView)enableMultiView(false);
        m_camPreset=CameraPreset((int(m_camPreset)+(next?1:9))%10);return std::string{};
    });
    add("camera.focus","Focus spacecraft",ImGuiKey_Home,[this]{
        if(m_multiView)enableMultiView(false);m_camPreset=CameraPreset::SatelliteOrbit;
        m_satOrbitDistance=8*modelFramingScale();return std::string{};
    });
    add("camera.fov.reset","Reset field of view to 45 degrees",0,[this]{m_cameraFov=45;return std::string{};});
    add("views.toggle","Toggle multiple views",ImGuiKey_F4,[this]{enableMultiView(!m_multiView);return std::string{};});
    add("trail.toggle","Show / hide satellite trail",ImGuiKey_T,[this]{m_showTrail=!m_showTrail;return std::string{};});
    add("trail.hide","Hide satellite trail",0,[this]{m_showTrail=false;return std::string{};});
    add("trail.clear","Clear satellite trail",0,[this]{m_trail={{m_satPosECI,true}};m_trailInterval=1;m_nextTrailTime=m_simTimeSec+1;return std::string{};});
    add("recording.start","Start path recording",0,[this]{
        if(m_replayActive)return std::string("Cannot record while replaying an imported orbit.");
        if(m_recording.active())return std::string{};
        if(m_recording.unsaved())return std::string("Save the current recording before starting another.");
        startPathRecording();return m_recording.active()?std::string{}:m_recording.status;
    });
    add("recording.stop","Stop path recording",0,[this]{m_recording.stop();return std::string{};});
    add("recording.toggle","Start / stop path recording",ImGuiKey_F9,[this]{
        const bool ok=m_actions.run(m_recording.active()?"recording.stop":"recording.start");return ok?std::string{}:m_actions.status;
    });
    add("recording.save","Save recorded path",ImGuiMod_Ctrl|ImGuiKey_S,[this]{
        if(m_recording.active()||m_recording.count()<2)return std::string("Stop recording with at least two samples before saving.");
        auto destination=m_recording.path();destination.replace_extension(".csv");
        if(destination==m_recording.path())destination=destination.parent_path()/(destination.stem().string()+"-export.csv");
        return m_recording.save(destination)?std::string{}:m_recording.status;
    });
    add("music.previous","Previous music track",ImGuiKey_F6,[this]{if(m_audio)m_audio->previous();return std::string{};});
    add("music.toggle","Play / pause music",ImGuiKey_F7,[this]{if(m_audio)m_audio->togglePause();return std::string{};});
    add("music.next","Next music track",ImGuiKey_F8,[this]{if(m_audio)m_audio->next();return std::string{};});
    add("radio.toggle_mute","Mute / unmute radio",ImGuiKey_R,[this]{if(m_audio)m_audio->setRadioMuted(!m_audio->radioMuted());return std::string{};});
    add("alarm.toggle_mute","Mute / unmute entry alarm",0,[this]{m_alarmMuted=!m_alarmMuted;updateAlarm(!m_alarmMuted);return std::string{};});
    add("view.save","Remember camera and HUD preset (session)",0,[this]{
        m_preferredCamera=m_multiView?m_views[0].camera:captureCamera();m_preferredHud=m_hudVisible;m_preferredTrail=m_showTrail;
        m_preferredMarker=m_markerOpacity;m_preferredFov=m_cameraFov;m_preferredRealScale=m_realScale;m_hasPreferredView=true;return std::string{};
    });
    add("view.restore","Restore remembered camera and HUD",0,[this]{
        if(!m_hasPreferredView)return std::string("Use view.save to remember a view first (this session).");
        if(m_multiView)enableMultiView(false);setRealScale(m_preferredRealScale);applyCamera(m_preferredCamera);m_hudVisible=m_preferredHud;m_showTrail=m_preferredTrail;
        m_cameraFov=m_preferredFov;setMarkerOpacity(m_preferredMarker);return std::string{};
    });
    m_actions.macros={
        {"Clean view",ImGuiMod_Ctrl|ImGuiKey_F1,{"view.save","hud.hide","hud.marker.hide","trail.hide"}},
        {"Inspect spacecraft",ImGuiMod_Ctrl|ImGuiKey_F2,{"sim.pause","camera.focus","camera.fov.reset"}},
        {"Record orbit",0,{"recording.start","trail.clear","hud.show"}},
        {"Restore view",ImGuiMod_Ctrl|ImGuiKey_F3,{"view.restore"}}
    };
}
double ADCSView::cameraAltitudeKm() const {return m_cameraEye.norm()*(6378.137/12)-6378.137;}
double ADCSView::satelliteAltitudeKm() const {return m_satPosECI.norm()*(6378.137/12)-6378.137;}
void ADCSView::drawAltimeters() {
    if(!m_hudVisible||!m_showAltimeters)return;
    const auto* viewport=ImGui::GetMainViewport();
    const bool below=cameraAltitudeKm()<0;
    const char* title=m_multiView?"Altitude above Earth surface, view 1":"Altitude above Earth surface";
    char camera[64],satellite[64],range[64];
    snprintf(camera,sizeof(camera),"CAMERA     %12.3f km",cameraAltitudeKm());
    snprintf(satellite,sizeof(satellite),"SATELLITE  %12.3f km",satelliteAltitudeKm());
    snprintf(range,sizeof(range),"RANGE      %12.3f km",(m_cameraEye-m_satPosECI).norm()*(6378.137/12));
    const float pad=10*ImGui::GetStyle().FontScaleMain,line=ImGui::GetTextLineHeightWithSpacing();
    const float height=(below?5:4)*line+2*pad;
    ImVec2 origin(viewport->Pos.x+10,viewport->Pos.y+viewport->Size.y-height-16);
    auto* draw=ImGui::GetBackgroundDrawList();
    auto text=[&](int row,const char* value,ImU32 color){draw->AddText(ImVec2(origin.x+pad,origin.y+pad+row*line),color,value);};
    text(0,title,IM_COL32(135,160,184,255));text(1,camera,IM_COL32(115,218,255,255));
    text(2,satellite,IM_COL32(115,218,255,255));text(3,range,IM_COL32(160,180,198,255));
    if(below)text(4,"Camera below Earth surface",IM_COL32(255,128,51,255));
}
}

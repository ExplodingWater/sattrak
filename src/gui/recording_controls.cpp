#include "gui/adcs_view.hpp"
#include "imgui.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <commdlg.h>
#endif
namespace gui {
void ADCSView::startPathRecording() {
    if(m_replayActive)return;
    if(m_recording.start("local/recordings")){
        m_recordStartUtc=std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        m_recordStartSim=m_simTimeSec;m_recordNext=m_simTimeSec;captureOrbitSample();
    }
}
void ADCSView::findRecoveryRecordings() {
    m_recoveryFiles.clear();std::error_code ec;
    for(std::filesystem::directory_iterator it("local/recordings",ec),end;!ec&&it!=end;it.increment(ec)){
        auto marker=it->path();marker += ".saved";
        if(it->path().extension()==".partial"&&!std::filesystem::exists(marker,ec))m_recoveryFiles.push_back(it->path());
    }
    std::sort(m_recoveryFiles.rbegin(),m_recoveryFiles.rend());
    if(!m_recoveryFiles.empty()){m_replayWindow=true;m_recording.status="Unsaved recording journals found. Choose one under Recovery journal to reopen complete samples.";}
    else m_recording.status="No unsaved recovery journals found.";
}
OrbitSample ADCSView::currentRecordingSample() const {
    constexpr double pi=3.141592653589793,k=6378.137/12;
    aero::math::Vec3 r{m_satPosECI.x*k,-m_satPosECI.z*k,m_satPosECI.y*k},v;
    if(m_numericalOrbit)v=m_numericalState.velocity;
    else {
        const double a=m_trueAnomalyRad,i=m_orbitInclinationDeg*pi/180;
        double speed=std::sqrt(398600.44/(6378.137+m_orbitAltitudeKm))*(m_realScale?m_satelliteSpeedFactor:1);
        v=aero::math::Vec3{-std::sin(a),-std::cos(a)*std::cos(i),std::cos(a)*std::sin(i)}*speed;
        if(m_entryPreview)v=v-r.normalized()*m_descentKmS;
    }
    // Record Earth-fixed states so replay preserves even the preview's chosen Earth orientation.
    const double a=m_earthSpinAngleRad,c=std::cos(a),s=std::sin(a),spin=m_autoEarthSpin?7.292115e-5*m_earthSpinWarp:0;
    v=v-aero::math::Vec3{0,0,spin}.cross(r);
    auto rotate=[&](aero::math::Vec3 p){return aero::math::Vec3{c*p.x+s*p.y,-s*p.x+c*p.y,p.z};};
    return {m_recordStartUtc+m_simTimeSec-m_recordStartSim,rotate(r),rotate(v)};
}
void ADCSView::captureOrbitSample() {
    if(!m_recording.active()||m_simTimeSec+1e-6<m_recordNext)return;
    if(m_replayActive){m_recording.stop();m_recording.status="Recording stopped before imported replay.";return;}
    auto sample=currentRecordingSample();
    const double dt=sample.utc-m_lastRecorded.utc;
    if(m_recording.count()&&((sample.position-m_lastRecorded.position).norm()>0.5+2*dt*std::max(sample.velocity.norm(),m_lastRecorded.velocity.norm()))){m_recording.stop();m_recording.status="Recording stopped after an orbit/rotation discontinuity. Save this segment, then start another.";return;}
    if(m_recording.append(sample)){m_lastRecorded=sample;m_recordNext=m_simTimeSec+1;}
}
void ADCSView::drawRecordingControls() {
    ImGui::SeparatorText("Path recording");
    ImGui::BeginDisabled(m_replayActive||m_recording.active());
    if(ImGui::Button("Record path"))startPathRecording();
    ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(!m_recording.active());
    if(ImGui::Button("Stop recording"))m_actions.run("recording.stop");ImGui::EndDisabled();
    ImGui::Text("%s | %zu samples",m_recording.active()?"RECORDING":"Stopped",m_recording.count());
    ImGui::TextWrapped("Samples use simulation time (one per second); pausing simulation pauses recording. Files are synthetic ECEF trajectories, not measured satellite data.");
    ImGui::BeginDisabled(m_recording.count()<2||m_recording.active());
    if(ImGui::Button("Save")) {
        m_actions.run("recording.save");
    }
#ifdef _WIN32
    ImGui::SameLine();
    if(ImGui::Button("Save As...")) {
        wchar_t file[4096]=L"recorded-orbit.csv";
        OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);
        if(auto* window=glfwGetCurrentContext())dialog.hwndOwner=glfwGetWin32Window(window);
        dialog.lpstrFilter=L"Orbit CSV (*.csv)\0*.csv\0\0";
        dialog.lpstrFile=file;dialog.nMaxFile=4096;dialog.lpstrDefExt=L"csv";
        dialog.lpstrTitle=L"Save satellite path as CSV (choose a new filename)";
        dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetSaveFileNameW(&dialog)) {
            auto destination=std::filesystem::path(file);
            if(m_recording.save(destination)) {
                WideCharToMultiByte(CP_UTF8,0,file,-1,m_recordSavePath,int(sizeof(m_recordSavePath)),nullptr,nullptr);
            }
        } else if(CommDlgExtendedError()!=0) {
            m_recording.status="Could not open Save dialog. The unsaved recording is still available; use Save for the default folder.";
        }
    }
#else
    ImGui::InputText("Save as path",m_recordSavePath,sizeof(m_recordSavePath));
    if(ImGui::Button("Save As..."))m_recording.save(std::filesystem::u8path(m_recordSavePath));
#endif
    ImGui::SameLine();if(ImGui::Button("Open in replay"))loadReplay(m_recording.path());
    ImGui::EndDisabled();
    if(ImGui::Button("Find recoverable recordings"))findRecoveryRecordings();
    ImGui::BeginDisabled(m_recording.active());
    if(!m_recoveryFiles.empty()&&ImGui::BeginCombo("Recovery journal","Choose a recording")){
        for(const auto& path:m_recoveryFiles)if(ImGui::Selectable(path.filename().string().c_str())){
            if(m_recording.recover(path))loadReplay(m_recording.path());
        }ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",m_recording.status.c_str());
    ImGui::TextWrapped("Save keeps a uniquely named CSV in local/recordings beneath the launch folder. Save As... chooses another location. Starting another recording or exiting discards unsaved work. At least two samples are needed. Saved exports are kept.");
}
}

#include "gui/adcs_view.hpp"
#include "gui/numeric_widgets.hpp"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cmath>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

namespace gui {
using aero::math::Vec3;
namespace {
constexpr double pi=3.141592653589793,scale=12/6378.137;
void hint(const char* text) {if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal|ImGuiHoveredFlags_NoSharedDelay))ImGui::SetTooltip("%s",text);}
bool browse(char* path,size_t capacity) {
#ifdef _WIN32
    wchar_t file[4096]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);
    dialog.lpstrFilter=L"Orbit files (*.csv;*.oem;*.txt)\0*.csv;*.oem;*.txt\0All files\0*.*\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=4096;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog))return false;
    return WideCharToMultiByte(CP_UTF8,0,file,-1,path,int(capacity),nullptr,nullptr)>0;
#else
    (void)path;(void)capacity;return false;
#endif
}
std::filesystem::path utf8path(const char* p){return std::filesystem::u8path(p);}
void exact(const char* label,double& value,float low,float high,const char* format="%.3f") {
    float v=float(value);if(sliderFloatExact(label,&v,low,high,format,format,ImGuiSliderFlags_Logarithmic))value=v;
}
}
bool ADCSView::loadReplay(const std::filesystem::path& path,bool reference) {
    OrbitReplay loaded;
    if(!loaded.load(path,m_replayMessage))return false;
    if(reference) {
        m_reference=std::move(loaded);m_comparisonLine.clear();
        const size_t stride=std::max(size_t(1),m_reference.samples.size()/5000);
        for(size_t i=0;i<m_reference.samples.size();i+=stride){auto p=orbitToScene(m_reference.inertial(m_reference.samples[i]).position);m_comparisonLine.insert(m_comparisonLine.end(),{float(p.x),float(p.y),float(p.z)});}
        m_replayMessage="Comparison loaded; separation is evaluated at matching UTC times.";
    } else {
        if(!m_replayActive){m_replaySavedAltitude=m_orbitAltitudeKm;m_replaySavedSpin=m_earthSpinAngleRad;}
        m_replay=std::move(loaded);m_replayActive=true;m_entryPreview=false;m_isPaused=true;
        m_showTrail=true;m_physicsAccumulator=0;seekReplay(0);
        m_replayMessage="Orbit loaded and paused at its first sample. Press Play to replay.";
    }
    return true;
}
void ADCSView::seekReplay(double seconds) {
    if(!m_replayActive)return;
    m_replaySeconds=std::clamp(seconds,0.0,m_replay.samples.back().utc-m_replay.samples.front().utc);
    OrbitSample local;
    if(!m_replay.sample(m_replay.samples.front().utc+m_replaySeconds,local)){m_replayMessage="Interpolation leaves the supported Earth-orbit domain; replay paused.";m_isPaused=true;return;}
    m_currentOrbit=m_replay.inertial(local);m_satPosECI=orbitToScene(m_currentOrbit.position);
    m_orbitAltitudeKm=m_currentOrbit.position.norm()-6378.137;m_earthSpinAngleRad=earthSiderealAngle(local.utc);
    m_trail.clear();m_groundTrack2D.clear();
    auto end=std::upper_bound(m_replay.samples.begin(),m_replay.samples.end(),local.utc,[](double t,const auto& s){return t<s.utc;});
    size_t count=size_t(end-m_replay.samples.begin()),stride=std::max(size_t(1),count/4096);
    auto add=[&](const OrbitSample& s){auto inertial=m_replay.inertial(s);auto p=orbitToScene(inertial.position);m_trail.push_back({p,m_trail.empty()});
        double a=earthSiderealAngle(s.utc),c=std::cos(a),sn=std::sin(a);Vec3 ecef{c*inertial.position.x+sn*inertial.position.y,-sn*inertial.position.x+c*inertial.position.y,inertial.position.z};
        m_groundTrack2D.push_back({float(std::asin(std::clamp(ecef.z/ecef.norm(),-1.0,1.0))*180/pi),float(std::atan2(ecef.y,ecef.x)*180/pi)});};
    for(size_t i=0;i<count;i+=stride)add(m_replay.samples[i]);add(local);
    m_previousTrailPosition=m_satPosECI;computeLineOfSight();updateReentry();
}
void ADCSView::advanceReplay(double dt) {
    const double duration=m_replay.samples.back().utc-m_replay.samples.front().utc;
    seekReplay(m_replaySeconds+dt*m_replaySpeed);
    if(m_replaySeconds>=duration){m_isPaused=true;m_replayMessage="End of recorded orbit; replay paused.";}
}
void ADCSView::stopReplay() {
    if(!m_replayActive)return;
    m_replayActive=false;m_orbitAltitudeKm=m_replaySavedAltitude;m_earthSpinAngleRad=m_replaySavedSpin;
    m_trail.clear();m_groundTrack2D.clear();m_numericalReady=false;m_physicsAccumulator=0;
    advancePhysics(.00001);m_isPaused=true;m_replayMessage="Returned to simulation, paused.";
}
void ADCSView::exportReplayReport(const std::filesystem::path& path) {
    if(!m_replayActive)return;
    std::error_code ec;
    if(std::filesystem::exists(path,ec)){m_replayMessage="Output already exists. Choose a new report filename.";return;}
    std::ofstream out(path);if(!out){m_replayMessage="Cannot create report at this path.";return;}
    out<<"utc,station,range_km,elevation_deg,above_mask,comparison_separation_km\n"<<std::setprecision(12);
    const auto fixed=latLonToECEF(m_targetLatDeg,m_targetLonDeg,12)/scale;
    for(const auto& raw:m_replay.samples){auto s=m_replay.inertial(raw);auto p=orbitToScene(s.position)/scale;double angle=earthSiderealAngle(s.utc),c=std::cos(angle),sn=std::sin(angle);
        Vec3 station{c*fixed.x+sn*fixed.z,fixed.y,-sn*fixed.x+c*fixed.z};auto relative=p-station;
        double elevation=std::asin(std::clamp(relative.normalized().dot(station.normalized()),-1.0,1.0))*180/pi;
        out<<formatOrbitUtc(s.utc)<<','<<std::quoted(m_targetName)<<','<<relative.norm()<<','<<elevation<<','<<(elevation>=m_elevationMask?1:0)<<',';
        OrbitSample ref;if(m_reference.sample(s.utc,ref))out<<(s.position-m_reference.inertial(ref).position).norm();out<<'\n';}
    m_replayMessage=out?"Exported station visibility and comparison report.":"Report write failed.";
}
void ADCSView::drawOrbitReplay() {
    if(!m_replayWindow)return;
    ImGui::SetNextWindowSize(ImVec2(620,610),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_Appearing,ImVec2(.5f,.5f));
    ImGui::SetNextWindowBgAlpha(.98f);
    if(ImGui::Begin("Recorded orbit replay",&m_replayWindow)) {
        ImGui::TextWrapped("Import timestamped position and velocity in kilometers and km/s. CSV and single-segment CCSDS OEM (KVN) are supported.");
        ImGui::InputText("Primary file",m_replayPath,sizeof(m_replayPath));
        if(ImGui::Button("Browse primary"))browse(m_replayPath,sizeof(m_replayPath));ImGui::SameLine();
        if(ImGui::Button("Load primary"))loadReplay(utf8path(m_replayPath));
        ImGui::InputText("Comparison file",m_referencePath,sizeof(m_referencePath));
        if(ImGui::Button("Browse comparison"))browse(m_referencePath,sizeof(m_referencePath));ImGui::SameLine();
        if(ImGui::Button("Load comparison"))loadReplay(utf8path(m_referencePath),true);
        if(!m_reference.samples.empty()){ImGui::SameLine();if(ImGui::Button("Clear comparison")){m_reference={};m_comparisonLine.clear();}}
        if(m_replayActive) {
            ImGui::Separator();ImGui::Text("%s | %zu samples",m_replay.name.c_str(),m_replay.samples.size());
            ImGui::Text("UTC: %s",formatOrbitUtc(m_currentOrbit.utc).c_str());
            if(ImGui::Button(m_isPaused?"Play replay":"Pause replay"))m_isPaused=!m_isPaused;ImGui::SameLine();
            if(ImGui::Button("Start"))seekReplay(0);ImGui::SameLine();
            if(ImGui::Button("Return to simulation"))stopReplay();
            float seconds=float(m_replaySeconds),duration=float(m_replay.samples.back().utc-m_replay.samples.front().utc);
            if(sliderFloatExact("Elapsed seconds",&seconds,0,duration,"%.1f s","%.3f"))seekReplay(seconds);
            sliderFloatExact("Replay speed",&m_replaySpeed,.1f,1000,"%.1fx","%.2f",ImGuiSliderFlags_Logarithmic);
            hint("Replay time multiplier; 1x is real time even when the camera uses overview scale. Stops at the last recorded epoch.");
            ImGui::Text("Remaining: %.1f s | Altitude: %.3f km | Speed: %.3f km/s",std::max(0.0,duration-m_replaySeconds),m_orbitAltitudeKm,m_currentOrbit.velocity.norm());
            ImGui::Text("%s: elevation %.2f deg | range %.2f km",m_targetName.c_str(),m_elevationDeg,m_slantRangeKm);
            OrbitSample other;
            if(m_reference.sample(m_currentOrbit.utc,other))ImGui::Text("Comparison separation: %.6f km",(m_currentOrbit.position-m_reference.inertial(other).position).norm());
            else if(!m_reference.samples.empty())ImGui::TextDisabled("Comparison has no sample coverage at this UTC.");
            ImGui::TextDisabled("Green: replay history | Gold: comparison orbit");
            ImGui::InputText("Report path",m_reportPath,sizeof(m_reportPath));
            if(ImGui::Button("Export station report (CSV)"))exportReplayReport(utf8path(m_reportPath));
        }
        ImGui::Separator();ImGui::TextWrapped("%s",m_replayMessage.c_str());
        ImGui::TextWrapped("CSV requires # frame=EME2000 or # frame=ECEF, followed by: utc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s");
        ImGui::TextWrapped("OEM: Earth-centered, UTC, EME2000/J2000 or ITRF/ECEF. Cubic Hermite interpolation; no extrapolation. Ground coordinates use approximate GMST rotation, without Earth-orientation corrections. Recorded positions override J2/drag and orbit-speed controls. Attitude is simulated, not imported.");
    }
    ImGui::End();
}
void ADCSView::initializeNumericalOrbit() {
    const double r=6378.137+m_orbitAltitudeKm,inc=m_orbitInclinationDeg*pi/180,a=m_trueAnomalyRad;
    m_numericalState={0,{r*std::cos(a),-r*std::sin(a)*std::cos(inc),r*std::sin(a)*std::sin(inc)},
        Vec3(-std::sin(a),-std::cos(a)*std::cos(inc),std::cos(a)*std::sin(inc))*std::sqrt(398600.4418/r)};
    m_numericalReady=true;
}
void ADCSView::drawExpertSettings() {
    if(!ImGui::CollapsingHeader("Expert physics & sensors"))return;
    ImGui::TextWrapped("Illustrative engineering controls. Mesh selection does not supply real spacecraft mass or inertia.");
    sliderFloatExact("Mass (kg)",&m_mass,.01f,1e6f,"%.3f","%.3f",ImGuiSliderFlags_Logarithmic);
    for(int i=0;i<3;++i){const char* names[]={"Inertia X (kg m2)","Inertia Y (kg m2)","Inertia Z (kg m2)"};sliderFloatExact(names[i],&m_inertiaValues[i],.0001f,1e8f,"%.4f","%.4f",ImGuiSliderFlags_Logarithmic);}
    sliderFloatExact("Wheel torque (Nm)",&m_wheelTorque,.000001f,1000,"%.6f","%.6f",ImGuiSliderFlags_Logarithmic);
    sliderFloatExact("Wheel capacity (Nms)",&m_wheelMomentum,.00001f,1e5f,"%.5f","%.5f",ImGuiSliderFlags_Logarithmic);
    if(ImGui::Button("Apply spacecraft properties")) {
        const double x=m_inertiaValues[0],y=m_inertiaValues[1],z=m_inertiaValues[2];
        if(x+y<z||x+z<y||y+z<x)m_expertMessage="Principal moments must satisfy Ix+Iy >= Iz (and permutations).";
        else {aero::adcs::SpacecraftParameters p;p.mass=m_mass;p.inertia=aero::math::Mat3::diag(x,y,z);p.inertiaInv=aero::math::Mat3::diag(1/x,1/y,1/z);p.maxWheelTorque=m_wheelTorque;p.maxWheelMomentum=m_wheelMomentum;
            m_dynamics=aero::adcs::SatelliteDynamics(p);m_orbitForces.mass=p.mass;
            m_satState.hWheel={};m_expertMessage="Properties applied; wheel momentum reset for the new configuration.";}
    }
    ImGui::TextWrapped("%s",m_expertMessage.c_str());
    ImGui::Checkbox("Gravity-gradient torque",&m_gravityGradient);
    hint("Adds the Earth's tidal attitude torque using your inertia tensor and current orbital radius.");
    ImGui::Checkbox("Custom slow slew limits",&m_customSlew);
    if(m_customSlew){sliderFloatExact("Rate limit (deg/s)",&m_maxSlew,.001f,5,"%.3f","%.3f");sliderFloatExact("Acceleration (deg/s2)",&m_slewAcceleration,.0001f,1,"%.4f","%.4f");}
    if(sliderFloatExact("Integration step (s)",&m_physicsStep,.001f,.05f,"%.3f","%.3f",ImGuiSliderFlags_Logarithmic))m_physicsAccumulator=0;
    hint("Fixed RK4 step for orbit and attitude; smaller steps improve numerical resolution at greater CPU cost.");
    ImGui::Checkbox("Simulate sensor noise and bias drift",&m_sensorNoise);
    sliderFloatExact("Gyro noise (rad/s/sqrt Hz)",&m_gyroNoise,.000001f,.01f,"%.6f","%.6f",ImGuiSliderFlags_Logarithmic);
    sliderFloatExact("Bias walk (rad/s/sqrt s)",&m_biasDrift,.0000001f,.001f,"%.7f","%.7f",ImGuiSliderFlags_Logarithmic);
    sliderFloatExact("Vector sensor sigma (rad)",&m_vectorNoise,.000001f,.1f,"%.6f","%.6f",ImGuiSliderFlags_Logarithmic);
    ImGui::InputFloat3("True gyro bias (rad/s)",m_biasValues,"%.5f");
    for(int i=0;i<3;++i){if(!std::isfinite(m_biasValues[i]))m_biasValues[i]=0;m_biasValues[i]=std::clamp(m_biasValues[i],-.1f,.1f);m_trueGyroBias[i]=m_biasValues[i];}
    if(ImGui::Button("Apply noise model / reset estimator")){aero::adcs::MEKFConfig c;c.sigmaGyroWhiteNoise=m_gyroNoise;c.sigmaGyroBiasDrift=m_biasDrift;m_mekf=aero::adcs::MEKF(c);m_biasWalk={};m_sensorRandom.seed(42);}
    hint("Aligns filter noise assumptions with these sensor settings and restarts bias estimation. Random seed is fixed for repeatable experiments.");
    sliderFloatExact("Station elevation mask (deg)",&m_elevationMask,0,30,"%.1f","%.1f");
    ImGui::BeginDisabled(m_replayActive);
    if(ImGui::Checkbox("Numerically propagated orbit",&m_numericalOrbit)){m_numericalReady=false;m_entryPreview=false;m_satelliteSpeedFactor=1;}
    hint("Start a circular trajectory from the current altitude and inclination, then integrate gravity and optional J2/drag forces. Orbit-speed override is disabled.");
    if(m_numericalOrbit){ImGui::Checkbox("Earth J2 (oblateness)",&m_orbitForces.j2);ImGui::Checkbox("Atmospheric drag",&m_orbitForces.drag);
        exact("Drag coefficient",m_orbitForces.cd,.1f,5);exact("Drag area (m2)",m_orbitForces.area,.001f,2000);
        exact("Density multiplier",m_orbitForces.densityScale,.01f,100);
        ImGui::TextWrapped("Drag uses a co-rotating, log-interpolated density table and fixed area. It does not model weather, ablation, or breakup. Propagation stops at 80 km altitude.");
        if(ImGui::Button("Reset numerical orbit to circular"))m_numericalReady=false;}
    ImGui::EndDisabled();
}
void ADCSView::drawReplayComparison(const Mat4& vp) {
    if(!m_replayActive||m_comparisonLine.empty())return;
    m_lineShader.use();m_lineShader.setMat4("uMVP",vp);m_lineShader.setVec4("uColor",1,.75f,.2f,1);
    glBindVertexArray(m_lineVao);glBindBuffer(GL_ARRAY_BUFFER,m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER,m_comparisonLine.size()*sizeof(float),m_comparisonLine.data(),GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINE_STRIP,0,GLsizei(m_comparisonLine.size()/3));glBindVertexArray(0);
}
}

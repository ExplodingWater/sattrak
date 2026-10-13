#include "gui/adcs_view.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cctype>

namespace gui {
namespace {
constexpr double pi=3.141592653589793,scale=12/6378.137;
void field(const char* name,const std::string& value){ImGui::TextWrapped("%s: %s",name,value.empty()||value=="None"?"Not available":value.c_str());}
std::string lower(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
}
void ADCSView::drawCatalogDetails() {
    if(!m_showResearch||!m_researchUnlocked)return;
    const float ui=ImGui::GetStyle().FontScaleMain;
    if(m_catalogBrowser) {
        ImGui::SetNextWindowSize(ImVec2(480*ui,420*ui),ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Research satellite catalog",&m_catalogBrowser)) {
            ImGui::InputText("Search name / NORAD",m_catalogSearch,sizeof(m_catalogSearch));
            ImGui::Text("%zu catalog entries | %zu predictions drawn",m_researchCatalog.satellites.size(),size_t(m_catalogRendered));
            ImGui::Checkbox("Inspect stale orbital predictions",&m_showStaleResearch);
            ImGui::TextWrapped("Elements beyond 14 days of simulation UTC are hidden by default. Freshness is not an accuracy guarantee.");
            auto query=lower(m_catalogSearch);std::vector<size_t> filtered;
            for(size_t i=0;i<m_researchCatalog.satellites.size();++i){const auto& s=m_researchCatalog.satellites[i];if(query.empty()||lower(s.name).find(query)!=std::string::npos||std::to_string(s.norad).find(query)!=std::string::npos)filtered.push_back(i);}
            ImGui::BeginChild("Satellites");ImGuiListClipper clipper;clipper.Begin(int(filtered.size()));
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto& s=m_researchCatalog.satellites[filtered[i]];auto label=s.name+" | NORAD "+std::to_string(s.norad);ImGui::PushID(int(s.norad));if(ImGui::Selectable(label.c_str(),m_selectedResearch==s.norad))m_selectedResearch=s.norad;ImGui::PopID();}
            ImGui::EndChild();
        }ImGui::End();
    }
    if(m_selectedResearch) {
        bool open=true;auto display=ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(std::min(display.x-24,520*ui),std::min(display.y-60,650*ui)),ImGuiCond_Appearing);
        ImGui::SetNextWindowBgAlpha(1);
        if(ImGui::Begin("Satellite information",&open)) {
            auto found=std::find_if(m_researchCatalog.satellites.begin(),m_researchCatalog.satellites.end(),[&](const ResearchSatellite& s){return s.norad==m_selectedResearch;});
            if(found==m_researchCatalog.satellites.end())ImGui::TextWrapped("The selected satellite is not present in the current orbital catalog.");
            else {
                const auto& satellite=*found;auto date=m_researchCatalog.epoch.AddSeconds(m_simTimeSec);
                ImGui::TextColored(ImVec4(.4f,.9f,1,1),"%s | NORAD %u",satellite.name.c_str(),satellite.norad);
                ImGui::SeparatorText("Predicted position and motion");field("Simulation UTC",date.ToString());
                const auto index=size_t(found-m_researchCatalog.satellites.begin());
                const bool stale=m_researchCatalog.stale(index,m_simTimeSec);
                if(stale)ImGui::TextColored(ImVec4(1,.4f,.2f,1),"STALE ELEMENTS: prediction %s",m_showStaleResearch?"unreliable":"hidden");
                auto prediction=m_researchCatalog.prediction(index,m_simTimeSec,m_showStaleResearch);
                if(prediction) {
                    const auto& state=*prediction;auto geo=state.ToGeodetic();
                    ImGui::Text("Latitude: %.5f deg | Longitude: %.5f deg",geo.latitude*180/pi,std::remainder(geo.longitude*180/pi,360.0));
                    ImGui::Text("Altitude: %.3f km | Speed: %.4f km/s",geo.altitude,state.Velocity().Magnitude());
                    auto r=state.Position(),v=state.Velocity();
                    ImGui::TextWrapped("TEME position (km): %.3f, %.3f, %.3f",r.x,r.y,r.z);
                    ImGui::TextWrapped("TEME velocity (km/s): %.5f, %.5f, %.5f",v.x,v.y,v.z);
                    ImGui::TextWrapped("Coordinates and speed are SGP4 predictions, not live measurements.");
                }else ImGui::TextWrapped("No usable prediction at this simulation time. Refresh orbital data or inspect a historical time near the element epoch. A new download does not make old elements current; this does not confirm decay.");
                ImGui::TextWrapped("Attitude / pointing: unavailable. Orbital elements do not describe spacecraft orientation.");
                ImGui::SeparatorText("Orbit and freshness");
                ImGui::Text("Inclination: %.4f deg | Eccentricity: %.7f",satellite.inclination,satellite.eccentricity);
                if(satellite.meanMotion>0)ImGui::Text("Period: %.2f min | Mean motion: %.6f rev/day",1440/satellite.meanMotion,satellite.meanMotion);
                field("Orbital source",satellite.source);field("Element epoch UTC",satellite.elementEpoch.ToString());
                double age=(date-satellite.elementEpoch).TotalDays();
                auto color=std::abs(age)>7?ImVec4(1,.3f,.2f,1):std::abs(age)>2?ImVec4(1,.75f,.2f,1):ImVec4(.45f,1,.6f,1);
                ImGui::TextColored(color,"Elements are %.2f days %s simulation time",std::abs(age),age>=0?"older than":"ahead of");
                ImGui::Text("Age now: %.2f days",(libsgp4::DateTime::Now()-satellite.elementEpoch).TotalDays());
                ImGui::TextWrapped("Age colors are advisory: 2 and 7 days. They are not accuracy guarantees; maneuvers can invalidate recent elements.");
                field("CelesTrak storage",m_researchCatalog.dataOrigin);field("CelesTrak snapshot UTC",m_researchCatalog.snapshotEpoch.ToString());field("SatNOGS storage",m_researchCatalog.satnogsOrigin);field("SatNOGS retrieved UTC",m_researchCatalog.satnogsDownloaded);
                ImGui::SeparatorText("SatNOGS identity and radio");auto info=m_researchCatalog.info.find(satellite.norad);
                if(info==m_researchCatalog.info.end())ImGui::TextWrapped("No matching SatNOGS metadata. Orbital details remain available above.");
                else {
                    const auto& data=info->second;field("Aliases",data.aliases);field("Status (snapshot)",data.status);field("Operator",data.operatorName);field("Countries",data.countries);
                    field("Launch",data.launched);field("Deployment",data.deployed);field("Metadata updated",data.updated);
                    if(!data.satId.empty()){auto url="https://db.satnogs.org/satellite/"+data.satId+"/";ImGui::TextLinkOpenURL("Open SatNOGS satellite record",url.c_str());}
                    if(data.transmitters.empty())ImGui::TextUnformatted("No transmitter information available.");
                    for(const auto& radio:data.transmitters)ImGui::TextWrapped("%s",radio.c_str());
                }
                ImGui::Separator();ImGui::TextWrapped("SatNOGS / Libre Space Foundation contributors. Data: CC BY-SA 4.0.");
            }
        }ImGui::End();if(!open)m_selectedResearch=0;
    }
    if(m_selectedCatalogStation) {
        bool open=true;ImGui::SetNextWindowSize(ImVec2(440*ui,400*ui),ImGuiCond_Appearing);
        ImGui::SetNextWindowBgAlpha(1);
        if(ImGui::Begin("SatNOGS ground station",&open)) {
            auto found=std::find_if(m_researchCatalog.stations.begin(),m_researchCatalog.stations.end(),[&](const CatalogStation& s){return s.id==m_selectedCatalogStation;});
            if(found!=m_researchCatalog.stations.end()) {
                const auto& s=*found;ImGui::TextColored(ImVec4(1,.3f,.25f,1),"%s | #%d",s.name.c_str(),s.id);
                ImGui::Text("Latitude: %.5f | Longitude: %.5f",s.latitude,s.longitude);ImGui::Text("Altitude: %.0f m | Horizon mask: %.0f deg",s.altitude,s.horizon);
                field("Status at retrieval",s.status);field("Last seen UTC",s.lastSeen);field("Antennas",s.antennas);ImGui::Text("Published observations: %d",s.observations);field("Retrieved UTC",m_researchCatalog.satnogsDownloaded);
                auto url="https://network.satnogs.org/stations/"+std::to_string(s.id)+"/";ImGui::TextLinkOpenURL("Open station on SatNOGS",url.c_str());
                ImGui::TextWrapped("Public station location. Status is from the saved snapshot; this is not a live connection. Station targeting is not implemented.");
                ImGui::TextWrapped("SatNOGS contributors | CC BY-SA 4.0");
            }else ImGui::TextUnformatted("Station absent from the current snapshot.");
        }ImGui::End();if(!open)m_selectedCatalogStation=0;
    }
}
void ADCSView::drawCatalogLayer(const Mat4& vp,int width,int height) {
    using aero::math::Vec3;(void)width;(void)height;
    std::vector<float> satellites,stations;m_catalogRendered=0;
    auto& io=ImGui::GetIO();const float radius=10*ImGui::GetStyle().FontScaleMain;
    float nearest=radius*radius;unsigned hoverSatellite=0;int hoverStation=0;
    std::string hover,pinned;ImVec2 pinnedAt(io.DisplaySize.x*.5f,io.DisplaySize.y-42);
    auto transform=[&](Vec3 p){double c=std::cos(m_earthSpinAngleRad),s=std::sin(m_earthSpinAngleRad);return Vec3{c*p.x+s*p.z,p.y,-s*p.x+c*p.z};};
    auto project=[&](Vec3 p,ImVec2& screen){
        double clip[4]{};for(int row=0;row<4;++row)clip[row]=vp.m[row]*p.x+vp.m[4+row]*p.y+vp.m[8+row]*p.z+vp.m[12+row];
        if(clip[3]<=0)return false;double x=clip[0]/clip[3],y=clip[1]/clip[3];
        screen={float((x*.5+.5)*io.DisplaySize.x),float((.5-y*.5)*io.DisplaySize.y)};
        auto delta=p-m_cameraEye;double distance=delta.norm();if(distance<1e-9)return false;
        auto direction=delta/distance;double b=m_cameraEye.dot(direction),c=m_cameraEye.dot(m_cameraEye)-144,disc=b*b-c;
        bool hidden=disc>=0&&-b-std::sqrt(disc)>0&&-b-std::sqrt(disc)<distance;
        return !hidden&&std::abs(x)<=1&&std::abs(y)<=1&&clip[2]>=-clip[3]&&clip[2]<=clip[3];
    };
    for(size_t i=0;i<m_researchCatalog.satellites.size();++i) {
        const auto& satellite=m_researchCatalog.satellites[i];Vec3 p;bool valid=m_researchCatalog.position(i,m_simTimeSec,p,m_showStaleResearch);
        const bool stale=m_researchCatalog.stale(i,m_simTimeSec);
        if(!valid){if(satellite.norad==m_selectedResearch)pinned=satellite.name+(stale&&!m_showStaleResearch?" | Stale prediction hidden":" | Invalid / unavailable prediction");continue;}
        p=transform(p*scale);satellites.insert(satellites.end(),{float(p.x),float(p.y),float(p.z)});++m_catalogRendered;
        ImVec2 screen;bool visible=project(p,screen);auto label=satellite.name+" | NORAD "+std::to_string(satellite.norad);
        if(stale)label+=" | STALE / UNRELIABLE";
        if(satellite.norad==m_selectedResearch){pinned=label+(visible?"":" | Out of view / behind Earth");if(visible)pinnedAt=ImVec2(screen.x+10,screen.y-24);}
        if(!visible)continue;float distance=(screen.x-io.MousePos.x)*(screen.x-io.MousePos.x)+(screen.y-io.MousePos.y)*(screen.y-io.MousePos.y);
        if(distance<nearest){nearest=distance;hover=label;hoverSatellite=satellite.norad;hoverStation=0;}
    }
    if(m_showCatalogStations)for(const auto& station:m_researchCatalog.stations) {
        if(!m_includeOfflineStations&&station.status!="Online")continue;
        double lat=station.latitude*pi/180,lon=station.longitude*pi/180,r=12.008+std::max(0.0,station.altitude)*scale/1000;
        Vec3 p=transform({r*std::cos(lat)*std::cos(lon),r*std::sin(lat),-r*std::cos(lat)*std::sin(lon)});
        stations.insert(stations.end(),{float(p.x),float(p.y),float(p.z)});ImVec2 screen;if(!project(p,screen))continue;
        float distance=(screen.x-io.MousePos.x)*(screen.x-io.MousePos.x)+(screen.y-io.MousePos.y)*(screen.y-io.MousePos.y);
        if(distance<nearest){nearest=distance;hover=station.name+" | SatNOGS station #"+std::to_string(station.id);hoverStation=station.id;hoverSatellite=0;}
    }
    auto drawPoints=[&](const std::vector<float>& points,bool red){
        m_lineShader.use();m_lineShader.setMat4("uMVP",vp);m_lineShader.setVec4("uColor",red?1:.35f,red?.18f:.85f,red?.15f:1,1);
        glBindVertexArray(m_lineVao);glBindBuffer(GL_ARRAY_BUFFER,m_lineVbo);glBufferData(GL_ARRAY_BUFFER,std::max(size_t(3072),points.size())*sizeof(float),nullptr,GL_DYNAMIC_DRAW);
        if(!points.empty())glBufferSubData(GL_ARRAY_BUFFER,0,points.size()*sizeof(float),points.data());
        glPointSize((red?4:5)*ImGui::GetStyle().FontScaleMain);glDrawArrays(GL_POINTS,0,GLsizei(points.size()/3));glPointSize(1);glBindVertexArray(0);
    };
    drawPoints(satellites,false);drawPoints(stations,true);if(m_renderingMulti)return;
    auto label=[&](const std::string& text,ImVec2 at,bool red){auto size=ImGui::CalcTextSize(text.c_str());at.x=std::clamp(at.x,8.0f,std::max(8.0f,io.DisplaySize.x-size.x-8));at.y=std::clamp(at.y,30.0f,std::max(30.0f,io.DisplaySize.y-size.y-8));auto* draw=ImGui::GetBackgroundDrawList();draw->AddRectFilled({at.x-4,at.y-3},{at.x+size.x+4,at.y+size.y+3},IM_COL32(0,0,0,235),3);draw->AddText(at,red?IM_COL32(255,100,80,255):IM_COL32(160,230,255,255),text.c_str());};
    if(!pinned.empty())label(pinned,pinnedAt,false);
    if(!hover.empty()&&sceneAcceptsInput()) {
        label(hover,{io.MousePos.x+12,io.MousePos.y-25},hoverStation!=0);
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)){if(hoverSatellite)m_selectedResearch=hoverSatellite;else m_selectedCatalogStation=hoverStation;}
    }
}
}

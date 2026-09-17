#include "gui/adcs_view.hpp"
#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#endif
namespace gui {
namespace {
std::string download(const std::wstring& path) {
#ifdef _WIN32
    struct Handle { HINTERNET h; ~Handle(){if(h)WinHttpCloseHandle(h);} };
    Handle session{WinHttpOpen(L"sattrak/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.h)throw std::runtime_error("Network initialization failed");
    WinHttpSetTimeouts(session.h,5000,5000,5000,10000);
    Handle connection{WinHttpConnect(session.h,L"celestrak.org",INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection.h)throw std::runtime_error("Connection failed");
    Handle request{WinHttpOpenRequest(connection.h,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.h || !WinHttpSendRequest(request.h,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request.h,nullptr))throw std::runtime_error("Network unavailable");
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)throw std::runtime_error("Data service unavailable");
    std::string body;char buffer[16384];DWORD received;
    for(;;){if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&received))throw std::runtime_error("Download interrupted");if(!received)break;body.append(buffer,received);if(body.size()>4000000)throw std::runtime_error("Response too large");}
    if(body.empty())throw std::runtime_error("Empty orbital data response");
    return body;
#else
    throw std::runtime_error("Online updates unavailable on this platform");
#endif
}
void saveChoice(int choice) {std::error_code ec;std::filesystem::create_directories("local",ec);std::ofstream("local/internet-choice.txt")<<choice;}
}
void ADCSView::initInternetChoice() {
    std::ifstream choice("local/internet-choice.txt");if(!(choice>>m_internetChoice)||(m_internetChoice!=0&&m_internetChoice!=1))m_internetChoice=-1;
    m_askInternet=m_internetChoice<0;
    std::string cached;std::ifstream("local/catalog-current.txt")>>cached;
    if(cached.starts_with("local/catalog-")&&cached.find("..") == std::string::npos){ResearchCatalog next;if(next.load(cached)){m_researchCatalog=std::move(next);m_dataStatus="Offline prediction from cached elements";}}
    if(m_internetChoice==1)startCatalogRefresh();
    m_researchCatalog.epoch=libsgp4::DateTime::Now().AddSeconds(-m_simTimeSec);
}
void ADCSView::startCatalogRefresh() {
    if(m_catalogDownload.valid())return;
    long long previous=0;std::ifstream("local/catalog-attempt.txt")>>previous;
    const auto now=std::time(nullptr);
    if(now>=previous&&now-previous<7200){m_dataStatus="Using saved elements; online checks limited to once per 2 hours";return;}
    try{std::filesystem::create_directories("local");std::ofstream("local/catalog-attempt.txt")<<now;}catch(...){m_dataStatus="Cache directory unavailable; using offline prediction";return;}
    m_dataStatus="Refreshing orbital elements...";
    m_catalogDownload=std::async(std::launch::async,[now] {
        try {
            std::string data;
            for(const auto* group:{L"science",L"resource",L"weather"})data+=download(std::wstring(L"/NORAD/elements/gp.php?GROUP=")+group+L"&FORMAT=TLE")+"\n";
            std::istringstream lines(data);std::string line,normalized;
            while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty())normalized+=line+'\n';}
            const std::string folder="local/catalog-"+std::to_string(now);
            std::filesystem::create_directories(folder);
            {std::ofstream out(folder+"/research.tle");out<<normalized;if(!out)throw std::runtime_error("Cache write failed");}
            std::tm date{};
#ifdef _WIN32
            gmtime_s(&date,&now);
#else
            gmtime_r(&now,&date);
#endif
            {std::ofstream out(folder+"/epoch.txt");out<<date.tm_year+1900<<' '<<date.tm_mon+1<<' '<<date.tm_mday<<' '<<date.tm_hour<<' '<<date.tm_min<<' '<<date.tm_sec;if(!out)throw std::runtime_error("Cache date write failed");}
            ResearchCatalog verified;if(!verified.load(folder)||verified.status.find("invalid")!=std::string::npos)throw std::runtime_error("Invalid orbital data");
            return folder;
        }catch(const std::exception& e){return std::string("ERROR: ")+e.what();}
    });
}
void ADCSView::drawInternetChoice() {
    if(m_catalogDownload.valid()&&m_catalogDownload.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto result=m_catalogDownload.get();ResearchCatalog next;
        if(!result.starts_with("ERROR:")&&next.load(result)){
            next.epoch=libsgp4::DateTime::Now().AddSeconds(-m_simTimeSec);
            m_researchCatalog=std::move(next);std::ofstream("local/catalog-current.txt")<<result;
            m_dataStatus="CelesTrak updated; predicting from refreshed elements";
        }else m_dataStatus="Online update failed; using saved prediction. "+result;
    }
    if(m_askInternet){ImGui::OpenPopup("Satellite data source");m_askInternet=false;}
    ImGui::SetNextWindowSize(ImVec2(540,0),ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Satellite data source",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Allow sattrak to download updated satellite orbital elements from CelesTrak? Offline mode predicts movement from saved data. Online mode also uses prediction, with fresher starting data, and falls back automatically if unavailable.");
        ImGui::TextWrapped("This updates the research satellite catalog. Your simulated spacecraft and imported orbit files are unchanged.");
        if(ImGui::Button("Use internet updates")){m_internetChoice=1;saveChoice(1);startCatalogRefresh();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Use offline prediction")){m_internetChoice=0;saveChoice(0);ImGui::CloseCurrentPopup();}
        ImGui::TextDisabled("You can change this later in Settings.");ImGui::EndPopup();
    }
}
void ADCSView::drawInternetSettings() {
    ImGui::SeparatorText("Satellite orbit data");
    bool online=m_internetChoice==1;
    ImGui::BeginDisabled(m_catalogDownload.valid());
    if(ImGui::Checkbox("Allow internet access to fetch satellite orbit data",&online)){m_internetChoice=online?1:0;saveChoice(m_internetChoice);if(online)startCatalogRefresh();else m_dataStatus="Offline prediction from saved elements";}
    ImGui::BeginDisabled(!online);
    if(ImGui::Button("Check for updated satellite data"))startCatalogRefresh();
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::Text("%zu satellites in the loaded catalog",m_researchCatalog.satellites.size());
    ImGui::TextWrapped("%s",m_dataStatus.c_str());
    ImGui::TextWrapped("Permission is remembered. Checks are limited to once per two hours; offline or failed requests use saved orbital data.");
    ImGui::TextWrapped("Catalog updates use CelesTrak TLE data for supported five-digit NORAD IDs. Movement is predicted, not live telemetry.");
    ImGui::Separator();
}
}

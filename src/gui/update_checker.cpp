#include "gui/update_checker.hpp"
#include "gui/update_release.hpp"
#include "gui/auto_update_win.hpp"
#include "gui/native_update.hpp"
#include "imgui.h"
#include <chrono>
#include <thread>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace gui {
namespace {
constexpr size_t maxResponse=2*1024*1024;
std::string fetchReleases() {
    std::string body;
#ifdef _WIN32
    struct Handle { HINTERNET value; ~Handle(){if(value)WinHttpCloseHandle(value);} };
    Handle session{WinHttpOpen(L"sattrak-update-check",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value)throw std::runtime_error("Network unavailable");
    WinHttpSetTimeouts(session.value,3000,3000,3000,5000);
    Handle connection{WinHttpConnect(session.value,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection.value)throw std::runtime_error("Connection failed");
    Handle request{WinHttpOpenRequest(connection.value,L"GET",L"/repos/ExplodingWater/sattrak/releases?per_page=30",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.value)throw std::runtime_error("Request failed");
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    if(!WinHttpSendRequest(request.value,L"Accept: application/vnd.github+json\r\n",DWORD(-1),nullptr,0,0,0) || !WinHttpReceiveResponse(request.value,nullptr))throw std::runtime_error("Request failed");
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX) || status!=200)throw std::runtime_error("Release service unavailable");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    char buffer[16384];DWORD received=0;
    for(;;) {
        if(std::chrono::steady_clock::now()>deadline || !WinHttpReadData(request.value,buffer,sizeof(buffer),&received))throw std::runtime_error("Response interrupted");
        if(!received)break;
        if(body.size()+received>maxResponse)throw std::runtime_error("Response too large");
        body.append(buffer,received);
    }
#else
    static const auto initialized=curl_global_init(CURL_GLOBAL_DEFAULT);
    if(initialized!=CURLE_OK)throw std::runtime_error("Network unavailable");
    auto handle=std::unique_ptr<CURL,decltype(&curl_easy_cleanup)>(curl_easy_init(),curl_easy_cleanup);
    if(!handle)throw std::runtime_error("Network unavailable");
    curl_easy_setopt(handle.get(),CURLOPT_URL,"https://api.github.com/repos/ExplodingWater/sattrak/releases?per_page=30");
    curl_easy_setopt(handle.get(),CURLOPT_USERAGENT,"sattrak-update-check");
    curl_easy_setopt(handle.get(),CURLOPT_CONNECTTIMEOUT,3L);
    curl_easy_setopt(handle.get(),CURLOPT_TIMEOUT,10L);
    curl_easy_setopt(handle.get(),CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(handle.get(),CURLOPT_FAILONERROR,1L);
    curl_easy_setopt(handle.get(),CURLOPT_WRITEDATA,&body);
    curl_easy_setopt(handle.get(),CURLOPT_WRITEFUNCTION,+[](char* data,size_t size,size_t count,void* context)->size_t {
        auto& output=*static_cast<std::string*>(context);
        if(size && count>maxResponse/size)return 0;
        const auto bytes=size*count;if(bytes>maxResponse-output.size())return 0;
        try{output.append(data,bytes);return bytes;}catch(...){return 0;}
    });
    if(curl_easy_perform(handle.get())!=CURLE_OK)throw std::runtime_error("Request failed");
    long status=0;curl_easy_getinfo(handle.get(),CURLINFO_RESPONSE_CODE,&status);
    if(status!=200)throw std::runtime_error("Release service unavailable");
#endif
    return body;
}
}
const char* UpdateChecker::installedVersion(){return SATTRAK_RELEASE_VERSION;}
void UpdateChecker::start() {
#ifdef _WIN32
    {std::lock_guard lock(state->mutex);
        if(!state->configured) {
            state->configured=true;state->target=autoupdate::installedFolder();
            try {
                int enabled=1;std::ifstream(autoupdate::cacheFolder()/L"automatic.txt")>>enabled;
                state->automatic=enabled!=0;
                std::ifstream result(autoupdate::cacheFolder()/L"result.txt");std::getline(result,state->detail);
                if(state->detail.size()>200)state->detail.clear();
            }catch(...){}
        }
    }
#else
    {std::lock_guard lock(state->mutex);
        if(!state->configured) {
            state->configured=true;state->target=nativeupdate::installedFolder();
            try {
                int enabled=1;std::ifstream(nativeupdate::cacheFolder()/"automatic.txt")>>enabled;
                state->automatic=enabled!=0;
                std::ifstream result(nativeupdate::cacheFolder()/"result.txt");std::getline(result,state->detail);
                if(state->detail.size()>200)state->detail.clear();
            }catch(...){}
        }
    }
#endif
    startWith(fetchReleases);
}
void UpdateChecker::startWith(std::function<std::string()> fetch) {
    const auto shared=state;
    {std::lock_guard lock(shared->mutex);if(shared->closing || shared->status==Status::Checking || shared->status==Status::Downloading || shared->status==Status::Ready)return;shared->status=Status::Checking;shared->dismissed=false;}
    try {
        // Shared state lets shutdown proceed without waiting for a network timeout
        std::thread([shared,fetch=std::move(fetch)] {
            try {
                const auto release=updates::selectRelease(fetch(),installedVersion(),updates::buildPlatform());
                bool automatic=false;
                {std::lock_guard lock(shared->mutex);
                    shared->status=release?Status::Available:Status::Current;
                    shared->tag=release?release->tag:"";shared->url=release?release->url:"";
                    shared->package=release?release->package:std::nullopt;
                    automatic=release && release->package && shared->automatic && !shared->target.empty() && !shared->closing;
                    if(automatic){shared->status=Status::Downloading;shared->received=0;shared->detail.clear();}
                }
                if(automatic)download(shared,*release->package);
            }catch(...) {std::lock_guard lock(shared->mutex);shared->status=Status::Unavailable;}
        }).detach();
    }catch(...) {std::lock_guard lock(shared->mutex);shared->status=Status::Unavailable;}
}
int UpdateChecker::commandLineCheck() {
    try {
        const auto release=updates::selectRelease(fetchReleases(),installedVersion(),updates::buildPlatform());
        std::cout<<"Installed "<<installedVersion()<<'\n';
        if(release)std::cout<<"Available "<<release->tag<<'\n'<<release->url<<'\n';
        else std::cout<<"No newer release found\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"Update check unavailable — "<<error.what()<<'\n';return 1;}
}
void UpdateChecker::download(const std::shared_ptr<State>& shared,const updates::Package& package) {
#ifdef _WIN32
    std::filesystem::path partial;
    try {
        using namespace autoupdate;
        const auto cache=cacheFolder(),destination=cache/(package.sha256+".exe");
        auto cachedValid=[&] {
            Handle file(CreateFileW(destination.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
            return verify(file.value,package.sha256,package.size);
        };
        if(!cachedValid()) {
            partial=cache/(package.sha256+"-"+std::to_string(GetCurrentProcessId())+".part");
            struct Internet {HINTERNET value;~Internet(){if(value)WinHttpCloseHandle(value);}};
            Internet session{WinHttpOpen(L"sattrak-updater",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
            if(!session.value)throw std::runtime_error("Network unavailable");
            WinHttpSetTimeouts(session.value,5000,5000,5000,15000);
            std::wstring url(package.url.begin(),package.url.end());
            bool complete=false;
            for(int redirectCount=0;redirectCount<5 && !complete;++redirectCount) {
                URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=parts.dwPasswordLength=DWORD(-1);
                if(!WinHttpCrackUrl(url.c_str(),DWORD(url.size()),0,&parts) || parts.nScheme!=INTERNET_SCHEME_HTTPS || parts.nPort!=443 || parts.dwUserNameLength || parts.dwPasswordLength)throw std::runtime_error("Invalid update URL");
                const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
                if(host!=L"github.com" && host!=L"release-assets.githubusercontent.com" && host!=L"objects.githubusercontent.com")throw std::runtime_error("Untrusted update host");
                std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);
                if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
                Internet connection{WinHttpConnect(session.value,host.c_str(),443,0)};
                if(!connection.value)throw std::runtime_error("Connection failed");
                Internet request{WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
                if(!request.value)throw std::runtime_error("Request failed");
                DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
                if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy)) ||
                   !WinHttpSendRequest(request.value,nullptr,0,nullptr,0,0,0) || !WinHttpReceiveResponse(request.value,nullptr))throw std::runtime_error("Download failed");
                DWORD status=0,size=sizeof(status);
                if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid response");
                if(status==301 || status==302 || status==303 || status==307 || status==308) {
                    size=0;WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&size,WINHTTP_NO_HEADER_INDEX);
                    if(size==0 || size>65536)throw std::runtime_error("Invalid redirect");
                    std::vector<wchar_t> location(size/sizeof(wchar_t)+1);
                    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid redirect");
                    url=location.data();continue;
                }
                if(status!=200)throw std::runtime_error("Download unavailable");
                {
                    Handle file(CreateFileW(partial.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
                    if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot save download");
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
                    uint64_t received=0;char buffer[65536];DWORD count=0,written=0;
                    for(;;) {
                        {std::lock_guard lock(shared->mutex);if(!shared->automatic || shared->closing)throw std::runtime_error("Update paused");}
                        if(std::chrono::steady_clock::now()>deadline || !WinHttpReadData(request.value,buffer,sizeof(buffer),&count))throw std::runtime_error("Download interrupted");
                        if(!count)break;
                        if(count>package.size-received || !WriteFile(file.value,buffer,count,&written,nullptr) || written!=count)throw std::runtime_error("Invalid or incomplete download");
                        received+=count;
                        {std::lock_guard lock(shared->mutex);shared->received=received;}
                    }
                    if(!FlushFileBuffers(file.value) || !verify(file.value,package.sha256,package.size))throw std::runtime_error("Update verification failed");
                }
                if(!MoveFileExW(partial.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot stage update");
                partial.clear();complete=true;
            }
            if(!complete)throw std::runtime_error("Too many redirects");
        }
        std::lock_guard lock(shared->mutex);
        shared->installer=destination;shared->status=Status::Ready;shared->received=package.size;
    }catch(...) {
        if(!partial.empty()){std::error_code error;std::filesystem::remove(partial,error);}
        std::lock_guard lock(shared->mutex);shared->status=Status::Available;
        shared->detail=shared->automatic?"Download or verification failed — use View release or retry":"Automatic installation is off";
    }
#else
    std::filesystem::path partial;
    try {
        const auto cache=nativeupdate::cacheFolder();std::filesystem::create_directories(cache);
        chmod(cache.c_str(),0700);
        const auto destination=cache/(package.sha256+".tar.gz");
        if(!nativeupdate::verify(destination,package.sha256,package.size)) {
            partial=cache/(package.sha256+"-"+std::to_string(getpid())+"-"+
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".part");
            const int fd=open(partial.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
            if(fd<0)throw std::runtime_error("Cannot save download");
            FILE* raw=fdopen(fd,"wb");if(!raw){close(fd);throw std::runtime_error("Cannot save download");}
            auto file=std::unique_ptr<FILE,decltype(&fclose)>(raw,fclose);
            static const auto initialized=curl_global_init(CURL_GLOBAL_DEFAULT);
            if(initialized!=CURLE_OK)throw std::runtime_error("Network unavailable");
            auto curl=std::unique_ptr<CURL,decltype(&curl_easy_cleanup)>(curl_easy_init(),curl_easy_cleanup);
            if(!curl)throw std::runtime_error("Network unavailable");
            struct Transfer { FILE* file;const std::shared_ptr<State>& state;uint64_t size,received=0; } transfer{file.get(),shared,package.size};
            curl_easy_setopt(curl.get(),CURLOPT_URL,package.url.c_str());
            curl_easy_setopt(curl.get(),CURLOPT_USERAGENT,"sattrak-updater");
            curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,1L);
            curl_easy_setopt(curl.get(),CURLOPT_MAXREDIRS,5L);
#if LIBCURL_VERSION_NUM >= 0x075500
            curl_easy_setopt(curl.get(),CURLOPT_PROTOCOLS_STR,"https");
            curl_easy_setopt(curl.get(),CURLOPT_REDIR_PROTOCOLS_STR,"https");
#else
            curl_easy_setopt(curl.get(),CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);
            curl_easy_setopt(curl.get(),CURLOPT_REDIR_PROTOCOLS,CURLPROTO_HTTPS);
#endif
            curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,10L);
            curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT,600L);
            curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_LIMIT,1024L);
            curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_TIME,30L);
            curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);
            curl_easy_setopt(curl.get(),CURLOPT_FAILONERROR,1L);
            curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&transfer);
            curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,+[](char* data,size_t size,size_t count,void* context)->size_t {
                auto& t=*static_cast<Transfer*>(context);
                if(size && count>(t.size-t.received)/size)return 0;
                const size_t bytes=size*count;
                {std::lock_guard lock(t.state->mutex);if(!t.state->automatic || t.state->closing)return 0;}
                if(fwrite(data,1,bytes,t.file)!=bytes)return 0;
                t.received+=bytes;
                {std::lock_guard lock(t.state->mutex);t.state->received=t.received;}
                return bytes;
            });
            if(curl_easy_perform(curl.get())!=CURLE_OK)throw std::runtime_error("Download interrupted");
            long status=0;curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status);
            if(status!=200 || fflush(file.get())!=0 || fsync(fd)!=0)throw std::runtime_error("Incomplete download");
            file.reset();
            if(!nativeupdate::verify(partial,package.sha256,package.size))throw std::runtime_error("Update verification failed");
            std::filesystem::rename(partial,destination);partial.clear();
        }
        std::lock_guard lock(shared->mutex);shared->installer=destination;shared->received=package.size;shared->status=Status::Ready;
    }catch(const std::exception& error) {
        if(!partial.empty()){std::error_code ignored;std::filesystem::remove(partial,ignored);}
        std::lock_guard lock(shared->mutex);shared->status=Status::Available;
        shared->detail=std::string("Update not staged: ")+error.what();
    }
#endif
}
int UpdateChecker::downloadTest() {
    try {
        const auto release=updates::selectRelease(fetchReleases(),"0.0.0-preview.0",updates::buildPlatform());
        if(!release || !release->package)throw std::runtime_error("No verifiable package for this platform");
        auto test=std::make_shared<State>();test->package=release->package;
        download(test,*release->package);
        if(test->status!=Status::Ready)throw std::runtime_error(test->detail);
        std::cout<<"Downloaded and verified "<<release->tag<<" without installing\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
void UpdateChecker::installOnExit() {
#ifdef _WIN32
    std::filesystem::path installer,target;std::optional<updates::Package> package;std::string tag;
    {std::lock_guard lock(state->mutex);
        state->closing=true;
        if(!state->automatic || state->status!=Status::Ready || !state->package)return;
        installer=state->installer;target=state->target;package=state->package;tag=state->tag;
    }
    try {
        if(!autoupdate::registeredTarget(target))throw std::runtime_error("Installation changed");
        const auto cache=autoupdate::cacheFolder();
        const auto helper=cache/("sattrak-updater-"+std::to_string(GetCurrentProcessId())+".exe");
        std::filesystem::copy_file(target/L"sattrak_updater.exe",helper,std::filesystem::copy_options::overwrite_existing);
        const auto arguments=L" "+std::to_wstring(GetCurrentProcessId())+L" "+autoupdate::quote(installer)+L" "+autoupdate::quote(target)+
            L" "+std::wstring(package->sha256.begin(),package->sha256.end())+L" "+std::to_wstring(package->size)+L" "+autoupdate::quote(cache/L"update-install.log")+L" "+std::wstring(tag.begin(),tag.end());
        autoupdate::Handle process(autoupdate::launch(helper,arguments,cache));
        if(!process.value)throw std::runtime_error("Updater could not start");
    }catch(...) {
        try{std::ofstream(autoupdate::cacheFolder()/L"result.txt")<<"Update could not start — retry from App updates\n";}catch(...){}
    }
#else
    std::filesystem::path archive,target;std::optional<updates::Package> package;std::string tag;
    {std::lock_guard lock(state->mutex);
        state->closing=true;
        if(!state->automatic || state->status!=Status::Ready || !state->package || state->target.empty())return;
        archive=state->installer;target=state->target;package=state->package;tag=state->tag;
    }
    try {
        nativeupdate::launchOnExit(target,archive,package->sha256,package->size,tag);
    }catch(const std::exception& error) {
        try{std::ofstream(nativeupdate::cacheFolder()/"result.txt")<<"Update could not start: "<<error.what()<<'\n';}catch(...){}
    }
#endif
}
void UpdateChecker::drawSettings() {
    ImGui::SeparatorText("App updates");
    ImGui::Text("Installed %s",installedVersion());
    Status status;std::string tag,url,detail;bool automatic,eligible;uint64_t received,total;
    {std::lock_guard lock(state->mutex);status=state->status;tag=state->tag;url=state->url;detail=state->detail;automatic=state->automatic;eligible=!state->target.empty();received=state->received;total=state->package?state->package->size:0;}
    const char* labels[]={"Not checked","Checking GitHub releases","No newer release found","Update available","Update check unavailable","Downloading update","Update ready for next close"};
    ImGui::TextUnformatted(labels[int(status)]);
    if(!eligible)automatic=false;
    ImGui::BeginDisabled(!eligible);
    if(ImGui::Checkbox("Install updates automatically when closing sattrak",&automatic)) {
        {std::lock_guard lock(state->mutex);state->automatic=automatic;}
#ifdef _WIN32
        try{std::ofstream(autoupdate::cacheFolder()/L"automatic.txt")<<int(automatic);}catch(...){}
#else
        try{std::filesystem::create_directories(nativeupdate::cacheFolder());std::ofstream(nativeupdate::cacheFolder()/"automatic.txt")<<int(automatic);}catch(...){}
#endif
        if(automatic)start();
    }
    ImGui::EndDisabled();
#ifdef _WIN32
    if(!eligible)ImGui::TextWrapped("Automatic installation requires the installed Windows app");
#else
    if(!eligible)ImGui::TextWrapped("Automatic installation requires a managed per-user macOS or Debian install. Source builds and system packages update manually.");
#endif
    if(status==Status::Ready && !automatic)ImGui::TextUnformatted("Automatic installation is off");
    if(status==Status::Downloading && total)ImGui::ProgressBar(float(double(received)/double(total)));
    if(!detail.empty())ImGui::TextWrapped("%s",detail.c_str());
    ImGui::BeginDisabled(status==Status::Checking || status==Status::Downloading || status==Status::Ready);
    if(ImGui::Button("Check now"))start();
    ImGui::EndDisabled();
    if(status==Status::Available || status==Status::Downloading || status==Status::Ready){ImGui::Text("Available %s",tag.c_str());ImGui::TextLinkOpenURL("View release",url.c_str());}
    ImGui::TextWrapped("Checks GitHub at launch without interrupting startup");
}
void UpdateChecker::drawNotice(bool allowed) {
    if(!allowed)return;
    std::string tag,url;bool ready=false;
    {std::lock_guard lock(state->mutex);if((state->status!=Status::Available && state->status!=Status::Ready) || state->dismissed)return;tag=state->tag;url=state->url;ready=state->status==Status::Ready && state->automatic;}
    const auto size=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(size.x-16,72),ImGuiCond_Appearing,ImVec2(1,0));
    ImGui::SetNextWindowSize(ImVec2(std::min(380.f*ImGui::GetStyle().FontScaleMain,std::max(100.f,size.x-32)),0));
    bool open=true;
    if(ImGui::Begin("sattrak update available",&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::TextWrapped("New release %s",tag.c_str());
        if(ready)ImGui::TextWrapped("Downloaded and verified — installs after you close sattrak");
        ImGui::TextLinkOpenURL("View release and downloads",url.c_str());
        if(ImGui::Button("Later"))open=false;
    }
    ImGui::End();
    if(!open){std::lock_guard lock(state->mutex);state->dismissed=true;}
}
}

#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include <vector>
#include <tlhelp32.h>

namespace gui::autoupdate {
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;
    explicit Handle(HANDLE value=nullptr):value(value){}
};
inline std::filesystem::path executable() {
    std::wstring buffer(32768,L'\0');auto count=GetModuleFileNameW(nullptr,buffer.data(),DWORD(buffer.size()));
    if(!count || count>=buffer.size())return {};
    buffer.resize(count);return buffer;
}
inline std::filesystem::path registeredFolder() {
#ifndef SATTRAK_UPDATE_REGISTRY_KEY
#define SATTRAK_UPDATE_REGISTRY_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{B06EC57D-96F9-419D-B250-65D07DE148DE}_is1"
#endif
    constexpr auto key=SATTRAK_UPDATE_REGISTRY_KEY;
    std::wstring buffer(32768,L'\0');DWORD size=DWORD(buffer.size()*sizeof(wchar_t));
    if(RegGetValueW(HKEY_CURRENT_USER,key,L"InstallLocation",RRF_RT_REG_SZ|RRF_SUBKEY_WOW6464KEY,nullptr,buffer.data(),&size)!=ERROR_SUCCESS)return {};
    buffer.resize(wcslen(buffer.c_str()));return buffer;
}
inline bool registeredTarget(const std::filesystem::path& target) {
    const auto registered=registeredFolder();std::error_code error;
    return !registered.empty() && std::filesystem::equivalent(registered,target,error) && !error &&
        std::filesystem::is_regular_file(target/L"unins000.exe",error);
}
inline std::filesystem::path installedFolder() {
    const auto folder=executable().parent_path();
    std::error_code error;
    return registeredTarget(folder) && std::filesystem::is_regular_file(folder/L"sattrak_updater.exe",error) &&
        std::filesystem::is_regular_file(folder/L"release-version.txt",error)?folder:std::filesystem::path{};
}
inline bool anotherSession(const std::filesystem::path& target) {
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));
    if(snapshot.value==INVALID_HANDLE_VALUE)return true;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(!Process32FirstW(snapshot.value,&entry))return true;
    do {
        if(_wcsicmp(entry.szExeFile,L"ag.exe")!=0)continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID));
        if(!process.value)return true;
        std::wstring path(32768,L'\0');DWORD size=DWORD(path.size());
        if(!QueryFullProcessImageNameW(process.value,0,path.data(),&size))return true;
        path.resize(size);std::error_code error;
        if(std::filesystem::equivalent(path,target/L"ag.exe",error))return true;
    }while(Process32NextW(snapshot.value,&entry));
    return false;
}
inline std::filesystem::path cacheFolder() {
    wchar_t* root=nullptr;size_t size=0;
    if(_wdupenv_s(&root,&size,L"LOCALAPPDATA")!=0 || !root)throw std::runtime_error("User folder unavailable");
#ifndef SATTRAK_UPDATE_CACHE_APP
#define SATTRAK_UPDATE_CACHE_APP L"sattrak"
#endif
    std::filesystem::path path=std::filesystem::path(root)/SATTRAK_UPDATE_CACHE_APP/L"updates";std::free(root);
    std::filesystem::create_directories(path);return path;
}
inline std::wstring quote(const std::filesystem::path& path) {
    auto value=path.wstring();
    if(value.find(L'"')!=std::wstring::npos || value.empty())throw std::runtime_error("Invalid update path");
    size_t trailing=0;for(size_t i=value.size();i>0 && value[i-1]==L'\\';--i)++trailing;
    value.append(trailing,L'\\');
    return L"\""+value+L"\"";
}
inline std::string hash(HANDLE file) {
    LARGE_INTEGER zero{};if(!SetFilePointerEx(file,zero,nullptr,FILE_BEGIN))throw std::runtime_error("Cannot read update");
    struct Algorithm { BCRYPT_ALG_HANDLE value=nullptr;~Algorithm(){if(value)BCryptCloseAlgorithmProvider(value,0);} } algorithm;
    struct Hash { BCRYPT_HASH_HANDLE value=nullptr;~Hash(){if(value)BCryptDestroyHash(value);} } digest;
    if(BCryptOpenAlgorithmProvider(&algorithm.value,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0 ||
       BCryptCreateHash(algorithm.value,&digest.value,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("Hash initialization failed");
    unsigned char buffer[65536];DWORD read=0;
    for(;;) {
        if(!ReadFile(file,buffer,sizeof(buffer),&read,nullptr))throw std::runtime_error("Cannot read update");
        if(!read)break;
        if(BCryptHashData(digest.value,buffer,read,0)<0)throw std::runtime_error("Hash failed");
    }
    unsigned char bytes[32];if(BCryptFinishHash(digest.value,bytes,32,0)<0)throw std::runtime_error("Hash failed");
    constexpr char hex[]="0123456789abcdef";std::string result;
    for(auto byte:bytes){result+=hex[byte>>4];result+=hex[byte&15];}
    return result;
}
inline bool verify(HANDLE file,const std::string& expected,uint64_t expectedSize) {
    LARGE_INTEGER size{};
    return file && file!=INVALID_HANDLE_VALUE && expected.size()==64 && GetFileSizeEx(file,&size) &&
        uint64_t(size.QuadPart)==expectedSize && hash(file)==expected;
}
inline std::wstring installerArguments(const std::filesystem::path& target,const std::filesystem::path& log) {
    return L" /VERYSILENT /SUPPRESSMSGBOXES /SP- /NORESTART /NOCLOSEAPPLICATIONS /NORESTARTAPPLICATIONS /DIR="+quote(target)+L" /LOG="+quote(log);
}
inline HANDLE launch(const std::filesystem::path& program,const std::wstring& arguments,const std::filesystem::path& working) {
    auto command=quote(program)+arguments;STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(program.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,working.c_str(),&startup,&process))return nullptr;
    CloseHandle(process.hThread);return process.hProcess;
}
}
#endif

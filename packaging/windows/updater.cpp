#include "gui/auto_update_win.hpp"
#include "gui/update_release.hpp"
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    using namespace gui::autoupdate;
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!args)return 2;
    std::vector<std::wstring> values(args,args+count);LocalFree(args);
    try {
        if(count!=8)return 2;
        const auto parentId=std::stoul(values[1]);
        Handle parent(OpenProcess(SYNCHRONIZE,FALSE,parentId));
        if(!parent.value && GetLastError()!=ERROR_INVALID_PARAMETER)return 3;
        const std::filesystem::path installer=values[2],target=values[3];
        const std::string expected(values[4].begin(),values[4].end());
        const auto expectedSize=std::stoull(values[5]);
        const std::filesystem::path log=values[6];
        if(parent.value && WaitForSingleObject(parent.value,INFINITE)!=WAIT_OBJECT_0)return 3;
        const auto cache=cacheFolder();
        std::error_code error;
        if(!registeredTarget(target) || !std::filesystem::equivalent(installer.parent_path(),cache,error) || error)return 4;
        std::string installed;std::ifstream(target/L"release-version.txt")>>installed;
        const auto current=gui::updates::version(installed);
        const auto next=gui::updates::version(std::string(values[7].begin(),values[7].end()));
        if(!current || !next || !gui::updates::newer(*next,*current)) {
            std::ofstream(cache/L"result.txt")<<"Update skipped — installed version is current or could not be verified\n";
            return 7;
        }
        if(anotherSession(target)) {
            std::ofstream(cache/L"result.txt")<<"Update postponed — another sattrak session is open\n";
            return 8;
        }
        // Keep the verified file locked against writes and replacement until setup exits
        Handle file(CreateFileW(installer.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!verify(file.value,expected,expectedSize))throw std::runtime_error("Update verification failed");
        Handle process(launch(installer,installerArguments(target,log),cache));
        if(!process.value)throw std::runtime_error("Installer could not start");
        if(WaitForSingleObject(process.value,INFINITE)!=WAIT_OBJECT_0)throw std::runtime_error("Installer wait failed");
        DWORD result=1;if(!GetExitCodeProcess(process.value,&result))result=1;
        std::ofstream(cache/L"result.txt")<<(result==0?"Update installed":"Update failed — see update-install.log")<<'\n';
        if(result==0){CloseHandle(file.value);file.value=nullptr;DeleteFileW(installer.c_str());}
        return result==0?0:5;
    }catch(...) {
        try{std::ofstream(cacheFolder()/L"result.txt")<<"Update failed — see update-install.log\n";}catch(...){}
        return 6;
    }
}

#include "gui/auto_update_win.hpp"
#include <iostream>
int main() {
    using namespace gui::autoupdate;
    const auto path=std::filesystem::temp_directory_path()/("sattrak-hash-test-"+std::to_string(GetCurrentProcessId()));
    {std::ofstream file(path,std::ios::binary);file<<"abc";}
    int failures=0;auto check=[&](bool value,const char* label){if(!value){std::cerr<<label<<'\n';++failures;}};
    const std::string digest="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    {
        Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        check(verify(file.value,digest,3),"Verify known SHA-256");
        check(!verify(file.value,digest,4),"Reject truncated download");
        check(!verify(file.value,std::string(64,'0'),3),"Reject changed content");
        Handle writer(CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        check(writer.value==INVALID_HANDLE_VALUE,"Verified installer must stay locked against writes");
    }
    check(!registeredTarget(path),"Reject an unregistered target");
    const auto arguments=installerArguments(L"C:\\Users\\Test User\\sattrak",L"C:\\Users\\Test User\\update.log");
    check(arguments.find(L"/NORESTART ")!=std::wstring::npos && arguments.find(L"/NOCLOSEAPPLICATIONS ")!=std::wstring::npos,"Do not reboot or close another session");
    check(arguments.find(L"/DIR=\"C:\\Users\\Test User\\sattrak\"")!=std::wstring::npos,"Preserve installation paths with spaces");
    check(quote(L"C:\\")==L"\"C:\\\\\"","Quote trailing backslashes");
    std::error_code error;std::filesystem::remove(path,error);
    return failures?1:0;
}

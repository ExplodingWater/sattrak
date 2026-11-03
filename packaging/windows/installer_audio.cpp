#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <array>
#include <cstdlib>
#include <string>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace {
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value)CloseHandle(value);}
};

int play(const wchar_t* file,const std::wstring& prefix,DWORD parentId) {
    // Named events work across the 32-bit wizard and this 64-bit audio helper.
    Handle stop{OpenEventW(SYNCHRONIZE,FALSE,(prefix+L".stop").c_str())};
    Handle mute{OpenEventW(SYNCHRONIZE,FALSE,(prefix+L".mute").c_str())};
    Handle ready{OpenEventW(EVENT_MODIFY_STATE,FALSE,(prefix+L".ready").c_str())};
    Handle muted{OpenEventW(EVENT_MODIFY_STATE,FALSE,(prefix+L".muted").c_str())};
    Handle parent{OpenProcess(SYNCHRONIZE,FALSE,parentId)};
    if(!stop.value||!mute.value||!ready.value||!muted.value||!parent.value)return 1;

    ma_engine engine{};
    if(ma_engine_init(nullptr,&engine)!=MA_SUCCESS)return 2;
    ma_sound sound{};
    if(ma_sound_init_from_file_w(&engine,file,MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_NO_SPATIALIZATION,
        nullptr,nullptr,&sound)!=MA_SUCCESS){ma_engine_uninit(&engine);return 3;}
    ma_sound_set_looping(&sound,MA_TRUE);
    ma_sound_set_volume(&sound,.25f);
    if(ma_sound_start(&sound)!=MA_SUCCESS){ma_sound_uninit(&sound);ma_engine_uninit(&engine);return 4;}
    SetEvent(ready.value);
    const std::array<HANDLE,2> shutdown{stop.value,parent.value};
    int result=0;
    while(true) {
        const DWORD wait=WaitForMultipleObjects(DWORD(shutdown.size()),shutdown.data(),FALSE,50);
        if(wait==WAIT_OBJECT_0||wait==WAIT_OBJECT_0+1)break;
        if(wait==WAIT_FAILED){result=5;break;}
        const bool silent=WaitForSingleObject(mute.value,0)==WAIT_OBJECT_0;
        // Mute only this sound, keeping its decoding/playhead and the system volume intact.
        ma_sound_set_volume(&sound,silent?0.f:.25f);
        if(ma_sound_get_volume(&sound)==0)SetEvent(muted.value);
        else ResetEvent(muted.value);
    }
    ma_sound_stop(&sound);
    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
    return result;
}
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int argc=0;
    auto** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv)return 1;
    int result=1;
    try {
        if(argc==4) {
            wchar_t* end=nullptr;
            const auto parent=std::wcstoul(argv[3],&end,10);
            if(parent && end && *end==L'\0')result=play(argv[1],argv[2],DWORD(parent));
        }
    } catch(...) {result=1;}
    LocalFree(argv);
    return result;
}

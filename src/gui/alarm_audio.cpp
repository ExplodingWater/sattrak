#include "gui/alarm_audio.hpp"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>
#include <atomic>
namespace gui {
struct AlarmAudio::Impl {
    ma_device device{};
    bool ready=false,attempted=false,playing=false,error=false;
    ma_uint64 frame=0;
    std::atomic<float> gain{1};
    static void render(ma_device* device,void* output,const void*,ma_uint32 count) {
        auto& self=*static_cast<Impl*>(device->pUserData);auto* pcm=static_cast<float*>(output);
        constexpr double twopi = 6.283185307179586;
        constexpr double square_hz = 248.0;
        for(ma_uint32 i=0;i<count;++i){const double t=double(self.frame++%device->sampleRate)/device->sampleRate,phase=std::fmod(t,.25);
            const double envelope=t<.5?std::min({1.0,phase/.01,(.25-phase)/.02}):0;
            const double waveform = std::sin(twopi * square_hz * t) >= 0.0 ? 1.0 : -1.0;
            pcm[i]=self.gain.load(std::memory_order_relaxed)*float(.14*envelope*waveform);
        }
    }
    ~Impl(){if(ready)ma_device_uninit(&device);}
};
AlarmAudio::AlarmAudio():impl(std::make_unique<Impl>()){}
AlarmAudio::~AlarmAudio()=default;
bool AlarmAudio::failed() const{return impl->error;}
void AlarmAudio::setGain(float gain){impl->gain.store(std::isfinite(gain)?std::clamp(gain,0.0f,2.0f):1.0f,std::memory_order_relaxed);}
bool AlarmAudio::setActive(bool active) {
    auto& p=*impl;
    if(!active){if(p.playing){ma_device_stop(&p.device);p.playing=false;}return true;}
    if(p.playing)return true;
    if(!p.attempted){
        p.attempted=true;auto config=ma_device_config_init(ma_device_type_playback);
        config.playback.format=ma_format_f32;config.playback.channels=1;config.sampleRate=48000;config.dataCallback=Impl::render;config.pUserData=&p;
        p.ready=ma_device_init(nullptr,&config,&p.device)==MA_SUCCESS;p.error=!p.ready;
    }
    if(!p.ready||p.error)return false;
    p.frame=0;p.playing=ma_device_start(&p.device)==MA_SUCCESS;p.error=!p.playing;return p.playing;
}
}

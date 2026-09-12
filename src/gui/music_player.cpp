#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "gui/music_player.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <cmath>

namespace gui {
namespace {
bool supported(const std::filesystem::path& path) {
    std::string ext=path.extension().string();
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
    return ext==".mp3" || ext==".wav" || ext==".ogg" || ext==".flac";
}
std::string normalized(std::string text) {
    std::string result;
    for(unsigned char c:text) if(std::isalnum(c)) result.push_back(char(std::tolower(c)));
    return result;
}
}

struct MusicPlayer::Impl {
    ma_engine engine{};
    ma_sound sound{};
    bool engineReady=false,soundReady=false,paused=false,started=false,fading=false;
    int volume=100;
    size_t index=0;
    double startAt=0,fadeStart=0,wallTime=0;
    std::vector<std::filesystem::path> tracks;
    std::filesystem::path volumePreference;
    std::string display="Music: no tracks installed",error;

    ~Impl() {
        if(soundReady)ma_sound_uninit(&sound);
        if(engineReady)ma_engine_uninit(&engine);
        if(!volumePreference.empty()) {
            std::ofstream saved(volumePreference);
            if(saved)saved<<volume<<'\n';
        }
    }
    void applyVolume(double wallTime) {
        float gain=float(volume)/100.0f;
        if(fading) {
            float fade=float(std::clamp((wallTime-fadeStart)/5.0,0.0,1.0));
            gain*=fade;
            if(fade>=1)fading=false;
        }
        if(soundReady)ma_sound_set_volume(&sound,gain);
    }
    bool begin(size_t requested,bool fade,double wallTime) {
        if(tracks.empty()||!engineReady)return false;
        if(soundReady){ma_sound_uninit(&sound);soundReady=false;}
        index=requested%tracks.size();
        started=true;
        if(ma_sound_init_from_file(&engine,tracks[index].string().c_str(),MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_NO_SPATIALIZATION,nullptr,nullptr,&sound)!=MA_SUCCESS){
            error="Could not decode "+tracks[index].filename().string();display="Music unavailable";return false;
        }
        soundReady=true;paused=false;
        const std::string key=normalized(tracks[index].stem().string());
        fading=fade || key.find("endofthewinter")!=std::string::npos;
        fadeStart=wallTime;
        display=tracks[index].stem().string();
        applyVolume(wallTime);
        if(ma_sound_start(&sound)!=MA_SUCCESS){error="Could not start audio output";return false;}
        return true;
    }
};

MusicPlayer::MusicPlayer():m_impl(std::make_unique<Impl>()){}
MusicPlayer::~MusicPlayer()=default;

bool MusicPlayer::init(const std::filesystem::path& folder) {
    m_impl->volumePreference="music_volume.txt";
    int savedVolume=100;
    std::ifstream saved(m_impl->volumePreference);
    if(saved>>savedVolume)m_impl->volume=std::clamp(savedVolume,0,200);
    std::error_code ec;
    if(std::filesystem::exists(folder,ec)) {
        for(const auto& item:std::filesystem::directory_iterator(folder,ec))
            if(item.is_regular_file()&&supported(item.path()))m_impl->tracks.push_back(item.path());
        std::sort(m_impl->tracks.begin(),m_impl->tracks.end());
    }
    std::mt19937 random(std::random_device{}());
    m_impl->startAt=std::uniform_real_distribution<double>(45.0,60.0)(random);
    if(m_impl->tracks.empty())return true;
    if(ma_engine_init(nullptr,&m_impl->engine)!=MA_SUCCESS){m_impl->error="Audio device initialization failed";m_impl->display="Music unavailable";return false;}
    m_impl->engineReady=true;
    m_impl->display="Music begins shortly";
    return true;
}
void MusicPlayer::update(double wallTime) {
    m_impl->wallTime=wallTime;
    if(!m_impl->started && !m_impl->tracks.empty() && wallTime>=m_impl->startAt)m_impl->begin(0,true,wallTime);
    if(m_impl->soundReady) {
        m_impl->applyVolume(wallTime);
        if(ma_sound_at_end(&m_impl->sound))m_impl->begin(m_impl->index+1,false,wallTime);
    }
}
void MusicPlayer::previous() {
    if(m_impl->tracks.empty())return;
    float cursor=0;
    if(m_impl->soundReady)ma_sound_get_cursor_in_seconds(&m_impl->sound,&cursor);
    if(m_impl->soundReady&&cursor>3){ma_sound_seek_to_pcm_frame(&m_impl->sound,0);return;}
    size_t target=m_impl->index==0?m_impl->tracks.size()-1:m_impl->index-1;
    m_impl->begin(target,false,m_impl->wallTime);
}
void MusicPlayer::togglePause() {
    if(m_impl->tracks.empty())return;
    if(!m_impl->started){m_impl->begin(0,true,m_impl->wallTime);return;}
    if(!m_impl->soundReady)return;
    if(m_impl->paused)ma_sound_start(&m_impl->sound);else ma_sound_stop(&m_impl->sound);
    m_impl->paused=!m_impl->paused;
}
void MusicPlayer::next(){if(!m_impl->tracks.empty())m_impl->begin(m_impl->index+1,false,m_impl->wallTime);}
double MusicPlayer::durationSeconds() const {
    float seconds=0;
    if(m_impl->soundReady)ma_sound_get_length_in_seconds(&m_impl->sound,&seconds);
    return seconds;
}
double MusicPlayer::elapsedSeconds() const {
    float seconds=0;
    if(m_impl->soundReady)ma_sound_get_cursor_in_seconds(&m_impl->sound,&seconds);
    return seconds;
}
void MusicPlayer::seekSeconds(double seconds) {
    if(!m_impl->soundReady || !std::isfinite(seconds))return;
    ma_uint32 rate=0;
    ma_sound_get_data_format(&m_impl->sound,nullptr,nullptr,&rate,nullptr,0);
    const auto frame=ma_uint64(std::clamp(seconds,0.0,(std::max)(0.0,durationSeconds()-0.001))*rate);
    ma_sound_seek_to_pcm_frame(&m_impl->sound,frame);
}
void MusicPlayer::setVolumePercent(int percent){m_impl->volume=std::clamp(percent,0,200);m_impl->applyVolume(m_impl->wallTime);}
bool MusicPlayer::hasTracks() const{return !m_impl->tracks.empty();}
bool MusicPlayer::isPlaying() const{return m_impl->soundReady&&!m_impl->paused;}
int MusicPlayer::volumePercent() const{return m_impl->volume;}
double MusicPlayer::startDelaySeconds() const{return m_impl->startAt;}
const std::string& MusicPlayer::displayText() const{return m_impl->display;}
const std::string& MusicPlayer::errorText() const{return m_impl->error;}
} // namespace gui

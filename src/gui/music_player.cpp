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
    ma_sound radio{};
    bool radioReady=false,radioEnabled=true,radioMuted=false;
    double radioNext=0,radioStarted=0;
    float radioVolume=.1f,radioEndAt=-1;
    size_t radioIndex=0;
    std::vector<std::filesystem::path> radioTracks;
    std::filesystem::path radioFolder;
    std::string radioStatus="No radio recordings installed";
    bool engineReady=false,soundReady=false,paused=false,started=false,fading=false;
    bool betweenTracks=false;
    double nextTrackAt=0;
    std::mt19937 random{std::random_device{}()};
    int volume=100;
    int masterVolume=100,defaultRadioVolume=10;
    size_t index=0;
    double startAt=0,fadeStart=0,wallTime=0;
    std::vector<std::filesystem::path> tracks;
    std::filesystem::path volumePreference;
    std::string display="Music: no tracks installed",error;

    ~Impl() {
        if(radioReady)ma_sound_uninit(&radio);
        if(soundReady)ma_sound_uninit(&sound);
        if(engineReady)ma_engine_uninit(&engine);
        if(!volumePreference.empty()) {
            std::ofstream saved(volumePreference);
            if(saved)saved<<volume<<'\n'<<masterVolume<<'\n'<<defaultRadioVolume<<'\n';
        }
    }
    void scheduleRadio() { radioNext=wallTime+std::uniform_real_distribution<double>(900,1800)(random); }
    void stopRadio() {
        if(radioReady)ma_sound_uninit(&radio);
        radioReady=false;scheduleRadio();radioStatus=radioEnabled?"Waiting for the next recording":"Radio disabled";
    }
    void startRadio() {
        if(!radioEnabled||radioTracks.empty()||!engineReady)return;
        if(radioReady)ma_sound_uninit(&radio);
        radioReady=false;
        auto path=radioTracks[radioIndex++%radioTracks.size()];
        radioVolume=defaultRadioVolume/100.0f;radioEndAt=-1;
        if(ma_sound_init_from_file(&engine,path.string().c_str(),MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_NO_SPATIALIZATION,nullptr,nullptr,&radio)!=MA_SUCCESS){radioStatus="Could not decode "+path.filename().string();scheduleRadio();return;}
        radioReady=true;radioStarted=wallTime;
        ma_sound_set_volume(&radio,0);ma_sound_set_looping(&radio,MA_FALSE);
        if(ma_sound_start(&radio)!=MA_SUCCESS){stopRadio();radioStatus="Radio output unavailable";return;}
        radioStatus=path.stem().string();
    }
    void updateRadio() {
        if(radioReady) {
            ma_sound_set_volume(&radio,radioMuted?0:radioVolume*(masterVolume/100.0f)*float(std::clamp((wallTime-radioStarted)/5,0.0,1.0)));
            float elapsed=0;ma_sound_get_cursor_in_seconds(&radio,&elapsed);
            if(ma_sound_at_end(&radio)||(radioEndAt>0&&elapsed>=radioEndAt))stopRadio();
        } else if(radioEnabled&&!radioTracks.empty()&&wallTime>=radioNext)startRadio();
    }
    void applyVolume(double wallTime) {
        float gain=(float(volume)/100.0f)*(masterVolume/100.0f);
        if(fading) {
            float fade=float(std::clamp((wallTime-fadeStart)/5.0,0.0,1.0));
            gain*=fade;
            if(fade>=1)fading=false;
        }
        if(soundReady)ma_sound_set_volume(&sound,gain);
    }
    bool begin(size_t requested,bool fade,double wallTime) {
        if(tracks.empty()||!engineReady)return false;
        betweenTracks=false;error.clear();
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
    if(saved>>savedVolume)m_impl->masterVolume=std::clamp(savedVolume,0,200);
    if(saved>>savedVolume)m_impl->defaultRadioVolume=std::clamp(savedVolume,0,200);
    std::error_code ec;
    if(std::filesystem::exists(folder,ec)) {
        for(const auto& item:std::filesystem::directory_iterator(folder,ec))
            if(item.is_regular_file()&&supported(item.path()))m_impl->tracks.push_back(item.path());
        std::shuffle(m_impl->tracks.begin(),m_impl->tracks.end(),m_impl->random);
    }
    std::mt19937 random(std::random_device{}());
    m_impl->startAt=std::uniform_real_distribution<double>(45.0,60.0)(random);
    if(m_impl->tracks.empty())return true;
    if(ma_engine_init(nullptr,&m_impl->engine)!=MA_SUCCESS){m_impl->error="Audio device initialization failed";m_impl->display="Music unavailable";return false;}
    m_impl->engineReady=true;
    m_impl->display.clear();
    return true;
}
void MusicPlayer::update(double wallTime) {
    m_impl->wallTime=wallTime;
    m_impl->updateRadio();
    if(!m_impl->started && !m_impl->tracks.empty() && wallTime>=m_impl->startAt)m_impl->begin(0,true,wallTime);
    if(m_impl->soundReady) {
        if(m_impl->betweenTracks) {
            if(wallTime>=m_impl->nextTrackAt)m_impl->begin(m_impl->index+1,true,wallTime);
            else {
                m_impl->display.clear();
            }
            return;
        }
        m_impl->applyVolume(wallTime);
        if(ma_sound_at_end(&m_impl->sound)) {
            m_impl->betweenTracks=true;
            m_impl->nextTrackAt=wallTime+std::uniform_real_distribution<double>(90,180)(m_impl->random);
            m_impl->display.clear();
        }
    }
}
bool MusicPlayer::initRadio(const std::filesystem::path& folder) {
    std::error_code ec;
    m_impl->radioFolder=std::filesystem::weakly_canonical(folder,ec);
    if(ec)return false;
    if(std::filesystem::exists(folder,ec))for(const auto& item:std::filesystem::directory_iterator(folder,ec))
        if(item.is_regular_file()&&supported(item.path()))m_impl->radioTracks.push_back(item.path());
    if(m_impl->radioTracks.empty())return true;
    std::sort(m_impl->radioTracks.begin(),m_impl->radioTracks.end());
    if(!m_impl->engineReady){if(ma_engine_init(nullptr,&m_impl->engine)!=MA_SUCCESS){m_impl->radioStatus="Radio audio device unavailable";return false;}m_impl->engineReady=true;}
    m_impl->scheduleRadio();m_impl->radioStatus="Waiting for the first recording";return true;
}
void MusicPlayer::setRadioEnabled(bool enabled){if(enabled==m_impl->radioEnabled)return;m_impl->radioEnabled=enabled;m_impl->stopRadio();}
bool MusicPlayer::radioEnabled() const{return m_impl->radioEnabled;}
void MusicPlayer::setRadioMuted(bool muted){m_impl->radioMuted=muted;if(m_impl->radioReady)ma_sound_set_volume(&m_impl->radio,muted?0:m_impl->radioVolume*(m_impl->masterVolume/100.0f)*float(std::clamp((m_impl->wallTime-m_impl->radioStarted)/5,0.0,1.0)));}
void MusicPlayer::setMasterVolumePercent(int percent){m_impl->masterVolume=std::clamp(percent,0,200);m_impl->applyVolume(m_impl->wallTime);setRadioMuted(m_impl->radioMuted);}
int MusicPlayer::masterVolumePercent() const{return m_impl->masterVolume;}
void MusicPlayer::setRadioVolumePercent(int percent){m_impl->defaultRadioVolume=std::clamp(percent,0,200);m_impl->radioVolume=m_impl->defaultRadioVolume/100.0f;setRadioMuted(m_impl->radioMuted);}
int MusicPlayer::radioVolumePercent() const{return m_impl->defaultRadioVolume;}
bool MusicPlayer::radioMuted() const{return m_impl->radioMuted;}
bool MusicPlayer::radioPlaying() const{return m_impl->radioReady;}
float MusicPlayer::radioGain() const{return m_impl->radioReady?ma_sound_get_volume(&m_impl->radio):0;}
double MusicPlayer::secondsUntilRadio() const{return (std::max)(0.0,m_impl->radioNext-m_impl->wallTime);}
const std::string& MusicPlayer::radioStatus() const{return m_impl->radioStatus;}
void MusicPlayer::playRadioNow(){m_impl->startRadio();}
bool MusicPlayer::playRadioFile(const std::filesystem::path& requested,float gain,double endAt,std::string& message) {
    std::error_code ec;
    if(!std::isfinite(gain)||gain<0||gain>2){message="Volume must be between 0 and 2.";return false;}
    auto& p=*m_impl;
    const auto name=requested.string();
    if(name.empty()||name.find_first_of("/\\:")!=std::string::npos||requested.has_parent_path()||requested.is_absolute()||name=="."||name==".."){message="Use a track number or filename from assets/radio; other paths are not allowed.";return false;}
    if(p.radioFolder.empty()){message="Radio folder unavailable.";return false;}
    const bool numbered=std::all_of(name.begin(),name.end(),[](unsigned char c){return c>='0'&&c<='9';});
    std::filesystem::path path;
    if(numbered){
        const auto prefix=(name.size()==1?"0"+name:name)+" - ";
        for(std::filesystem::directory_iterator it(p.radioFolder,ec),end;!ec&&it!=end;it.increment(ec)){
            if(it->path().filename().string().starts_with(prefix)&&supported(it->path())){
                if(!path.empty()){message="Track number is ambiguous; use its full filename.";return false;}path=it->path();
            }
        }
        if(path.empty()){message="No matching radio track number in assets/radio.";return false;}
    }else path=p.radioFolder/requested;
    path=std::filesystem::canonical(path,ec);
    if(ec||path.parent_path()!=p.radioFolder||!supported(path)||!std::filesystem::is_regular_file(path,ec)){message="Audio file missing, unsupported, or outside assets/radio.";return false;}
    if(!p.engineReady){if(ma_engine_init(nullptr,&p.engine)!=MA_SUCCESS){message="Audio output unavailable.";return false;}p.engineReady=true;}
    p.stopRadio();
#ifdef _WIN32
    auto result=ma_sound_init_from_file_w(&p.engine,path.c_str(),MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_NO_SPATIALIZATION,nullptr,nullptr,&p.radio);
#else
    auto result=ma_sound_init_from_file(&p.engine,path.c_str(),MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_NO_SPATIALIZATION,nullptr,nullptr,&p.radio);
#endif
    if(result!=MA_SUCCESS){message="Could not decode audio file.";return false;}
    p.radioReady=true;p.radioVolume=gain;p.radioEndAt=-1;p.radioStarted=p.wallTime;
    float duration=0;ma_sound_get_length_in_seconds(&p.radio,&duration);
    if(std::isfinite(endAt)&&endAt>0&&endAt<=duration)p.radioEndAt=float(endAt);
    ma_sound_set_volume(&p.radio,0);ma_sound_set_looping(&p.radio,MA_FALSE);
    if(ma_sound_start(&p.radio)!=MA_SUCCESS){p.stopRadio();message="Could not start playback.";return false;}
    p.radioStatus=path.stem().string();message="Playing "+path.filename().string()+" at "+std::to_string(int(gain*100))+"%";
    if(p.radioMuted)message+=" (radio is muted)";
    if(p.radioEndAt>0)message+="; stop at "+std::to_string(p.radioEndAt)+" seconds";
    else message+="; natural end (no valid --endat limit)";
    return true;
}
void MusicPlayer::stopRadio(){m_impl->stopRadio();}
void MusicPlayer::previous() {
    if(m_impl->tracks.empty())return;
    if(m_impl->betweenTracks){m_impl->begin(m_impl->index,false,m_impl->wallTime);return;}
    float cursor=0;
    if(m_impl->soundReady)ma_sound_get_cursor_in_seconds(&m_impl->sound,&cursor);
    if(m_impl->soundReady&&cursor>3){ma_sound_seek_to_pcm_frame(&m_impl->sound,0);return;}
    size_t target=m_impl->index==0?m_impl->tracks.size()-1:m_impl->index-1;
    m_impl->begin(target,false,m_impl->wallTime);
}
void MusicPlayer::togglePause() {
    if(m_impl->tracks.empty())return;
    if(m_impl->betweenTracks){m_impl->begin(m_impl->index+1,true,m_impl->wallTime);return;}
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
    if(m_impl->betweenTracks){m_impl->betweenTracks=false;m_impl->display=m_impl->tracks[m_impl->index].stem().string();ma_sound_start(&m_impl->sound);}
}
void MusicPlayer::setVolumePercent(int percent){m_impl->volume=std::clamp(percent,0,200);m_impl->applyVolume(m_impl->wallTime);}
bool MusicPlayer::hasTracks() const{return !m_impl->tracks.empty();}
bool MusicPlayer::isPlaying() const{return m_impl->soundReady&&!m_impl->paused&&!m_impl->betweenTracks;}
int MusicPlayer::volumePercent() const{return m_impl->volume;}
double MusicPlayer::startDelaySeconds() const{return m_impl->startAt;}
const std::string& MusicPlayer::displayText() const{return m_impl->display;}
const std::string& MusicPlayer::errorText() const{return m_impl->error;}
} // namespace gui

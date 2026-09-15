#include "gui/orbit_recording.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <stdexcept>
namespace gui {
bool OrbitRecording::start(const std::filesystem::path& folder) {
    if(active())return false;
    discard();
    try {
        std::filesystem::create_directories(folder);
        const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
        journal=folder/("orbit-"+std::to_string(stamp)+".partial");
        if(std::filesystem::exists(journal))throw std::runtime_error("Recording filename already exists");
        ownsJournal=true;saved=false;
        output.clear();output.open(journal,std::ios::binary);output.imbue(std::locale::classic());
        output<<"# frame=ECEF\n# Synthetic sattrak trajectory; UTC anchored at recording start\nutc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s\n"<<std::setprecision(15);output.flush();
        if(!output)throw std::runtime_error("Cannot create recovery journal");
        recoveredSource.clear();samples=0;lastUtc=0;status="Recording; each sample is flushed to a recovery journal.";return true;
    }catch(const std::exception& e){stop();status=e.what();return false;}
}
bool OrbitRecording::append(const OrbitSample& s) {
    if(!active())return false;
    if(!std::isfinite(s.utc)||!std::isfinite(s.position.norm())||!std::isfinite(s.velocity.norm())||s.position.norm()<6370||s.position.norm()>1e7||s.velocity.norm()>100||(samples&&s.utc<=lastUtc)||samples>=490000){stop();status="Recording stopped: invalid state, non-increasing time, or sample limit.";return false;}
    output<<formatOrbitUtc(s.utc)<<','<<s.position.x<<','<<s.position.y<<','<<s.position.z<<','<<s.velocity.x<<','<<s.velocity.y<<','<<s.velocity.z<<'\n';output.flush();
    if(!output){stop();status="Recording stopped: disk write failed. Earlier samples remain recoverable.";return false;}
    ++samples;lastUtc=s.utc;return true;
}
void OrbitRecording::stop(){
    if(!output.is_open())return;
    output.flush();output.close();
    status="Recording stopped. Choose Save or Save As... to keep it.";
}
void OrbitRecording::discard(){
    stop();
    // Only remove this instance's temporary journal, never a user export or recovery source.
    if(ownsJournal&&!saved&&!journal.empty()) {
        std::error_code ec;std::filesystem::remove(journal,ec);
        if(ec)status="Could not discard temporary recording: "+ec.message();
    }
    ownsJournal=false;saved=false;samples=0;journal.clear();recoveredSource.clear();
}
bool OrbitRecording::save(const std::filesystem::path& destination) {
    stop();
    try {
        OrbitReplay check;std::string error;if(!check.load(journal,error))throw std::runtime_error(error);
        if(destination.extension()!=".csv")throw std::runtime_error("Use a .csv filename");
        std::filesystem::copy_file(journal,destination,std::filesystem::copy_options::none);
        saved=true;
        auto marker=journal;marker += ".saved";std::ofstream(marker)<<destination.string();
        if(!recoveredSource.empty()){auto originalMarker=recoveredSource;originalMarker += ".saved";std::ofstream(originalMarker)<<destination.string();}
        status="Saved: "+destination.string();return true;
    }catch(const std::exception& e){status=std::string("Save failed: ")+e.what();return false;}
}
bool OrbitRecording::recover(const std::filesystem::path& source) {
    if(active()){status="Stop the current recording before recovery.";return false;}
    try {
        if(std::filesystem::file_size(source)>64*1024*1024)throw std::runtime_error("Recovery journal exceeds 64 MiB");
        std::ifstream in(source,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});
        auto end=bytes.find_last_of('\n');if(end==std::string::npos)throw std::runtime_error("No complete samples");
        // A crash can leave an incomplete last line; never silently repair interior corruption.
        bytes.resize(end+1);
        auto candidate=source;candidate += ".recovered-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".csv";
        {std::ofstream out(candidate,std::ios::binary);out<<bytes;if(!out)throw std::runtime_error("Cannot write recovery copy");}
        OrbitReplay check;std::string error;if(!check.load(candidate,error))throw std::runtime_error(error);
        discard();ownsJournal=true;saved=false;
        journal=candidate;recoveredSource=source;samples=check.samples.size();lastUtc=check.samples.back().utc;
        status="Recovered complete samples; the original journal was preserved.";return true;
    }catch(const std::exception& e){status=std::string("Recovery failed: ")+e.what();return false;}
}
double stationElevation(const OrbitReplay& orbit,double utc,double latitude,double longitude) {
    OrbitSample raw;if(!orbit.sample(utc,raw))return -90;
    const double pi=3.141592653589793,lat=latitude*pi/180,lon=longitude*pi/180+earthSiderealAngle(utc);
    aero::math::Vec3 up{std::cos(lat)*std::cos(lon),std::cos(lat)*std::sin(lon),std::sin(lat)};
    auto line=orbit.inertial(raw).position-up*6378.137;
    return std::asin(std::clamp(line.normalized().dot(up),-1.0,1.0))*180/pi;
}
std::vector<StationPass> findStationPasses(const OrbitReplay& orbit,double lat,double lon,double mask) {
    std::vector<StationPass> result;if(orbit.samples.size()<2)return result;
    const double first=orbit.samples.front().utc,last=orbit.samples.back().utc;
    if(last-first>7*86400)throw std::runtime_error("Pass search supports up to seven days per file. Split longer ephemerides.");
    auto elevation=[&](double t){return stationElevation(orbit,t,lat,lon);};
    auto crossing=[&](double a,double b,bool rising){for(int i=0;i<24;++i){double m=(a+b)/2;if((elevation(m)>=mask)==rising)b=m;else a=m;}return (a+b)/2;};
    double previous=first,e=elevation(first);bool visible=e>=mask;StationPass pass;
    if(visible)pass={first,first,first,e,true,false};
    auto finish=[&](double end,bool clipped){pass.end=end;pass.clippedEnd=clipped;
        double a=std::max(pass.start,pass.peak-10),b=std::min(pass.end,pass.peak+10);
        for(int i=0;i<40;++i){double l=a+(b-a)/3,r=b-(b-a)/3;if(elevation(l)<elevation(r))a=l;else b=r;}
        double peak=(a+b)/2,value=elevation(peak);if(value>pass.maxElevation){pass.peak=peak;pass.maxElevation=value;}result.push_back(pass);};
    while(previous<last){double t=std::min(last,previous+10),next=elevation(t);bool above=next>=mask;
        if(!visible&&above)pass={crossing(previous,t,true),t,t,next,false,false};
        if(above&&next>pass.maxElevation){pass.peak=t;pass.maxElevation=next;}
        if(visible&&!above)finish(crossing(previous,t,false),false);
        visible=above;previous=t;e=next;
    }
    if(visible)finish(last,true);return result;
}
}

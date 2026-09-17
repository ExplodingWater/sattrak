#include "gui/orbit_recording.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace gui;
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(){try{
    auto folder=std::filesystem::path("recording-tests")/std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    OrbitRecording recording;require(recording.start(folder),"Start journal");
    double utc=parseOrbitUtc("2026-09-13T00:00:00.123456Z");
    require(std::abs(parseOrbitUtc(formatOrbitUtc(utc))-utc)<1e-6,"Subsecond timestamps survive round trip");
    require(recording.append({utc,{7000,0,0},{0,7,0}}),"First sample");
    require(recording.append({utc+1,{7000,7,0},{0,7,0}}),"Second sample");
    auto journal=recording.path();recording.stop();auto saved=folder/"saved.csv";
    auto automatic=journal;automatic.replace_extension(".csv");
    require(!std::filesystem::exists(automatic)&&recording.unsaved(),"Stop retains unsaved work without creating a CSV");
    require(recording.save(saved),"Save CSV");require(!recording.save(saved),"Do not overwrite existing output");
    OrbitReplay replay;std::string error;require(replay.load(saved,error),"Reopen saved CSV");require(replay.samples.size()==2,"Two saved samples");
    {std::ofstream damaged(journal,std::ios::app);damaged<<"2026-09-13T00:00:02,700";}
    OrbitRecording recovered;require(recovered.recover(journal),"Recover complete lines after interrupted append");
    require(replay.load(recovered.path(),error)&&replay.samples.size()==2,"Recovery retains complete samples");
    auto draft=recovered.path();recovered.discard();
    require(!std::filesystem::exists(draft)&&std::filesystem::exists(journal),"Discard removes temporary copy, preserving recovery source");
    require(recording.start(folder),"Start another recording");
    require(std::filesystem::exists(saved),"Starting again preserves saved exports");
    auto abandoned=recording.path();recording.stop();require(recording.start(folder),"Replace unsaved recording");
    require(!std::filesystem::exists(abandoned),"Starting again discards unsaved journal");
    std::filesystem::path closing;
    {OrbitRecording session;require(session.start(folder),"Start closing session");closing=session.path();}
    require(!std::filesystem::exists(closing),"Normal shutdown discards unsaved journal");
    OrbitRecording invalid;require(invalid.start(folder),"New journal");require(invalid.append({utc,{7000,0,0},{0,7,0}}),"Valid first row");require(!invalid.append({utc,{7000,0,0},{0,7,0}})&&!invalid.active(),"Duplicate epoch stops recorder");
    // Independently construct an Earth-fixed equatorial arc crossing a station at longitude zero.
    OrbitReplay arc;arc.frame=OrbitFrame::ECEF;const double rate=.001,r=7000;
    for(int t=-1000;t<=1000;t+=5){double a=rate*t;arc.samples.push_back({utc+t,{r*std::cos(a),r*std::sin(a),0},{-r*rate*std::sin(a),r*rate*std::cos(a),0}});}
    auto passes=findStationPasses(arc,0,0,0);require(passes.size()==1,"Single equatorial pass");
    const double half=std::acos(6378.137/r)/rate;
    require(std::abs(passes[0].start-(utc-half))<.01&&std::abs(passes[0].end-(utc+half))<.01,"Horizon crossings match analytic geometry");
    require(std::abs(passes[0].peak-utc)<.01&&passes[0].maxElevation>89.99,"Zenith and peak time");
    auto high=findStationPasses(arc,0,0,30);require(high.size()==1&&high[0].start>passes[0].start&&high[0].end<passes[0].end,"Elevation mask narrows the pass");
    auto none=findStationPasses(arc,90,0,0);require(none.empty(),"Polar station cannot see equatorial low orbit");
    arc.samples.erase(arc.samples.begin(),arc.samples.begin()+200);passes=findStationPasses(arc,0,0,0);require(passes.size()==1&&passes[0].clippedStart,"Partial coverage marked");
    std::cout<<"Recording, save/reopen, interrupted-write recovery and analytic pass checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

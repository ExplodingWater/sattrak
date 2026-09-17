#pragma once
#include "gui/orbit_replay.hpp"
#include <fstream>
namespace gui {
class OrbitRecording {
public:
    ~OrbitRecording() { discard(); }
    bool start(const std::filesystem::path& folder);
    bool append(const OrbitSample& sample);
    void stop();
    void discard();
    bool unsaved() const { return samples>0 && !saved; }
    bool save(const std::filesystem::path& destination);
    bool recover(const std::filesystem::path& journal);
    bool active() const {return output.is_open();}
    size_t count() const {return samples;}
    const std::filesystem::path& path() const {return journal;}
    std::string status;
private:
    std::ofstream output;
    std::filesystem::path journal;
    std::filesystem::path recoveredSource;
    bool saved=false,ownsJournal=false;
    size_t samples=0;
    double lastUtc=0;
};
struct StationPass {
    double start=0,peak=0,end=0,maxElevation=0;
    bool clippedStart=false,clippedEnd=false;
};
double stationElevation(const OrbitReplay& orbit,double utc,double latitude,double longitude);
std::vector<StationPass> findStationPasses(const OrbitReplay& orbit,double latitude,double longitude,double mask);
}

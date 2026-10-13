#include "gui/research_catalog.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <limits>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try {
    if(argc!=2)return 1;
    const std::string assets=argv[1];gui::ResearchCatalog catalog;
    require(catalog.load(assets+"/catalog"),"Bundled CelesTrak catalog loads");
    auto original=catalog.satellites.size();auto snapshot=catalog.snapshotEpoch;
    std::string error;require(catalog.mergeSatnogs(assets+"/satnogs",error),error.c_str());
    require(catalog.satellites.size()>original,"SatNOGS adds usable satellites");
    require(catalog.stations.size()>100,"Public ground stations loaded");
    require(catalog.info.contains(25544),"ISS metadata available");
    auto count=catalog.satellites.size();std::set<unsigned> ids;
    for(size_t i=0;i<count;++i){const auto& sat=catalog.satellites[i];require(ids.insert(sat.norad).second,"NORAD identifiers are unique");require(sat.meanMotion>0,"Valid mean motion");}
    require(catalog.mergeSatnogs(assets+"/satnogs",error)&&catalog.satellites.size()==count,"Repeated merges are idempotent");
    catalog.epoch=snapshot;require(catalog.snapshotEpoch==snapshot,"Session time does not replace download epoch");
    size_t positions=0;for(size_t i=0;i<count;++i){aero::math::Vec3 p;if(catalog.position(i,0,p))++positions;}
    require(positions>0,"Fresh catalog entries predict positions");
    bool checked=false;
    for(size_t i=0;i<count;++i)if(catalog.satellites[i].norad==51840){
        checked=true;const auto& satellite=catalog.satellites[i];
        catalog.epoch=libsgp4::DateTime(2026,9,25);
        try {
            auto raw=satellite.orbit.FindPosition(catalog.epoch);
            std::cout<<"51840 raw extrapolation: altitude "<<raw.ToGeodetic().altitude<<" km, speed "<<raw.Velocity().Magnitude()<<" km/s\n";
        }catch(const std::exception& e){std::cout<<"51840 raw extrapolation rejected by SGP4: "<<e.what()<<'\n';}
        aero::math::Vec3 point;
        require(catalog.stale(i,0)&&!catalog.position(i,0,point),"51840 stale extrapolation is hidden");
        require(!catalog.prediction(i,0,true),"Stale inspection cannot expose 51840's nonphysical speed");
        require(catalog.mergeSatnogs(assets+"/satnogs",error)&&catalog.stale(i,0),"Downloading the same elements does not refresh their epoch");
        catalog.epoch=catalog.satellites[i].elementEpoch;
        require(catalog.position(i,0,point),"51840 remains available near its historical epoch");
        require(!catalog.stale(i,14*86400)&&catalog.stale(i,14*86400+1)&&catalog.stale(i,-14*86400-1),"Freshness follows simulation time in either direction");
        require(!catalog.prediction(i,std::numeric_limits<double>::quiet_NaN(),true),"Inspection rejects invalid times");
        break;
    }
    require(checked,"51840 regression fixture present");
    for(size_t i=0;i<count;++i)if(catalog.satellites[i].norad==25544){
        catalog.epoch=catalog.satellites[i].elementEpoch.AddDays(15);
        require(!catalog.prediction(i,0)&&catalog.prediction(i,0,true).has_value(),"Explicit inspection can expose a plausible but stale prediction");
        require(catalog.prediction(i,-15*86400).has_value(),"Historical playback uses simulation UTC for freshness");
    }
    auto folder=std::filesystem::path("catalog-invalid-test");std::filesystem::create_directories(folder);std::ofstream(folder/"satellites.json")<<"not-json";
    require(!catalog.mergeSatnogs(folder.string(),error)&&catalog.satellites.size()==count&&!catalog.stations.empty(),"Invalid download preserves good data");
    std::cout<<count<<" unique satellites, "<<catalog.stations.size()<<" stations, "<<positions<<" current predictions; metadata and fallback checks passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#pragma once
#include "common/math_types.hpp"
#include "SGP4.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
namespace gui {
struct ResearchSatellite {
    std::string name;
    unsigned int norad;
    libsgp4::SGP4 orbit;
    libsgp4::DateTime elementEpoch;
    double inclination=0,eccentricity=0,meanMotion=0;
    std::string source="CelesTrak";
};
struct SatelliteInfo {
    std::string satId,name,aliases,status,launched,deployed,operatorName,countries,updated;
    std::vector<std::string> transmitters;
};
struct CatalogStation {
    int id=0;std::string name,status,lastSeen,antennas;
    double latitude=0,longitude=0,altitude=0,horizon=0;
    int observations=0;
};
class ResearchCatalog {
public:
    bool load(const std::string& folder);
    std::vector<ResearchSatellite> satellites;
    libsgp4::DateTime epoch;
    libsgp4::DateTime snapshotEpoch;
    std::string dataOrigin="Bundled CelesTrak snapshot",satnogsOrigin,satnogsDownloaded;
    std::map<unsigned,SatelliteInfo> info;
    std::vector<CatalogStation> stations;
    bool mergeSatnogs(const std::string& folder,std::string& error);
    std::string status;
    // Display policy, not an accuracy guarantee. Compare against simulation UTC.
    static constexpr double maxPredictionAgeDays=14;
    bool stale(size_t index,double seconds) const;
    std::optional<libsgp4::Eci> prediction(size_t index,double seconds,bool allowStale=false) const;
    // Earth-fixed kilometers, mapped to the renderer's Y-up convention.
    bool position(size_t index, double seconds, aero::math::Vec3& point,bool allowStale=false) const;
};
}

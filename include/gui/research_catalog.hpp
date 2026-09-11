#pragma once
#include "common/math_types.hpp"
#include "SGP4.h"
#include <string>
#include <vector>
namespace gui {
struct ResearchSatellite {
    std::string name;
    unsigned int norad;
    libsgp4::SGP4 orbit;
};
class ResearchCatalog {
public:
    bool load(const std::string& folder);
    std::vector<ResearchSatellite> satellites;
    libsgp4::DateTime epoch;
    std::string status;
    // Earth-fixed kilometers, mapped to the renderer's Y-up convention.
    bool position(size_t index, double seconds, aero::math::Vec3& point) const;
};
}

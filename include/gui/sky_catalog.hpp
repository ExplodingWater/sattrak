#pragma once
#include "common/math_types.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace gui {
// J2000 equatorial coordinates in degrees -> scene axes: +X vernal equinox,
// +Y north celestial pole, -Z right ascension 90 degrees. Same as orbitToScene.
aero::math::Vec3 skyDirection(double raDegrees, double decDegrees);
struct SkyStar {
    int hip = 0;
    aero::math::Vec3 direction;
    float magnitude = 0, bv = 0;
};
struct SkySegment { aero::math::Vec3 a, b; };
struct SkyLabel { std::string id, name; aero::math::Vec3 direction; };
struct SkyCatalog {
    std::vector<SkyStar> stars;
    std::vector<SkySegment> segments;
    std::vector<SkyLabel> labels;
    // Transactional load; malformed assets cannot leave a partly loaded sky.
    bool load(const std::filesystem::path& folder, std::string& error);
};
} // namespace gui

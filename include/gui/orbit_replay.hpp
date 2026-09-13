#pragma once
#include "common/math_types.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace gui {
struct OrbitSample {
    double utc=0;
    aero::math::Vec3 position,velocity; // Standard Z-up Cartesian axes, km and km/s.
};
enum class OrbitFrame { EME2000, ECEF };
double parseOrbitUtc(const std::string& text);
std::string formatOrbitUtc(double utc);
double earthSiderealAngle(double utc);
aero::math::Vec3 orbitToScene(const aero::math::Vec3& p);
class OrbitReplay {
public:
    // Loading is transactional: invalid files leave the previously loaded orbit intact.
    bool load(const std::filesystem::path& path,std::string& error);
    bool sample(double utc,OrbitSample& result) const; // No extrapolation.
    OrbitSample inertial(const OrbitSample& state) const;
    std::vector<OrbitSample> samples;
    OrbitFrame frame=OrbitFrame::EME2000;
    std::string name;
};
struct OrbitForces {
    bool j2=false,drag=false;
    double mass=6,area=1,cd=2.2,densityScale=1;
};
double atmosphereDensity(double altitudeKm);
aero::math::Vec3 orbitAcceleration(const aero::math::Vec3& r,const aero::math::Vec3& v,const OrbitForces& forces);
OrbitSample integrateOrbit(const OrbitSample& state,double dt,const OrbitForces& forces);
}

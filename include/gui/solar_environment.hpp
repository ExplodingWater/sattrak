#pragma once
#include "common/math_types.hpp"
#include <cmath>

namespace gui {
struct SolarEnvironment { aero::math::Vec3 direction; double distanceAu, angularRadius; };
// Approximate apparent equatorial coordinates from the USNO solar algorithm
// https://aa.usno.navy.mil/faq/sun_approx
inline SolarEnvironment solarEnvironment(double unixSeconds) {
    constexpr double rad=3.14159265358979323846/180;
    const double days=(unixSeconds-946728000.0)/86400.0;
    const double anomaly=std::remainder(357.529+0.98560028*days,360.0)*rad;
    const double longitude=(std::remainder(280.459+0.98564736*days,360.0)+1.915*std::sin(anomaly)+.020*std::sin(2*anomaly))*rad;
    const double obliquity=(23.439-.00000036*days)*rad;
    const double distance=1.00014-.01671*std::cos(anomaly)-.00014*std::cos(2*anomaly);
    // Scene axes are equatorial X, north Y, and negative equatorial Y on Z
    return {{std::cos(longitude),std::sin(obliquity)*std::sin(longitude),-std::cos(obliquity)*std::sin(longitude)},distance,.2666*rad/distance};
}
}

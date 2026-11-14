#include "gui/solar_environment.hpp"
#include <iostream>
int main() {
    const auto j2000=gui::solarEnvironment(946728000);
    // USNO J2000 solar position is approximately RA 281.28, declination -23.03
    const double ra=std::atan2(-j2000.direction.z,j2000.direction.x)*180/3.141592653589793;
    const double dec=std::asin(j2000.direction.y)*180/3.141592653589793;
    if(std::abs(ra+78.72)>.02 || std::abs(dec+23.03)>.02)return 1;
    for(int day=-365;day<=365;++day) {
        const auto sun=gui::solarEnvironment(946728000+day*86400.0);
        if(std::abs(sun.direction.norm()-1)>1e-12 || sun.distanceAu<.983 || sun.distanceAu>1.0171)return 2;
        const double diameter=sun.angularRadius*360/3.141592653589793;
        if(diameter<.524 || diameter>.546)return 3;
    }
    const auto summer=gui::solarEnvironment(946728000+172*86400.0);
    if(summer.direction.y<.39)return 4;
    std::cout<<"Solar direction, seasonal motion and apparent diameter passed\n";
}

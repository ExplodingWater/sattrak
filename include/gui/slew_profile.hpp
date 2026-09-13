#pragma once
#include "adcs/quaternion.hpp"
#include <algorithm>
#include <cmath>
namespace gui {
// Conservative visualization profile, not a flight-qualified vehicle controller.
inline aero::math::Vec3 slewAcceleration(const aero::adcs::Quaternion& attitude,
    const aero::math::Vec3& rate,const aero::adcs::Quaternion& target,double maxRate,double maxAcceleration) {
    auto error=attitude.inverse()*target;
    if(error.w<0)error=error*-1;
    const double angle=2*std::acos(std::clamp(error.w,-1.0,1.0));
    const double speed=std::min({maxRate,std::sqrt(2*maxAcceleration*angle),angle*.5});
    const auto wanted=error.vec().norm()>1e-12?error.vec().normalized()*speed:aero::math::Vec3::zero();
    auto acceleration=(wanted-rate)*.5;
    if(acceleration.norm()>maxAcceleration)acceleration=acceleration.normalized()*maxAcceleration;
    return acceleration;
}
}

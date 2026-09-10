#pragma once
#include "common/constants.hpp"
#include <cmath>
#include <optional>
#include <stdexcept>

namespace aero::cfd {
struct ObliqueShock {
    double betaRad;
    double mach2;
    double pressureRatio;
    double densityRatio;
    double temperatureRatio;
};

// First crossing of theta(beta) is the weak attached branch. No crossing
// means the requested deflection lies in the detached-shock regime.
inline std::optional<ObliqueShock> weakObliqueShock(double mach, double theta,
                                                   double gamma = constants::GAMMA_AIR) {
    if (!std::isfinite(mach) || mach <= 1 || !std::isfinite(theta) || theta < 0 ||
        theta >= constants::PI / 2 || !std::isfinite(gamma) || gamma <= 1)
        throw std::invalid_argument("Invalid oblique shock parameters");
    const double mu = std::asin(1 / mach);
    if (theta == 0) return ObliqueShock{mu, mach, 1, 1, 1};
    auto f = [&](double beta) {
        const double mn = mach * std::sin(beta);
        return 2 / std::tan(beta) * (mn * mn - 1) /
            (mach * mach * (gamma + std::cos(2 * beta)) + 2) - std::tan(theta);
    };
    double lo = mu, hi = mu;
    bool bracketed = false;
    for (int k = 1; k <= 4096; ++k) {
        hi = mu + (constants::PI / 2 - mu) * k / 4096.0;
        if (f(hi) >= 0) { bracketed = true; break; }
        lo = hi;
    }
    if (!bracketed) return std::nullopt;
    for (int k = 0; k < 60; ++k) {
        const double mid = (lo + hi) / 2;
        if (f(mid) >= 0) hi = mid; else lo = mid;
    }
    const double beta = (lo + hi) / 2;
    const double mn2 = std::pow(mach * std::sin(beta), 2);
    const double pressure = 1 + 2 * gamma / (gamma + 1) * (mn2 - 1);
    const double density = (gamma + 1) * mn2 / ((gamma - 1) * mn2 + 2);
    const double mach2 = std::sqrt((1 + (gamma - 1) * mn2 / 2) /
                                  (gamma * mn2 - (gamma - 1) / 2)) / std::sin(beta - theta);
    return ObliqueShock{beta, mach2, pressure, density, pressure / density};
}
} // namespace aero::cfd

#pragma once

#include "cfd/euler_state.hpp"
#include <cmath>
#include <algorithm>

namespace aero::cfd {

enum class LimiterType {
    FirstOrder, // 1st-order Godunov (no reconstruction)
    Minmod,     // Most dissipative TVD limiter, highly robust
    VanLeer,    // Smooth, balanced limiter
    Superbee    // Sharpest TVD limiter for contact discontinuities
};

/**
 * @brief Evaluates TVD slope limiter function phi(r).
 * @param r Consecutive gradient ratio r = (u_i - u_{i-1}) / (u_{i+1} - u_i)
 */
inline double evaluateLimiter(double r, LimiterType type) {
    if (r <= 0.0) return 0.0;

    switch (type) {
        case LimiterType::FirstOrder:
            return 0.0;
        case LimiterType::Minmod:
            return std::min(1.0, r);
        case LimiterType::VanLeer:
            return (2.0 * r) / (1.0 + r);
        case LimiterType::Superbee:
            return std::max({0.0, std::min(2.0 * r, 1.0), std::min(r, 2.0)});
        default:
            return 0.0;
    }
}

/**
 * @brief Reconstructs interface values at i + 1/2 using MUSCL TVD formulation.
 * @param u_im1 Value at cell i - 1
 * @param u_i   Value at cell i
 * @param u_ip1 Value at cell i + 1
 * @param u_ip2 Value at cell i + 2
 * @param uL_half Reconstructed Left state at face i + 1/2
 * @param uR_half Reconstructed Right state at face i + 1/2
 */
inline void reconstructInterface1D(
    double u_im1, double u_i, double u_ip1, double u_ip2,
    LimiterType type,
    double& uL_half, double& uR_half)
{
    if (type == LimiterType::FirstOrder) {
        uL_half = u_i;
        uR_half = u_ip1;
        return;
    }

    const double delta_i_minus = u_i - u_im1;
    const double delta_i_plus  = u_ip1 - u_i;
    const double delta_ip1_plus = u_ip2 - u_ip1;

    // Slope at cell i
    double r_i = 1.0;
    if (std::abs(delta_i_plus) > 1e-12) {
        r_i = delta_i_minus / delta_i_plus;
    }
    const double phi_i = evaluateLimiter(r_i, type);
    uL_half = u_i + 0.5 * phi_i * delta_i_plus;

    // Slope at cell i + 1 (approaching from right)
    double r_ip1 = 1.0;
    if (std::abs(delta_i_plus) > 1e-12) {
        r_ip1 = delta_ip1_plus / delta_i_plus;
    }
    const double phi_ip1 = evaluateLimiter(r_ip1, type);
    uR_half = u_ip1 - 0.5 * phi_ip1 * delta_i_plus;
}

/**
 * @brief 2nd-Order MUSCL reconstruction for primitive fluid states.
 */
inline void reconstructPrimitiveInterface(
    const PrimitiveState& p_im1,
    const PrimitiveState& p_i,
    const PrimitiveState& p_ip1,
    const PrimitiveState& p_ip2,
    LimiterType type,
    PrimitiveState& pL,
    PrimitiveState& pR)
{
    reconstructInterface1D(p_im1.rho, p_i.rho, p_ip1.rho, p_ip2.rho, type, pL.rho, pR.rho);
    reconstructInterface1D(p_im1.u,   p_i.u,   p_ip1.u,   p_ip2.u,   type, pL.u,   pR.u);
    reconstructInterface1D(p_im1.v,   p_i.v,   p_ip1.v,   p_ip2.v,   type, pL.v,   pR.v);
    reconstructInterface1D(p_im1.p,   p_i.p,   p_ip1.p,   p_ip2.p,   type, pL.p,   pR.p);

    // Safeguard positive density and pressure
    pL.rho = std::max(1e-8, pL.rho);
    pR.rho = std::max(1e-8, pR.rho);
    pL.p   = std::max(1e-4, pL.p);
    pR.p   = std::max(1e-4, pR.p);
}

} // namespace aero::cfd

#pragma once

#include "cfd/euler_state.hpp"
#include <cmath>
#include <algorithm>

namespace aero::cfd {

/**
 * @brief Rusanov / Local Lax-Friedrichs (LLF) Approximate Riemann Solver.
 * High robustness, guaranteed positivity, ideal for strong shock initialization.
 */
inline ConservativeState computeRusanovFlux(
    const ConservativeState& UL,
    const ConservativeState& UR,
    double nx,
    double ny,
    double gamma = constants::GAMMA_AIR) 
{
    const PrimitiveState pL = UL.toPrimitive(gamma);
    const PrimitiveState pR = UR.toPrimitive(gamma);

    const double vnL = pL.u * nx + pL.v * ny;
    const double vnR = pR.u * nx + pR.v * ny;

    const double aL = pL.speedOfSound(gamma);
    const double aR = pR.speedOfSound(gamma);

    const double sMax = std::max(std::abs(vnL) + aL, std::abs(vnR) + aR);

    const ConservativeState fL = UL.normalFlux(nx, ny, gamma);
    const ConservativeState fR = UR.normalFlux(nx, ny, gamma);

    return (fL + fR) * 0.5 - (UR - UL) * (0.5 * sMax);
}

/**
 * @brief Roe's Approximate Riemann Solver with Harten-Hyman Entropy Fix.
 * Low numerical dissipation, sharp resolution of boundary layers, contact discontinuities, and shocks.
 */
inline ConservativeState computeRoeFlux(
    const ConservativeState& UL,
    const ConservativeState& UR,
    double nx,
    double ny,
    double gamma = constants::GAMMA_AIR) 
{
    const PrimitiveState pL = UL.toPrimitive(gamma);
    const PrimitiveState pR = UR.toPrimitive(gamma);

    // Roe-averaged quantities
    const double sqrtRhoL = std::sqrt(pL.rho);
    const double sqrtRhoR = std::sqrt(pR.rho);
    const double invSqrtSum = 1.0 / (sqrtRhoL + sqrtRhoR);

    const double uRoe = (sqrtRhoL * pL.u + sqrtRhoR * pR.u) * invSqrtSum;
    const double vRoe = (sqrtRhoL * pL.v + sqrtRhoR * pR.v) * invSqrtSum;
    const double hL = pL.totalEnthalpy(gamma);
    const double hR = pR.totalEnthalpy(gamma);
    const double hRoe = (sqrtRhoL * hL + sqrtRhoR * hR) * invSqrtSum;

    const double q2Roe = uRoe * uRoe + vRoe * vRoe;
    const double a2Roe = std::max(1e-10, (gamma - 1.0) * (hRoe - 0.5 * q2Roe));
    const double aRoe = std::sqrt(a2Roe);
    const double vnRoe = uRoe * nx + vRoe * ny;

    // Eigenvalues
    double lambda[4] = {
        vnRoe - aRoe,
        vnRoe,
        vnRoe,
        vnRoe + aRoe
    };

    // Harten-Hyman Entropy Fix to prevent expansion shocks
    const double delta = 0.1 * aRoe;
    for (double& lam : lambda) {
        if (std::abs(lam) < delta) {
            lam = 0.5 * (lam * lam + delta * delta) / delta;
        } else {
            lam = std::abs(lam);
        }
    }

    // Differences in primitive variables
    const double dRho = pR.rho - pL.rho;
    const double dP   = pR.p - pL.p;
    const double du   = pR.u - pL.u;
    const double dv   = pR.v - pL.v;
    const double dvn  = du * nx + dv * ny;
    const double dvt  = -du * ny + dv * nx; // Tangential velocity difference

    const double rhoRoe = sqrtRhoL * sqrtRhoR;

    // Wave strengths (projections on left eigenvectors)
    const double alpha1 = (dP - rhoRoe * aRoe * dvn) / (2.0 * a2Roe);
    const double alpha2 = dRho - dP / a2Roe;
    const double alpha3 = rhoRoe * dvt;
    const double alpha4 = (dP + rhoRoe * aRoe * dvn) / (2.0 * a2Roe);

    // Right eigenvectors
    const double r1[4] = {
        1.0,
        uRoe - aRoe * nx,
        vRoe - aRoe * ny,
        hRoe - aRoe * vnRoe
    };

    const double r2[4] = {
        1.0,
        uRoe,
        vRoe,
        0.5 * q2Roe
    };

    const double r3[4] = {
        0.0,
        -ny,
        nx,
        -uRoe * ny + vRoe * nx
    };

    const double r4[4] = {
        1.0,
        uRoe + aRoe * nx,
        vRoe + aRoe * ny,
        hRoe + aRoe * vnRoe
    };

    // Dissipation vector D = sum(alpha_k * |lambda_k| * r_k)
    ConservativeState diss(0.0, 0.0, 0.0, 0.0);
    const double aL1 = alpha1 * lambda[0];
    const double aL2 = alpha2 * lambda[1];
    const double aL3 = alpha3 * lambda[2];
    const double aL4 = alpha4 * lambda[3];

    for (size_t i = 0; i < 4; ++i) {
        diss.u[i] = aL1 * r1[i] + aL2 * r2[i] + aL3 * r3[i] + aL4 * r4[i];
    }

    const ConservativeState fL = UL.normalFlux(nx, ny, gamma);
    const ConservativeState fR = UR.normalFlux(nx, ny, gamma);

    return (fL + fR) * 0.5 - diss * 0.5;
}

} // namespace aero::cfd

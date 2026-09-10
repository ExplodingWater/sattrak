#include "cfd/euler_state.hpp"
#include "cfd/riemann_solvers.hpp"
#include "cfd/muscl_reconstruction.hpp"
#include "common/constants.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

#define ASSERT_NEAR(a, b, tol) \
    do { \
        if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(tol) || std::abs((a) - (b)) > (tol)) { \
            std::cerr << "Assertion failed: |" #a " - " #b "| = " \
                      << std::abs((a) - (b)) << " > " << (tol) \
                      << " at line " << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at line " << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

int testStateConversionRoundtrip() {
    std::cout << "[Test 1] Primitive <-> Conservative State Inversion...";
    const aero::cfd::PrimitiveState pOrig{1.225, 450.0, -120.0, 101325.0};
    const auto uCons = aero::cfd::ConservativeState::fromPrimitive(pOrig);
    const auto pBack = uCons.toPrimitive();

    ASSERT_NEAR(pBack.rho, pOrig.rho, 1e-12);
    ASSERT_NEAR(pBack.u,   pOrig.u,   1e-12);
    ASSERT_NEAR(pBack.v,   pOrig.v,   1e-12);
    ASSERT_NEAR(pBack.p,   pOrig.p,   1e-8);

    std::cout << " PASSED\n";
    return 0;
}

int testNormalFluxProperties() {
    std::cout << "[Test 2] Normal Flux Directionality & Anti-symmetry...";
    const aero::cfd::PrimitiveState prim{1.225, 300.0, 150.0, 101325.0};
    const auto uCons = aero::cfd::ConservativeState::fromPrimitive(prim);

    const double nx = 0.6, ny = 0.8;
    const auto fluxPos = uCons.normalFlux(nx, ny);
    const auto fluxNeg = uCons.normalFlux(-nx, -ny);

    for (size_t i = 0; i < 4; ++i) {
        ASSERT_NEAR(fluxPos.u[i], -fluxNeg.u[i], 1e-10);
    }

    std::cout << " PASSED\n";
    return 0;
}

int testRankineHugoniotShockJump() {
    std::cout << "[Test 3] Analytical Rankine-Hugoniot Normal Shock Jump Conservation...";
    const double gamma = aero::constants::GAMMA_AIR;
    const double M1 = 2.5;
    const double p1 = 101325.0;
    const double rho1 = 1.225;
    const double a1 = std::sqrt(gamma * p1 / rho1);
    const double u1 = M1 * a1;

    // Exact normal shock relations
    const double pRatio = 1.0 + (2.0 * gamma / (gamma + 1.0)) * (M1 * M1 - 1.0);
    const double p2 = p1 * pRatio;

    const double rhoRatio = ((gamma + 1.0) * M1 * M1) / ((gamma - 1.0) * M1 * M1 + 2.0);
    const double rho2 = rho1 * rhoRatio;

    // Continuity: rho1 * u1 = rho2 * u2 => u2 = (rho1 / rho2) * u1
    const double u2 = u1 / rhoRatio;

    const aero::cfd::PrimitiveState pState1{rho1, u1, 0.0, p1};
    const aero::cfd::PrimitiveState pState2{rho2, u2, 0.0, p2};

    const auto uCons1 = aero::cfd::ConservativeState::fromPrimitive(pState1);
    const auto uCons2 = aero::cfd::ConservativeState::fromPrimitive(pState2);

    const auto flux1 = uCons1.normalFlux(1.0, 0.0);
    const auto flux2 = uCons2.normalFlux(1.0, 0.0);

    // Mass flux: rho * u
    ASSERT_NEAR(flux1.u[0], flux2.u[0], 1e-7);
    // Momentum flux: rho * u^2 + p
    ASSERT_NEAR(flux1.u[1], flux2.u[1], 1e-5);
    // Transverse momentum flux: 0
    ASSERT_NEAR(flux1.u[2], flux2.u[2], 1e-10);
    // Energy flux: (E + p) * u
    ASSERT_NEAR(flux1.u[3], flux2.u[3], 1e-3);

    std::cout << " PASSED\n";
    return 0;
}

int testRoeStationaryShockResolution() {
    std::cout << "[Test 4] Roe Riemann Solver Stationary Shock Resolution...";
    const double gamma = aero::constants::GAMMA_AIR;
    const double M1 = 2.0;
    const double p1 = 101325.0;
    const double rho1 = 1.225;
    const double a1 = std::sqrt(gamma * p1 / rho1);
    const double u1 = M1 * a1;

    const double pRatio = 1.0 + (2.0 * gamma / (gamma + 1.0)) * (M1 * M1 - 1.0);
    const double p2 = p1 * pRatio;
    const double rhoRatio = ((gamma + 1.0) * M1 * M1) / ((gamma - 1.0) * M1 * M1 + 2.0);
    const double rho2 = rho1 * rhoRatio;
    const double u2 = u1 / rhoRatio;

    const auto uL = aero::cfd::ConservativeState::fromPrimitive({rho1, u1, 0.0, p1});
    const auto uR = aero::cfd::ConservativeState::fromPrimitive({rho2, u2, 0.0, p2});

    // Roe numerical flux through normal interface
    const auto roeFlux = aero::cfd::computeRoeFlux(uL, uR, 1.0, 0.0);
    const auto exactFlux = uL.normalFlux(1.0, 0.0);

    // Roe solver captures stationary shock: relative difference within entropy fix tolerance (< 3%)
    for (size_t i = 0; i < 4; ++i) {
        if (std::abs(exactFlux.u[i]) > 1.0) {
            const double relDiff = std::abs(roeFlux.u[i] - exactFlux.u[i]) / std::abs(exactFlux.u[i]);
            ASSERT_TRUE(relDiff < 0.03); // within 3% due to entropy fix smoothing
        } else {
            ASSERT_NEAR(roeFlux.u[i], exactFlux.u[i], 1e-5);
        }
    }

    std::cout << " PASSED\n";
    return 0;
}

int testTVDLimitingMonotonicity() {
    std::cout << "[Test 5] TVD Slope Limiter Monotonicity Preservation...";
    // Discontinuous step profile: [1.0, 1.0, 5.0, 5.0]
    double uL = 0.0, uR = 0.0;

    // Test Minmod
    aero::cfd::reconstructInterface1D(1.0, 1.0, 5.0, 5.0, aero::cfd::LimiterType::Minmod, uL, uR);
    // Must be bounded within [1.0, 5.0] without overshoots
    ASSERT_TRUE(uL >= 1.0 && uL <= 5.0);
    ASSERT_TRUE(uR >= 1.0 && uR <= 5.0);

    // Test Van Leer
    aero::cfd::reconstructInterface1D(1.0, 1.0, 5.0, 5.0, aero::cfd::LimiterType::VanLeer, uL, uR);
    ASSERT_TRUE(uL >= 1.0 && uL <= 5.0);
    ASSERT_TRUE(uR >= 1.0 && uR <= 5.0);

    std::cout << " PASSED\n";
    return 0;
}

int main() {
    std::cout << "\n=========================================\n";
    std::cout << "    RUNNING CFD SHOCK UNIT TEST SUITE    \n";
    std::cout << "=========================================\n";

    if (testStateConversionRoundtrip()) return 1;
    if (testNormalFluxProperties()) return 1;
    if (testRankineHugoniotShockJump()) return 1;
    if (testRoeStationaryShockResolution()) return 1;
    if (testTVDLimitingMonotonicity()) return 1;

    std::cout << "\n>>> ALL CFD SHOCK TESTS PASSED SUCCESSFULLY! <<<\n\n";
    return 0;
}

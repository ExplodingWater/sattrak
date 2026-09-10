#include "adcs/satellite_dynamics.hpp"
#include "adcs/mekf.hpp"
#include "cfd/cfd_solver.hpp"
#include "cfd/oblique_shock.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace aero;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance, const char* message) {
    check(std::isfinite(actual) && std::isfinite(expected) &&
          std::abs(actual - expected) <= tolerance, message);
}
template<class F> void rejects(F operation, const char* message) {
    bool threw = false;
    try { operation(); } catch (const std::exception&) { threw = true; }
    check(threw, message);
}

void attitudeRegression() {
    adcs::SpacecraftParameters p;
    p.inertia = math::Mat3::diag(0.04, 0.04, 0.04);
    p.inertiaInv = p.inertia.inverse();
    adcs::SatelliteDynamics dynamics(p);
    adcs::SatelliteState truth;
    truth.q = adcs::Quaternion::fromRotationVector({0.7, -0.4, 0.2});
    truth.omega = {0.2, 0.1, -0.15};
    auto propagated = truth.q;
    const auto start = truth.q;
    for (int i = 0; i < 100; ++i) {
        truth = dynamics.step(truth, {}, {}, 0.01);
        propagated = propagated.propagate(truth.omega, 0.01);
    }
    // Independent constant-rate solution: Rodrigues rotation in the body frame.
    const auto v = math::Vec3{0.3, 0.4, -0.2};
    const auto axis = truth.omega.normalized();
    const double angle = truth.omega.norm();
    const auto initialBody = start.rotate(v);
    const auto expectedBody = initialBody * std::cos(angle) - axis.cross(initialBody) * std::sin(angle)
        + axis * (axis.dot(initialBody) * (1.0 - std::cos(angle)));
    near((propagated.rotate(v) - expectedBody).norm(), 0, 1e-11, "Quaternion frame convention");
    near((truth.q.rotate(v) - expectedBody).norm(), 0, 1e-11, "RK4 frame convention");

    adcs::MEKF estimator;
    const math::Vec3 bias{0.02, -0.015, 0.01};
    for (int i = 0; i < 3000; ++i) {
        truth = dynamics.step(truth, {}, {}, 0.01);
        estimator.predict(truth.omega + bias, 0.01);
        if (i % 10 == 0) {
            for (const auto& ref : {math::Vec3{1,0,0}, math::Vec3{0,1,0}})
                estimator.updateVector(truth.q.rotate(ref), ref, 0.0005);
        }
    }
    near((estimator.gyroBias() - bias).norm(), 0, 0.001, "Independent MEKF bias truth");
    near((estimator.attitude().rotate(v) - truth.q.rotate(v)).norm(), 0, 0.002, "Independent MEKF attitude truth");

    for (double sign : {-1.0, 1.0}) {
        adcs::SatelliteState state;
        state.hWheel = {sign * 0.0499, 0, 0};
        const auto initialH = p.inertia * state.omega + state.hWheel;
        state = dynamics.step(state, {}, {-sign * 0.005, 0, 0}, 1.0);
        near(state.hWheel.x, sign * 0.05, 1e-12, "Wheel capacity");
        near((p.inertia * state.omega + state.hWheel - initialH).norm(), 0, 1e-12, "Wheel momentum exchange");
        state = dynamics.step(state, {}, {sign * 0.005, 0, 0}, 0.1);
        check(std::abs(state.hWheel.x) < 0.05, "Wheel can unload at capacity");
    }
}

void cfdRegression() {
    const auto shock = cfd::weakObliqueShock(2.5, 15 * constants::PI / 180);
    check(shock.has_value(), "Attached shock exists");
    near(shock->betaRad * 180 / constants::PI, 36.9449, 0.0001, "Weak shock branch");
    check(!cfd::weakObliqueShock(1.2, 25 * constants::PI / 180), "Detached shock rejected");
    near(cfd::weakObliqueShock(2.5, 0)->pressureRatio, 1, 1e-12, "Zero deflection");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { near(nan, 0, 1, "NaN"); }, "Near assertion must reject NaN");
    for (auto state : {cfd::ConservativeState{-1,0,0,1}, cfd::ConservativeState{1,10,0,1},
                       cfd::ConservativeState{nan,0,0,1}})
        rejects([&] { (void)state.toPrimitive(); }, "Invalid CFD states must not be masked");
    auto grid = cfd::StructuredGrid::createCompressionRamp(12,6,0,1.8,0.9,0.4,0);
    cfd::PrimitiveState free{1.225,850,0,101325};
    cfd::CFDSolver uniform(grid, free);
    const double dt = uniform.computeTimeStep();
    uniform.setCfl(0.3);
    near(uniform.computeTimeStep(), dt / 2, 1e-14, "Live CFL update");
    rejects([&] { uniform.setCfl(0); }, "Zero CFL rejected");
    rejects([&] { uniform.setCfl(nan); }, "NaN CFL rejected");
    check(uniform.solve(false), "Uniform flow should converge");
    for (const auto& state : uniform.states()) near(state.rho(), free.rho, 1e-12, "Uniform flow preservation");

    grid = cfd::StructuredGrid::createCompressionRamp(90,45,0,1.8,0.9,0.4,15);
    const double rho = 101325 / (constants::R_SPECIFIC_AIR * 288.15);
    free = {rho, 2.5 * std::sqrt(1.4 * 101325 / rho), 0, 101325};
    cfd::SolverConfig cfg;
    cfg.cfl = 0.55; cfg.convergenceTol = 2e-5; cfg.maxIterations = 4000;
    cfd::CFDSolver wedge(grid, free, cfg);
    check(wedge.solve(false), "Wedge solver must converge");
    for (const auto& state : wedge.states()) check(state.isPhysical(), "Wedge positivity");
    const auto downstream = wedge.cellState(80,1).toPrimitive();
    // Fixed independent reference values for M=2.5, theta=15 degrees, gamma=1.4.
    near(downstream.p / 250019.4503, 1, 0.01, "Wedge pressure benchmark");
    near(downstream.rho / 2.2865, 1, 0.03, "Wedge density benchmark");
    near(downstream.mach() / 1.8735, 1, 0.03, "Wedge Mach benchmark");
    cfg.maxIterations = 1;
    cfd::CFDSolver unfinished(grid, free, cfg);
    check(!unfinished.solve(false), "Insufficient iterations must fail");
    cfg.cfl = 1e-8;
    cfg.maxIterations = 60;
    cfd::CFDSolver stalled(grid, free, cfg);
    check(!stalled.solve(false), "Tiny CFL must not produce false convergence");
    std::cout << "Wedge: p=" << downstream.p << " rho=" << downstream.rho
              << " Mach=" << downstream.mach() << '\n';
}
int main() try {
    attitudeRegression();
    cfdRegression();
    std::cout << "Numerical regressions passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}

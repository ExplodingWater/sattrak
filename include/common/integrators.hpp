#pragma once

#include <functional>

namespace aero::math {

/**
 * @brief Classical 4th-order Runge-Kutta (RK4) integrator.
 * Solves dx/dt = f(t, x).
 * 
 * @tparam StateType Vector or container supporting +, *, and scalar multiplication.
 * @tparam DerivFunc Invocable with signature StateType(double t, const StateType& x)
 */
template <typename StateType, typename DerivFunc>
StateType rk4Step(const StateType& x, double t, double dt, DerivFunc&& f) {
    const StateType k1 = f(t, x);
    const StateType k2 = f(t + 0.5 * dt, x + k1 * (0.5 * dt));
    const StateType k3 = f(t + 0.5 * dt, x + k2 * (0.5 * dt));
    const StateType k4 = f(t + dt, x + k3 * dt);
    return x + (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (dt / 6.0);
}

/**
 * @brief 2nd-Order Strong Stability Preserving Runge-Kutta (SSP-RK2 / Heun's method).
 * Optimal for hyperbolic conservation laws (CFD) with TVD property.
 */
template <typename StateType, typename DerivFunc>
StateType sspRk2Step(const StateType& u0, double dt, DerivFunc&& rhs) {
    // u(1) = u0 + dt * L(u0)
    const StateType u1 = u0 + rhs(u0) * dt;
    // u(2) = 0.5 * u0 + 0.5 * u1 + 0.5 * dt * L(u1)
    return u0 * 0.5 + u1 * 0.5 + rhs(u1) * (0.5 * dt);
}

/**
 * @brief 3rd-Order Strong Stability Preserving Runge-Kutta (SSP-RK3 / Shu-Osher scheme).
 */
template <typename StateType, typename DerivFunc>
StateType sspRk3Step(const StateType& u0, double dt, DerivFunc&& rhs) {
    // Stage 1: u(1) = u0 + dt * L(u0)
    const StateType u1 = u0 + rhs(u0) * dt;
    // Stage 2: u(2) = 3/4 u0 + 1/4 u1 + 1/4 dt * L(u1)
    const StateType u2 = u0 * 0.75 + u1 * 0.25 + rhs(u1) * (0.25 * dt);
    // Stage 3: u(n+1) = 1/3 u0 + 2/3 u2 + 2/3 dt * L(u2)
    return u0 * (1.0 / 3.0) + u2 * (2.0 / 3.0) + rhs(u2) * (dt * 2.0 / 3.0);
}

} // namespace aero::math

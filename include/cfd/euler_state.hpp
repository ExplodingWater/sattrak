#pragma once

#include "common/math_types.hpp"
#include "common/constants.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace aero::cfd {

/**
 * @brief Primitive fluid state: [density, u-velocity, v-velocity, static pressure].
 */
struct PrimitiveState {
    double rho{1.225}; // Density [kg/m^3]
    double u{0.0};     // X-velocity [m/s]
    double v{0.0};     // Y-velocity [m/s]
    double p{101325.0};// Static pressure [Pa]

    constexpr PrimitiveState() = default;
    constexpr PrimitiveState(double rho_, double u_, double v_, double p_)
        : rho(rho_), u(u_), v(v_), p(p_) {}

    [[nodiscard]] double speedOfSound(double gamma = constants::GAMMA_AIR) const {
        return std::sqrt(std::max(1e-10, gamma * p / rho));
    }

    [[nodiscard]] double speed() const {
        return std::sqrt(u * u + v * v);
    }

    [[nodiscard]] double mach(double gamma = constants::GAMMA_AIR) const {
        return speed() / speedOfSound(gamma);
    }

    [[nodiscard]] double temperature(double R = constants::R_SPECIFIC_AIR) const {
        return p / (rho * R);
    }

    [[nodiscard]] double totalEnthalpy(double gamma = constants::GAMMA_AIR) const {
        const double a2 = gamma * p / rho;
        return a2 / (gamma - 1.0) + 0.5 * (u * u + v * v);
    }
};

/**
 * @brief Conservative state vector: [rho, rho*u, rho*v, E].
 */
struct ConservativeState {
    math::Vec4 u; // [0]=rho, [1]=rho*u, [2]=rho*v, [3]=E

    constexpr ConservativeState() = default;
    constexpr explicit ConservativeState(const math::Vec4& v) : u(v) {}
    constexpr ConservativeState(double rho, double rhou, double rhov, double E)
        : u(rho, rhou, rhov, E) {}

    constexpr double rho() const { return u[0]; }
    constexpr double rhou() const { return u[1]; }
    constexpr double rhov() const { return u[2]; }
    constexpr double E() const { return u[3]; }

    constexpr double& rho() { return u[0]; }
    constexpr double& rhou() { return u[1]; }
    constexpr double& rhov() { return u[2]; }
    constexpr double& E() { return u[3]; }

    constexpr ConservativeState operator+(const ConservativeState& o) const {
        return ConservativeState(u + o.u);
    }
    constexpr ConservativeState operator-(const ConservativeState& o) const {
        return ConservativeState(u - o.u);
    }
    constexpr ConservativeState operator*(double s) const {
        return ConservativeState(u * s);
    }
    ConservativeState& operator+=(const ConservativeState& o) {
        u += o.u;
        return *this;
    }
    ConservativeState& operator-=(const ConservativeState& o) {
        u -= o.u;
        return *this;
    }
    ConservativeState& operator*=(double s) {
        u *= s;
        return *this;
    }

    /**
     * @brief Converts conservative state to primitive state.
     */
    [[nodiscard]] bool isPhysical() const {
        for (size_t k = 0; k < 4; ++k) if (!std::isfinite(u[k])) return false;
        if (u[0] <= 0.0) return false;
        const double internal = u[3] - 0.5 * (u[1] * u[1] + u[2] * u[2]) / u[0];
        return std::isfinite(internal) && internal > 0.0;
    }

    [[nodiscard]] PrimitiveState toPrimitive(double gamma = constants::GAMMA_AIR) const {
        if (!isPhysical()) throw std::runtime_error("Nonphysical CFD conservative state");
        const double density = u[0];
        const double invRho = 1.0 / density;
        const double uVel = u[1] * invRho;
        const double vVel = u[2] * invRho;
        const double kineticEnergy = 0.5 * density * (uVel * uVel + vVel * vVel);
        const double internalEnergy = u[3] - kineticEnergy;
        const double pressure = (gamma - 1.0) * internalEnergy;
        return {density, uVel, vVel, pressure};
    }

    /**
     * @brief Converts primitive state to conservative state.
     */
    static ConservativeState fromPrimitive(const PrimitiveState& prim, double gamma = constants::GAMMA_AIR) {
        const double rhou = prim.rho * prim.u;
        const double rhov = prim.rho * prim.v;
        const double kinetic = 0.5 * prim.rho * (prim.u * prim.u + prim.v * prim.v);
        const double E = prim.p / (gamma - 1.0) + kinetic;
        return {prim.rho, rhou, rhov, E};
    }

    /**
     * @brief Evaluates the physical normal flux Fn = F*nx + G*ny.
     */
    [[nodiscard]] ConservativeState normalFlux(double nx, double ny, double gamma = constants::GAMMA_AIR) const {
        const auto p = toPrimitive(gamma);
        const double vn = p.u * nx + p.v * ny;
        return {
            p.rho * vn,
            p.rho * p.u * vn + p.p * nx,
            p.rho * p.v * vn + p.p * ny,
            (u[3] + p.p) * vn
        };
    }
};

inline ConservativeState operator*(double s, const ConservativeState& c) {
    return c * s;
}

} // namespace aero::cfd

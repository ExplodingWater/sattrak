#pragma once

#include "common/math_types.hpp"
#include "common/constants.hpp"
#include "adcs/quaternion.hpp"
#include "common/integrators.hpp"

namespace aero::adcs {

/**
 * @brief Spacecraft physical properties and inertia matrix.
 */
struct SpacecraftParameters {
    math::Mat3 inertia{math::Mat3::diag(0.04, 0.04, 0.02)}; // Default 3U-6U CubeSat [kg*m^2]
    math::Mat3 inertiaInv{inertia.inverse()};
    double mass{6.0};                                       // Satellite wet mass [kg]
    double maxWheelTorque{0.005};                            // Max reaction wheel torque [N*m]
    double maxWheelMomentum{0.05};                          // Max wheel momentum capacity [N*m*s]
};

/**
 * @brief Spacecraft state containing true attitude, angular velocity, and wheel momentum.
 */
struct SatelliteState {
    Quaternion q{1.0, 0.0, 0.0, 0.0};              // Inertial to Body quaternion
    math::Vec3 omega{math::Vec3::zero()};           // Angular velocity in Body frame [rad/s]
    math::Vec3 hWheel{math::Vec3::zero()};          // Reaction wheel angular momentum in Body frame [N*m*s]
};

/**
 * @brief 6-DOF Rotational Satellite Dynamics Engine.
 */
class SatelliteDynamics {
public:
    explicit SatelliteDynamics(SpacecraftParameters params = {})
        : params_(params) {}

    /**
     * @brief Computes gravity gradient disturbance torque in Body frame.
     * tau_gg = (3 * mu / r^3) * (r_B x (J * r_B))
     * @param rOrbit Orbital radius from Earth center [m]
     * @param rB_unit Unit vector pointing from Earth center to satellite in Body frame
     */
    [[nodiscard]] math::Vec3 computeGravityGradient(double rOrbit, const math::Vec3& rB_unit) const {
        const double factor = (3.0 * constants::MU_EARTH) / (rOrbit * rOrbit * rOrbit);
        const math::Vec3 Jr = params_.inertia * rB_unit;
        return rB_unit.cross(Jr) * factor;
    }

    /**
     * @brief Evaluates the rotational equations of motion.
     * J * d(omega)/dt + omega x (J * omega + h_wheel) = tau_ext + tau_wheel
     */
    [[nodiscard]] math::Vec3 computeAngularAcceleration(
        const math::Vec3& omega,
        const math::Vec3& hWheel,
        const math::Vec3& tauExt,
        const math::Vec3& tauWheel) const 
    {
        const math::Vec3 totalMomentum = params_.inertia * omega + hWheel;
        const math::Vec3 gyroscopic = omega.cross(totalMomentum);
        const math::Vec3 netTorque = tauExt + tauWheel - gyroscopic;
        return params_.inertiaInv * netTorque;
    }

    /**
     * @brief Integrates the satellite rotational dynamics forward by dt using RK4.
     */
    SatelliteState step(
        const SatelliteState& current,
        const math::Vec3& tauExt,
        const math::Vec3& tauWheelCmd,
        double dt) const 
    {
        if (!std::isfinite(dt) || dt <= 0.0) {
            throw std::invalid_argument("Satellite timestep must be finite and positive");
        }
        // Saturate wheel torque based on physical limits
        math::Vec3 tauWheel = tauWheelCmd;
        for (size_t i = 0; i < 3; ++i) {
            tauWheel[i] = std::clamp(tauWheel[i], -params_.maxWheelTorque, params_.maxWheelTorque);
            if (!std::isfinite(current.hWheel[i]) ||
                std::abs(current.hWheel[i]) > params_.maxWheelMomentum + 1e-12) {
                throw std::invalid_argument("Initial wheel momentum exceeds capacity");
            }
            // h_next = h - torque * dt: constrain the transfer itself so the
            // spacecraft receives exactly the momentum removed from the wheel.
            const double minTorque = (current.hWheel[i] - params_.maxWheelMomentum) / dt;
            const double maxTorque = (current.hWheel[i] + params_.maxWheelMomentum) / dt;
            tauWheel[i] = std::clamp(tauWheel[i], minTorque, maxTorque);
        }

        // Fully coupled 10-state numerical integration (Quaternion + Omega + Wheel momentum) via RK4
        struct State10 {
            std::array<double, 10> d{0.0};
            double& operator[](size_t i) { return d[i]; }
            const double& operator[](size_t i) const { return d[i]; }
            State10 operator+(const State10& o) const {
                State10 r;
                for (size_t i = 0; i < 10; ++i) r.d[i] = d[i] + o.d[i];
                return r;
            }
            State10 operator*(double s) const {
                State10 r;
                for (size_t i = 0; i < 10; ++i) r.d[i] = d[i] * s;
                return r;
            }
        };

        State10 y0;
        y0[0] = current.q.w; y0[1] = current.q.x; y0[2] = current.q.y; y0[3] = current.q.z;
        y0[4] = current.omega.x; y0[5] = current.omega.y; y0[6] = current.omega.z;
        y0[7] = current.hWheel.x; y0[8] = current.hWheel.y; y0[9] = current.hWheel.z;

        auto derivs = [this, &tauExt, &tauWheel](double /*t*/, const State10& s) -> State10 {
            State10 ds;
            const double qw = s[0], qx = s[1], qy = s[2], qz = s[3];
            const math::Vec3 w{s[4], s[5], s[6]};
            const math::Vec3 hW{s[7], s[8], s[9]};

            // Quaternion kinematics: dq/dt = 0.5 * Omega(w) * q
            ds[0] = 0.5 * (-w.x * qx - w.y * qy - w.z * qz);
            ds[1] = 0.5 * ( w.x * qw + w.z * qy - w.y * qz);
            ds[2] = 0.5 * ( w.y * qw - w.z * qx + w.x * qz);
            ds[3] = 0.5 * ( w.z * qw + w.y * qx - w.x * qy);

            // Dynamics: d(omega)/dt
            const math::Vec3 dw = computeAngularAcceleration(w, hW, tauExt, tauWheel);
            ds[4] = dw.x; ds[5] = dw.y; ds[6] = dw.z;

            // Reaction wheels: dh_w/dt = -tauWheel
            ds[7] = -tauWheel.x; ds[8] = -tauWheel.y; ds[9] = -tauWheel.z;

            return ds;
        };

        const State10 yNext = math::rk4Step(y0, 0.0, dt, derivs);

        Quaternion qNext(yNext[0], yNext[1], yNext[2], yNext[3]);
        qNext.normalize();

        const math::Vec3 nextOmega{yNext[4], yNext[5], yNext[6]};
        const math::Vec3 nextHWheel{yNext[7], yNext[8], yNext[9]};

        return SatelliteState{qNext, nextOmega, nextHWheel};
    }

    [[nodiscard]] const SpacecraftParameters& parameters() const { return params_; }

private:
    SpacecraftParameters params_;
};

} // namespace aero::adcs

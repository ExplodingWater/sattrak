#pragma once

#include "common/math_types.hpp"
#include "adcs/quaternion.hpp"

namespace aero::adcs {

/**
 * @brief Attitude controller gains and limits.
 */
struct ControllerConfig {
    double Kp{0.08};        // Proportional attitude error gain [N*m/rad]
    double Kd{0.15};        // Derivative rate damping gain [N*m*s/rad]
    double maxTorque{0.005}; // Max allowable reaction wheel torque [N*m]
};

/**
 * @brief Quaternion Feedback PD Attitude Controller for 3-Axis Reaction Wheel Pointing.
 * Globally asymptotically stable via Lyapunov design.
 */
class AttitudeController {
public:
    explicit AttitudeController(const ControllerConfig& config = {})
        : config_(config) {}

    /**
     * @brief Computes reaction wheel control torque to track target quaternion.
     * @param qEstimated Current estimated attitude quaternion
     * @param omegaEstimated Current estimated angular velocity [rad/s]
     * @param qTarget Desired target attitude quaternion
     * @return Commanded reaction wheel torque vector in Body frame [N*m]
     */
    [[nodiscard]] math::Vec3 computeControlTorque(
        const Quaternion& qEstimated,
        const math::Vec3& omegaEstimated,
        const Quaternion& qTarget) const 
    {
        // Quaternion error: q_err = qTarget.inverse() * qEstimated
        Quaternion qErr = qTarget.inverse() * qEstimated;

        // Ensure unwinding prevention: shortest path rotation on S^3
        if (qErr.w < 0.0) {
            qErr = Quaternion(-qErr.w, -qErr.x, -qErr.y, -qErr.z);
        }

        // Proportional restoring torque: tau_p = -2 * Kp * q_err_vec
        // Derivative damping torque:    tau_d = -Kd * omega
        const math::Vec3 tauP = qErr.vec() * (-2.0 * config_.Kp);
        const math::Vec3 tauD = omegaEstimated * (-config_.Kd);

        math::Vec3 tauCmd = tauP + tauD;

        // Apply physical actuator saturation
        for (size_t i = 0; i < 3; ++i) {
            tauCmd[i] = std::clamp(tauCmd[i], -config_.maxTorque, config_.maxTorque);
        }

        return tauCmd;
    }

    [[nodiscard]] const ControllerConfig& config() const { return config_; }

private:
    ControllerConfig config_;
};

} // namespace aero::adcs

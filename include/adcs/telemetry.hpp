#pragma once

#include "common/math_types.hpp"
#include "adcs/quaternion.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

namespace aero::adcs {

/**
 * @brief Telemetry snapshot at a single simulation timestamp.
 */
struct TelemetryRecord {
    double time{0.0};
    Quaternion qTrue;
    Quaternion qEst;
    math::Vec3 omegaTrue;
    math::Vec3 omegaEst;
    math::Vec3 biasTrue;
    math::Vec3 biasEst;
    math::Vec3 att3Sigma;
    math::Vec3 bias3Sigma;
    math::Vec3 hWheel;
    math::Vec3 tauCmd;
    double attitudeErrorAngleDeg{0.0};
};

/**
 * @brief High-precision CSV Telemetry Logger for Flight Software.
 */
class TelemetryLogger {
public:
    void record(const TelemetryRecord& rec) {
        records_.push_back(rec);
    }

    bool exportCSV(const std::string& filepath) const {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << "time,"
             << "q_true_w,q_true_x,q_true_y,q_true_z,"
             << "q_est_w,q_est_x,q_est_y,q_est_z,"
             << "omega_true_x,omega_true_y,omega_true_z,"
             << "omega_est_x,omega_est_y,omega_est_z,"
             << "bias_true_x,bias_true_y,bias_true_z,"
             << "bias_est_x,bias_est_y,bias_est_z,"
             << "att_3sig_x,att_3sig_y,att_3sig_z,"
             << "bias_3sig_x,bias_3sig_y,bias_3sig_z,"
             << "h_wheel_x,h_wheel_y,h_wheel_z,"
             << "tau_cmd_x,tau_cmd_y,tau_cmd_z,"
             << "att_error_deg\n";

        file << std::setprecision(8) << std::fixed;

        for (const auto& r : records_) {
            file << r.time << ","
                 << r.qTrue.w << "," << r.qTrue.x << "," << r.qTrue.y << "," << r.qTrue.z << ","
                 << r.qEst.w << "," << r.qEst.x << "," << r.qEst.y << "," << r.qEst.z << ","
                 << r.omegaTrue.x << "," << r.omegaTrue.y << "," << r.omegaTrue.z << ","
                 << r.omegaEst.x << "," << r.omegaEst.y << "," << r.omegaEst.z << ","
                 << r.biasTrue.x << "," << r.biasTrue.y << "," << r.biasTrue.z << ","
                 << r.biasEst.x << "," << r.biasEst.y << "," << r.biasEst.z << ","
                 << r.att3Sigma.x << "," << r.att3Sigma.y << "," << r.att3Sigma.z << ","
                 << r.bias3Sigma.x << "," << r.bias3Sigma.y << "," << r.bias3Sigma.z << ","
                 << r.hWheel.x << "," << r.hWheel.y << "," << r.hWheel.z << ","
                 << r.tauCmd.x << "," << r.tauCmd.y << "," << r.tauCmd.z << ","
                 << r.attitudeErrorAngleDeg << "\n";
        }

        file.flush();
        return static_cast<bool>(file);
    }

    [[nodiscard]] const std::vector<TelemetryRecord>& records() const { return records_; }

private:
    std::vector<TelemetryRecord> records_;
};

} // namespace aero::adcs

#pragma once

#include "common/math_types.hpp"
#include "adcs/quaternion.hpp"

namespace aero::adcs {

/**
 * @brief Multiplicative Extended Kalman Filter (MEKF) Configuration.
 */
struct MEKFConfig {
    double sigmaGyroWhiteNoise{0.0005}; // Gyro ARW [rad/s / sqrt(Hz)]
    double sigmaGyroBiasDrift{0.00005};  // Gyro RRW [rad/s^2 / sqrt(Hz)]
    double initAttitudeStdDev{0.1};      // Initial attitude uncertainty [rad] (~5.7 deg)
    double initBiasStdDev{0.05};         // Initial gyro bias uncertainty [rad/s]
};

/**
 * @brief Multiplicative Extended Kalman Filter for Spacecraft Attitude Estimation.
 * Estimates attitude quaternion (unit 4-vector) and 3-axis rate gyro bias.
 * Parameterizes attitude error as a 3-DOF rotation vector delta_theta to eliminate covariance singularities.
 */
class MEKF {
public:
    explicit MEKF(const MEKFConfig& config = {})
        : config_(config) 
    {
        reset();
    }

    void reset() {
        q_ = Quaternion::identity();
        bias_ = math::Vec3::zero();

        // Initialize 6x6 covariance matrix P
        P_ = math::Mat6::identity();
        const double varAtt = config_.initAttitudeStdDev * config_.initAttitudeStdDev;
        const double varBias = config_.initBiasStdDev * config_.initBiasStdDev;

        for (size_t i = 0; i < 3; ++i) {
            P_[i][i] = varAtt;
            P_[i + 3][i + 3] = varBias;
        }
    }

    /**
     * @brief State & Covariance Time Propagation Step.
     * @param omegaMeas Raw angular rate from gyroscope [rad/s]
     * @param dt Time step [s]
     */
    void predict(const math::Vec3& omegaMeas, double dt) {
        // Unbiased angular velocity estimate
        const math::Vec3 omegaHat = omegaMeas - bias_;

        // 1. Propagate nominal quaternion
        q_ = q_.propagate(omegaHat, dt);

        // 2. Compute 6x6 state transition matrix Phi
        const math::Mat3 I3 = math::Mat3::identity();
        const math::Mat3 wSkew = math::Mat3::skew(omegaHat);
        const math::Mat3 wSkew2 = wSkew * wSkew;

        // Phi_00 = I - [w x]*dt + 0.5*[w x]^2*dt^2
        const math::Mat3 phi00 = I3 - wSkew * dt + wSkew2 * (0.5 * dt * dt);
        // Phi_01 = -I*dt + 0.5*[w x]*dt^2
        const math::Mat3 phi01 = -I3 * dt + wSkew * (0.5 * dt * dt);
        const math::Mat3 phi10 = math::Mat3::diag(0.0, 0.0, 0.0);
        const math::Mat3 phi11 = I3;

        const math::Mat6 Phi = math::Mat6::fromBlocks(phi00, phi01, phi10, phi11);

        // 3. Discrete process noise covariance Qd
        const double sV2 = config_.sigmaGyroWhiteNoise * config_.sigmaGyroWhiteNoise;
        const double sU2 = config_.sigmaGyroBiasDrift * config_.sigmaGyroBiasDrift;

        const double q00 = sV2 * dt + (1.0 / 3.0) * sU2 * dt * dt * dt;
        const double q01 = -0.5 * sU2 * dt * dt;
        const double q11 = sU2 * dt;

        math::Mat6 Qd;
        for (size_t i = 0; i < 3; ++i) {
            Qd[i][i]         = q00;
            Qd[i][i + 3]     = q01;
            Qd[i + 3][i]     = q01;
            Qd[i + 3][i + 3] = q11;
        }

        // P = Phi * P * Phi^T + Qd
        P_ = Phi * P_ * Phi.transpose() + Qd;
        P_.makeSymmetric();
    }

    /**
     * @brief Vector Measurement Update Step (Star Tracker, Sun Sensor, Magnetometer).
     * @param yMeas Measured unit vector in Body frame
     * @param rInertial Known reference unit vector in Inertial frame
     * @param noiseStdDev Measurement noise standard deviation [rad]
     */
    void updateVector(
        const math::Vec3& yMeas,
        const math::Vec3& rInertial,
        double noiseStdDev) 
    {
        // Predicted measurement in Body frame
        const math::Vec3 yHat = q_.rotate(rInertial);

        // Innovation / Residual: dy = yMeas - yHat
        const math::Vec3 dy = yMeas - yHat;

        // Measurement sensitivity matrix H (3x6): H = [ [yHat x], 0_{3x3} ]
        const math::Mat3 ySkew = math::Mat3::skew(yHat);

        // Compute P * H^T (6x3 matrix)
        // Since H = [ ySkew, 0 ], H^T = [ ySkew^T; 0 ] = [ -ySkew; 0 ]
        std::array<std::array<double, 3>, 6> P_HT{{{0}}};
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = 0; c < 3; ++c) {
                double sum = 0.0;
                for (size_t k = 0; k < 3; ++k) {
                    sum += P_[r][k] * ySkew[c][k]; // -ySkew[k][c] = ySkew[c][k]
                }
                P_HT[r][c] = sum;
            }
        }

        // Innovation covariance S (3x3): S = H * (P * H^T) + R
        const double rVar = noiseStdDev * noiseStdDev;
        math::Mat3 S;
        for (size_t r = 0; r < 3; ++r) {
            for (size_t c = 0; c < 3; ++c) {
                double sum = 0.0;
                for (size_t k = 0; k < 3; ++k) {
                    sum += ySkew[r][k] * P_HT[k][c];
                }
                if (r == c) sum += rVar;
                S[r][c] = sum;
            }
        }

        const math::Mat3 S_inv = S.inverse();

        // Kalman Gain K (6x3): K = (P * H^T) * S_inv
        std::array<std::array<double, 3>, 6> K{{{0}}};
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = 0; c < 3; ++c) {
                double sum = 0.0;
                for (size_t k = 0; k < 3; ++k) {
                    sum += P_HT[r][k] * S_inv[k][c];
                }
                K[r][c] = sum;
            }
        }

        // Error state correction: delta_x = K * dy (6x1)
        math::Vec6 deltaX;
        for (size_t r = 0; r < 6; ++r) {
            double sum = 0.0;
            for (size_t c = 0; c < 3; ++c) {
                sum += K[r][c] * dy[c];
            }
            deltaX[r] = sum;
        }

        const math::Vec3 deltaTheta = deltaX.top3();
        const math::Vec3 deltaBeta  = deltaX.bottom3();

        // Update nominal quaternion using multiplicative error
        const Quaternion deltaQ = Quaternion::fromSmallAngle(deltaTheta);
        // H uses a body-frame attitude error, so inject on the right.
        q_ = (q_ * deltaQ).normalized();

        // Update gyro bias estimate
        bias_ += deltaBeta;

        // Joseph stabilized covariance update: P = (I - KH)*P*(I - KH)^T + K*R*K^T
        // Compute KH (6x6)
        math::Mat6 KH;
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = 0; c < 3; ++c) {
                double sum = 0.0;
                for (size_t k = 0; k < 3; ++k) {
                    sum += K[r][k] * ySkew[k][c];
                }
                KH[r][c] = sum;
            }
            // Columns 3..5 of KH are 0 because H[:, 3..5] is 0
        }

        const math::Mat6 I_KH = math::Mat6::identity() - KH;

        // Compute KRK^T (6x6) = rVar * K * K^T
        math::Mat6 KRK;
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = 0; c < 6; ++c) {
                double sum = 0.0;
                for (size_t k = 0; k < 3; ++k) {
                    sum += K[r][k] * K[c][k];
                }
                KRK[r][c] = sum * rVar;
            }
        }

        P_ = I_KH * P_ * I_KH.transpose() + KRK;
        P_.makeSymmetric();
    }

    [[nodiscard]] const Quaternion& attitude() const { return q_; }
    [[nodiscard]] const math::Vec3& gyroBias() const { return bias_; }
    [[nodiscard]] const math::Mat6& covariance() const { return P_; }

    /**
     * @brief Returns 3-sigma attitude uncertainty envelope in radians [x, y, z].
     */
    [[nodiscard]] math::Vec3 attitude3Sigma() const {
        return {
            3.0 * std::sqrt(std::max(0.0, P_[0][0])),
            3.0 * std::sqrt(std::max(0.0, P_[1][1])),
            3.0 * std::sqrt(std::max(0.0, P_[2][2]))
        };
    }

    /**
     * @brief Returns 3-sigma gyro bias uncertainty envelope in rad/s [x, y, z].
     */
    [[nodiscard]] math::Vec3 bias3Sigma() const {
        return {
            3.0 * std::sqrt(std::max(0.0, P_[3][3])),
            3.0 * std::sqrt(std::max(0.0, P_[4][4])),
            3.0 * std::sqrt(std::max(0.0, P_[5][5]))
        };
    }

private:
    MEKFConfig config_;
    Quaternion q_{Quaternion::identity()};
    math::Vec3 bias_{math::Vec3::zero()};
    math::Mat6 P_{math::Mat6::identity()};
};

} // namespace aero::adcs

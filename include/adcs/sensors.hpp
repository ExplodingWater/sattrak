#pragma once

#include "common/math_types.hpp"
#include "adcs/quaternion.hpp"
#include <random>

namespace aero::adcs {

/**
 * @brief Rate Gyroscope noise characteristics.
 */
struct GyroParameters {
    double noiseDensity{0.0005};       // Angle Random Walk (ARW) [rad/s / sqrt(Hz)]
    double biasInstability{0.00005};    // Rate Random Walk (RRW) [rad/s^2 / sqrt(Hz)]
    math::Vec3 initialBias{0.015, -0.02, 0.01}; // Initial constant turn-on bias [rad/s]
};

/**
 * @brief Vector sensor parameters (Star Tracker / Sun Sensor / Magnetometer).
 */
struct VectorSensorParameters {
    double noiseStdDev{0.001}; // Measurement noise standard deviation [rad]
};

/**
 * @brief Realistic Aerospace Sensor Corrupter.
 */
class SensorSuite {
public:
    explicit SensorSuite(
        GyroParameters gyroParams = {},
        VectorSensorParameters starTrackerParams = {0.0005}, // High-accuracy star tracker (~100 arcsec)
        VectorSensorParameters magParams = {0.01},          // Magnetometer (~0.57 deg)
        uint32_t seed = 42)
        : gyroParams_(gyroParams),
          starTrackerParams_(starTrackerParams),
          magParams_(magParams),
          rng_(seed),
          normalDist_(0.0, 1.0),
          currentBias_(gyroParams.initialBias) {}

    /**
     * @brief Corrupts true angular rate with bias drift and high-frequency white noise.
     */
    math::Vec3 measureAngularRate(const math::Vec3& trueOmega, double dt) {
        // Rate Random Walk: bias drift
        const double biasStd = gyroParams_.biasInstability * std::sqrt(dt);
        currentBias_.x += sampleGaussian() * biasStd;
        currentBias_.y += sampleGaussian() * biasStd;
        currentBias_.z += sampleGaussian() * biasStd;

        // Angle Random Walk: measurement white noise
        const double noiseStd = gyroParams_.noiseDensity / std::sqrt(dt);
        math::Vec3 noise{
            sampleGaussian() * noiseStd,
            sampleGaussian() * noiseStd,
            sampleGaussian() * noiseStd
        };

        return trueOmega + currentBias_ + noise;
    }

    /**
     * @brief Simulates a directional vector measurement (e.g. Star Tracker / Sun / Mag).
     * @param trueQ True spacecraft attitude
     * @param rInertial Known unit direction vector in Inertial frame
     * @param params Sensor noise parameters
     */
    math::Vec3 measureVector(
        const Quaternion& trueQ,
        const math::Vec3& rInertial,
        const VectorSensorParameters& params) 
    {
        const math::Vec3 rBody = trueQ.rotate(rInertial);
        const math::Vec3 noise{
            sampleGaussian() * params.noiseStdDev,
            sampleGaussian() * params.noiseStdDev,
            sampleGaussian() * params.noiseStdDev
        };
        return (rBody + noise).normalized();
    }

    math::Vec3 measureStarTracker(const Quaternion& trueQ, const math::Vec3& starInertial) {
        return measureVector(trueQ, starInertial, starTrackerParams_);
    }

    math::Vec3 measureMagnetometer(const Quaternion& trueQ, const math::Vec3& bInertial) {
        return measureVector(trueQ, bInertial, magParams_);
    }

    [[nodiscard]] const math::Vec3& trueGyroBias() const { return currentBias_; }
    void setGyroBias(const math::Vec3& bias) { currentBias_ = bias; }

private:
    double sampleGaussian() {
        return normalDist_(rng_);
    }

    GyroParameters gyroParams_;
    VectorSensorParameters starTrackerParams_;
    VectorSensorParameters magParams_;
    std::mt19937 rng_;
    std::normal_distribution<double> normalDist_;
    math::Vec3 currentBias_;
};

} // namespace aero::adcs

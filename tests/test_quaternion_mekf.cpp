#include "common/math_types.hpp"
#include "adcs/quaternion.hpp"
#include "adcs/satellite_dynamics.hpp"
#include "adcs/sensors.hpp"
#include "adcs/mekf.hpp"
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

int testQuaternionProperties() {
    std::cout << "[Test 1] Quaternion Algebra & Rotation Matrix Orthogonality...";
    const aero::adcs::Quaternion q = aero::adcs::Quaternion::fromRotationVector(
        aero::math::Vec3{0.3, -0.4, 0.5});

    ASSERT_NEAR(q.norm(), 1.0, 1e-12);

    const aero::math::Mat3 A = q.toRotationMatrix();
    const aero::math::Mat3 A_AT = A * A.transpose();

    // Verify A * A^T = I
    for (size_t r = 0; r < 3; ++r) {
        for (size_t c = 0; c < 3; ++c) {
            const double expected = (r == c) ? 1.0 : 0.0;
            ASSERT_NEAR(A_AT[r][c], expected, 1e-12);
        }
    }

    ASSERT_NEAR(A.determinant(), 1.0, 1e-12);
    std::cout << " PASSED\n";
    return 0;
}

int testFrameInvariance() {
    std::cout << "[Test 2] Coordinate Frame Invariance (Vector Norm & Cross-Product)...";
    const aero::adcs::Quaternion q = aero::adcs::Quaternion::fromRotationVector(
        aero::math::Vec3{-0.6, 0.8, 0.2});

    const aero::math::Vec3 u{1.5, -2.3, 4.1};
    const aero::math::Vec3 v{-0.7, 3.2, 1.8};

    // Vector length must be preserved under rotation
    const aero::math::Vec3 uRot = q.rotate(u);
    const aero::math::Vec3 vRot = q.rotate(v);
    ASSERT_NEAR(uRot.norm(), u.norm(), 1e-12);
    ASSERT_NEAR(vRot.norm(), v.norm(), 1e-12);

    // Cross product must transform covariantly: rot(u x v) == rot(u) x rot(v)
    const aero::math::Vec3 crossRot = q.rotate(u.cross(v));
    const aero::math::Vec3 rotCross = uRot.cross(vRot);
    ASSERT_NEAR(crossRot.x, rotCross.x, 1e-12);
    ASSERT_NEAR(crossRot.y, rotCross.y, 1e-12);
    ASSERT_NEAR(crossRot.z, rotCross.z, 1e-12);

    std::cout << " PASSED\n";
    return 0;
}

int testAngularMomentumConservation() {
    std::cout << "[Test 3] Rigid-Body Angular Momentum Conservation...";
    aero::adcs::SpacecraftParameters params;
    // Asymmetric inertia tensor to test gyroscopic coupling
    params.inertia = aero::math::Mat3::diag(0.05, 0.03, 0.02);
    params.inertiaInv = params.inertia.inverse();

    aero::adcs::SatelliteDynamics dynamics(params);
    aero::adcs::SatelliteState state;
    state.omega = aero::math::Vec3{0.2, 0.15, -0.25};

    const aero::math::Vec3 h0_body = params.inertia * state.omega;
    const aero::math::Vec3 h0_inertial = state.q.rotateInverse(h0_body);
    const double initialHMag = h0_inertial.norm();

    // Propagate for 500 steps with zero external torque
    const double dt = 0.01;
    for (int step = 0; step < 500; ++step) {
        state = dynamics.step(state, aero::math::Vec3::zero(), aero::math::Vec3::zero(), dt);
    }

    const aero::math::Vec3 hEnd_body = params.inertia * state.omega;
    const aero::math::Vec3 hEnd_inertial = state.q.rotateInverse(hEnd_body);
    const double finalHMag = hEnd_inertial.norm();

    // Inertial angular momentum vector must be strictly conserved
    ASSERT_NEAR(hEnd_inertial.x, h0_inertial.x, 1e-6);
    ASSERT_NEAR(hEnd_inertial.y, h0_inertial.y, 1e-6);
    ASSERT_NEAR(hEnd_inertial.z, h0_inertial.z, 1e-6);
    ASSERT_NEAR(finalHMag, initialHMag, 1e-7);

    std::cout << " PASSED\n";
    return 0;
}

int testMEKFCovarianceSymmetry() {
    std::cout << "[Test 4] MEKF Covariance Symmetry and Positive-Definiteness...";
    aero::adcs::MEKF estimator;
    const aero::math::Vec3 refVec{0.0, 0.0, 1.0};
    const double dt = 0.02;

    for (int step = 0; step < 300; ++step) {
        estimator.predict(aero::math::Vec3{0.05, -0.02, 0.03}, dt);

        if (step % 5 == 0) {
            const aero::math::Vec3 yMeas = estimator.attitude().rotate(refVec);
            estimator.updateVector(yMeas, refVec, 0.001);
        }

        const auto& P = estimator.covariance();
        // Check symmetry P == P^T
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = 0; c < 6; ++c) {
                ASSERT_NEAR(P[r][c], P[c][r], 1e-14);
            }
            // Check positive diagonal variance
            ASSERT_TRUE(P[r][r] > 0.0);
        }
        double L[6][6]{};
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = 0; c <= r; ++c) {
                double value = P[r][c];
                for (size_t k = 0; k < c; ++k) value -= L[r][k] * L[c][k];
                if (r == c) {
                    ASSERT_TRUE(std::isfinite(value) && value > 0.0);
                    L[r][c] = std::sqrt(value);
                } else L[r][c] = value / L[c][c];
            }
        }
    }

    std::cout << " PASSED\n";
    return 0;
}

int testMEKFBiasConvergence() {
    std::cout << "[Test 5] MEKF Gyro Bias Estimation Convergence Benchmark...";
    aero::adcs::GyroParameters gyroParams;
    gyroParams.initialBias = aero::math::Vec3{0.02, -0.015, 0.03};
    gyroParams.noiseDensity = 0.0005;
    gyroParams.biasInstability = 0.00001;

    aero::adcs::SensorSuite sensors(gyroParams);
    aero::adcs::MEKF estimator;

    const aero::math::Vec3 trueOmega{0.02, 0.01, -0.015};
    aero::adcs::Quaternion trueQ = aero::adcs::Quaternion::identity();
    aero::adcs::SpacecraftParameters params;
    params.inertia = aero::math::Mat3::diag(0.04, 0.04, 0.04);
    params.inertiaInv = params.inertia.inverse();
    aero::adcs::SatelliteDynamics dynamics(params);
    aero::adcs::SatelliteState truth;
    truth.q = aero::adcs::Quaternion::fromRotationVector({0.3, -0.2, 0.1});
    truth.omega = trueOmega;

    const aero::math::Vec3 star1{1.0, 0.0, 0.0};
    const aero::math::Vec3 star2{0.0, 1.0, 0.0};
    const aero::math::Vec3 magRef{0.0, 0.0, 1.0};

    const double dt = 0.01;
    for (int step = 0; step < 3000; ++step) {
        truth = dynamics.step(truth, {}, {}, dt);
        trueQ = truth.q;
        const aero::math::Vec3 omegaMeas = sensors.measureAngularRate(trueOmega, dt);

        estimator.predict(omegaMeas, dt);

        if (step % 10 == 0) {
            estimator.updateVector(sensors.measureStarTracker(trueQ, star1), star1, 0.0005);
            estimator.updateVector(sensors.measureStarTracker(trueQ, star2), star2, 0.0005);
        }
        if (step % 5 == 0) {
            estimator.updateVector(sensors.measureMagnetometer(trueQ, magRef), magRef, 0.01);
        }
    }

    const aero::math::Vec3 estBias = estimator.gyroBias();
    const aero::math::Vec3 trueBias = sensors.trueGyroBias();
    const double biasError = (estBias - trueBias).norm();

    std::cout << "\n    Initial Bias: [" << gyroParams.initialBias.x << ", " << gyroParams.initialBias.y << ", " << gyroParams.initialBias.z << "] rad/s\n";
    std::cout << "    Estimated   : [" << estBias.x << ", " << estBias.y << ", " << estBias.z << "] rad/s\n";
    std::cout << "    Error Norm  : " << biasError << " rad/s";

    // Bias error must converge to less than 0.002 rad/s (< 5% residual)
    ASSERT_TRUE(biasError < 0.002);
    std::cout << " -> PASSED\n";
    return 0;
}

int main() {
    std::cout << "\n=========================================\n"
              << "   RUNNING ADCS MEKF UNIT TEST SUITE    \n"
              << "=========================================\n" << std::flush;

    try {
        std::cout << "Starting Test 1..." << std::endl;
        if (testQuaternionProperties()) return 1;

        std::cout << "Starting Test 2..." << std::endl;
        if (testFrameInvariance()) return 1;

        std::cout << "Starting Test 3..." << std::endl;
        if (testAngularMomentumConservation()) return 1;

        std::cout << "Starting Test 4..." << std::endl;
        if (testMEKFCovarianceSymmetry()) return 1;

        std::cout << "Starting Test 5..." << std::endl;
        if (testMEKFBiasConvergence()) return 1;

        std::cout << "\n>>> ALL ADCS MEKF TESTS PASSED SUCCESSFULLY! <<<\n\n" << std::flush;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION CAUGHT: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION CAUGHT" << std::endl;
        return 1;
    }
    return 0;
}

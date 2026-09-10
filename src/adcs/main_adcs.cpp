#include "adcs/satellite_dynamics.hpp"
#include "adcs/sensors.hpp"
#include "adcs/mekf.hpp"
#include "adcs/controller.hpp"
#include "adcs/telemetry.hpp"
#include "common/file_utils.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "====================================================================\n";
    std::cout << "       Satellite Attitude Determination & Control Simulation        \n";
    std::cout << "          Multiplicative Extended Kalman Filter (MEKF) GNC          \n";
    std::cout << "====================================================================\n\n";

    // 1. Mission Configuration
    constexpr double simDuration = 60.0;    // Total simulation time [s]
    constexpr double dt = 0.01;            // Simulation time step (100 Hz) [s]
    const size_t totalSteps = static_cast<size_t>(simDuration / dt);

    // Initial Spacecraft State: Tumbling CubeSat
    aero::adcs::SatelliteDynamics dynamics;
    aero::adcs::SatelliteState state;
    // Initial random tumble: ~8.6 deg/s
    state.omega = aero::math::Vec3{0.08, -0.10, 0.06};
    // Initial orientation: 45 degree yaw offset
    state.q = aero::adcs::Quaternion::fromRotationVector(aero::math::Vec3{0.2, 0.3, 0.5});

    // 2. Sensors & Noise Model
    aero::adcs::GyroParameters gyroParams;
    gyroParams.initialBias = aero::math::Vec3{0.025, -0.018, 0.012}; // ~1 deg/s bias
    gyroParams.noiseDensity = 0.0008;
    gyroParams.biasInstability = 0.00002;

    aero::adcs::SensorSuite sensors(gyroParams);

    // Inertial Reference Vectors (e.g. Polaris star, Sun, Earth B-field)
    const aero::math::Vec3 starRef1 = aero::math::Vec3{1.0, 0.0, 0.0}.normalized();
    const aero::math::Vec3 starRef2 = aero::math::Vec3{0.0, 1.0, 0.0}.normalized();
    const aero::math::Vec3 bFieldRef = aero::math::Vec3{0.2, 0.1, 0.9}.normalized();

    // 3. Multiplicative Extended Kalman Filter
    aero::adcs::MEKFConfig mekfConfig;
    mekfConfig.sigmaGyroWhiteNoise = gyroParams.noiseDensity;
    mekfConfig.sigmaGyroBiasDrift = gyroParams.biasInstability;
    mekfConfig.initAttitudeStdDev = 0.2; // 11 deg initial uncertainty
    mekfConfig.initBiasStdDev = 0.05;

    aero::adcs::MEKF estimator(mekfConfig);

    // 4. Closed-Loop Attitude Controller
    aero::adcs::ControllerConfig ctrlConfig;
    ctrlConfig.Kp = 0.04;
    ctrlConfig.Kd = 0.08;
    ctrlConfig.maxTorque = 0.005; // 5 mN*m reaction wheel limit
    aero::adcs::AttitudeController controller(ctrlConfig);

    // Target pointing attitude: Identity orientation (ECI aligned)
    const aero::adcs::Quaternion targetAttitude = aero::adcs::Quaternion::identity();

    // 5. Telemetry Logger
    aero::adcs::TelemetryLogger logger;

    std::cout << "Simulating closed-loop 3-axis pointing & bias estimation for "
              << simDuration << " seconds...\n";
    std::cout << "True Gyro Bias: ["
              << gyroParams.initialBias.x << ", "
              << gyroParams.initialBias.y << ", "
              << gyroParams.initialBias.z << "] rad/s\n\n";

    // 6. Simulation Loop
    for (size_t step = 0; step < totalSteps; ++step) {
        const double t = step * dt;

        // --- SENSORS & CORRUPTION ---
        const aero::math::Vec3 omegaMeas = sensors.measureAngularRate(state.omega, dt);

        // --- ESTIMATION: MEKF PREDICT ---
        estimator.predict(omegaMeas, dt);

        // --- ESTIMATION: MEKF UPDATE (Star Tracker at 10 Hz, Magnetometer at 20 Hz) ---
        if (step % 10 == 0) {
            const aero::math::Vec3 yStar1 = sensors.measureStarTracker(state.q, starRef1);
            estimator.updateVector(yStar1, starRef1, 0.0005);

            const aero::math::Vec3 yStar2 = sensors.measureStarTracker(state.q, starRef2);
            estimator.updateVector(yStar2, starRef2, 0.0005);
        }

        if (step % 5 == 0) {
            const aero::math::Vec3 yMag = sensors.measureMagnetometer(state.q, bFieldRef);
            estimator.updateVector(yMag, bFieldRef, 0.008);
        }

        const aero::adcs::Quaternion qEst = estimator.attitude();
        const aero::math::Vec3 omegaEst = omegaMeas - estimator.gyroBias();

        // --- ATTITUDE CONTROL ---
        aero::math::Vec3 tauCmd = aero::math::Vec3::zero();
        if (t > 1.0) { // Allow MEKF 1s initial convergence before activating torque
            tauCmd = controller.computeControlTorque(qEst, omegaEst, targetAttitude);
        }

        // --- ENVIRONMENTAL DISTURBANCES ---
        // Gravity gradient torque at 500 km LEO (r = 6878 km)
        const aero::math::Vec3 rNadirBody = state.q.rotate(aero::math::Vec3{0.0, 0.0, -1.0});
        const aero::math::Vec3 tauDist = dynamics.computeGravityGradient(6878e3, rNadirBody);

        // --- SATELLITE DYNAMICS INTEGRATION (RK4) ---
        state = dynamics.step(state, tauDist, tauCmd, dt);

        // --- TELEMETRY RECORDING ---
        const aero::adcs::Quaternion qErr = targetAttitude.inverse() * state.q;
        const double errorAngleDeg = 2.0 * std::acos(std::clamp(std::abs(qErr.w), 0.0, 1.0)) * 180.0 / aero::constants::PI;

        aero::adcs::TelemetryRecord rec;
        rec.time = t;
        rec.qTrue = state.q;
        rec.qEst = qEst;
        rec.omegaTrue = state.omega;
        rec.omegaEst = omegaEst;
        rec.biasTrue = sensors.trueGyroBias();
        rec.biasEst = estimator.gyroBias();
        rec.att3Sigma = estimator.attitude3Sigma();
        rec.bias3Sigma = estimator.bias3Sigma();
        rec.hWheel = state.hWheel;
        rec.tauCmd = tauCmd;
        rec.attitudeErrorAngleDeg = errorAngleDeg;
        logger.record(rec);

        if (step % 1000 == 0 || step == totalSteps - 1) {
            std::cout << "  t = " << std::setw(5) << std::fixed << std::setprecision(1) << t << " s | "
                      << "Att Error: " << std::setw(6) << std::setprecision(2) << errorAngleDeg << " deg | "
                      << "Omega: " << std::setw(7) << std::setprecision(4) << state.omega.norm() * 180.0 / aero::constants::PI << " deg/s | "
                      << "Bias Est Err: " << std::scientific << std::setprecision(2)
                      << (estimator.gyroBias() - sensors.trueGyroBias()).norm() << " rad/s\n";
        }
    }

    // 7. Output Verification Summary
    const auto& lastRec = logger.records().back();
    std::cout << "\n====================================================================\n";
    std::cout << "                  GNC Mission Performance Summary                   \n";
    std::cout << "====================================================================\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Final Attitude Pointing Error : " << lastRec.attitudeErrorAngleDeg << " deg\n";
    std::cout << "Final Spacecraft Angular Rate : " << lastRec.omegaTrue.norm() * 180.0 / aero::constants::PI << " deg/s\n";
    std::cout << "True Gyro Bias [x,y,z]        : ["
              << lastRec.biasTrue.x << ", " << lastRec.biasTrue.y << ", " << lastRec.biasTrue.z << "] rad/s\n";
    std::cout << "Estimated Gyro Bias [x,y,z]   : ["
              << lastRec.biasEst.x << ", " << lastRec.biasEst.y << ", " << lastRec.biasEst.z << "] rad/s\n";
    std::cout << "Residual Bias Estimation Error: "
              << (lastRec.biasEst - lastRec.biasTrue).norm() << " rad/s\n";
    std::cout << "Final 3-Sigma Attitude Bound  : "
              << lastRec.att3Sigma.norm() * 180.0 / aero::constants::PI << " deg\n";

    const std::string adcsDir = "data/adcs";
    aero::utils::ensureDirectoryExists(adcsDir);
    const std::string dateStr = aero::utils::getDateString();
    const std::string datedCsvFile = adcsDir + "/telemetry_adcs_" + dateStr + ".csv";

    if (logger.exportCSV(datedCsvFile)) {
        std::cout << "Dated telemetry recorded to: " << datedCsvFile << "\n";
    } else return 1;
    // Also save root copy for backward compatibility
    if (!logger.exportCSV("telemetry_adcs.csv")) return 1;

    std::cout << "====================================================================\n";
    return 0;
}

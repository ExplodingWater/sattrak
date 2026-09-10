#pragma once

#include "common/math_types.hpp"
#include <cmath>
#include <tuple>

namespace aero::adcs {

/**
 * @brief Unit Quaternion implementing Hamilton convention (Scalar-first: [q0, q1, q2, q3]).
 * Represents coordinate rotation from Inertial frame to Body frame: v_B = A(q) * v_I.
 */
struct Quaternion {
    double w{1.0}; // q0: scalar part
    double x{0.0}; // q1: vector part x
    double y{0.0}; // q2: vector part y
    double z{0.0}; // q3: vector part z

    constexpr Quaternion() = default;
    constexpr Quaternion(double w_, double x_, double y_, double z_)
        : w(w_), x(x_), y(y_), z(z_) {}

    constexpr Quaternion(double w_, const math::Vec3& v)
        : w(w_), x(v.x), y(v.y), z(v.z) {}

    [[nodiscard]] constexpr math::Vec3 vec() const { return {x, y, z}; }
    [[nodiscard]] constexpr double scalar() const { return w; }

    [[nodiscard]] double normSq() const { return w * w + x * x + y * y + z * z; }
    [[nodiscard]] double norm() const { return std::sqrt(normSq()); }

    [[nodiscard]] Quaternion normalized(double eps = 1e-14) const {
        const double n = norm();
        if (n > eps) {
            const double invN = 1.0 / n;
            return {w * invN, x * invN, y * invN, z * invN};
        }
        return {1.0, 0.0, 0.0, 0.0};
    }

    void normalize() {
        *this = normalized();
    }

    [[nodiscard]] constexpr Quaternion conjugate() const {
        return {w, -x, -y, -z};
    }

    [[nodiscard]] constexpr Quaternion inverse() const {
        return conjugate(); // For unit quaternion, inverse is conjugate
    }

    /**
     * @brief Hamilton quaternion multiplication (p * q).
     */
    constexpr Quaternion operator*(const Quaternion& q) const {
        return {
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w
        };
    }

    constexpr Quaternion operator+(const Quaternion& o) const {
        return {w + o.w, x + o.x, y + o.y, z + o.z};
    }

    constexpr Quaternion operator*(double s) const {
        return {w * s, x * s, y * s, z * s};
    }

    /**
     * @brief Direction Cosine Matrix (DCM) / Rotation matrix A(q) mapping Inertial to Body:
     * v_B = A(q) * v_I
     */
    [[nodiscard]] math::Mat3 toRotationMatrix() const {
        const double w2 = w * w, x2 = x * x, y2 = y * y, z2 = z * z;
        return {
            w2 + x2 - y2 - z2,     2.0 * (x * y + w * z), 2.0 * (x * z - w * y),
            2.0 * (x * y - w * z), w2 - x2 + y2 - z2,     2.0 * (y * z + w * x),
            2.0 * (x * z + w * y), 2.0 * (y * z - w * x), w2 - x2 - y2 + z2
        };
    }

    /**
     * @brief Rotates a 3D vector from Inertial to Body frame: v_B = A(q) * v_I.
     */
    [[nodiscard]] math::Vec3 rotate(const math::Vec3& v) const {
        return toRotationMatrix() * v;
    }

    /**
     * @brief Rotates a 3D vector from Body to Inertial frame (transpose / inverse).
     */
    [[nodiscard]] math::Vec3 rotateInverse(const math::Vec3& v) const {
        return toRotationMatrix().transpose() * v;
    }

    /**
     * @brief Construct quaternion from rotation vector (angle * axis).
     */
    static Quaternion fromRotationVector(const math::Vec3& rotVec) {
        const double theta = rotVec.norm();
        if (theta < 1e-12) {
            return {1.0 - 0.125 * theta * theta, 0.5 * rotVec};
        }
        const double halfTheta = 0.5 * theta;
        const double s = std::sin(halfTheta) / theta;
        return {std::cos(halfTheta), rotVec * s};
    }

    /**
     * @brief Small-angle rotation error vector to delta quaternion.
     * delta_q = [1, 0.5 * delta_theta]^T (normalized).
     */
    static Quaternion fromSmallAngle(const math::Vec3& deltaTheta) {
        return Quaternion(1.0, 0.5 * deltaTheta).normalized();
    }

    /**
     * @brief Closed-form kinematic propagation over time step dt with constant angular rate omega.
     */
    [[nodiscard]] Quaternion propagate(const math::Vec3& omega, double dt) const {
        const double omegaNorm = omega.norm();
        const double halfAngle = 0.5 * omegaNorm * dt;
        double s, c;
        if (omegaNorm * dt < 1e-8) {
            c = 1.0 - 0.5 * halfAngle * halfAngle;
            s = 0.5 * dt * (1.0 - (1.0 / 6.0) * halfAngle * halfAngle);
        } else {
            c = std::cos(halfAngle);
            s = std::sin(halfAngle) / omegaNorm;
        }
        const Quaternion deltaQ(c, omega * s);
        // Body-frame angular rate composes on the right (q_dot = q * omega / 2).
        return ((*this) * deltaQ).normalized();
    }

    /**
     * @brief Euler angles (Roll, Pitch, Yaw) in 3-2-1 (Z-Y-X) sequence in radians.
     * @return std::tuple<roll, pitch, yaw>
     */
    [[nodiscard]] std::tuple<double, double, double> toEulerAngles() const {
        const double roll = std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y));
        double sinp = 2.0 * (w * y - z * x);
        sinp = std::clamp(sinp, -1.0, 1.0);
        const double pitch = std::asin(sinp);
        const double yaw = std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
        return {roll, pitch, yaw};
    }

    static constexpr Quaternion identity() {
        return {1.0, 0.0, 0.0, 0.0};
    }
};

} // namespace aero::adcs

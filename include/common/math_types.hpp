#pragma once

#include <cmath>
#include <array>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace aero::math {

/**
 * @brief 3D Vector class with essential linear algebra operations.
 */
struct Vec3 {
    double x{0.0}, y{0.0}, z{0.0};

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    constexpr double& operator[](size_t i) {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    constexpr const double& operator[](size_t i) const {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(double s) { x /= s; y /= s; z /= s; return *this; }

    [[nodiscard]] constexpr double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

    [[nodiscard]] constexpr Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] double normSq() const { return dot(*this); }
    [[nodiscard]] double norm() const { return std::sqrt(normSq()); }

    [[nodiscard]] Vec3 normalized(double eps = 1e-15) const {
        const double n = norm();
        return (n > eps) ? (*this / n) : Vec3{0.0, 0.0, 0.0};
    }

    static constexpr Vec3 zero() { return {0.0, 0.0, 0.0}; }
    static constexpr Vec3 unitX() { return {1.0, 0.0, 0.0}; }
    static constexpr Vec3 unitY() { return {0.0, 1.0, 0.0}; }
    static constexpr Vec3 unitZ() { return {0.0, 0.0, 1.0}; }
};

inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

/**
 * @brief 3x3 Matrix class for rotations, inertia tensors, and sub-block transformations.
 */
struct Mat3 {
    std::array<std::array<double, 3>, 3> m{{{0,0,0}, {0,0,0}, {0,0,0}}};

    constexpr Mat3() = default;
    constexpr Mat3(double m00, double m01, double m02,
                   double m10, double m11, double m12,
                   double m20, double m21, double m22) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }

    std::array<double, 3>& operator[](size_t r) { return m[r]; }
    const std::array<double, 3>& operator[](size_t r) const { return m[r]; }

    constexpr Mat3 operator-() const {
        Mat3 res;
        for (size_t r = 0; r < 3; ++r)
            for (size_t c = 0; c < 3; ++c)
                res.m[r][c] = -m[r][c];
        return res;
    }

    constexpr Mat3 operator+(const Mat3& o) const {
        Mat3 res;
        for (size_t r = 0; r < 3; ++r)
            for (size_t c = 0; c < 3; ++c)
                res.m[r][c] = m[r][c] + o.m[r][c];
        return res;
    }

    constexpr Mat3 operator-(const Mat3& o) const {
        Mat3 res;
        for (size_t r = 0; r < 3; ++r)
            for (size_t c = 0; c < 3; ++c)
                res.m[r][c] = m[r][c] - o.m[r][c];
        return res;
    }

    constexpr Mat3 operator*(double s) const {
        Mat3 res;
        for (size_t r = 0; r < 3; ++r)
            for (size_t c = 0; c < 3; ++c)
                res.m[r][c] = m[r][c] * s;
        return res;
    }

    constexpr Vec3 operator*(const Vec3& v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    constexpr Mat3 operator*(const Mat3& o) const {
        Mat3 res;
        for (size_t r = 0; r < 3; ++r) {
            for (size_t c = 0; c < 3; ++c) {
                double sum = 0.0;
                for (size_t k = 0; k < 3; ++k) {
                    sum += m[r][k] * o.m[k][c];
                }
                res.m[r][c] = sum;
            }
        }
        return res;
    }

    [[nodiscard]] constexpr Mat3 transpose() const {
        return {
            m[0][0], m[1][0], m[2][0],
            m[0][1], m[1][1], m[2][1],
            m[0][2], m[1][2], m[2][2]
        };
    }

    [[nodiscard]] constexpr double determinant() const {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    [[nodiscard]] Mat3 inverse() const {
        const double det = determinant();
        if (std::abs(det) < 1e-30) {
            throw std::runtime_error("Mat3: Singular matrix cannot be inverted.");
        }
        const double invDet = 1.0 / det;
        return {
            (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet,
            (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet,
            (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet,

            (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet,
            (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet,
            (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet,

            (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet,
            (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet,
            (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet
        };
    }

    static constexpr Mat3 identity() {
        return {
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
        };
    }

    static constexpr Mat3 diag(double d0, double d1, double d2) {
        return {
            d0,  0.0, 0.0,
            0.0, d1,  0.0,
            0.0, 0.0, d2
        };
    }

    static constexpr Mat3 skew(const Vec3& v) {
        return {
             0.0, -v.z,  v.y,
             v.z,  0.0, -v.x,
            -v.y,  v.x,  0.0
        };
    }
};

/**
 * @brief 4D Vector class (used for quaternions and 2D Euler conserved variables).
 */
struct Vec4 {
    double v[4]{0.0, 0.0, 0.0, 0.0};

    constexpr Vec4() = default;
    constexpr Vec4(double v0, double v1, double v2, double v3) : v{v0, v1, v2, v3} {}

    constexpr double& operator[](size_t i) { return v[i]; }
    constexpr const double& operator[](size_t i) const { return v[i]; }

    constexpr Vec4 operator+(const Vec4& o) const {
        return {v[0] + o.v[0], v[1] + o.v[1], v[2] + o.v[2], v[3] + o.v[3]};
    }
    constexpr Vec4 operator-(const Vec4& o) const {
        return {v[0] - o.v[0], v[1] - o.v[1], v[2] - o.v[2], v[3] - o.v[3]};
    }
    constexpr Vec4 operator*(double s) const {
        return {v[0] * s, v[1] * s, v[2] * s, v[3] * s};
    }
    constexpr Vec4 operator/(double s) const {
        return {v[0] / s, v[1] / s, v[2] / s, v[3] / s};
    }

    Vec4& operator+=(const Vec4& o) {
        for (size_t i = 0; i < 4; ++i) v[i] += o.v[i];
        return *this;
    }
    Vec4& operator-=(const Vec4& o) {
        for (size_t i = 0; i < 4; ++i) v[i] -= o.v[i];
        return *this;
    }
    Vec4& operator*=(double s) {
        for (size_t i = 0; i < 4; ++i) v[i] *= s;
        return *this;
    }

    [[nodiscard]] constexpr double dot(const Vec4& o) const {
        return v[0] * o.v[0] + v[1] * o.v[1] + v[2] * o.v[2] + v[3] * o.v[3];
    }
    [[nodiscard]] double norm() const { return std::sqrt(dot(*this)); }

    static constexpr Vec4 zero() { return {0.0, 0.0, 0.0, 0.0}; }
};

inline Vec4 operator*(double s, const Vec4& vec) { return vec * s; }

/**
 * @brief 6D Vector class (used for MEKF state errors and 6-DOF states).
 */
struct Vec6 {
    std::array<double, 6> v{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    constexpr Vec6() = default;
    constexpr Vec6(double v0, double v1, double v2, double v3, double v4, double v5)
        : v{v0, v1, v2, v3, v4, v5} {}
    constexpr Vec6(const Vec3& top, const Vec3& bottom)
        : v{top.x, top.y, top.z, bottom.x, bottom.y, bottom.z} {}

    constexpr double& operator[](size_t i) { return v[i]; }
    constexpr const double& operator[](size_t i) const { return v[i]; }

    constexpr Vec6 operator+(const Vec6& o) const {
        Vec6 r;
        for (size_t i = 0; i < 6; ++i) r.v[i] = v[i] + o.v[i];
        return r;
    }
    constexpr Vec6 operator-(const Vec6& o) const {
        Vec6 r;
        for (size_t i = 0; i < 6; ++i) r.v[i] = v[i] - o.v[i];
        return r;
    }
    constexpr Vec6 operator*(double s) const {
        Vec6 r;
        for (size_t i = 0; i < 6; ++i) r.v[i] = v[i] * s;
        return r;
    }

    Vec6& operator+=(const Vec6& o) {
        for (size_t i = 0; i < 6; ++i) v[i] += o.v[i];
        return *this;
    }

    [[nodiscard]] Vec3 top3() const { return {v[0], v[1], v[2]}; }
    [[nodiscard]] Vec3 bottom3() const { return {v[3], v[4], v[5]}; }

    static constexpr Vec6 zero() { return {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}; }
};

inline Vec6 operator*(double s, const Vec6& vec) { return vec * s; }

/**
 * @brief 6x6 Matrix class for MEKF error covariance and transition matrices.
 */
struct Mat6 {
    std::array<std::array<double, 6>, 6> m{{{0}}};

    constexpr Mat6() = default;

    std::array<double, 6>& operator[](size_t r) { return m[r]; }
    const std::array<double, 6>& operator[](size_t r) const { return m[r]; }

    constexpr Mat6 operator-() const {
        Mat6 res;
        for (size_t r = 0; r < 6; ++r)
            for (size_t c = 0; c < 6; ++c)
                res.m[r][c] = -m[r][c];
        return res;
    }

    constexpr Mat6 operator+(const Mat6& o) const {
        Mat6 res;
        for (size_t r = 0; r < 6; ++r)
            for (size_t c = 0; c < 6; ++c)
                res.m[r][c] = m[r][c] + o.m[r][c];
        return res;
    }

    constexpr Mat6 operator-(const Mat6& o) const {
        Mat6 res;
        for (size_t r = 0; r < 6; ++r)
            for (size_t c = 0; c < 6; ++c)
                res.m[r][c] = m[r][c] - o.m[r][c];
        return res;
    }

    constexpr Mat6 operator*(double s) const {
        Mat6 res;
        for (size_t r = 0; r < 6; ++r)
            for (size_t c = 0; c < 6; ++c)
                res.m[r][c] = m[r][c] * s;
        return res;
    }

    constexpr Vec6 operator*(const Vec6& vec) const {
        Vec6 res;
        for (size_t r = 0; r < 6; ++r) {
            double sum = 0.0;
            for (size_t c = 0; c < 6; ++c) {
                sum += m[r][c] * vec[c];
            }
            res[r] = sum;
        }
        return res;
    }

    constexpr Mat6 operator*(const Mat6& o) const {
        Mat6 res;
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = 0; c < 6; ++c) {
                double sum = 0.0;
                for (size_t k = 0; k < 6; ++k) {
                    sum += m[r][k] * o.m[k][c];
                }
                res.m[r][c] = sum;
            }
        }
        return res;
    }

    [[nodiscard]] constexpr Mat6 transpose() const {
        Mat6 res;
        for (size_t r = 0; r < 6; ++r)
            for (size_t c = 0; c < 6; ++c)
                res.m[r][c] = m[c][r];
        return res;
    }

    void makeSymmetric() {
        for (size_t r = 0; r < 6; ++r) {
            for (size_t c = r + 1; c < 6; ++c) {
                const double avg = 0.5 * (m[r][c] + m[c][r]);
                m[r][c] = avg;
                m[c][r] = avg;
            }
        }
    }

    static constexpr Mat6 identity() {
        Mat6 res;
        for (size_t i = 0; i < 6; ++i) res.m[i][i] = 1.0;
        return res;
    }

    static Mat6 fromBlocks(const Mat3& A, const Mat3& B, const Mat3& C, const Mat3& D) {
        Mat6 res;
        for (size_t r = 0; r < 3; ++r) {
            for (size_t c = 0; c < 3; ++c) {
                res.m[r][c]         = A[r][c];
                res.m[r][c + 3]     = B[r][c];
                res.m[r + 3][c]     = C[r][c];
                res.m[r + 3][c + 3] = D[r][c];
            }
        }
        return res;
    }
};

} // namespace aero::math

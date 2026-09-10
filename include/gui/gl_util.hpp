#pragma once

#include <vector>
#include <string>
#include <cmath>
#include "glad/glad.h"
#include "common/math_types.hpp"
#include "adcs/quaternion.hpp"

namespace gui {

// 4x4 Matrix (Column-Major for OpenGL)
struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = 1.0f; r.m[5] = 1.0f; r.m[10] = 1.0f; r.m[15] = 1.0f;
        return r;
    }

    static Mat4 perspective(float fovYRad, float aspect, float nearZ, float farZ) {
        Mat4 r{};
        float tanHalfFov = std::tan(fovYRad * 0.5f);
        r.m[0] = 1.0f / (aspect * tanHalfFov);
        r.m[5] = 1.0f / tanHalfFov;
        r.m[10] = -(farZ + nearZ) / (farZ - nearZ);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
        return r;
    }

    static Mat4 lookAt(const aero::math::Vec3& eye, const aero::math::Vec3& target, const aero::math::Vec3& up) {
        aero::math::Vec3 f = (target - eye).normalized();
        aero::math::Vec3 s = f.cross(up).normalized();
        aero::math::Vec3 u = s.cross(f);

        Mat4 r = identity();
        r.m[0] = float(s[0]);
        r.m[4] = float(s[1]);
        r.m[8] = float(s[2]);

        r.m[1] = float(u[0]);
        r.m[5] = float(u[1]);
        r.m[9] = float(u[2]);

        r.m[2] = -float(f[0]);
        r.m[6] = -float(f[1]);
        r.m[10] = -float(f[2]);

        r.m[12] = -float(s.dot(eye));
        r.m[13] = -float(u.dot(eye));
        r.m[14] = float(f.dot(eye));
        return r;
    }

    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += a.m[k * 4 + row] * b.m[c * 4 + k];
                }
                r.m[c * 4 + row] = sum;
            }
        }
        return r;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[12] = x;
        r.m[13] = y;
        r.m[14] = z;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r = identity();
        r.m[0] = sx;
        r.m[5] = sy;
        r.m[10] = sz;
        return r;
    }

    static Mat4 rotateY(float angleRad) {
        Mat4 r = identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        r.m[0] = c;
        r.m[2] = -s;
        r.m[8] = s;
        r.m[10] = c;
        return r;
    }

    static Mat4 rotateX(float angleRad) {
        Mat4 r = identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        r.m[5] = c;
        r.m[6] = s;
        r.m[9] = -s;
        r.m[10] = c;
        return r;
    }

    static Mat4 rotateZ(float angleRad) {
        Mat4 r = identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        r.m[0] = c;
        r.m[1] = s;
        r.m[4] = -s;
        r.m[5] = c;
        return r;
    }

    static Mat4 fromQuaternion(const aero::adcs::Quaternion& q) {
        double q0 = q.w;
        double q1 = q.x;
        double q2 = q.y;
        double q3 = q.z;

        Mat4 r = identity();
        r.m[0] = float(1.0 - 2.0 * (q2*q2 + q3*q3));
        r.m[1] = float(2.0 * (q1*q2 + q0*q3));
        r.m[2] = float(2.0 * (q1*q3 - q0*q2));

        r.m[4] = float(2.0 * (q1*q2 - q0*q3));
        r.m[5] = float(1.0 - 2.0 * (q1*q1 + q3*q3));
        r.m[6] = float(2.0 * (q2*q3 + q0*q1));

        r.m[8] = float(2.0 * (q1*q3 + q0*q2));
        r.m[9] = float(2.0 * (q2*q3 - q0*q1));
        r.m[10] = float(1.0 - 2.0 * (q1*q1 + q2*q2));
        return r;
    }

    const float* data() const { return m; }
};

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct Mesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    int indexCount = 0;

    void init(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void draw() const;
    void cleanup();
};

struct ShaderProgram {
    unsigned int id = 0;

    bool loadFromSource(const char* vertexSrc, const char* fragmentSrc);
    void use() const;
    void setMat4(const char* name, const Mat4& mat) const;
    void setVec3(const char* name, float x, float y, float z) const;
    void setVec4(const char* name, float x, float y, float z, float w) const;
    void setInt(const char* name, int val) const;
    void setFloat(const char* name, float val) const;
    void cleanup();
};

Mesh createSphereMesh(float radius, int rings, int sectors);
Mesh createBoxMesh(float width, float height, float depth);
Mesh createCylinderMesh(float radiusBottom, float radiusTop, float height, int slices);
Mesh createQuadMesh();

unsigned int loadTextureFromFile(const std::string& path);
unsigned int createDynamicRGBATexture(int width, int height);
void updateDynamicRGBATexture(unsigned int texId, int width, int height, const unsigned char* data);

struct ColorRGB {
    unsigned char r, g, b;
};
ColorRGB sampleTurboColormap(float t);
ColorRGB sampleInfernoColormap(float t);
ColorRGB sampleCoolWarmColormap(float t);

} // namespace gui

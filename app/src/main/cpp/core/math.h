#pragma once

#include <algorithm>
#include <cmath>

namespace core {

constexpr float kPi = 3.14159265358979f;

inline float radians(float deg) { return deg * kPi / 180.0f; }

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
inline Vec3& operator+=(Vec3& a, Vec3 b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(Vec3 v) {
    const float len = length(v);
    return len > 1e-8f ? v * (1.0f / len) : Vec3{};
}
inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline Vec3 vmin(Vec3 a, Vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline Vec3 vmax(Vec3 a, Vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline float axisVal(const Vec3& v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }
inline float& axisRef(Vec3& v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

// Matriz 4x4 em ordem de colunas (como o OpenGL espera): m[coluna * 4 + linha].
struct Mat4 {
    float m[16];
};

inline Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

inline Mat4 multiply(const Mat4& a, const Mat4& b) {
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

inline Mat4 perspective(float fovyRadians, float aspect, float nearZ, float farZ) {
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    Mat4 r{};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

inline Mat4 translate(float x, float y, float z) {
    Mat4 r = identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

inline Mat4 scaleMatrix(Vec3 s) {
    Mat4 r = identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

inline Mat4 rotateX(float a) {
    Mat4 r = identity();
    r.m[5] = std::cos(a);
    r.m[6] = std::sin(a);
    r.m[9] = -std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

inline Mat4 rotateY(float a) {
    Mat4 r = identity();
    r.m[0] = std::cos(a);
    r.m[2] = -std::sin(a);
    r.m[8] = std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

inline Mat4 rotateZ(float a) {
    Mat4 r = identity();
    r.m[0] = std::cos(a);
    r.m[1] = std::sin(a);
    r.m[4] = -std::sin(a);
    r.m[5] = std::cos(a);
    return r;
}

inline Vec3 transformPoint(const Mat4& a, Vec3 p) {
    return {a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z + a.m[12],
            a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z + a.m[13],
            a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14]};
}

inline Vec3 transformDir(const Mat4& a, Vec3 d) {
    return {a.m[0] * d.x + a.m[4] * d.y + a.m[8] * d.z,
            a.m[1] * d.x + a.m[5] * d.y + a.m[9] * d.z,
            a.m[2] * d.x + a.m[6] * d.y + a.m[10] * d.z};
}

// Inversa de uma transformação afim (rotação/escala/translação). Devolve identidade se for singular.
inline Mat4 inverseAffine(const Mat4& a) {
    const float A00 = a.m[0], A10 = a.m[1], A20 = a.m[2];
    const float A01 = a.m[4], A11 = a.m[5], A21 = a.m[6];
    const float A02 = a.m[8], A12 = a.m[9], A22 = a.m[10];
    const float det = A00 * (A11 * A22 - A12 * A21) - A01 * (A10 * A22 - A12 * A20) +
                      A02 * (A10 * A21 - A11 * A20);
    if (std::fabs(det) < 1e-20f) return identity();
    const float inv = 1.0f / det;
    const float i00 = (A11 * A22 - A12 * A21) * inv;
    const float i01 = (A02 * A21 - A01 * A22) * inv;
    const float i02 = (A01 * A12 - A02 * A11) * inv;
    const float i10 = (A12 * A20 - A10 * A22) * inv;
    const float i11 = (A00 * A22 - A02 * A20) * inv;
    const float i12 = (A02 * A10 - A00 * A12) * inv;
    const float i20 = (A10 * A21 - A11 * A20) * inv;
    const float i21 = (A01 * A20 - A00 * A21) * inv;
    const float i22 = (A00 * A11 - A01 * A10) * inv;
    Mat4 r = identity();
    r.m[0] = i00; r.m[1] = i10; r.m[2] = i20;
    r.m[4] = i01; r.m[5] = i11; r.m[6] = i21;
    r.m[8] = i02; r.m[9] = i12; r.m[10] = i22;
    const float tx = a.m[12], ty = a.m[13], tz = a.m[14];
    r.m[12] = -(i00 * tx + i01 * ty + i02 * tz);
    r.m[13] = -(i10 * tx + i11 * ty + i12 * tz);
    r.m[14] = -(i20 * tx + i21 * ty + i22 * tz);
    return r;
}

}  // namespace core

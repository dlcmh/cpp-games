#pragma once

#include <cmath>

namespace planetlab {

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

inline Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 operator-(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(Vec3 v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}
inline float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 v) {
    return std::sqrt(dot(v, v));
}
inline Vec3 normalize(Vec3 v) {
    const float m = length(v);
    return m > 0.0f ? v * (1.0f / m) : Vec3{};
}

struct Mat4 {
    float m[16]{};
};

inline Mat4 identity() {
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k) {
                value += a.m[k * 4 + row] * b.m[column * 4 + k];
            }
            r.m[column * 4 + row] = value;
        }
    }
    return r;
}

inline Mat4 scaling(float s) {
    Mat4 r{};
    r.m[0] = s;
    r.m[5] = s;
    r.m[10] = s;
    r.m[15] = 1.0f;
    return r;
}

inline Mat4 rotationX(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 r = identity();
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

inline Mat4 rotationY(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 r = identity();
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

inline Mat4 rotationZ(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 r = identity();
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

inline Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 r{};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = farPlane / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = nearPlane * farPlane / (nearPlane - farPlane);
    return r;
}

inline Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 forward = normalize(center - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 correctedUp = cross(right, forward);
    Mat4 r = identity();
    r.m[0] = right.x;
    r.m[1] = correctedUp.x;
    r.m[2] = -forward.x;
    r.m[4] = right.y;
    r.m[5] = correctedUp.y;
    r.m[6] = -forward.y;
    r.m[8] = right.z;
    r.m[9] = correctedUp.z;
    r.m[10] = -forward.z;
    r.m[12] = -dot(right, eye);
    r.m[13] = -dot(correctedUp, eye);
    r.m[14] = dot(forward, eye);
    return r;
}

}  // namespace planetlab

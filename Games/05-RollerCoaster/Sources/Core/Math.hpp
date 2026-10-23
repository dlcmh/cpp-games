#pragma once

#include <cmath>

namespace rollercoaster {

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
inline Vec3 operator*(Vec3 a, float s) {
    return {a.x * s, a.y * s, a.z * s};
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
    const float n = length(v);
    return n > 0.0001f ? v * (1.0f / n) : Vec3{0.0f, 1.0f, 0.0f};
}

struct Mat4 {
    float m[16]{};
};

inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[column * 4 + k];
            }
            result.m[column * 4 + row] = sum;
        }
    }
    return result;
}

inline Mat4 perspective(float fovY, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 result{};
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = farPlane / (nearPlane - farPlane);
    result.m[11] = -1.0f;
    result.m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    return result;
}

inline Mat4 lookAt(Vec3 eye, Vec3 centre, Vec3 up) {
    const Vec3 forward = normalize(centre - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 correctedUp = cross(right, forward);
    Mat4 result{};
    result.m[0] = right.x;
    result.m[4] = right.y;
    result.m[8] = right.z;
    result.m[12] = -dot(right, eye);
    result.m[1] = correctedUp.x;
    result.m[5] = correctedUp.y;
    result.m[9] = correctedUp.z;
    result.m[13] = -dot(correctedUp, eye);
    result.m[2] = -forward.x;
    result.m[6] = -forward.y;
    result.m[10] = -forward.z;
    result.m[14] = dot(forward, eye);
    result.m[15] = 1.0f;
    return result;
}

}  // namespace rollercoaster

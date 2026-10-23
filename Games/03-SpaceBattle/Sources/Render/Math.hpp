#pragma once

#include <cmath>

namespace spacebattle {

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

inline Vec3 operator*(Vec3 v, float scalar) {
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}

inline Vec3 operator-(Vec3 v) {
    return {-v.x, -v.y, -v.z};
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
    const float magnitude = length(v);
    return magnitude > 0.0f ? v * (1.0f / magnitude) : Vec3{};
}

inline Vec3 mix(Vec3 a, Vec3 b, float t) {
    return a + (b - a) * t;
}

// Column-major 4x4 matrix, matching Slang/MSL's float4x4 memory layout.
struct Mat4 {
    float m[16]{};
};

inline Mat4 identity() {
    Mat4 result{};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k) {
                value += a.m[k * 4 + row] * b.m[column * 4 + k];
            }
            result.m[column * 4 + row] = value;
        }
    }
    return result;
}

inline Mat4 translation(Vec3 value) {
    Mat4 result = identity();
    result.m[12] = value.x;
    result.m[13] = value.y;
    result.m[14] = value.z;
    return result;
}

inline Mat4 scaling(Vec3 value) {
    Mat4 result{};
    result.m[0] = value.x;
    result.m[5] = value.y;
    result.m[10] = value.z;
    result.m[15] = 1.0f;
    return result;
}

inline Mat4 scaling(float uniformScale) {
    return scaling({uniformScale, uniformScale, uniformScale});
}

inline Mat4 rotationX(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result = identity();
    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;
    return result;
}

inline Mat4 rotationY(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result = identity();
    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;
    return result;
}

inline Mat4 rotationZ(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result = identity();
    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;
    return result;
}

inline Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 result{};
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = farPlane / (nearPlane - farPlane);
    result.m[11] = -1.0f;
    result.m[14] = nearPlane * farPlane / (nearPlane - farPlane);
    return result;
}

inline Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 forward = normalize(center - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 correctedUp = cross(right, forward);

    Mat4 result = identity();
    result.m[0] = right.x;
    result.m[1] = correctedUp.x;
    result.m[2] = -forward.x;
    result.m[4] = right.y;
    result.m[5] = correctedUp.y;
    result.m[6] = -forward.y;
    result.m[8] = right.z;
    result.m[9] = correctedUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -dot(right, eye);
    result.m[13] = -dot(correctedUp, eye);
    result.m[14] = dot(forward, eye);
    return result;
}

// A unit cube transformed into a camera-facing-ish beam between two points.
// Used for laser bolts without needing a separate line primitive.
inline Mat4 beamTransform(Vec3 from, Vec3 to, float width) {
    const Vec3 direction = normalize(to - from);
    const Vec3 referenceUp =
        std::abs(direction.y) > 0.95f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 right = normalize(cross(referenceUp, direction));
    const Vec3 up = cross(direction, right);
    const float beamLength = length(to - from);
    const Vec3 center = (from + to) * 0.5f;

    Mat4 result = identity();
    result.m[0] = right.x * width;
    result.m[1] = right.y * width;
    result.m[2] = right.z * width;
    result.m[4] = up.x * width;
    result.m[5] = up.y * width;
    result.m[6] = up.z * width;
    result.m[8] = direction.x * beamLength;
    result.m[9] = direction.y * beamLength;
    result.m[10] = direction.z * beamLength;
    result.m[12] = center.x;
    result.m[13] = center.y;
    result.m[14] = center.z;
    return result;
}

}  // namespace spacebattle

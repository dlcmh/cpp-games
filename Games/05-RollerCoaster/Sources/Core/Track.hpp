#pragma once

#include "Core/Math.hpp"

#include <vector>

namespace rollercoaster {

struct PathSample {
    Vec3 position;
    Vec3 tangent;
    Vec3 right;
    Vec3 up;
};

struct Track {
    std::vector<PathSample> samples;
    float length{0.0f};
    float startHeight{0.0f};
};

// Generates the closed route from a small list of control points.
Track buildTrack();

// Returns the path pose at an arc-length distance around the loop.
PathSample sampleAt(const Track& track, float distance);

}  // namespace rollercoaster

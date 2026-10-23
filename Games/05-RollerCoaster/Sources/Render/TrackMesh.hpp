#pragma once

#include "Core/Track.hpp"

#include <cstddef>
#include <vector>

namespace rollercoaster {

struct TrackVertex {
    float position[3];
    float color[3];
};

struct TrackMesh {
    std::vector<TrackVertex> vertices;
};

// Builds the visible rails, ties, supports, and ground from the same samples
// used to drive the camera.
TrackMesh buildTrackMesh(const Track& track);

}  // namespace rollercoaster

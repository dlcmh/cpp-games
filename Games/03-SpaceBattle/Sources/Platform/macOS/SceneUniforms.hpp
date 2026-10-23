#pragma once

#include <cstddef>

#include "Render/Math.hpp"

namespace spacebattle {

// Keep in sync with FrameUniforms/ObjectUniforms in Shaders/scene.slang.
struct alignas(16) FrameUniforms {
    Mat4 viewProjection;
    float cameraPosition[4];
    float lightDirection[4];
    float timing[4];
};

struct alignas(16) ObjectUniforms {
    Mat4 model;
    float color[4];
    float params[4];
};

static_assert(sizeof(FrameUniforms) == 112, "FrameUniforms must match Slang");
static_assert(sizeof(ObjectUniforms) == 96, "ObjectUniforms must match Slang");

}  // namespace spacebattle

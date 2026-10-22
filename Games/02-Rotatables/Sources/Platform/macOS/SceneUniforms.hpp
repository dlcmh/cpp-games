#pragma once

#include <cstddef>

namespace rotatables {

// Must match SceneUniforms in Shaders/common.slang byte for byte.
struct alignas(16) SceneUniforms {
    float resolution[2];
    float time;
    float angleA;
    float angleB;
    float selectedIndex;
    float skinA;
    float skinB;
};

static_assert(sizeof(SceneUniforms) == 32, "SceneUniforms must match the Slang layout");
static_assert(offsetof(SceneUniforms, selectedIndex) == 20, "SceneUniforms layout drifted");

}  // namespace rotatables

#pragma once

#include "Core/BattleState.hpp"
#include "Render/RenderFrame.hpp"

namespace spacebattle {

// Serializes the cinematic state into a renderer-neutral list of 3D draws and
// a camera. This is the CPU scene graph: transforms, camera paths, spawn
// positions, and effect timing happen here, not in Metal.
class SceneBuilder {
public:
    void build(const BattleState& state, RenderFrame& frame);
};

}  // namespace spacebattle

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Render/Math.hpp"
#include "Render/MeshId.hpp"

namespace spacebattle {

struct MeshVertex {
    Vec3 position;
    Vec3 normal;
    Vec3 color;
};

struct MeshRange {
    std::uint32_t firstIndex{0};
    std::uint32_t indexCount{0};
};

struct MeshBatch {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::array<MeshRange, static_cast<std::size_t>(MeshId::Count)> ranges{};
};

// Builds every 3D model from code at startup. There are no imported meshes or
// textures: spheres, asteroids, TIEs, the Star Destroyer, and the trench are
// all static vertex/index data produced on the CPU.
MeshBatch buildMeshBatch();

}  // namespace spacebattle

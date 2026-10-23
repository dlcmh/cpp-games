#pragma once

#include <cstddef>

namespace spacebattle {

enum class MeshId : std::size_t {
    Sphere = 0,
    Asteroid,
    TieFighter,
    StarDestroyer,
    Trench,
    Box,
    Count,
};

enum class Material : int {
    Lit = 0,
    Emissive,
    Planet,
    DeathStar,
    Trench,
    Asteroid,
    Tie,
    StarDestroyer,
    Sky,
    Laser,
    Explosion,
};

}  // namespace spacebattle

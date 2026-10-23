#pragma once

#include <vector>

#include "Lasers/LaserTypes.hpp"
#include "Render/Math.hpp"
#include "Render/MeshId.hpp"

namespace spacebattle {

struct Camera {
    Vec3 position{};
    Vec3 forward{0.0f, 0.0f, -1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fovYRadians{1.0f};
};

struct DrawItem {
    MeshId mesh{MeshId::Box};
    Material material{Material::Lit};
    Mat4 transform{identity()};
    Vec3 color{1.0f, 1.0f, 1.0f};
    float alpha{1.0f};
    float phase{0.0f};
};

struct RenderFrame {
    Camera camera{};
    float time{0.0f};
    float phaseTime{0.0f};
    float jump{0.0f};
    float flash{0.0f};
    float steer{0.0f};
    float firing{0.0f};
    int phase{0};
    int pixelWidth{0};
    int pixelHeight{0};
    std::vector<DrawItem> opaque;
    std::vector<DrawItem> additive;
    std::vector<cppgames::lasers::LaserBeam> lasers;
};

}  // namespace spacebattle

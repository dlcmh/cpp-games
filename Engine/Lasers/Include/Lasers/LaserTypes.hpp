#pragma once

namespace cppgames::lasers {

enum class LaserStyle : int {
    XWing = 0,
    Tie,
    Ion,
    Heavy,
};

// Renderer-neutral beam description. Apps can build these from gameplay state
// without depending on Metal.
struct LaserBeam {
    float start[3]{};
    float end[3]{};
    float width{0.04f};
    float color[3]{1.0f, 0.18f, 0.10f};
    float intensity{1.0f};
    int style{0};
    float seed{0.0f};
};

struct LaserFrame {
    float viewProjection[16]{};
    float cameraPosition[3]{};
    float time{0.0f};
    float projectionScale{1.0f};
    float aspect{1.0f};
};

}  // namespace cppgames::lasers

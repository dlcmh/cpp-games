#pragma once

namespace barebones {

// Everything the renderer needs for one frame. This stays free of native
// graphics types so Core and App do not depend on Metal.
struct RenderFrame {
    float angleRadians{0.0f};
    float elapsedSeconds{0.0f};
    int pixelWidth{0};
    int pixelHeight{0};
};

}  // namespace barebones

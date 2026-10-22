#pragma once

#include <array>
#include <cstddef>

namespace rotatables {

inline constexpr std::size_t kRenderGlyphCount = 2;

struct CharacterFrame {
    float angleRadians{0.0f};
    int skin{0};
};

// Plain data passed across the renderer seam. No Metal types leak into Core or
// App.
struct RenderFrame {
    float elapsedSeconds{0.0f};
    int pixelWidth{0};
    int pixelHeight{0};
    std::size_t selectedIndex{0};
    std::array<CharacterFrame, kRenderGlyphCount> characters{};
};

}  // namespace rotatables

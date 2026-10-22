#pragma once

#include <array>
#include <cstddef>

#include "Core/Skins.hpp"

namespace rotatables {

enum class Glyph : std::size_t {
    A = 0,
    B = 1,
};

inline constexpr std::size_t kGlyphCount = 2;

struct CharacterState {
    float angleRadians{0.0f};
    float rotationSpeed{0.0f};
    Skin skin{Skin::Aurora};
};

struct GameState {
    std::array<CharacterState, kGlyphCount> characters{};
    std::size_t selectedIndex{0};
    float elapsedSeconds{0.0f};
};

[[nodiscard]] inline std::size_t glyphIndex(Glyph glyph) {
    return static_cast<std::size_t>(glyph);
}

}  // namespace rotatables

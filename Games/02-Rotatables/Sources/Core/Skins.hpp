#pragma once

#include <array>
#include <cstddef>

namespace rotatables {

enum class Skin : int {
    Aurora = 0,
    Ember,
    Arc,
    Frost,
    Prism,
    Count,
};

inline constexpr int kSkinCount = static_cast<int>(Skin::Count);
inline constexpr std::array<const char*, kSkinCount> kSkinNames{
    "Aurora", "Ember", "Arc", "Frost", "Prism",
};

[[nodiscard]] inline Skin nextSkin(Skin skin) {
    return static_cast<Skin>((static_cast<int>(skin) + 1) % kSkinCount);
}

[[nodiscard]] inline const char* skinName(Skin skin) {
    const int index = static_cast<int>(skin);
    return kSkinNames[static_cast<std::size_t>(index)];
}

}  // namespace rotatables

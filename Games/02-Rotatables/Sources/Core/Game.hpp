#pragma once

#include <cstddef>

#include "Core/GameState.hpp"

namespace rotatables {

class Game {
public:
    Game();

    void update(float deltaSeconds);

    void select(Glyph glyph);
    void cycleSelectedSkin();
    void reset();

    [[nodiscard]] const GameState& state() const noexcept;

private:
    GameState state_{};
};

}  // namespace rotatables

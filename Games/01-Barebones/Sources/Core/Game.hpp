#pragma once

#include "Core/GameTypes.hpp"

namespace barebones {

class Game {
public:
    void update(float deltaSeconds);

    void togglePause();
    void increaseSpeed();
    void decreaseSpeed();
    void reset();

    [[nodiscard]] const GameState& state() const noexcept;

private:
    GameState state_{};
};

}  // namespace barebones

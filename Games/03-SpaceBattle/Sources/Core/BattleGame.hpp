#pragma once

#include "Core/BattleState.hpp"

namespace spacebattle {

class BattleGame {
public:
    BattleGame();

    void update(float deltaSeconds);

    void setSteer(float direction);
    void fire();
    void togglePause();
    void previewPhase(int oneBasedPhase);

    [[nodiscard]] const BattleState& state() const noexcept;

private:
    void advancePhase();

    BattleState state_{};
};

}  // namespace spacebattle

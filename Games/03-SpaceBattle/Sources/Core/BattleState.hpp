#pragma once

#include <string_view>

namespace spacebattle {

enum class BattlePhase {
    Approach = 0,
    TrenchRun,
    Explosion,
    Jump,
    Gauntlet,
    Count,
};

struct BattleState {
    BattlePhase phase{BattlePhase::Approach};
    float phaseTime{0.0f};
    float elapsedSeconds{0.0f};
    float steer{0.0f};
    float steerTarget{0.0f};
    float firePulse{0.0f};
    int deathStarsDestroyed{0};
    bool paused{false};
};

[[nodiscard]] std::string_view phaseName(BattlePhase phase);

}  // namespace spacebattle

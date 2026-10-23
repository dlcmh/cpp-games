#pragma once

#include "Core/BattleState.hpp"
#include "Planets/PlanetSpec.hpp"

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
    [[nodiscard]] const cppgames::planets::PlanetSpec& planet() const noexcept;

private:
    void advancePhase();
    void rollPlanet();

    BattleState state_{};
    cppgames::planets::PlanetSpec planet_{};
    std::uint64_t planetSeed_{};
};

}  // namespace spacebattle

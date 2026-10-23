#include "Core/BattleGame.hpp"
#include "Core/BattleState.hpp"
#include "Planets/PlanetSpec.hpp"

#include <algorithm>
#include <string_view>

namespace spacebattle {
namespace {

constexpr float kApproachDuration = 7.0f;
constexpr float kTrenchDuration = 11.0f;
constexpr float kExplosionDuration = 4.5f;
constexpr float kJumpDuration = 2.8f;
constexpr float kGauntletDuration = 11.0f;

float phaseDuration(BattlePhase phase) {
    switch (phase) {
        case BattlePhase::Approach:
            return kApproachDuration;
        case BattlePhase::TrenchRun:
            return kTrenchDuration;
        case BattlePhase::Explosion:
            return kExplosionDuration;
        case BattlePhase::Jump:
            return kJumpDuration;
        case BattlePhase::Gauntlet:
            return kGauntletDuration;
        case BattlePhase::Count:
            break;
    }
    return kApproachDuration;
}

}  // namespace

std::string_view phaseName(BattlePhase phase) {
    switch (phase) {
        case BattlePhase::Approach:
            return "Death Star approach";
        case BattlePhase::TrenchRun:
            return "Trench run";
        case BattlePhase::Explosion:
            return "Death Star destroyed";
        case BattlePhase::Jump:
            return "Hyperspace jump";
        case BattlePhase::Gauntlet:
            return "Asteroid gauntlet";
        case BattlePhase::Count:
            break;
    }
    return "Unknown";
}

BattleGame::BattleGame() {
    state_.steer = 0.0f;

    // Same showcase planet the Planet Lab opens with.
    planet_ = cppgames::planets::generatePlanet(cppgames::planets::kShowcasePlanetSeed);
}

void BattleGame::update(float deltaSeconds) {
    if (deltaSeconds <= 0.0f || state_.paused) {
        return;
    }

    state_.elapsedSeconds += deltaSeconds;
    state_.phaseTime += deltaSeconds;
    state_.firePulse = std::max(0.0f, state_.firePulse - deltaSeconds * 5.0f);
    state_.steer += (state_.steerTarget - state_.steer) * std::min(1.0f, deltaSeconds * 6.0f);

    if (state_.phaseTime >= phaseDuration(state_.phase)) {
        advancePhase();
    }
}

void BattleGame::setSteer(float direction) {
    state_.steerTarget = std::clamp(direction, -1.0f, 1.0f);
}

void BattleGame::fire() {
    state_.firePulse = 1.0f;
}

void BattleGame::togglePause() {
    state_.paused = !state_.paused;
}

void BattleGame::previewPhase(int oneBasedPhase) {
    const int phaseIndex = std::clamp(oneBasedPhase, 1, static_cast<int>(BattlePhase::Count)) - 1;
    state_.phase = static_cast<BattlePhase>(phaseIndex);
    state_.phaseTime = 0.0f;
    state_.paused = false;

    // Start previews a little way into each phase so the representative
    // camera and effect state are immediately visible.
    switch (state_.phase) {
        case BattlePhase::Approach:
            state_.phaseTime = 1.5f;
            break;
        case BattlePhase::TrenchRun:
            state_.phaseTime = 5.0f;
            break;
        case BattlePhase::Explosion:
            state_.phaseTime = 1.0f;
            break;
        case BattlePhase::Jump:
            state_.phaseTime = 1.2f;
            break;
        case BattlePhase::Gauntlet:
            state_.phaseTime = 5.0f;
            break;
        case BattlePhase::Count:
            break;
    }
}

const BattleState& BattleGame::state() const noexcept {
    return state_;
}

const cppgames::planets::PlanetSpec& BattleGame::planet() const noexcept {
    return planet_;
}

void BattleGame::advancePhase() {
    state_.phaseTime = 0.0f;
    switch (state_.phase) {
        case BattlePhase::Approach:
            state_.phase = BattlePhase::TrenchRun;
            break;
        case BattlePhase::TrenchRun:
            state_.phase = BattlePhase::Explosion;
            ++state_.deathStarsDestroyed;
            break;
        case BattlePhase::Explosion:
            state_.phase = BattlePhase::Jump;
            break;
        case BattlePhase::Jump:
            state_.phase = BattlePhase::Gauntlet;
            break;
        case BattlePhase::Gauntlet:
            state_.phase = BattlePhase::Approach;
            break;
        case BattlePhase::Count:
            state_.phase = BattlePhase::Approach;
            break;
    }
}

}  // namespace spacebattle

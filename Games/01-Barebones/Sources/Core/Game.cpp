#include "Core/Game.hpp"

#include <algorithm>
#include <cmath>

namespace barebones {
namespace {

constexpr float kTwoPi = 6.28318530718f;
constexpr float kMinSpeed = 0.2f;
constexpr float kMaxSpeed = 8.0f;
constexpr float kDefaultSpeed = 1.8f;

}  // namespace

void Game::update(float deltaSeconds) {
    if (state_.paused || deltaSeconds <= 0.0f) {
        return;
    }

    state_.angleRadians =
        std::fmod(state_.angleRadians + state_.rotationSpeed * deltaSeconds, kTwoPi);
    if (state_.angleRadians < 0.0f) {
        state_.angleRadians += kTwoPi;
    }
    state_.elapsedSeconds += deltaSeconds;
}

void Game::togglePause() {
    state_.paused = !state_.paused;
}

void Game::increaseSpeed() {
    state_.rotationSpeed = std::clamp(state_.rotationSpeed * 1.25f, kMinSpeed, kMaxSpeed);
}

void Game::decreaseSpeed() {
    state_.rotationSpeed = std::clamp(state_.rotationSpeed / 1.25f, kMinSpeed, kMaxSpeed);
}

void Game::reset() {
    const bool wasPaused = state_.paused;
    state_ = GameState{};
    state_.rotationSpeed = kDefaultSpeed;
    state_.paused = wasPaused;
}

const GameState& Game::state() const noexcept {
    return state_;
}

}  // namespace barebones

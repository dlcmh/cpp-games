#include "Core/Game.hpp"
#include "Core/GameState.hpp"
#include "Core/Skins.hpp"

#include <cmath>

namespace rotatables {
namespace {

constexpr float kTwoPi = 6.28318530718f;
constexpr float kAngleA = 0.0f;
constexpr float kAngleB = 0.0f;
constexpr float kSpeedA = 1.55f;
constexpr float kSpeedB = -1.15f;

float wrapAngle(float angle) {
    angle = std::fmod(angle, kTwoPi);
    return angle < 0.0f ? angle + kTwoPi : angle;
}

}  // namespace

Game::Game() {
    reset();
}

void Game::update(float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }

    for (CharacterState& character : state_.characters) {
        character.angleRadians =
            wrapAngle(character.angleRadians + character.rotationSpeed * deltaSeconds);
    }
    state_.elapsedSeconds += deltaSeconds;
}

void Game::select(Glyph glyph) {
    state_.selectedIndex = glyphIndex(glyph);
}

void Game::cycleSelectedSkin() {
    CharacterState& character = state_.characters[state_.selectedIndex];
    character.skin = nextSkin(character.skin);
}

void Game::reset() {
    state_ = GameState{};
    state_.characters[0] = CharacterState{kAngleA, kSpeedA, Skin::Aurora};
    state_.characters[1] = CharacterState{kAngleB, kSpeedB, Skin::Ember};
}

const GameState& Game::state() const noexcept {
    return state_;
}

}  // namespace rotatables

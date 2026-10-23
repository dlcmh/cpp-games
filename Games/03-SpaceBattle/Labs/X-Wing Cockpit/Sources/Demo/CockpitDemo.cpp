#include "Demo/CockpitDemo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace cockpitlab {
namespace {

constexpr const char* kPresetNames[] = {
    "Classic",
    "HUD Focus",
    "Heavy Glass",
    "Minimal",
};

}  // namespace

CockpitDemo::CockpitDemo() {
    reset();
}

void CockpitDemo::update(float deltaSeconds) {
    if (deltaSeconds <= 0.0f || paused_) {
        return;
    }

    state_.time += deltaSeconds;
    state_.phaseTime += deltaSeconds;

    if (autoDemo_) {
        autoFireTimer_ += deltaSeconds;
        autoJumpTimer_ += deltaSeconds;
        autoFlashTimer_ += deltaSeconds;

        if (autoFireTimer_ > 1.35f) {
            fire();
            autoFireTimer_ = 0.0f;
        }
        if (autoJumpTimer_ > 8.0f) {
            triggerJump();
            autoJumpTimer_ = 0.0f;
        }
        if (autoFlashTimer_ > 5.0f) {
            triggerFlash();
            autoFlashTimer_ = 0.0f;
        }
    }

    const float steerGoal = autoDemo_ && std::abs(steerTarget_) < 0.01f
                                ? std::sin(state_.time * 0.45f) * 0.72f
                                : steerTarget_;
    state_.steer += (steerGoal - state_.steer) * std::min(1.0f, deltaSeconds * 3.6f);

    jumpRemaining_ = std::max(0.0f, jumpRemaining_ - deltaSeconds);
    state_.jump = std::min(1.0f, jumpRemaining_ / 1.5f);
    state_.flash = std::max(0.0f, state_.flash - deltaSeconds * 1.7f);
    state_.firing = std::max(0.0f, state_.firing - deltaSeconds * 6.0f);
}

void CockpitDemo::setSteer(float direction) {
    steerTarget_ = std::clamp(direction, -1.0f, 1.0f);
}

void CockpitDemo::fire() {
    state_.firing = 1.0f;
}

void CockpitDemo::triggerJump() {
    jumpRemaining_ = 2.2f;
    state_.jump = 1.0f;
}

void CockpitDemo::triggerFlash() {
    state_.flash = 1.0f;
}

void CockpitDemo::togglePause() {
    paused_ = !paused_;
}

void CockpitDemo::toggleAuto() {
    autoDemo_ = !autoDemo_;
}

void CockpitDemo::toggleCanopy() {
    state_.showCanopy = !state_.showCanopy;
    markCustom();
}

void CockpitDemo::toggleDashboard() {
    state_.showDashboard = !state_.showDashboard;
    markCustom();
}

void CockpitDemo::toggleHud() {
    state_.showHud = !state_.showHud;
    markCustom();
}

void CockpitDemo::adjustGlassTint(float delta) {
    state_.glassTint = std::clamp(state_.glassTint + delta, 0.0f, 0.45f);
    markCustom();
}

void CockpitDemo::adjustHudScale(float delta) {
    state_.hudScale = std::clamp(state_.hudScale + delta, 0.6f, 1.5f);
    markCustom();
}

void CockpitDemo::applyPreset(int oneBasedPreset) {
    setPreset(std::clamp(oneBasedPreset, 1, 4) - 1);
}

void CockpitDemo::reset() {
    state_ = cppgames::cockpit::CockpitState{};
    preset_ = 0;
    steerTarget_ = 0.0f;
    jumpRemaining_ = 0.0f;
    autoFireTimer_ = 0.0f;
    autoJumpTimer_ = 0.0f;
    autoFlashTimer_ = 0.0f;
    autoDemo_ = true;
    paused_ = false;
}

const cppgames::cockpit::CockpitState& CockpitDemo::state() const noexcept {
    return state_;
}

std::string CockpitDemo::windowTitle() const {
    const char* presetName = preset_ >= 0 ? kPresetNames[preset_] : "Custom";
    char title[320];
    std::snprintf(title, sizeof(title),
                  "X-Wing Cockpit Lab - %s | HUD %s | Canopy %s | Dash %s | tint %.2f | "
                  "hud %.2f | %s%s",
                  presetName, state_.showHud ? "on" : "off", state_.showCanopy ? "on" : "off",
                  state_.showDashboard ? "on" : "off", static_cast<double>(state_.glassTint),
                  static_cast<double>(state_.hudScale),
                  paused_ ? "PAUSED" : (autoDemo_ ? "AUTO" : "MANUAL"),
                  state_.jump > 0.0f ? " | JUMP" : "");
    return title;
}

void CockpitDemo::markCustom() {
    preset_ = -1;
}

void CockpitDemo::setPreset(int index) {
    preset_ = index;
    state_.showCanopy = true;
    state_.showDashboard = true;
    state_.showHud = true;
    state_.glassTint = 0.10f;
    state_.hudScale = 1.0f;

    switch (index) {
        case 0:
            break;
        case 1:
            state_.showCanopy = false;
            state_.showDashboard = false;
            state_.glassTint = 0.0f;
            state_.hudScale = 1.08f;
            break;
        case 2:
            state_.glassTint = 0.28f;
            state_.hudScale = 0.92f;
            break;
        case 3:
            state_.showDashboard = false;
            state_.glassTint = 0.04f;
            state_.hudScale = 0.88f;
            break;
        default:
            break;
    }
}

}  // namespace cockpitlab

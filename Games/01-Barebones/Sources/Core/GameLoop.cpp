#include "Core/GameLoop.hpp"

#include <algorithm>
#include <cmath>

namespace barebones {

GameLoop::GameLoop(double fixedStepSeconds)
    : fixedStepSeconds_(fixedStepSeconds > 0.0 ? fixedStepSeconds : 1.0 / 120.0) {}

void GameLoop::advance(double frameSeconds, const UpdateCallback& update) {
    if (!std::isfinite(frameSeconds) || frameSeconds <= 0.0) {
        return;
    }

    // If the process was suspended (debugger, laptop lid), do not replay
    // minutes of simulation in a single frame.
    accumulator_ += std::min(frameSeconds, 0.25);

    int steps = 0;
    while (accumulator_ >= fixedStepSeconds_ && steps < kMaxStepsPerFrame) {
        update(fixedStepSeconds_);
        accumulator_ -= fixedStepSeconds_;
        ++steps;
    }

    if (steps == kMaxStepsPerFrame) {
        accumulator_ = std::fmod(accumulator_, fixedStepSeconds_);
    }
}

double GameLoop::fixedStepSeconds() const noexcept {
    return fixedStepSeconds_;
}

double GameLoop::remainderSeconds() const noexcept {
    return accumulator_;
}

}  // namespace barebones

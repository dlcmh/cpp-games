#pragma once

#include <functional>

namespace rotatables {

// Fixed-timestep update sequencer. Rendering happens once per display frame,
// while simulation advances in deterministic chunks.
class GameLoop {
public:
    using UpdateCallback = std::function<void(double fixedDeltaSeconds)>;

    explicit GameLoop(double fixedStepSeconds = 1.0 / 120.0);

    void advance(double frameSeconds, const UpdateCallback& update);

    [[nodiscard]] double fixedStepSeconds() const noexcept;
    [[nodiscard]] double remainderSeconds() const noexcept;

private:
    static constexpr int kMaxStepsPerFrame = 8;

    double fixedStepSeconds_;
    double accumulator_{0.0};
};

}  // namespace rotatables

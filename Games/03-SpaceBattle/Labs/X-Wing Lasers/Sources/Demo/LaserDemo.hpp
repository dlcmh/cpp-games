#pragma once

#include <string>
#include <vector>

#include "Cockpit/CockpitState.hpp"
#include "Lasers/LaserTypes.hpp"

namespace laserlab {

// Standalone laser-effect state. It owns only demo timing, bolt positions, and
// the style controls exposed to the presenter; rendering is the shared
// Engine/Lasers module.
class LaserDemo {
public:
    LaserDemo();

    void update(float deltaSeconds);

    void fire();
    void toggleAuto();
    void togglePause();
    void toggleCockpit();
    void reset();

    [[nodiscard]] const cppgames::cockpit::CockpitState& cockpitState() const noexcept;
    [[nodiscard]] std::vector<cppgames::lasers::LaserBeam> beams() const;
    [[nodiscard]] std::string windowTitle() const;

private:
    cppgames::cockpit::CockpitState cockpit_{};
    bool autoFire_{true};
    bool paused_{false};
    bool showCockpit_{true};
    float manualBurstTime_{-1.0f};
    float flash_{0.0f};
};

}  // namespace laserlab

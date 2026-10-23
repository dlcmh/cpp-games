#pragma once

#include <string>

#include "Cockpit/CockpitState.hpp"

namespace cockpitlab {

// The lab's control surface. It owns only presentation/demo state; the actual
// cockpit rendering is the shared Engine/Cockpit module.
class CockpitDemo {
public:
    CockpitDemo();

    void update(float deltaSeconds);

    void setSteer(float direction);
    void fire();
    void triggerJump();
    void triggerFlash();

    void togglePause();
    void toggleAuto();
    void toggleCanopy();
    void toggleDashboard();
    void toggleHud();

    void adjustGlassTint(float delta);
    void adjustHudScale(float delta);
    void applyPreset(int oneBasedPreset);
    void reset();

    [[nodiscard]] const cppgames::cockpit::CockpitState& state() const noexcept;
    [[nodiscard]] std::string windowTitle() const;

private:
    void markCustom();
    void setPreset(int index);

    cppgames::cockpit::CockpitState state_{};
    bool paused_{false};
    bool autoDemo_{true};
    int preset_{0};
    float steerTarget_{0.0f};
    float jumpRemaining_{0.0f};
    float autoFireTimer_{0.0f};
    float autoJumpTimer_{0.0f};
    float autoFlashTimer_{0.0f};
};

}  // namespace cockpitlab

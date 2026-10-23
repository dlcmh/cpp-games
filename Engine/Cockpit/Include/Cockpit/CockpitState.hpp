#pragma once

namespace cppgames::cockpit {

// Renderer-neutral inputs for the X-Wing cockpit overlay. Games fill this from
// gameplay state; the shared CockpitOverlay converts it to GPU uniforms.
struct CockpitState {
    float time{0.0f};
    float phase{0.0f};
    float phaseTime{0.0f};
    float flash{0.0f};
    float jump{0.0f};
    float steer{0.0f};
    float firing{0.0f};

    bool showCanopy{true};
    bool showDashboard{true};
    bool showHud{true};

    // 0 = clear glass, 1 = heavily tinted canopy.
    float glassTint{0.10f};
    // Scales the reticle/target bracket around the screen centre.
    float hudScale{1.0f};
};

}  // namespace cppgames::cockpit

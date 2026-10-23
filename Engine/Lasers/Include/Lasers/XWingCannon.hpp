#pragma once

#include <vector>

#include "Lasers/LaserTypes.hpp"

namespace cppgames::lasers {

// World-space basis for the cannon. The lab uses a fixed camera basis; the game
// derives one from its cockpit camera.
struct XWingCannonFrame {
    float origin[3]{};
    float right[3]{1.0f, 0.0f, 0.0f};
    float up[3]{0.0f, 1.0f, 0.0f};
    float forward[3]{0.0f, 0.0f, -1.0f};
};

struct XWingCannonSettings {
    float firePeriod{0.62f};
    float boltSpeed{30.0f};
    float boltLength{8.0f};
    float muzzleSide{1.25f};
    float muzzleUp{-0.55f};
    float muzzleForward{4.0f};
    float travelRange{125.0f};
};

// Produces the same continuous four-cannon volley stream in the lab and the
// game. Automatic volleys are derived from the clock, so no per-frame state is
// needed. manualAge >= 0 adds one extra volley at that age in seconds.
[[nodiscard]] std::vector<LaserBeam> buildXWingCannonBeams(const XWingCannonSettings& settings,
                                                           const XWingCannonFrame& frame,
                                                           float time, bool automatic = true,
                                                           float manualAge = -1.0f);

}  // namespace cppgames::lasers

#include "Lasers/XWingCannon.hpp"
#include "Lasers/LaserTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace cppgames::lasers {
namespace {

void appendVolley(std::vector<LaserBeam>& beams, const XWingCannonSettings& settings,
                  const XWingCannonFrame& frame, float time, float age) {
    const float travel = age * settings.boltSpeed;
    const float endTravel = std::min(travel + settings.boltLength, settings.convergenceDistance);
    if (travel >= settings.convergenceDistance || endTravel <= travel) {
        return;
    }

    // The stream converges on the shared cockpit reticle. aimNdc is in
    // normalized device coordinates, so this is the same screen point the HUD
    // draws whether we are in the lab or the game.
    const float targetX =
        settings.aimNdcX * settings.convergenceDistance / settings.projectionScale;
    const float targetY =
        settings.aimNdcY * settings.convergenceDistance / settings.projectionScale;
    const float targetWorldX = frame.origin[0] + frame.right[0] * targetX + frame.up[0] * targetY +
                               frame.forward[0] * settings.convergenceDistance;
    const float targetWorldY = frame.origin[1] + frame.right[1] * targetX + frame.up[1] * targetY +
                               frame.forward[1] * settings.convergenceDistance;
    const float targetWorldZ = frame.origin[2] + frame.right[2] * targetX + frame.up[2] * targetY +
                               frame.forward[2] * settings.convergenceDistance;

    for (int side = -1; side <= 1; side += 2) {
        const float sideOffset = settings.muzzleSide * static_cast<float>(side);
        const float startWorldX = frame.origin[0] + frame.right[0] * sideOffset +
                                  frame.up[0] * settings.muzzleUp +
                                  frame.forward[0] * settings.muzzleForward;
        const float startWorldY = frame.origin[1] + frame.right[1] * sideOffset +
                                  frame.up[1] * settings.muzzleUp +
                                  frame.forward[1] * settings.muzzleForward;
        const float startWorldZ = frame.origin[2] + frame.right[2] * sideOffset +
                                  frame.up[2] * settings.muzzleUp +
                                  frame.forward[2] * settings.muzzleForward;

        const float axisX = targetWorldX - startWorldX;
        const float axisY = targetWorldY - startWorldY;
        const float axisZ = targetWorldZ - startWorldZ;
        const float axisLength = std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
        if (axisLength < 0.0001f) {
            continue;
        }

        const float directionX = axisX / axisLength;
        const float directionY = axisY / axisLength;
        const float directionZ = axisZ / axisLength;

        LaserBeam beam{};
        beam.start[0] = startWorldX + directionX * travel;
        beam.start[1] = startWorldY + directionY * travel;
        beam.start[2] = startWorldZ + directionZ * travel;
        beam.end[0] = startWorldX + directionX * endTravel;
        beam.end[1] = startWorldY + directionY * endTravel;
        beam.end[2] = startWorldZ + directionZ * endTravel;
        beam.width = kXWingLaserVisual.width;
        beam.color[0] = kXWingLaserVisual.color[0];
        beam.color[1] = kXWingLaserVisual.color[1];
        beam.color[2] = kXWingLaserVisual.color[2];
        beam.intensity = kXWingLaserVisual.intensity;
        beam.style = static_cast<int>(LaserStyle::XWing);
        beam.seed = time * 0.73f + static_cast<float>(side) * 11.0f + age;
        beams.push_back(beam);
    }
}

}  // namespace

std::vector<LaserBeam> buildXWingCannonBeams(const XWingCannonSettings& settings,
                                             const XWingCannonFrame& frame, float time,
                                             bool automatic, float manualAge) {
    std::vector<LaserBeam> beams;

    if (automatic) {
        const float phase = std::fmod(std::max(time, 0.0f), settings.firePeriod);
        const int volleys = static_cast<int>(settings.convergenceDistance /
                                             (settings.boltSpeed * settings.firePeriod)) +
                            1;
        for (int index = 0; index < volleys; ++index) {
            appendVolley(beams, settings, frame, time,
                         phase + static_cast<float>(index) * settings.firePeriod);
        }
    }

    if (manualAge >= 0.0f) {
        appendVolley(beams, settings, frame, time, manualAge);
    }
    return beams;
}

}  // namespace cppgames::lasers

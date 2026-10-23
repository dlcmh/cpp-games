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
    if (travel > settings.travelRange) {
        return;
    }

    const float u0 = std::clamp(travel / settings.travelRange, 0.0f, 1.0f);
    const float u1 = std::clamp((travel + settings.boltLength) / settings.travelRange, 0.0f, 1.0f);
    for (int side = -1; side <= 1; side += 2) {
        const float x0 = static_cast<float>(side) * (settings.muzzleSide - 0.95f * u0);
        const float x1 = static_cast<float>(side) * (settings.muzzleSide - 0.95f * u1);
        const float y0 = settings.muzzleUp + 0.90f * u0;
        const float y1 = settings.muzzleUp + 0.90f * u1;
        const float z0 = settings.muzzleForward + travel;
        const float z1 = z0 + settings.boltLength;

        const float startX =
            frame.origin[0] + frame.right[0] * x0 + frame.up[0] * y0 + frame.forward[0] * z0;
        const float startY =
            frame.origin[1] + frame.right[1] * x0 + frame.up[1] * y0 + frame.forward[1] * z0;
        const float startZ =
            frame.origin[2] + frame.right[2] * x0 + frame.up[2] * y0 + frame.forward[2] * z0;
        const float endX =
            frame.origin[0] + frame.right[0] * x1 + frame.up[0] * y1 + frame.forward[0] * z1;
        const float endY =
            frame.origin[1] + frame.right[1] * x1 + frame.up[1] * y1 + frame.forward[1] * z1;
        const float endZ =
            frame.origin[2] + frame.right[2] * x1 + frame.up[2] * y1 + frame.forward[2] * z1;

        LaserBeam beam{};
        beam.start[0] = startX;
        beam.start[1] = startY;
        beam.start[2] = startZ;
        beam.end[0] = endX;
        beam.end[1] = endY;
        beam.end[2] = endZ;
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
        const int volleys =
            static_cast<int>(settings.travelRange / (settings.boltSpeed * settings.firePeriod)) + 1;
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

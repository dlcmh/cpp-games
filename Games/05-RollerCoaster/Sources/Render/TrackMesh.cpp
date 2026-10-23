#include "Render/TrackMesh.hpp"
#include "Core/Math.hpp"
#include "Core/Track.hpp"

#include <cstddef>

namespace rollercoaster {
namespace {

void addQuad(TrackMesh& mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 color) {
    const TrackVertex va{{a.x, a.y, a.z}, {color.x, color.y, color.z}};
    const TrackVertex vb{{b.x, b.y, b.z}, {color.x, color.y, color.z}};
    const TrackVertex vc{{c.x, c.y, c.z}, {color.x, color.y, color.z}};
    const TrackVertex vd{{d.x, d.y, d.z}, {color.x, color.y, color.z}};
    mesh.vertices.insert(mesh.vertices.end(), {va, vb, vc, va, vc, vd});
}

}  // namespace

TrackMesh buildTrackMesh(const Track& track) {
    TrackMesh mesh;
    const Vec3 railColor{0.95f, 0.78f, 0.42f};
    const Vec3 tieColor{0.26f, 0.17f, 0.10f};
    const Vec3 supportColor{0.18f, 0.19f, 0.18f};

    addQuad(mesh, {-300.0f, 0.0f, -300.0f}, {300.0f, 0.0f, -300.0f}, {300.0f, 0.0f, 300.0f},
            {-300.0f, 0.0f, 300.0f}, {0.10f, 0.16f, 0.10f});

    const std::size_t count = track.samples.size();
    for (std::size_t i = 0; i < count; ++i) {
        const PathSample& a = track.samples[i];
        const PathSample& b = track.samples[(i + 1) % count];

        for (int side = -1; side <= 1; side += 2) {
            const Vec3 centreA = a.position + a.right * (0.58f * static_cast<float>(side));
            const Vec3 centreB = b.position + b.right * (0.58f * static_cast<float>(side));
            const Vec3 widthA = a.right * 0.045f;
            const Vec3 widthB = b.right * 0.045f;
            addQuad(mesh, centreA - widthA, centreA + widthA, centreB + widthB, centreB - widthB,
                    railColor);
        }

        if (i % 3 == 0) {
            const Vec3 tangentA = a.tangent * 0.07f;
            const Vec3 left = a.right * 0.84f;
            addQuad(mesh, a.position - left - tangentA, a.position + left - tangentA,
                    a.position + left + tangentA, a.position - left + tangentA, tieColor);
        }

        if (i % 25 == 0) {
            const Vec3 left = a.right * 0.08f;
            const Vec3 topLeft = a.position - left;
            const Vec3 topRight = a.position + left;
            const Vec3 bottomLeft{topLeft.x, 0.0f, topLeft.z};
            const Vec3 bottomRight{topRight.x, 0.0f, topRight.z};
            addQuad(mesh, topLeft, topRight, bottomRight, bottomLeft, supportColor);
        }
    }
    return mesh;
}

}  // namespace rollercoaster

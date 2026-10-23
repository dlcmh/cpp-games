#include "Platform/macOS/MeshLibrary.hpp"
#include "Render/Math.hpp"
#include "Render/MeshId.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace spacebattle {
namespace {

constexpr float kPi = 3.14159265359f;

struct MeshBuilder {
    MeshBatch batch;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;

    std::uint32_t vertex(Vec3 position, Vec3 normal, Vec3 color) {
        vertices.push_back({position, normalize(normal), color});
        return static_cast<std::uint32_t>(vertices.size() - 1);
    }

    void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    void beginMesh(MeshId id) {
        batch.ranges[static_cast<std::size_t>(id)].firstIndex =
            static_cast<std::uint32_t>(indices.size());
    }

    void endMesh(MeshId id) {
        MeshRange& range = batch.ranges[static_cast<std::size_t>(id)];
        range.indexCount = static_cast<std::uint32_t>(indices.size()) - range.firstIndex;
    }
};

Vec3 transformPoint(const Mat4& matrix, Vec3 point) {
    return {matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
            matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
            matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]};
}

Vec3 transformDirection(const Mat4& matrix, Vec3 direction) {
    return {matrix.m[0] * direction.x + matrix.m[4] * direction.y + matrix.m[8] * direction.z,
            matrix.m[1] * direction.x + matrix.m[5] * direction.y + matrix.m[9] * direction.z,
            matrix.m[2] * direction.x + matrix.m[6] * direction.y + matrix.m[10] * direction.z};
}

void appendTriangle(MeshBuilder& builder, Vec3 a, Vec3 b, Vec3 c, Vec3 color,
                    const Mat4& transform) {
    const Vec3 normal = normalize(cross(b - a, c - a));
    const std::uint32_t ia =
        builder.vertex(transformPoint(transform, a), transformDirection(transform, normal), color);
    const std::uint32_t ib =
        builder.vertex(transformPoint(transform, b), transformDirection(transform, normal), color);
    const std::uint32_t ic =
        builder.vertex(transformPoint(transform, c), transformDirection(transform, normal), color);
    builder.triangle(ia, ib, ic);
}

void appendQuad(MeshBuilder& builder, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 color,
                const Mat4& transform) {
    appendTriangle(builder, a, b, c, color, transform);
    appendTriangle(builder, a, c, d, color, transform);
}

void appendBox(MeshBuilder& builder, Vec3 center, Vec3 halfSize, Vec3 color,
               const Mat4& transform = identity()) {
    const Vec3 p000{center.x - halfSize.x, center.y - halfSize.y, center.z - halfSize.z};
    const Vec3 p001{center.x - halfSize.x, center.y - halfSize.y, center.z + halfSize.z};
    const Vec3 p010{center.x - halfSize.x, center.y + halfSize.y, center.z - halfSize.z};
    const Vec3 p011{center.x - halfSize.x, center.y + halfSize.y, center.z + halfSize.z};
    const Vec3 p100{center.x + halfSize.x, center.y - halfSize.y, center.z - halfSize.z};
    const Vec3 p101{center.x + halfSize.x, center.y - halfSize.y, center.z + halfSize.z};
    const Vec3 p110{center.x + halfSize.x, center.y + halfSize.y, center.z - halfSize.z};
    const Vec3 p111{center.x + halfSize.x, center.y + halfSize.y, center.z + halfSize.z};

    appendQuad(builder, p001, p101, p111, p011, color, transform);  // +Z
    appendQuad(builder, p100, p000, p010, p110, color, transform);  // -Z
    appendQuad(builder, p000, p001, p011, p010, color, transform);  // -X
    appendQuad(builder, p101, p100, p110, p111, color, transform);  // +X
    appendQuad(builder, p010, p011, p111, p110, color, transform);  // +Y
    appendQuad(builder, p001, p000, p100, p101, color, transform);  // -Y
}

void appendSphere(MeshBuilder& builder, int rings, int segments, Vec3 color, const Mat4& transform,
                  bool rough = false) {
    const std::uint32_t start = static_cast<std::uint32_t>(builder.vertices.size());
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float theta = v * kPi;
        for (int segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float phi = u * kPi * 2.0f;
            Vec3 normal{std::sin(theta) * std::cos(phi), std::cos(theta),
                        std::sin(theta) * std::sin(phi)};
            float radius = 1.0f;
            if (rough) {
                const float wobble = std::sin(phi * 3.1f + theta * 7.3f) * 0.12f +
                                     std::sin(phi * 7.7f - theta * 4.1f) * 0.07f;
                radius = 0.82f + wobble;
            }
            builder.vertex(transformPoint(transform, normal * radius),
                           transformDirection(transform, normal), color);
        }
    }

    const int stride = segments + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            const std::uint32_t a = start + static_cast<std::uint32_t>(ring * stride + segment);
            const std::uint32_t b = a + static_cast<std::uint32_t>(stride);
            builder.triangle(a, a + 1, b);
            builder.triangle(a + 1, b + 1, b);
        }
    }
}

void appendHexWing(MeshBuilder& builder, float side, Vec3 color) {
    const float x = side * 1.0f;
    const float thickness = 0.09f;
    const float radius = 0.88f;
    Vec3 points[6];
    for (int i = 0; i < 6; ++i) {
        const float angle = static_cast<float>(i) / 6.0f * kPi * 2.0f;
        points[i] = {x, std::sin(angle) * radius, std::cos(angle) * radius};
    }

    const Vec3 normal{side, 0.0f, 0.0f};
    const std::uint32_t centerFront =
        builder.vertex({x + side * thickness, 0.0f, 0.0f}, normal, color);
    const std::uint32_t centerBack =
        builder.vertex({x - side * thickness, 0.0f, 0.0f}, -normal, color);

    for (int i = 0; i < 6; ++i) {
        const Vec3& a = points[i];
        const Vec3& b = points[(i + 1) % 6];
        const Vec3 aFront{a.x + side * thickness, a.y, a.z};
        const Vec3 bFront{b.x + side * thickness, b.y, b.z};
        const Vec3 aBack{a.x - side * thickness, a.y, a.z};
        const Vec3 bBack{b.x - side * thickness, b.y, b.z};

        const std::uint32_t af = builder.vertex(aFront, normal, color);
        const std::uint32_t bf = builder.vertex(bFront, normal, color);
        const std::uint32_t ab = builder.vertex(aBack, -normal, color);
        const std::uint32_t bb = builder.vertex(bBack, -normal, color);

        builder.triangle(centerFront, af, bf);
        builder.triangle(centerBack, bb, ab);
        builder.triangle(af, ab, bf);
        builder.triangle(bf, ab, bb);
    }
}

void buildSphere(MeshBuilder& builder, MeshId id) {
    builder.beginMesh(id);
    appendSphere(builder, 32, 64, {0.75f, 0.78f, 0.82f}, identity());
    builder.endMesh(id);
}

void buildAsteroid(MeshBuilder& builder) {
    builder.beginMesh(MeshId::Asteroid);
    appendSphere(builder, 8, 12, {0.34f, 0.29f, 0.24f}, identity(), true);
    builder.endMesh(MeshId::Asteroid);
}

void buildTie(MeshBuilder& builder) {
    builder.beginMesh(MeshId::TieFighter);
    appendSphere(builder, 10, 16, {0.38f, 0.43f, 0.47f}, scaling(0.58f));
    appendBox(builder, {-0.72f, 0.0f, 0.0f}, {0.30f, 0.055f, 0.055f}, {0.28f, 0.31f, 0.34f});
    appendBox(builder, {0.72f, 0.0f, 0.0f}, {0.30f, 0.055f, 0.055f}, {0.28f, 0.31f, 0.34f});
    appendHexWing(builder, -1.0f, {0.24f, 0.28f, 0.32f});
    appendHexWing(builder, 1.0f, {0.24f, 0.28f, 0.32f});
    builder.endMesh(MeshId::TieFighter);
}

void buildStarDestroyer(MeshBuilder& builder) {
    builder.beginMesh(MeshId::StarDestroyer);
    const Vec3 hullColor{0.50f, 0.53f, 0.57f};

    const Vec3 backTopLeft{-50.0f, 5.0f, 42.0f};
    const Vec3 backTopRight{50.0f, 5.0f, 42.0f};
    const Vec3 backBottomLeft{-50.0f, -5.0f, 42.0f};
    const Vec3 backBottomRight{50.0f, -5.0f, 42.0f};
    const Vec3 frontTopLeft{-3.0f, 1.0f, -62.0f};
    const Vec3 frontTopRight{3.0f, 1.0f, -62.0f};
    const Vec3 frontBottomLeft{-3.0f, -1.0f, -62.0f};
    const Vec3 frontBottomRight{3.0f, -1.0f, -62.0f};

    appendQuad(builder, backTopLeft, backTopRight, frontTopRight, frontTopLeft, hullColor,
               identity());
    appendQuad(builder, frontBottomLeft, frontBottomRight, backBottomRight, backBottomLeft,
               hullColor * 0.75f, identity());
    appendQuad(builder, backTopLeft, frontTopLeft, frontBottomLeft, backBottomLeft,
               hullColor * 0.85f, identity());
    appendQuad(builder, frontTopRight, backTopRight, backBottomRight, frontBottomRight,
               hullColor * 0.9f, identity());
    appendQuad(builder, backTopRight, backTopLeft, backBottomLeft, backBottomRight,
               hullColor * 0.65f, identity());

    appendBox(builder, {0.0f, 9.0f, 26.0f}, {12.0f, 4.0f, 11.0f}, hullColor * 1.1f);
    appendBox(builder, {0.0f, 15.0f, 27.0f}, {7.0f, 3.0f, 7.0f}, hullColor);
    appendSphere(builder, 8, 12, hullColor * 1.15f,
                 translation({-5.0f, 19.0f, 27.0f}) * scaling(2.8f));
    appendSphere(builder, 8, 12, hullColor * 1.15f,
                 translation({5.0f, 19.0f, 27.0f}) * scaling(2.8f));

    // Engine banks at the back.
    for (int engine = -1; engine <= 1; ++engine) {
        appendBox(builder, {static_cast<float>(engine) * 22.0f, 0.0f, 44.0f}, {6.0f, 2.2f, 1.5f},
                  {0.25f, 0.55f, 1.0f});
    }
    builder.endMesh(MeshId::StarDestroyer);
}

void buildTrench(MeshBuilder& builder) {
    builder.beginMesh(MeshId::Trench);
    const Vec3 floorColor{0.30f, 0.33f, 0.36f};
    const Vec3 wallColor{0.35f, 0.38f, 0.40f};
    const Vec3 ribColor{0.42f, 0.45f, 0.48f};

    appendBox(builder, {0.0f, -2.2f, -290.0f}, {5.2f, 0.22f, 330.0f}, floorColor);
    appendBox(builder, {-5.3f, 0.45f, -290.0f}, {0.25f, 2.7f, 330.0f}, wallColor);
    appendBox(builder, {5.3f, 0.45f, -290.0f}, {0.25f, 2.7f, 330.0f}, wallColor);

    for (int z = 30; z >= -600; z -= 12) {
        const float zf = static_cast<float>(z);
        appendBox(builder, {0.0f, -1.95f, zf}, {5.0f, 0.10f, 0.28f}, ribColor);
        appendBox(builder, {-5.1f, 0.3f, zf}, {0.18f, 2.4f, 0.28f}, ribColor);
        appendBox(builder, {5.1f, 0.3f, zf}, {0.18f, 2.4f, 0.28f}, ribColor);
        appendBox(builder, {0.0f, 2.95f, zf}, {5.2f, 0.12f, 0.28f}, ribColor * 0.85f);
    }

    for (int z = 18; z >= -600; z -= 36) {
        const float zf = static_cast<float>(z);
        for (int side = -1; side <= 1; side += 2) {
            const float x = static_cast<float>(side) * 5.8f;
            appendBox(builder, {x, 2.9f, zf}, {0.35f, 2.4f, 0.35f}, wallColor * 0.9f);
            appendBox(builder, {x, 5.3f, zf}, {0.5f, 0.2f, 0.5f}, ribColor);
        }
    }
    builder.endMesh(MeshId::Trench);
}

void buildBox(MeshBuilder& builder) {
    builder.beginMesh(MeshId::Box);
    appendBox(builder, {0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f});
    builder.endMesh(MeshId::Box);
}

}  // namespace

MeshBatch buildMeshBatch() {
    MeshBuilder builder;
    builder.vertices.reserve(30000);
    builder.indices.reserve(60000);

    buildSphere(builder, MeshId::Sphere);
    buildAsteroid(builder);
    buildTie(builder);
    buildStarDestroyer(builder);
    buildTrench(builder);
    buildBox(builder);

    builder.batch.vertices = std::move(builder.vertices);
    builder.batch.indices = std::move(builder.indices);
    return builder.batch;
}

}  // namespace spacebattle

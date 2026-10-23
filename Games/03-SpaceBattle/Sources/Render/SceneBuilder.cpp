#include "Render/SceneBuilder.hpp"
#include "Core/BattleState.hpp"
#include "Render/Math.hpp"
#include "Render/MeshId.hpp"
#include "Render/RenderFrame.hpp"

#include <algorithm>
#include <cmath>

namespace spacebattle {
namespace {

constexpr float kPi = 3.14159265359f;

struct CameraBasis {
    Vec3 right;
    Vec3 up;
    Vec3 forward;
};

float random01(int seed) {
    const float value = std::sin(static_cast<float>(seed) * 12.9898f) * 43758.5453f;
    return value - std::floor(value);
}

void setCamera(RenderFrame& frame, Vec3 eye, Vec3 target, Vec3 up, float fovY = 1.02f) {
    const Vec3 forward = normalize(target - eye);
    frame.camera.position = eye;
    frame.camera.forward = forward;
    frame.camera.up = normalize(up - forward * dot(up, forward));
    frame.camera.fovYRadians = fovY;
}

CameraBasis cameraBasis(const RenderFrame& frame) {
    const Vec3 forward = normalize(frame.camera.forward);
    const Vec3 right = normalize(cross(forward, frame.camera.up));
    return {right, cross(right, forward), forward};
}

void addOpaque(RenderFrame& frame, DrawItem item) {
    frame.opaque.push_back(item);
}

void addAdditive(RenderFrame& frame, DrawItem item) {
    frame.additive.push_back(item);
}

void addSky(RenderFrame& frame) {
    DrawItem sky{};
    sky.mesh = MeshId::Sphere;
    sky.material = Material::Sky;
    sky.transform = translation(frame.camera.position) * scaling(750.0f);
    addOpaque(frame, sky);
}

void addSphere(RenderFrame& frame, Vec3 position, float radius, Material material, Vec3 color,
               float spin = 0.0f) {
    DrawItem item{};
    item.mesh = MeshId::Sphere;
    item.material = material;
    item.transform = translation(position) * rotationY(spin) * scaling(radius);
    item.color = color;
    addOpaque(frame, item);
}

void addBeam(RenderFrame& frame, Vec3 from, Vec3 to, float width, Vec3 color, float alpha) {
    DrawItem item{};
    item.mesh = MeshId::Box;
    item.material = Material::Laser;
    item.transform = beamTransform(from, to, width);
    item.color = color;
    item.alpha = alpha;
    addAdditive(frame, item);
}

void addPlayerLasers(RenderFrame& frame, const CameraBasis& basis, bool firing) {
    if (!firing) {
        return;
    }

    const Vec3 origin = frame.camera.position;
    const Vec3 leftMuzzle =
        origin + basis.right * -0.85f + basis.up * -0.45f + basis.forward * 6.0f;
    const Vec3 rightMuzzle =
        origin + basis.right * 0.85f + basis.up * -0.45f + basis.forward * 6.0f;
    const Vec3 target = origin + basis.forward * 90.0f;
    addBeam(frame, leftMuzzle, target, 0.030f, {1.0f, 0.22f, 0.10f}, 0.85f);
    addBeam(frame, rightMuzzle, target, 0.030f, {1.0f, 0.22f, 0.10f}, 0.85f);
}

void addTieFighters(RenderFrame& frame, float time, Vec3 around) {
    for (int index = 0; index < 3; ++index) {
        const float phase = time * 0.55f + static_cast<float>(index) * 2.1f;
        DrawItem tie{};
        tie.mesh = MeshId::TieFighter;
        tie.material = Material::Tie;
        tie.color = {0.50f, 0.56f, 0.62f};
        const Vec3 position =
            around + Vec3{std::sin(phase) * (14.0f + index * 5.0f), std::cos(phase * 1.3f) * 8.0f,
                          -static_cast<float>(index) * 20.0f};
        tie.transform = translation(position) * rotationY(phase * 0.35f) *
                        rotationZ(std::sin(phase) * 0.25f) * scaling(2.1f);

        const float hitCycle = std::fmod(time * 0.55f + static_cast<float>(index) * 0.41f, 1.0f);
        if (hitCycle < 0.86f) {
            addOpaque(frame, tie);

            DrawItem engine{};
            engine.mesh = MeshId::Sphere;
            engine.material = Material::Emissive;
            engine.transform = translation(position) * scaling(0.26f);
            engine.color = {0.25f, 1.0f, 0.45f};
            addAdditive(frame, engine);
        } else {
            // Occasional player hits turn a TIE into a brief expanding flash.
            const float burst = (hitCycle - 0.86f) / 0.14f;
            DrawItem explosion{};
            explosion.mesh = MeshId::Sphere;
            explosion.material = Material::Explosion;
            explosion.transform = translation(position) * scaling(0.35f + burst * 1.6f);
            explosion.color = {1.0f, 0.55f, 0.15f};
            explosion.alpha = (1.0f - burst) * 0.7f;
            explosion.phase = burst;
            addAdditive(frame, explosion);
        }
    }
}

void buildApproach(const BattleState& state, RenderFrame& frame) {
    const float t = state.phaseTime;
    const Vec3 deathStar{20.0f + std::sin(state.elapsedSeconds * 0.08f) * 5.0f,
                         8.0f + std::cos(state.elapsedSeconds * 0.11f) * 2.0f, -170.0f};
    const Vec3 eye{state.steer * 8.0f + std::sin(t * 0.5f) * 1.5f,
                   3.2f + std::sin(t * 0.7f) * 0.35f, 0.0f};
    const Vec3 target = deathStar + Vec3{0.0f, -2.0f, 0.0f};

    setCamera(frame, eye, target, {0.0f, 1.0f, 0.0f});
    addSky(frame);
    addSphere(frame, {-95.0f, -70.0f, -330.0f}, 112.0f, Material::Planet, {0.18f, 0.38f, 0.72f},
              state.elapsedSeconds * 0.01f);
    addSphere(frame, deathStar, 27.0f, Material::DeathStar, {0.55f, 0.57f, 0.60f},
              state.elapsedSeconds * 0.03f);
    addTieFighters(frame, t, {-12.0f, 6.0f, -58.0f});

    const CameraBasis basis = cameraBasis(frame);
    const bool autoFire = std::fmod(t, 0.72f) < 0.09f;
    addPlayerLasers(frame, basis, autoFire || state.firePulse > 0.0f);

    // Enemy fire streaks past the canopy.
    for (int index = 0; index < 2; ++index) {
        const float boltTime = std::fmod(t * 1.25f + static_cast<float>(index) * 0.55f, 1.0f);
        const Vec3 from{18.0f + index * 5.0f, 10.0f - index * 6.0f, -75.0f - index * 20.0f};
        const Vec3 to = from + Vec3{-8.0f, -2.0f, 38.0f} * boltTime;
        addBeam(frame, to, to + Vec3{-2.0f, -0.2f, 7.0f}, 0.07f, {0.4f, 1.0f, 0.35f}, 0.85f);
    }

    frame.flash = std::max(0.0f, 1.0f - t * 5.0f) * 0.15f;
}

void buildTrench(const BattleState& state, RenderFrame& frame) {
    const float t = state.phaseTime;
    const float cameraZ = 20.0f - t * 40.0f;
    const Vec3 eye{state.steer * 1.9f + std::sin(t * 0.9f) * 0.55f,
                   0.25f + std::sin(t * 1.2f) * 0.08f, cameraZ};
    const Vec3 target = eye + Vec3{-0.03f, -0.08f, -1.0f};

    setCamera(frame, eye, target, {0.0f, 1.0f, 0.0f}, 1.08f);
    addSky(frame);
    addSphere(frame, {-100.0f, -55.0f, cameraZ - 360.0f}, 90.0f, Material::Planet,
              {0.18f, 0.40f, 0.74f}, state.elapsedSeconds * 0.01f);

    DrawItem trench{};
    trench.mesh = MeshId::Trench;
    trench.material = Material::Trench;
    addOpaque(frame, trench);

    // Exhaust-port target at the end of the run.
    DrawItem targetPort{};
    targetPort.mesh = MeshId::Box;
    targetPort.material = Material::Emissive;
    targetPort.transform = translation({0.0f, -1.2f, -435.0f}) * scaling({2.2f, 0.16f, 1.4f});
    targetPort.color = {0.25f, 1.0f, 0.45f};
    addOpaque(frame, targetPort);

    // Two towers guarding the port.
    for (int side = -1; side <= 1; side += 2) {
        DrawItem tower{};
        tower.mesh = MeshId::Box;
        tower.material = Material::Lit;
        tower.transform = translation({static_cast<float>(side) * 3.3f, 0.2f, -427.0f}) *
                          scaling({0.5f, 3.3f, 0.5f});
        tower.color = {0.32f, 0.35f, 0.38f};
        addOpaque(frame, tower);
    }

    // TIE fighters weave above the trench.
    for (int index = 0; index < 2; ++index) {
        DrawItem tie{};
        tie.mesh = MeshId::TieFighter;
        tie.material = Material::Tie;
        tie.color = {0.50f, 0.56f, 0.62f};
        tie.transform = translation({-8.0f + static_cast<float>(index) * 16.0f,
                                     9.0f + std::sin(t * 0.9f + index) * 2.0f,
                                     cameraZ - 95.0f - static_cast<float>(index) * 35.0f}) *
                        rotationY(0.8f) * scaling(1.6f);
        addOpaque(frame, tie);
    }

    const CameraBasis basis = cameraBasis(frame);
    const bool autoFire = std::fmod(t, 0.52f) < 0.075f;
    addPlayerLasers(frame, basis, autoFire || state.firePulse > 0.0f);

    // Wall guns fire across the trench.
    for (int index = 0; index < 3; ++index) {
        const float zig = std::fmod(t * 0.7f + static_cast<float>(index) * 0.31f, 1.0f);
        const float z = cameraZ - 18.0f - zig * 60.0f;
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        addBeam(frame, {side * 4.6f, 1.0f, z}, {side * -4.6f, 1.0f, z - 16.0f}, 0.06f,
                {1.0f, 0.25f, 0.15f}, 0.75f);
    }

    // Torpedo shot in the final approach to the port.
    if (t > 9.6f) {
        addBeam(frame, eye + basis.up * -0.5f, eye + basis.forward * 65.0f, 0.2f,
                {0.4f, 1.0f, 0.5f}, 1.0f);
        frame.flash = std::max(frame.flash, (t - 9.6f) * 0.12f);
    }
    frame.flash = std::max(frame.flash, std::max(0.0f, 0.22f - t) * 3.5f);
}

void buildExplosion(const BattleState& state, RenderFrame& frame) {
    const float t = state.phaseTime;
    const Vec3 deathStar{0.0f, 0.0f, -180.0f};
    const Vec3 eye{0.0f, 10.0f - t * 0.25f, 15.0f - t * 0.8f};
    setCamera(frame, eye, deathStar, {0.0f, 1.0f, 0.0f}, 1.05f);
    addSky(frame);
    addSphere(frame, {-100.0f, -68.0f, -380.0f}, 125.0f, Material::Planet, {0.18f, 0.40f, 0.74f},
              state.elapsedSeconds * 0.01f);

    if (t < 0.45f) {
        addSphere(frame, deathStar, 40.0f, Material::DeathStar, {0.35f, 0.36f, 0.39f},
                  state.elapsedSeconds * 0.04f);
    }

    const float expansion = 18.0f + t * 15.0f;
    for (int shell = 0; shell < 3; ++shell) {
        DrawItem burst{};
        burst.mesh = MeshId::Sphere;
        burst.material = Material::Explosion;
        burst.transform = translation(deathStar + Vec3{static_cast<float>(shell - 1) * 2.0f,
                                                       static_cast<float>(shell) * 1.5f, 0.0f}) *
                          scaling(expansion + static_cast<float>(shell) * 6.0f);
        burst.color = shell == 0
                          ? Vec3{1.0f, 0.72f, 0.24f}
                          : (shell == 1 ? Vec3{1.0f, 0.22f, 0.04f} : Vec3{0.25f, 0.45f, 1.0f});
        burst.alpha = std::clamp(1.0f - t / 5.0f, 0.0f, 1.0f) *
                      (shell == 0 ? 0.24f : (shell == 1 ? 0.18f : 0.08f));
        burst.phase = t;
        addAdditive(frame, burst);
    }

    for (int debris = 0; debris < 12; ++debris) {
        const float angle = random01(debris + 13) * kPi * 2.0f;
        const float speed = 18.0f + random01(debris + 71) * 46.0f;
        const Vec3 offset{std::cos(angle) * speed * t, (random01(debris + 22) - 0.35f) * speed * t,
                          std::sin(angle) * speed * t};
        DrawItem fragment{};
        fragment.mesh = MeshId::Box;
        fragment.material = Material::Explosion;
        fragment.transform = translation(deathStar + offset) * rotationY(angle + t) *
                             rotationZ(angle * 0.5f) * scaling(0.8f + random01(debris + 9) * 1.4f);
        fragment.color = {1.0f, 0.5f + random01(debris) * 0.3f, 0.12f};
        fragment.alpha = std::clamp(1.0f - t / 4.5f, 0.0f, 1.0f);
        addAdditive(frame, fragment);
    }

    frame.flash = std::max(0.0f, 1.0f - t * 3.0f);
}

void buildJump(const BattleState& state, RenderFrame& frame) {
    setCamera(frame, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, 1.1f);
    addSky(frame);
    frame.jump = 1.0f;
    frame.flash = std::max(0.0f, 0.35f - state.phaseTime) * 2.5f;

    const CameraBasis basis = cameraBasis(frame);
    for (int index = 0; index < 5; ++index) {
        const float angle = random01(index + 41) * kPi * 2.0f;
        const float radius = 3.0f + random01(index + 5) * 10.0f;
        const Vec3 from = frame.camera.position + basis.right * (std::cos(angle) * radius) +
                          basis.up * (std::sin(angle) * radius) + basis.forward * 12.0f;
        addBeam(frame, from, from + basis.forward * (35.0f + random01(index) * 30.0f), 0.035f,
                {0.55f, 0.82f, 1.0f}, 0.55f);
    }
}

void buildGauntlet(const BattleState& state, RenderFrame& frame) {
    const float t = state.phaseTime;
    const float cameraZ = -t * 9.0f;
    const Vec3 eye{state.steer * 10.0f + std::sin(t * 0.55f) * 3.0f,
                   2.0f + std::sin(t * 0.9f) * 1.2f, cameraZ};
    const Vec3 up{std::sin(t * 0.42f) * -0.12f, 1.0f, 0.0f};
    const Vec3 target =
        eye + Vec3{std::sin(t * 0.45f) * 0.16f - state.steer * 0.08f, -0.03f, -1.0f};

    setCamera(frame, eye, target, up, 1.04f);
    addSky(frame);
    addSphere(frame, {-140.0f, -80.0f, cameraZ - 380.0f}, 110.0f, Material::Planet,
              {0.16f, 0.34f, 0.66f}, state.elapsedSeconds * 0.01f);

    // Star Destroyer hanging above the escape corridor.
    DrawItem starDestroyer{};
    starDestroyer.mesh = MeshId::StarDestroyer;
    starDestroyer.material = Material::StarDestroyer;
    starDestroyer.color = {0.56f, 0.59f, 0.63f};
    starDestroyer.transform = translation({48.0f, 28.0f, cameraZ - 250.0f}) * rotationY(0.65f) *
                              rotationZ(-0.05f) * scaling(1.9f);
    addOpaque(frame, starDestroyer);

    // Deterministic asteroid field: positions wrap as the camera advances.
    for (int index = 0; index < 18; ++index) {
        const float lane = random01(index + 3) * 56.0f - 28.0f;
        const float height = random01(index + 17) * 26.0f - 12.0f;
        const float travel = std::fmod(random01(index + 50) * 210.0f - t * 22.0f + 420.0f, 210.0f);
        const Vec3 position{lane + std::sin(t * 0.4f + index) * 2.0f,
                            height + std::cos(t * 0.5f + index) * 1.5f, cameraZ - 25.0f - travel};
        DrawItem asteroid{};
        asteroid.mesh = MeshId::Asteroid;
        asteroid.material = Material::Asteroid;
        asteroid.color = {0.40f, 0.36f, 0.31f};
        asteroid.transform = translation(position) *
                             rotationY(random01(index + 8) * kPi * 2.0f + t * 0.35f) *
                             rotationX(t * 0.2f + random01(index + 12) * kPi) *
                             scaling(1.5f + random01(index + 4) * 3.6f);
        addOpaque(frame, asteroid);
    }

    addTieFighters(frame, t, {-25.0f, 8.0f, cameraZ - 80.0f});

    const CameraBasis basis = cameraBasis(frame);
    const bool autoFire = std::fmod(t, 0.46f) < 0.07f;
    addPlayerLasers(frame, basis, autoFire || state.firePulse > 0.0f);

    // Star Destroyer turbolaser fire converges on the cockpit.
    for (int index = 0; index < 5; ++index) {
        const float cycle = std::fmod(t * 0.55f + static_cast<float>(index) * 0.19f, 1.0f);
        const Vec3 from{30.0f + static_cast<float>(index) * 4.0f,
                        22.0f + std::sin(static_cast<float>(index) * 2.4f) * 6.0f,
                        cameraZ - 190.0f};
        const Vec3 to = from + normalize(eye - from) * (cycle * 140.0f);
        addBeam(frame, to, to + normalize(eye - from) * 9.0f, 0.10f, {0.35f, 1.0f, 0.42f}, 0.9f);
    }

    frame.flash = std::max(0.0f, 0.18f - t) * 2.0f;
}

}  // namespace

void SceneBuilder::build(const BattleState& state, RenderFrame& frame) {
    frame.opaque.clear();
    frame.additive.clear();
    frame.time = state.elapsedSeconds;
    frame.phaseTime = state.phaseTime;
    frame.phase = static_cast<int>(state.phase);
    frame.jump = 0.0f;
    frame.flash = 0.0f;
    frame.steer = state.steer;
    frame.firing = state.firePulse;

    switch (state.phase) {
        case BattlePhase::Approach:
            buildApproach(state, frame);
            break;
        case BattlePhase::TrenchRun:
            buildTrench(state, frame);
            break;
        case BattlePhase::Explosion:
            buildExplosion(state, frame);
            break;
        case BattlePhase::Jump:
            buildJump(state, frame);
            break;
        case BattlePhase::Gauntlet:
            buildGauntlet(state, frame);
            break;
        case BattlePhase::Count:
            break;
    }
}

}  // namespace spacebattle

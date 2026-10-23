#pragma once

#include <cstdint>

namespace cppgames::planets {

enum class PlanetType : int {
    Terran = 0,
    Desert,
    Arctic,
    Volcanic,
    GasGiant,
    Pockmarked,
    Toxic,
};

struct PlanetSpec {
    std::uint64_t seed{};
    PlanetType type{PlanetType::Terran};
    float time{0.0f};
    float radius{1.15f};
    float spinSpeed{0.05f};
    float tiltX{0.12f};
    float tiltZ{0.30f};
    float noiseScale{2.4f};
    float warp{0.55f};
    float iceCoverage{0.25f};
    float cloudCoverage{0.52f};
    float cloudSpeed{0.045f};
    float craterDensity{0.0f};
    float volcanicActivity{0.0f};
    float cityLights{0.0f};
    float specular{0.55f};
    float atmosphereDensity{0.65f};
    float colorA[3]{0.10f, 0.30f, 0.55f};
    float colorB[3]{0.22f, 0.42f, 0.18f};
    float colorC[3]{0.62f, 0.48f, 0.30f};
    float colorD[3]{0.92f, 0.96f, 1.0f};
    float atmosphereColor[3]{0.38f, 0.66f, 1.0f};
    float sunDirection[3]{-0.45f, 0.35f, 0.82f};
    bool hasRings{false};
    float ringInner{1.55f};
    float ringOuter{2.55f};
    float ringOpacity{0.6f};
    float ringTilt{0.30f};
    float ringColorA[3]{0.72f, 0.65f, 0.54f};
    float ringColorB[3]{0.35f, 0.30f, 0.25f};
};

// The planet both the game and the lab open with. Space in the lab explores
// beyond it.
inline constexpr std::uint64_t kShowcasePlanetSeed = 0x0DDA9BF55BA7CF8Bull;

[[nodiscard]] PlanetSpec generatePlanet(std::uint64_t seed);
[[nodiscard]] const char* planetTypeName(PlanetType type);
[[nodiscard]] std::uint64_t nextSeed(std::uint64_t current);

}  // namespace cppgames::planets

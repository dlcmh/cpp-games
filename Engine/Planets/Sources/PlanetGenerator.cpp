#include "Planets/PlanetSpec.hpp"

#include <cmath>
#include <cstdint>
#include <random>

namespace cppgames::planets {
namespace {

struct Rng {
    std::mt19937 engine;

    explicit Rng(std::uint64_t seed) : engine(static_cast<std::uint32_t>(seed ^ (seed >> 32))) {}

    float next() {
        return static_cast<float>(engine() & 0x00FFFFFFu) / 16777216.0f;
    }

    float range(float low, float high) {
        return low + (high - low) * next();
    }

    int rangeInt(int low, int high) {
        return low + static_cast<int>(next() * static_cast<float>(high - low + 1));
    }
};

void hsvToRgb(float h, float s, float v, float out[3]) {
    h = h - std::floor(h);
    const float i = std::floor(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    switch (static_cast<int>(i) % 6) {
        case 0:
            out[0] = v;
            out[1] = t;
            out[2] = p;
            break;
        case 1:
            out[0] = q;
            out[1] = v;
            out[2] = p;
            break;
        case 2:
            out[0] = p;
            out[1] = v;
            out[2] = t;
            break;
        case 3:
            out[0] = p;
            out[1] = q;
            out[2] = v;
            break;
        case 4:
            out[0] = t;
            out[1] = p;
            out[2] = v;
            break;
        default:
            out[0] = v;
            out[1] = p;
            out[2] = q;
            break;
    }
}

}  // namespace

PlanetSpec generatePlanet(std::uint64_t seed) {
    Rng rng(seed);
    PlanetSpec spec{};
    spec.seed = seed;

    const int roll = rng.rangeInt(0, 99);
    if (roll < 24) {
        spec.type = PlanetType::Terran;
    } else if (roll < 40) {
        spec.type = PlanetType::Desert;
    } else if (roll < 55) {
        spec.type = PlanetType::Arctic;
    } else if (roll < 70) {
        spec.type = PlanetType::Volcanic;
    } else if (roll < 84) {
        spec.type = PlanetType::GasGiant;
    } else if (roll < 94) {
        spec.type = PlanetType::Pockmarked;
    } else {
        spec.type = PlanetType::Toxic;
    }

    spec.radius = 1.15f;
    spec.spinSpeed = rng.range(0.025f, 0.085f) * (rng.next() > 0.5f ? 1.0f : -1.0f);
    spec.tiltX = rng.range(-0.30f, 0.30f);
    spec.tiltZ = rng.range(-0.55f, 0.55f);
    spec.noiseScale = rng.range(1.8f, 4.2f);
    spec.warp = rng.range(0.25f, 0.95f);
    spec.iceCoverage = rng.range(0.0f, 0.35f);
    spec.cloudCoverage = rng.range(0.25f, 0.72f);
    spec.cloudSpeed = rng.range(0.015f, 0.065f);
    spec.specular = rng.range(0.35f, 0.75f);
    spec.atmosphereDensity = rng.range(0.45f, 0.95f);
    spec.craterDensity = 0.0f;
    spec.volcanicActivity = 0.0f;
    spec.cityLights = 0.0f;

    const float atmosphereHue = rng.range(0.52f, 0.63f);
    hsvToRgb(atmosphereHue, rng.range(0.55f, 0.82f), rng.range(0.70f, 1.0f), spec.atmosphereColor);

    switch (spec.type) {
        case PlanetType::Terran: {
            hsvToRgb(rng.range(0.54f, 0.62f), rng.range(0.62f, 0.85f), rng.range(0.18f, 0.38f),
                     spec.colorA);
            hsvToRgb(rng.range(0.20f, 0.36f), rng.range(0.30f, 0.65f), rng.range(0.28f, 0.58f),
                     spec.colorB);
            hsvToRgb(rng.range(0.06f, 0.14f), rng.range(0.30f, 0.70f), rng.range(0.58f, 0.85f),
                     spec.colorC);
            hsvToRgb(0.55f, 0.10f, 0.97f, spec.colorD);
            spec.iceCoverage = rng.range(0.10f, 0.55f);
            spec.cloudCoverage = rng.range(0.35f, 0.78f);
            spec.cityLights = rng.range(0.30f, 0.90f);
            spec.specular = rng.range(0.55f, 0.95f);
            spec.atmosphereDensity = rng.range(0.65f, 1.05f);
            break;
        }
        case PlanetType::Desert: {
            hsvToRgb(rng.range(0.05f, 0.12f), rng.range(0.45f, 0.78f), rng.range(0.55f, 0.88f),
                     spec.colorA);
            hsvToRgb(rng.range(0.03f, 0.10f), rng.range(0.40f, 0.70f), rng.range(0.25f, 0.52f),
                     spec.colorB);
            hsvToRgb(rng.range(0.08f, 0.15f), rng.range(0.20f, 0.45f), rng.range(0.65f, 0.90f),
                     spec.colorC);
            spec.iceCoverage = rng.range(0.0f, 0.15f);
            spec.cloudCoverage = rng.range(0.05f, 0.35f);
            spec.craterDensity = rng.range(0.10f, 0.35f);
            spec.atmosphereDensity = rng.range(0.35f, 0.65f);
            break;
        }
        case PlanetType::Arctic: {
            hsvToRgb(rng.range(0.52f, 0.62f), rng.range(0.05f, 0.20f), rng.range(0.80f, 1.0f),
                     spec.colorA);
            hsvToRgb(rng.range(0.54f, 0.65f), rng.range(0.35f, 0.70f), rng.range(0.25f, 0.55f),
                     spec.colorB);
            hsvToRgb(rng.range(0.50f, 0.60f), rng.range(0.10f, 0.35f), rng.range(0.65f, 0.90f),
                     spec.colorC);
            spec.iceCoverage = rng.range(0.65f, 0.98f);
            spec.cloudCoverage = rng.range(0.10f, 0.40f);
            spec.craterDensity = rng.range(0.05f, 0.25f);
            spec.specular = rng.range(0.45f, 0.85f);
            spec.atmosphereDensity = rng.range(0.35f, 0.75f);
            break;
        }
        case PlanetType::Volcanic: {
            hsvToRgb(rng.range(0.0f, 0.10f), rng.range(0.08f, 0.25f), rng.range(0.035f, 0.12f),
                     spec.colorA);
            hsvToRgb(rng.range(0.015f, 0.08f), 1.0f, rng.range(0.85f, 1.0f), spec.colorB);
            hsvToRgb(rng.range(0.04f, 0.10f), rng.range(0.10f, 0.35f), rng.range(0.20f, 0.42f),
                     spec.colorC);
            spec.iceCoverage = 0.0f;
            spec.cloudCoverage = rng.range(0.05f, 0.30f);
            spec.craterDensity = rng.range(0.45f, 0.90f);
            spec.volcanicActivity = rng.range(0.55f, 1.0f);
            spec.atmosphereDensity = rng.range(0.25f, 0.65f);
            hsvToRgb(rng.range(0.02f, 0.08f), 0.75f, 0.9f, spec.atmosphereColor);
            break;
        }
        case PlanetType::GasGiant: {
            hsvToRgb(rng.range(0.04f, 0.15f), rng.range(0.30f, 0.65f), rng.range(0.65f, 0.95f),
                     spec.colorA);
            hsvToRgb(rng.range(0.02f, 0.12f), rng.range(0.35f, 0.70f), rng.range(0.42f, 0.75f),
                     spec.colorB);
            hsvToRgb(rng.range(0.55f, 0.68f), rng.range(0.20f, 0.55f), rng.range(0.45f, 0.80f),
                     spec.colorC);
            spec.colorD[0] = 0.95f;
            spec.colorD[1] = 0.86f;
            spec.colorD[2] = 0.68f;
            spec.iceCoverage = 0.0f;
            spec.cloudCoverage = 1.0f;
            spec.cityLights = 0.0f;
            spec.specular = 0.08f;
            spec.atmosphereDensity = rng.range(0.85f, 1.25f);
            spec.craterDensity = 0.0f;
            break;
        }
        case PlanetType::Pockmarked: {
            hsvToRgb(rng.range(0.0f, 0.12f), rng.range(0.01f, 0.10f), rng.range(0.32f, 0.66f),
                     spec.colorA);
            hsvToRgb(rng.range(0.0f, 0.12f), rng.range(0.0f, 0.08f), rng.range(0.16f, 0.34f),
                     spec.colorB);
            hsvToRgb(rng.range(0.0f, 0.12f), rng.range(0.02f, 0.12f), rng.range(0.55f, 0.82f),
                     spec.colorC);
            spec.iceCoverage = 0.0f;
            spec.cloudCoverage = 0.0f;
            spec.craterDensity = rng.range(0.75f, 1.0f);
            spec.atmosphereDensity = 0.0f;
            spec.specular = 0.05f;
            break;
        }
        case PlanetType::Toxic: {
            hsvToRgb(rng.range(0.18f, 0.30f), rng.range(0.55f, 0.90f), rng.range(0.35f, 0.65f),
                     spec.colorA);
            hsvToRgb(rng.range(0.10f, 0.20f), rng.range(0.60f, 0.95f), rng.range(0.60f, 0.90f),
                     spec.colorB);
            hsvToRgb(rng.range(0.25f, 0.40f), rng.range(0.40f, 0.75f), rng.range(0.15f, 0.35f),
                     spec.colorC);
            spec.iceCoverage = 0.0f;
            spec.cloudCoverage = rng.range(0.62f, 0.95f);
            spec.atmosphereDensity = rng.range(0.85f, 1.25f);
            spec.craterDensity = 0.0f;
            hsvToRgb(rng.range(0.15f, 0.25f), 0.75f, 0.95f, spec.atmosphereColor);
            break;
        }
    }

    const float ringChance = spec.type == PlanetType::GasGiant     ? 0.72f
                             : spec.type == PlanetType::Pockmarked ? 0.10f
                                                                   : 0.36f;
    spec.hasRings = rng.next() < ringChance;
    if (spec.hasRings) {
        spec.ringInner = rng.range(1.45f, 1.80f);
        spec.ringOuter = spec.ringInner + rng.range(0.70f, 1.25f);
        spec.ringOpacity = rng.range(0.45f, 0.85f);
        spec.ringTilt = rng.range(-0.65f, 0.65f);
        hsvToRgb(rng.range(0.07f, 0.14f), rng.range(0.10f, 0.45f), rng.range(0.68f, 0.98f),
                 spec.ringColorA);
        hsvToRgb(rng.range(0.04f, 0.11f), rng.range(0.15f, 0.50f), rng.range(0.35f, 0.60f),
                 spec.ringColorB);
    }

    // Keep the key light on the camera-facing hemisphere so the planet reads
    // immediately instead of starting as a dark crescent.
    const float side = rng.next() < 0.5f ? -1.0f : 1.0f;
    const float sunX = side * rng.range(0.55f, 0.85f);
    const float sunY = rng.range(0.15f, 0.45f);
    const float sunZ = rng.range(0.45f, 0.70f);
    const float sunLength = std::sqrt(sunX * sunX + sunY * sunY + sunZ * sunZ);
    spec.sunDirection[0] = sunX / sunLength;
    spec.sunDirection[1] = sunY / sunLength;
    spec.sunDirection[2] = sunZ / sunLength;

    return spec;
}

std::uint64_t randomPlanetSeed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32) ^ static_cast<std::uint64_t>(device());
}

const char* planetTypeName(PlanetType type) {
    switch (type) {
        case PlanetType::Terran:
            return "Terran";
        case PlanetType::Desert:
            return "Desert";
        case PlanetType::Arctic:
            return "Arctic";
        case PlanetType::Volcanic:
            return "Volcanic";
        case PlanetType::GasGiant:
            return "Gas giant";
        case PlanetType::Pockmarked:
            return "Pockmarked";
        case PlanetType::Toxic:
            return "Toxic";
    }
    return "Unknown";
}

std::uint64_t nextSeed(std::uint64_t current) {
    std::uint64_t z = current + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

}  // namespace cppgames::planets

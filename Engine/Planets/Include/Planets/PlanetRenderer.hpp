#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <memory>
#include <string>

#include "Planets/PlanetSpec.hpp"

namespace cppgames::planets {

struct PlanetDraw {
    float viewProjection[16]{};
    float model[16]{};
    float ringModel[16]{};
    float cameraPosition[3]{};
    float sunDirection[3]{0.0f, 1.0f, 0.0f};
    float time{0.0f};
    bool drawRings{false};
    bool drawAtmosphere{true};
};

// Shared macOS renderer for generated planets. The caller owns the render pass
// and depth attachment; this module draws the opaque planet, its atmosphere,
// and optional rings.
class PlanetRenderer {
public:
    PlanetRenderer();
    ~PlanetRenderer();
    PlanetRenderer(const PlanetRenderer&) = delete;
    PlanetRenderer& operator=(const PlanetRenderer&) = delete;

    bool build(MTL::Device* device, const std::string& shaderDirectory, bool depthAttachment,
               std::string& error);

    void encode(MTL::RenderCommandEncoder* encoder, const PlanetSpec& spec,
                const PlanetDraw& draw) const;

    [[nodiscard]] bool valid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cppgames::planets

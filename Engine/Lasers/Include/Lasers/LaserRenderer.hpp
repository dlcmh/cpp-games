#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <string>
#include <vector>

#include "Lasers/LaserTypes.hpp"

namespace cppgames::lasers {

// Reusable macOS renderer for camera-facing additive laser bolts. The module
// owns its pipeline/depth state and draws an instanced quad per beam.
class LaserRenderer {
public:
    bool build(MTL::Device* device, const std::string& shaderPath, bool depthAttachment,
               std::string& error);

    void encode(MTL::RenderCommandEncoder* encoder, const LaserFrame& frame,
                const std::vector<LaserBeam>& beams) const;

    [[nodiscard]] bool valid() const;

private:
    NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
    NS::SharedPtr<MTL::DepthStencilState> depthState_;
};

}  // namespace cppgames::lasers

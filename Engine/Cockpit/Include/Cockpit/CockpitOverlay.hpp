#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <string>

#include "Cockpit/CockpitState.hpp"

namespace cppgames::cockpit {

// Reusable macOS overlay renderer. It compiles the shared cockpit Slang output,
// owns the pipeline/depth state, and encodes one fullscreen triangle. Games are
// responsible for the surrounding render pass and for blitting/presenting.
class CockpitOverlay {
public:
    bool build(MTL::Device* device, const std::string& shaderPath, bool depthAttachment,
               std::string& error);

    void encode(MTL::RenderCommandEncoder* encoder, const CockpitState& state, int pixelWidth,
                int pixelHeight) const;

    [[nodiscard]] bool valid() const;

private:
    NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
    NS::SharedPtr<MTL::DepthStencilState> depthState_;
};

}  // namespace cppgames::cockpit

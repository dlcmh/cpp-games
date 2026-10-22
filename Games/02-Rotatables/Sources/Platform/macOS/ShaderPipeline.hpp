#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <string>

namespace rotatables {

// Compiles the build-time-generated Metal source and owns the render pipeline.
// Kept separate from MetalRenderer so device/layer management and shader
// compilation stay focused on one job each.
class ShaderPipeline {
public:
    bool build(MTL::Device* device, const std::string& shaderPath, std::string& error);

    [[nodiscard]] MTL::RenderPipelineState* state() const;

private:
    NS::SharedPtr<MTL::RenderPipelineState> pipeline_;
};

}  // namespace rotatables

#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <string>

namespace spacebattle {

// Owns the three render states needed by the frame:
//   opaque 3D and additive effects (lasers/explosion). The cockpit overlay
//   lives in the shared Engine/Cockpit module.
class Pipelines {
public:
    bool build(MTL::Device* device, const std::string& shaderDirectory, std::string& error);

    [[nodiscard]] MTL::RenderPipelineState* opaque() const;
    [[nodiscard]] MTL::RenderPipelineState* additive() const;
    [[nodiscard]] MTL::DepthStencilState* sceneDepth() const;
    [[nodiscard]] MTL::DepthStencilState* effectDepth() const;

private:
    NS::SharedPtr<MTL::RenderPipelineState> opaque_;
    NS::SharedPtr<MTL::RenderPipelineState> additive_;
    NS::SharedPtr<MTL::DepthStencilState> sceneDepth_;
    NS::SharedPtr<MTL::DepthStencilState> effectDepth_;
};

}  // namespace spacebattle

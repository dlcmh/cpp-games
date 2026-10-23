#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <string>

namespace spacebattle {

// Owns the three render states needed by the frame:
//   opaque 3D, additive effects (lasers/explosion), and the cockpit overlay.
class Pipelines {
public:
    bool build(MTL::Device* device, const std::string& shaderDirectory, std::string& error);

    [[nodiscard]] MTL::RenderPipelineState* opaque() const;
    [[nodiscard]] MTL::RenderPipelineState* additive() const;
    [[nodiscard]] MTL::RenderPipelineState* cockpit() const;
    [[nodiscard]] MTL::DepthStencilState* sceneDepth() const;
    [[nodiscard]] MTL::DepthStencilState* effectDepth() const;
    [[nodiscard]] MTL::DepthStencilState* overlayDepth() const;

private:
    NS::SharedPtr<MTL::RenderPipelineState> opaque_;
    NS::SharedPtr<MTL::RenderPipelineState> additive_;
    NS::SharedPtr<MTL::RenderPipelineState> cockpit_;
    NS::SharedPtr<MTL::DepthStencilState> sceneDepth_;
    NS::SharedPtr<MTL::DepthStencilState> effectDepth_;
    NS::SharedPtr<MTL::DepthStencilState> overlayDepth_;
};

}  // namespace spacebattle

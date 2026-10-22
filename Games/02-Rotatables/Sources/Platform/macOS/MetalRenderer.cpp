#include "Platform/macOS/MetalRenderer.hpp"

#include "Platform/macOS/SceneUniforms.hpp"
#include "Platform/macOS/ShaderPipeline.hpp"
#include "Render/IRenderer.hpp"
#include "Render/RenderFrame.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <CoreGraphics/CoreGraphics.h>

#include <memory>
#include <string>

namespace rotatables {
namespace {

class MetalRenderer final : public IRenderer {
public:
    MetalRenderer() = default;
    ~MetalRenderer() override = default;

    bool initialize(const NativeSurface& surface, const std::string& shaderPath,
                    std::string& error) override;
    void resize(int pixelWidth, int pixelHeight) override;
    bool draw(const RenderFrame& frame, std::string& error) override;

private:
    NS::SharedPtr<MTL::Device> device_;
    NS::SharedPtr<MTL::CommandQueue> commandQueue_;
    ShaderPipeline shader_;
    CA::MetalLayer* layer_{nullptr};
    int pixelWidth_{0};
    int pixelHeight_{0};
};

bool MetalRenderer::initialize(const NativeSurface& surface, const std::string& shaderPath,
                               std::string& error) {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    if (surface.metalLayer == nullptr) {
        error = "SDL did not provide a Metal layer for the window.";
        pool->release();
        return false;
    }

    layer_ = static_cast<CA::MetalLayer*>(surface.metalLayer);
    device_ = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
    if (!device_) {
        error = "No Metal-capable GPU was found.";
        pool->release();
        return false;
    }

    commandQueue_ = NS::TransferPtr(device_->newCommandQueue());
    if (!commandQueue_) {
        error = "Could not create a Metal command queue.";
        pool->release();
        return false;
    }

    layer_->setDevice(device_.get());
    layer_->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer_->setFramebufferOnly(true);
    layer_->setDisplaySyncEnabled(true);
    layer_->setMaximumDrawableCount(3);

    resize(surface.pixelWidth, surface.pixelHeight);

    const bool built = shader_.build(device_.get(), shaderPath, error);
    pool->release();
    return built;
}

void MetalRenderer::resize(int pixelWidth, int pixelHeight) {
    if (layer_ == nullptr || pixelWidth <= 0 || pixelHeight <= 0) {
        return;
    }
    if (pixelWidth == pixelWidth_ && pixelHeight == pixelHeight_) {
        return;
    }

    pixelWidth_ = pixelWidth;
    pixelHeight_ = pixelHeight;
    layer_->setDrawableSize(
        CGSizeMake(static_cast<CGFloat>(pixelWidth), static_cast<CGFloat>(pixelHeight)));
}

bool MetalRenderer::draw(const RenderFrame& frame, std::string& error) {
    if (frame.pixelWidth <= 0 || frame.pixelHeight <= 0) {
        return true;
    }

    resize(frame.pixelWidth, frame.pixelHeight);

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    CA::MetalDrawable* drawable = layer_->nextDrawable();
    if (drawable == nullptr) {
        pool->release();
        return true;
    }

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* attachment = pass->colorAttachments()->object(0);
    attachment->setTexture(drawable->texture());
    attachment->setLoadAction(MTL::LoadActionClear);
    attachment->setStoreAction(MTL::StoreActionStore);
    attachment->setClearColor(MTL::ClearColor(0.008, 0.012, 0.030, 1.0));

    SceneUniforms uniforms{};
    uniforms.resolution[0] = static_cast<float>(pixelWidth_);
    uniforms.resolution[1] = static_cast<float>(pixelHeight_);
    uniforms.time = frame.elapsedSeconds;
    uniforms.angleA = frame.characters[0].angleRadians;
    uniforms.angleB = frame.characters[1].angleRadians;
    uniforms.selectedIndex = static_cast<float>(frame.selectedIndex);
    uniforms.skinA = static_cast<float>(frame.characters[0].skin);
    uniforms.skinB = static_cast<float>(frame.characters[1].skin);

    MTL::CommandBuffer* commandBuffer = commandQueue_->commandBuffer();
    if (commandBuffer == nullptr) {
        pass->release();
        pool->release();
        error = "Could not create a Metal command buffer.";
        return false;
    }

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    encoder->setRenderPipelineState(shader_.state());
    encoder->setVertexBytes(&uniforms, sizeof(uniforms), 0);
    encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(3));
    encoder->endEncoding();

    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();

    pass->release();
    pool->release();
    return true;
}

}  // namespace

std::unique_ptr<IRenderer> createMetalRenderer() {
    return std::make_unique<MetalRenderer>();
}

}  // namespace rotatables

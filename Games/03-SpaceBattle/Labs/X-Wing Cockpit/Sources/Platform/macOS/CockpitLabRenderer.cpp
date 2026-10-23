#include "Platform/macOS/CockpitLabRenderer.hpp"

#include "Cockpit/CockpitOverlay.hpp"
#include "Cockpit/CockpitState.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <memory>
#include <string>

namespace cockpitlab {

struct CockpitLabRenderer::Impl {
    NS::SharedPtr<MTL::Device> device;
    NS::SharedPtr<MTL::CommandQueue> commandQueue;
    cppgames::cockpit::CockpitOverlay cockpit;
    CA::MetalLayer* layer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

CockpitLabRenderer::CockpitLabRenderer() : impl_(std::make_unique<Impl>()) {}

CockpitLabRenderer::~CockpitLabRenderer() = default;

bool CockpitLabRenderer::initialize(const NativeSurface& surface,
                                    const std::string& shaderDirectory, std::string& error) {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    if (surface.metalLayer == nullptr) {
        error = "SDL did not provide a Metal layer.";
        pool->release();
        return false;
    }

    impl_->layer = static_cast<CA::MetalLayer*>(surface.metalLayer);
    impl_->device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
    if (!impl_->device) {
        error = "No Metal-capable GPU was found.";
        pool->release();
        return false;
    }

    impl_->commandQueue = NS::TransferPtr(impl_->device->newCommandQueue());
    if (!impl_->commandQueue) {
        error = "Could not create a Metal command queue.";
        pool->release();
        return false;
    }

    impl_->layer->setDevice(impl_->device.get());
    impl_->layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    impl_->layer->setFramebufferOnly(true);
    impl_->layer->setDisplaySyncEnabled(true);
    impl_->layer->setMaximumDrawableCount(3);
    resize(surface.pixelWidth, surface.pixelHeight);

    if (!impl_->cockpit.build(impl_->device.get(), shaderDirectory + "/cockpit.metal", false,
                              error)) {
        pool->release();
        return false;
    }

    pool->release();
    return true;
}

void CockpitLabRenderer::resize(int pixelWidth, int pixelHeight) {
    if (impl_->layer == nullptr || pixelWidth <= 0 || pixelHeight <= 0) {
        return;
    }
    if (pixelWidth == impl_->pixelWidth && pixelHeight == impl_->pixelHeight) {
        return;
    }

    impl_->pixelWidth = pixelWidth;
    impl_->pixelHeight = pixelHeight;
    impl_->layer->setDrawableSize(
        CGSizeMake(static_cast<CGFloat>(pixelWidth), static_cast<CGFloat>(pixelHeight)));
}

bool CockpitLabRenderer::draw(const cppgames::cockpit::CockpitState& cockpitState,
                              std::string& error) {
    if (impl_->pixelWidth <= 0 || impl_->pixelHeight <= 0) {
        return true;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    CA::MetalDrawable* drawable = impl_->layer->nextDrawable();
    if (drawable == nullptr) {
        pool->release();
        return true;
    }

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* attachment = pass->colorAttachments()->object(0);
    attachment->setTexture(drawable->texture());
    attachment->setLoadAction(MTL::LoadActionClear);
    attachment->setStoreAction(MTL::StoreActionStore);
    attachment->setClearColor(MTL::ClearColor(0.008, 0.009, 0.012, 1.0));

    MTL::CommandBuffer* commandBuffer = impl_->commandQueue->commandBuffer();
    if (commandBuffer == nullptr) {
        pass->release();
        pool->release();
        error = "Could not create a Metal command buffer.";
        return false;
    }

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    impl_->cockpit.encode(encoder, cockpitState, impl_->pixelWidth, impl_->pixelHeight);

    encoder->endEncoding();
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();

    pass->release();
    pool->release();
    return true;
}

}  // namespace cockpitlab

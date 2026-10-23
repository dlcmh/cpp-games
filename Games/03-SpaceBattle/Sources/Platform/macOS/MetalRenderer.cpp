#include "Platform/macOS/MetalRenderer.hpp"

#include "Cockpit/CockpitOverlay.hpp"
#include "Cockpit/CockpitState.hpp"
#include "Lasers/LaserRenderer.hpp"
#include "Lasers/LaserTypes.hpp"
#include "Platform/macOS/MeshLibrary.hpp"
#include "Platform/macOS/Pipelines.hpp"
#include "Platform/macOS/SceneUniforms.hpp"
#include "Render/IRenderer.hpp"
#include "Render/Math.hpp"
#include "Render/MeshId.hpp"
#include "Render/RenderFrame.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace spacebattle {
namespace {

class MetalRenderer final : public IRenderer {
public:
    MetalRenderer() = default;
    ~MetalRenderer() override = default;

    bool initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                    std::string& error) override;
    void resize(int pixelWidth, int pixelHeight) override;
    bool draw(const RenderFrame& frame, std::string& error) override;

private:
    bool createBuffers(std::string& error);
    bool createDepthTexture(int width, int height);

    NS::SharedPtr<MTL::Device> device_;
    NS::SharedPtr<MTL::CommandQueue> commandQueue_;
    NS::SharedPtr<MTL::Buffer> vertexBuffer_;
    NS::SharedPtr<MTL::Buffer> indexBuffer_;
    NS::SharedPtr<MTL::Texture> depthTexture_;
    std::array<MeshRange, static_cast<std::size_t>(MeshId::Count)> meshRanges_{};
    Pipelines pipelines_;
    cppgames::cockpit::CockpitOverlay cockpit_;
    cppgames::lasers::LaserRenderer lasers_;
    CA::MetalLayer* layer_{nullptr};
    int pixelWidth_{0};
    int pixelHeight_{0};
};

bool MetalRenderer::initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                               std::string& error) {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    if (surface.metalLayer == nullptr) {
        error = "SDL did not provide a Metal layer.";
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

    if (!createBuffers(error) || !pipelines_.build(device_.get(), shaderDirectory, error)) {
        pool->release();
        return false;
    }

    if (!cockpit_.build(device_.get(), shaderDirectory + "/cockpit.metal", true, error)) {
        pool->release();
        return false;
    }
    if (!lasers_.build(device_.get(), shaderDirectory + "/laser.metal", true, error)) {
        pool->release();
        return false;
    }

    pool->release();
    return true;
}

bool MetalRenderer::createBuffers(std::string& error) {
    const MeshBatch batch = buildMeshBatch();
    meshRanges_ = batch.ranges;

    if (batch.vertices.empty() || batch.indices.empty()) {
        error = "The procedural mesh builder produced no geometry.";
        return false;
    }

    vertexBuffer_ = NS::TransferPtr(device_->newBuffer(batch.vertices.data(),
                                                       batch.vertices.size() * sizeof(MeshVertex),
                                                       MTL::ResourceStorageModeShared));
    indexBuffer_ = NS::TransferPtr(device_->newBuffer(batch.indices.data(),
                                                      batch.indices.size() * sizeof(std::uint32_t),
                                                      MTL::ResourceStorageModeShared));

    if (!vertexBuffer_ || !indexBuffer_) {
        error = "Could not allocate Metal mesh buffers.";
        return false;
    }
    return true;
}

bool MetalRenderer::createDepthTexture(int width, int height) {
    if (!device_ || width <= 0 || height <= 0) {
        return false;
    }

    MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatDepth32Float, static_cast<NS::UInteger>(width),
        static_cast<NS::UInteger>(height), false);
    descriptor->setUsage(MTL::TextureUsageRenderTarget);
    descriptor->setStorageMode(MTL::StorageModePrivate);
    depthTexture_ = NS::TransferPtr(device_->newTexture(descriptor));
    return depthTexture_.get() != nullptr;
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
    createDepthTexture(pixelWidth, pixelHeight);
}

bool MetalRenderer::draw(const RenderFrame& frame, std::string& error) {
    if (frame.pixelWidth <= 0 || frame.pixelHeight <= 0) {
        return true;
    }

    resize(frame.pixelWidth, frame.pixelHeight);
    if (!depthTexture_ || !pipelines_.opaque()) {
        error = "Renderer resources are not initialized.";
        return false;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    CA::MetalDrawable* drawable = layer_->nextDrawable();
    if (drawable == nullptr) {
        pool->release();
        return true;
    }

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* colorAttachment = pass->colorAttachments()->object(0);
    colorAttachment->setTexture(drawable->texture());
    colorAttachment->setLoadAction(MTL::LoadActionClear);
    colorAttachment->setStoreAction(MTL::StoreActionStore);
    colorAttachment->setClearColor(MTL::ClearColor(0.002, 0.004, 0.012, 1.0));
    MTL::RenderPassDepthAttachmentDescriptor* depthAttachment = pass->depthAttachment();
    depthAttachment->setTexture(depthTexture_.get());
    depthAttachment->setLoadAction(MTL::LoadActionClear);
    depthAttachment->setStoreAction(MTL::StoreActionDontCare);
    depthAttachment->setClearDepth(1.0);

    const float aspect = static_cast<float>(pixelWidth_) / static_cast<float>(pixelHeight_);
    const Mat4 view = lookAt(frame.camera.position, frame.camera.position + frame.camera.forward,
                             frame.camera.up);
    const Mat4 projection = perspective(frame.camera.fovYRadians, aspect, 0.1f, 1500.0f);

    FrameUniforms frameUniforms{};
    frameUniforms.viewProjection = projection * view;
    frameUniforms.cameraPosition[0] = frame.camera.position.x;
    frameUniforms.cameraPosition[1] = frame.camera.position.y;
    frameUniforms.cameraPosition[2] = frame.camera.position.z;
    const Vec3 light = normalize(Vec3{-0.35f, 0.80f, 0.45f});
    frameUniforms.lightDirection[0] = light.x;
    frameUniforms.lightDirection[1] = light.y;
    frameUniforms.lightDirection[2] = light.z;
    frameUniforms.timing[0] = frame.time;
    frameUniforms.timing[1] = static_cast<float>(frame.phase);
    frameUniforms.timing[2] = frame.phaseTime;
    frameUniforms.timing[3] = frame.jump;

    MTL::CommandBuffer* commandBuffer = commandQueue_->commandBuffer();
    if (commandBuffer == nullptr) {
        pass->release();
        pool->release();
        error = "Could not create a Metal command buffer.";
        return false;
    }

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    encoder->setCullMode(MTL::CullModeNone);

    // Opaque 3D scene.
    encoder->setRenderPipelineState(pipelines_.opaque());
    encoder->setDepthStencilState(pipelines_.sceneDepth());
    encoder->setVertexBuffer(vertexBuffer_.get(), 0, 2);
    encoder->setVertexBytes(&frameUniforms, sizeof(frameUniforms), 0);
    encoder->setFragmentBytes(&frameUniforms, sizeof(frameUniforms), 0);

    auto encodeItems = [&](const std::vector<DrawItem>& items) {
        for (const DrawItem& item : items) {
            ObjectUniforms objectUniforms{};
            objectUniforms.model = item.transform;
            objectUniforms.color[0] = item.color.x;
            objectUniforms.color[1] = item.color.y;
            objectUniforms.color[2] = item.color.z;
            objectUniforms.color[3] = item.alpha;
            objectUniforms.params[0] = static_cast<float>(item.material);
            objectUniforms.params[1] = item.phase;

            encoder->setVertexBytes(&objectUniforms, sizeof(objectUniforms), 1);
            encoder->setFragmentBytes(&objectUniforms, sizeof(objectUniforms), 1);

            const MeshRange& range = meshRanges_[static_cast<std::size_t>(item.mesh)];
            if (range.indexCount == 0) {
                continue;
            }
            encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, range.indexCount,
                                           MTL::IndexTypeUInt32, indexBuffer_.get(),
                                           static_cast<NS::UInteger>(range.firstIndex) *
                                               sizeof(std::uint32_t));
        }
    };

    encodeItems(frame.opaque);

    // Lasers and explosions use additive blending and do not write depth.
    encoder->setRenderPipelineState(pipelines_.additive());
    encoder->setDepthStencilState(pipelines_.effectDepth());
    encoder->setVertexBuffer(vertexBuffer_.get(), 0, 2);
    encoder->setVertexBytes(&frameUniforms, sizeof(frameUniforms), 0);
    encoder->setFragmentBytes(&frameUniforms, sizeof(frameUniforms), 0);
    encodeItems(frame.additive);

    // Shared additive laser renderer.
    cppgames::lasers::LaserFrame laserFrame{};
    const Mat4 viewProjection = projection * view;
    for (int index = 0; index < 16; ++index) {
        laserFrame.viewProjection[index] = viewProjection.m[index];
    }
    laserFrame.cameraPosition[0] = frame.camera.position.x;
    laserFrame.cameraPosition[1] = frame.camera.position.y;
    laserFrame.cameraPosition[2] = frame.camera.position.z;
    laserFrame.time = frame.time;
    laserFrame.projectionScale = projection.m[5];
    laserFrame.aspect =
        static_cast<float>(pixelWidth_) / static_cast<float>(std::max(pixelHeight_, 1));
    lasers_.encode(encoder, laserFrame, frame.lasers);

    // Screen-space cockpit overlay from the shared engine module.
    cppgames::cockpit::CockpitState cockpitState{};
    cockpitState.time = frame.time;
    cockpitState.phase = static_cast<float>(frame.phase);
    cockpitState.phaseTime = frame.phaseTime;
    cockpitState.flash = frame.flash;
    cockpitState.jump = frame.jump;
    cockpitState.steer = frame.steer;
    cockpitState.firing = frame.firing;
    cockpit_.encode(encoder, cockpitState, pixelWidth_, pixelHeight_);

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

}  // namespace spacebattle

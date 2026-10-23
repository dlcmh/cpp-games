#include "Platform/macOS/DeathStarLabRenderer.hpp"

#include "Cockpit/CockpitOverlay.hpp"
#include "Cockpit/CockpitState.hpp"
#include "Platform/macOS/MeshLibrary.hpp"
#include "Platform/macOS/Pipelines.hpp"
#include "Platform/macOS/SceneUniforms.hpp"
#include "Render/Math.hpp"
#include "Render/MeshId.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace deathstarlab {
namespace {

using spacebattle::identity;
using spacebattle::lookAt;
using spacebattle::Mat4;
using spacebattle::Material;
using spacebattle::MeshId;
using spacebattle::MeshRange;
using spacebattle::normalize;
using spacebattle::perspective;
using spacebattle::rotationY;
using spacebattle::scaling;
using spacebattle::translation;
using spacebattle::Vec3;

struct DrawItem {
    MeshId mesh{MeshId::Box};
    Material material{Material::Lit};
    Mat4 transform{identity()};
    Vec3 color{1.0f, 1.0f, 1.0f};
    float alpha{1.0f};
};

struct CameraPose {
    Vec3 eye;
    Vec3 target;
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fov{1.0f};
};

CameraPose approachCamera(float phaseTime) {
    const float u = std::clamp(phaseTime / 4.0f, 0.0f, 1.0f);
    const float eased = u * u * (3.0f - 2.0f * u);
    const float arc = std::sin(u * 3.14159265f);
    return {
        {std::sin(arc * 1.1f) * 18.0f, 60.0f + (-57.0f) * eased, 420.0f + (-336.0f) * eased},
        {0.0f, -4.0f * eased, 0.0f},
        normalize(Vec3{std::sin(arc) * 0.08f, 1.0f, 0.0f}),
        1.0f,
    };
}

CameraPose trenchCamera(float phaseTime) {
    const float settle = std::clamp(phaseTime / 1.2f, 0.0f, 1.0f);
    const float smoothSettle = settle * settle * (3.0f - 2.0f * settle);
    const float z = 20.0f - phaseTime * 28.0f;
    const Vec3 eye{0.0f, 7.5f + (-7.25f) * smoothSettle, z};
    return {eye, eye + Vec3{0.0f, -0.02f, -1.0f}, {0.0f, 1.0f, 0.0f}, 1.08f};
}

void addSky(std::vector<DrawItem>& items, Vec3 cameraPosition) {
    DrawItem sky{};
    sky.mesh = MeshId::Sphere;
    sky.material = Material::Sky;
    sky.transform = translation(cameraPosition) * scaling(750.0f);
    items.push_back(sky);
}

void addApproachScene(std::vector<DrawItem>& items, const CameraPose& camera, float time) {
    addSky(items, camera.eye);
    DrawItem deathStar{};
    deathStar.mesh = MeshId::Sphere;
    deathStar.material = Material::DeathStar;
    deathStar.color = {0.55f, 0.57f, 0.60f};
    deathStar.transform = rotationY(time * 0.018f) * scaling(70.0f);
    items.push_back(deathStar);
}

void addTrenchScene(std::vector<DrawItem>& items, const CameraPose& camera) {
    addSky(items, camera.eye);

    DrawItem trench{};
    trench.mesh = MeshId::Trench;
    trench.material = Material::Trench;
    items.push_back(trench);

    DrawItem port{};
    port.mesh = MeshId::Sphere;
    port.material = Material::Emissive;
    port.transform = translation({0.0f, -3.15f, -430.0f}) * scaling(3.4f);
    port.color = {0.30f, 1.0f, 0.50f};
    items.push_back(port);

    for (int side = -1; side <= 1; side += 2) {
        DrawItem tower{};
        tower.mesh = MeshId::Box;
        tower.material = Material::Lit;
        tower.transform = translation({static_cast<float>(side) * 3.3f, 0.2f, -427.0f}) *
                          scaling({0.5f, 3.3f, 0.5f});
        tower.color = {0.32f, 0.35f, 0.38f};
        items.push_back(tower);
    }
}

}  // namespace

struct DeathStarLabRenderer::Impl {
    NS::SharedPtr<MTL::Device> device;
    NS::SharedPtr<MTL::CommandQueue> commandQueue;
    NS::SharedPtr<MTL::Buffer> vertexBuffer;
    NS::SharedPtr<MTL::Buffer> indexBuffer;
    NS::SharedPtr<MTL::Texture> depthTexture;
    std::array<MeshRange, static_cast<std::size_t>(MeshId::Count)> meshRanges{};
    spacebattle::Pipelines pipelines;
    cppgames::cockpit::CockpitOverlay cockpit;
    CA::MetalLayer* layer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

DeathStarLabRenderer::DeathStarLabRenderer() : impl_(std::make_unique<Impl>()) {}

DeathStarLabRenderer::~DeathStarLabRenderer() = default;

bool DeathStarLabRenderer::initialize(const NativeSurface& surface,
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

    const spacebattle::MeshBatch batch = spacebattle::buildMeshBatch();
    impl_->meshRanges = batch.ranges;
    impl_->vertexBuffer = NS::TransferPtr(impl_->device->newBuffer(
        batch.vertices.data(), batch.vertices.size() * sizeof(spacebattle::MeshVertex),
        MTL::ResourceStorageModeShared));
    impl_->indexBuffer = NS::TransferPtr(
        impl_->device->newBuffer(batch.indices.data(), batch.indices.size() * sizeof(std::uint32_t),
                                 MTL::ResourceStorageModeShared));
    if (!impl_->vertexBuffer || !impl_->indexBuffer) {
        error = "Could not create mesh buffers.";
        pool->release();
        return false;
    }

    if (!impl_->pipelines.build(impl_->device.get(), shaderDirectory, error)) {
        pool->release();
        return false;
    }
    if (!impl_->cockpit.build(impl_->device.get(), shaderDirectory + "/cockpit.metal", true,
                              error)) {
        pool->release();
        return false;
    }

    pool->release();
    return true;
}

void DeathStarLabRenderer::resize(int pixelWidth, int pixelHeight) {
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

    MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
    descriptor->setTextureType(MTL::TextureType2D);
    descriptor->setPixelFormat(MTL::PixelFormatDepth32Float);
    descriptor->setWidth(static_cast<NS::UInteger>(pixelWidth));
    descriptor->setHeight(static_cast<NS::UInteger>(pixelHeight));
    descriptor->setUsage(MTL::TextureUsageRenderTarget);
    descriptor->setStorageMode(MTL::StorageModePrivate);
    impl_->depthTexture = NS::TransferPtr(impl_->device->newTexture(descriptor));
    descriptor->release();
}

bool DeathStarLabRenderer::draw(const RenderState& state, std::string& error) {
    if (impl_->pixelWidth <= 0 || impl_->pixelHeight <= 0 || !impl_->depthTexture) {
        return true;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    CA::MetalDrawable* drawable = impl_->layer->nextDrawable();
    if (drawable == nullptr) {
        pool->release();
        return true;
    }

    const CameraPose camera = state.phase == Phase::Approach ? approachCamera(state.phaseTime)
                                                             : trenchCamera(state.phaseTime);
    const bool blackout =
        state.phase == Phase::Approach && state.phaseTime > 3.80f && state.phaseTime < 4.0f;

    std::vector<DrawItem> items;
    if (!blackout) {
        if (state.phase == Phase::Approach) {
            addApproachScene(items, camera, state.time);
        } else {
            addTrenchScene(items, camera);
        }
    }

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
    color->setTexture(drawable->texture());
    color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore);
    color->setClearColor(MTL::ClearColor(0.001, 0.002, 0.006, 1.0));
    MTL::RenderPassDepthAttachmentDescriptor* depth = pass->depthAttachment();
    depth->setTexture(impl_->depthTexture.get());
    depth->setLoadAction(MTL::LoadActionClear);
    depth->setStoreAction(MTL::StoreActionStore);
    depth->setClearDepth(1.0);

    MTL::CommandBuffer* commandBuffer = impl_->commandQueue->commandBuffer();
    if (commandBuffer == nullptr) {
        pass->release();
        pool->release();
        error = "Could not create a Metal command buffer.";
        return false;
    }

    const float aspect =
        static_cast<float>(impl_->pixelWidth) / static_cast<float>(impl_->pixelHeight);
    const Mat4 view = lookAt(camera.eye, camera.target, camera.up);
    const Mat4 projection = perspective(camera.fov, aspect, 0.1f, 1500.0f);
    const Vec3 light = normalize(Vec3{-0.35f, 0.80f, 0.45f});

    spacebattle::FrameUniforms frameUniforms{};
    frameUniforms.viewProjection = projection * view;
    frameUniforms.cameraPosition[0] = camera.eye.x;
    frameUniforms.cameraPosition[1] = camera.eye.y;
    frameUniforms.cameraPosition[2] = camera.eye.z;
    frameUniforms.lightDirection[0] = light.x;
    frameUniforms.lightDirection[1] = light.y;
    frameUniforms.lightDirection[2] = light.z;
    frameUniforms.timing[0] = state.time;
    frameUniforms.timing[1] = state.phase == Phase::Approach ? 0.0f : 1.0f;
    frameUniforms.timing[2] = state.phaseTime;

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    encoder->setCullMode(MTL::CullModeNone);
    encoder->setRenderPipelineState(impl_->pipelines.opaque());
    encoder->setDepthStencilState(impl_->pipelines.sceneDepth());
    encoder->setVertexBuffer(impl_->vertexBuffer.get(), 0, 2);
    encoder->setVertexBytes(&frameUniforms, sizeof(frameUniforms), 0);
    encoder->setFragmentBytes(&frameUniforms, sizeof(frameUniforms), 0);

    for (const DrawItem& item : items) {
        spacebattle::ObjectUniforms object{};
        object.model = item.transform;
        object.color[0] = item.color.x;
        object.color[1] = item.color.y;
        object.color[2] = item.color.z;
        object.color[3] = item.alpha;
        object.params[0] = static_cast<float>(item.material);
        encoder->setVertexBytes(&object, sizeof(object), 1);
        encoder->setFragmentBytes(&object, sizeof(object), 1);

        const MeshRange& range = impl_->meshRanges[static_cast<std::size_t>(item.mesh)];
        if (range.indexCount == 0) {
            continue;
        }
        encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, range.indexCount,
                                       MTL::IndexTypeUInt32, impl_->indexBuffer.get(),
                                       static_cast<NS::UInteger>(range.firstIndex) *
                                           sizeof(std::uint32_t));
    }

    if (state.cockpitVisible) {
        cppgames::cockpit::CockpitState cockpit{};
        cockpit.time = state.time;
        cockpit.phaseTime = state.phaseTime;
        cockpit.steer = 0.0f;
        cockpit.firing = 0.0f;
        impl_->cockpit.encode(encoder, cockpit, impl_->pixelWidth, impl_->pixelHeight);
    }

    encoder->endEncoding();
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();

    pass->release();
    pool->release();
    return true;
}

}  // namespace deathstarlab

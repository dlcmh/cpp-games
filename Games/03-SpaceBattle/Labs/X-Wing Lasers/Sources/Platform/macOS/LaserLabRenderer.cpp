#include "Platform/macOS/LaserLabRenderer.hpp"

#include "Cockpit/CockpitOverlay.hpp"
#include "Cockpit/CockpitState.hpp"
#include "Lasers/LaserRenderer.hpp"
#include "Lasers/LaserTypes.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cmath>
#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace laserlab {
namespace {

struct alignas(16) BackdropUniforms {
    float resolution[2];
    float time;
    float jump;
    float steer;
    float padding[3];
};

static_assert(sizeof(BackdropUniforms) == 32, "BackdropUniforms must match Slang");

std::string readTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

NS::String* makeString(const char* text) {
    return NS::String::string(text, NS::UTF8StringEncoding);
}

std::string describeError(NS::Error* error) {
    if (error == nullptr) {
        return "unknown Metal error";
    }
    NS::String* description = error->localizedDescription();
    if (description == nullptr || description->utf8String() == nullptr) {
        return "unknown Metal error";
    }
    return description->utf8String();
}

MTL::RenderPipelineState* makeBackdropPipeline(MTL::Device* device, const std::string& shaderPath,
                                               std::string& error) {
    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {
        error = "Could not read backdrop shader at " + shaderPath;
        return nullptr;
    }

    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(makeString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Backdrop shader compilation failed: " + describeError(metalError);
        return nullptr;
    }

    MTL::Function* vertex = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragment = library->newFunction(makeString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "backdrop.metal is missing vertexMain or fragmentMain";
        if (vertex != nullptr) {
            vertex->release();
        }
        if (fragment != nullptr) {
            fragment->release();
        }
        library->release();
        return nullptr;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(makeString("Laser Lab Backdrop"));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Could not create backdrop pipeline: " + describeError(metalError);
    }

    descriptor->release();
    vertex->release();
    fragment->release();
    library->release();
    return pipeline;
}

void makeProjection(float aspect, float projection[16]) {
    const float fov = 1.02f;
    const float nearPlane = 0.1f;
    const float farPlane = 1200.0f;
    const float f = 1.0f / std::tan(fov * 0.5f);
    for (int index = 0; index < 16; ++index) {
        projection[index] = 0.0f;
    }
    projection[0] = f / aspect;
    projection[5] = f;
    projection[10] = farPlane / (nearPlane - farPlane);
    projection[11] = -1.0f;
    projection[14] = nearPlane * farPlane / (nearPlane - farPlane);
}

}  // namespace

struct LaserLabRenderer::Impl {
    NS::SharedPtr<MTL::Device> device;
    NS::SharedPtr<MTL::CommandQueue> commandQueue;
    NS::SharedPtr<MTL::RenderPipelineState> backdropPipeline;
    cppgames::cockpit::CockpitOverlay cockpit;
    cppgames::lasers::LaserRenderer lasers;
    CA::MetalLayer* layer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

LaserLabRenderer::LaserLabRenderer() : impl_(std::make_unique<Impl>()) {}

LaserLabRenderer::~LaserLabRenderer() = default;

bool LaserLabRenderer::initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                                  std::string& error) {
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

    impl_->backdropPipeline = NS::TransferPtr(
        makeBackdropPipeline(impl_->device.get(), shaderDirectory + "/backdrop.metal", error));
    if (!impl_->backdropPipeline) {
        pool->release();
        return false;
    }

    if (!impl_->cockpit.build(impl_->device.get(), shaderDirectory + "/cockpit.metal", false,
                              error)) {
        pool->release();
        return false;
    }
    if (!impl_->lasers.build(impl_->device.get(), shaderDirectory + "/laser.metal", false, error)) {
        pool->release();
        return false;
    }

    pool->release();
    return true;
}

void LaserLabRenderer::resize(int pixelWidth, int pixelHeight) {
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

bool LaserLabRenderer::draw(const cppgames::cockpit::CockpitState& cockpitState,
                            const std::vector<cppgames::lasers::LaserBeam>& beams,
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
    attachment->setClearColor(MTL::ClearColor(0.002, 0.004, 0.012, 1.0));

    BackdropUniforms backdropUniforms{};
    backdropUniforms.resolution[0] = static_cast<float>(impl_->pixelWidth);
    backdropUniforms.resolution[1] = static_cast<float>(impl_->pixelHeight);
    backdropUniforms.time = cockpitState.time;
    backdropUniforms.jump = cockpitState.jump;
    backdropUniforms.steer = cockpitState.steer;

    MTL::CommandBuffer* commandBuffer = impl_->commandQueue->commandBuffer();
    if (commandBuffer == nullptr) {
        pass->release();
        pool->release();
        error = "Could not create a Metal command buffer.";
        return false;
    }

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    encoder->setRenderPipelineState(impl_->backdropPipeline.get());
    encoder->setFragmentBytes(&backdropUniforms, sizeof(backdropUniforms), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(3));

    cppgames::lasers::LaserFrame laserFrame{};
    const float aspect =
        static_cast<float>(impl_->pixelWidth) / static_cast<float>(impl_->pixelHeight);
    makeProjection(aspect, laserFrame.viewProjection);
    laserFrame.time = cockpitState.time;
    laserFrame.projectionScale = laserFrame.viewProjection[5];
    laserFrame.aspect = aspect;
    impl_->lasers.encode(encoder, laserFrame, beams);

    impl_->cockpit.encode(encoder, cockpitState, impl_->pixelWidth, impl_->pixelHeight);

    encoder->endEncoding();
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();

    pass->release();
    pool->release();
    return true;
}

}  // namespace laserlab

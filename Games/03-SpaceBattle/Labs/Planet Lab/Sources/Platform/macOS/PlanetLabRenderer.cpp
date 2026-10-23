#include "Platform/macOS/PlanetLabRenderer.hpp"

#include "Planets/PlanetRenderer.hpp"
#include "Planets/PlanetSpec.hpp"
#include "Platform/macOS/Math.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>

namespace planetlab {
namespace {

struct alignas(16) StarfieldUniforms {
    float resolution[2];
    float time;
    float seed;
    float padding[3];
};

static_assert(sizeof(StarfieldUniforms) == 32, "StarfieldUniforms must match Slang");

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

MTL::RenderPipelineState* makeStarfieldPipeline(MTL::Device* device, const std::string& shaderPath,
                                                std::string& error) {
    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {
        error = "Could not read starfield shader at " + shaderPath;
        return nullptr;
    }

    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(makeString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Starfield shader compilation failed: " + describeError(metalError);
        return nullptr;
    }

    MTL::Function* vertex = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragment = library->newFunction(makeString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "starfield.metal is missing vertexMain or fragmentMain";
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
    descriptor->setLabel(makeString("Planet Lab Starfield"));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Could not create starfield pipeline: " + describeError(metalError);
    }

    descriptor->release();
    vertex->release();
    fragment->release();
    library->release();
    return pipeline;
}

}  // namespace

struct PlanetLabRenderer::Impl {
    NS::SharedPtr<MTL::Device> device;
    NS::SharedPtr<MTL::CommandQueue> commandQueue;
    NS::SharedPtr<MTL::RenderPipelineState> starfieldPipeline;
    NS::SharedPtr<MTL::DepthStencilState> noDepthTest;
    cppgames::planets::PlanetRenderer planetRenderer;
    NS::SharedPtr<MTL::Texture> depthTexture;
    CA::MetalLayer* layer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

PlanetLabRenderer::PlanetLabRenderer() : impl_(std::make_unique<Impl>()) {}

PlanetLabRenderer::~PlanetLabRenderer() = default;

bool PlanetLabRenderer::initialize(const NativeSurface& surface, const std::string& shaderDirectory,
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

    impl_->starfieldPipeline = NS::TransferPtr(
        makeStarfieldPipeline(impl_->device.get(), shaderDirectory + "/starfield.metal", error));
    if (!impl_->starfieldPipeline) {
        pool->release();
        return false;
    }

    if (!impl_->planetRenderer.build(impl_->device.get(), shaderDirectory, true, error)) {
        pool->release();
        return false;
    }

    MTL::DepthStencilDescriptor* depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
    depthDescriptor->setDepthWriteEnabled(false);
    impl_->noDepthTest = NS::TransferPtr(impl_->device->newDepthStencilState(depthDescriptor));
    depthDescriptor->release();

    pool->release();
    return true;
}

void PlanetLabRenderer::resize(int pixelWidth, int pixelHeight) {
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

bool PlanetLabRenderer::draw(const cppgames::planets::PlanetSpec& spec, float time,
                             std::string& error) {
    if (impl_->pixelWidth <= 0 || impl_->pixelHeight <= 0 || !impl_->depthTexture) {
        return true;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    CA::MetalDrawable* drawable = impl_->layer->nextDrawable();
    if (drawable == nullptr) {
        pool->release();
        return true;
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
    const Vec3 eye{0.0f, 0.30f, 6.2f};
    const Mat4 view = lookAt(eye, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 projection = perspective(0.95f, aspect, 0.1f, 40.0f);
    const Mat4 viewProjection = projection * view;

    StarfieldUniforms starfield{};
    starfield.resolution[0] = static_cast<float>(impl_->pixelWidth);
    starfield.resolution[1] = static_cast<float>(impl_->pixelHeight);
    starfield.time = time;
    starfield.seed = static_cast<float>(spec.seed % 9973u);

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    encoder->setRenderPipelineState(impl_->starfieldPipeline.get());
    encoder->setDepthStencilState(impl_->noDepthTest.get());
    encoder->setFragmentBytes(&starfield, sizeof(starfield), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(3));

    const Mat4 tilt = rotationZ(spec.tiltZ) * rotationX(spec.tiltX);
    const Mat4 planetModel = tilt * rotationY(time * spec.spinSpeed) * scaling(spec.radius);
    const Mat4 ringModel = tilt;

    cppgames::planets::PlanetDraw planetDraw{};
    for (int index = 0; index < 16; ++index) {
        planetDraw.viewProjection[index] = viewProjection.m[index];
        planetDraw.model[index] = planetModel.m[index];
        planetDraw.ringModel[index] = ringModel.m[index];
    }
    planetDraw.cameraPosition[0] = eye.x;
    planetDraw.cameraPosition[1] = eye.y;
    planetDraw.cameraPosition[2] = eye.z;
    planetDraw.sunDirection[0] = spec.sunDirection[0];
    planetDraw.sunDirection[1] = spec.sunDirection[1];
    planetDraw.sunDirection[2] = spec.sunDirection[2];
    planetDraw.time = time;
    planetDraw.drawRings = spec.hasRings;
    planetDraw.drawAtmosphere = spec.atmosphereDensity > 0.001f;

    impl_->planetRenderer.encode(encoder, spec, planetDraw);

    encoder->endEncoding();
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();

    pass->release();
    pool->release();
    return true;
}

}  // namespace planetlab

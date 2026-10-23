#include "Platform/macOS/RollerCoasterRenderer.hpp"
#include "Core/Math.hpp"
#include "Render/TrackMesh.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>

namespace rollercoaster {
namespace {

struct alignas(16) CameraUniforms {
    float viewProjection[16];
    float cameraPosition[4];
};

static_assert(sizeof(CameraUniforms) == 80, "CameraUniforms must match Slang");

NS::String* makeString(const char* text) {
    return NS::String::string(text, NS::UTF8StringEncoding);
}

std::string describeError(NS::Error* error) {
    if (error == nullptr || error->localizedDescription() == nullptr) {
        return "unknown Metal error";
    }
    return error->localizedDescription()->utf8String();
}

std::string readTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

MTL::RenderPipelineState* makePipeline(MTL::Device* device, const std::string& shaderPath,
                                       std::string& error) {
    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {
        error = "Could not read shader at " + shaderPath;
        return nullptr;
    }

    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(makeString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Shader compilation failed: " + describeError(metalError);
        return nullptr;
    }

    MTL::Function* vertex = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragment = library->newFunction(makeString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "roller-coaster.metal is missing vertexMain or fragmentMain";
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
    descriptor->setLabel(makeString("Roller Coaster"));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    vertexDescriptor->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    vertexDescriptor->attributes()->object(0)->setOffset(0);
    vertexDescriptor->attributes()->object(0)->setBufferIndex(2);
    vertexDescriptor->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    vertexDescriptor->attributes()->object(1)->setOffset(sizeof(float) * 3);
    vertexDescriptor->attributes()->object(1)->setBufferIndex(2);
    vertexDescriptor->layouts()->object(2)->setStride(sizeof(TrackVertex));
    descriptor->setVertexDescriptor(vertexDescriptor);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Pipeline creation failed: " + describeError(metalError);
    }

    descriptor->release();
    vertexDescriptor->release();
    vertex->release();
    fragment->release();
    library->release();
    return pipeline;
}

}  // namespace

struct RollerCoasterRenderer::Impl {
    NS::SharedPtr<MTL::Device> device;
    NS::SharedPtr<MTL::CommandQueue> commandQueue;
    NS::SharedPtr<MTL::RenderPipelineState> pipeline;
    NS::SharedPtr<MTL::DepthStencilState> depthState;
    NS::SharedPtr<MTL::Buffer> vertexBuffer;
    NS::SharedPtr<MTL::Texture> depthTexture;
    CA::MetalLayer* layer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
    std::size_t vertexCount{0};
};

RollerCoasterRenderer::RollerCoasterRenderer() : impl_(std::make_unique<Impl>()) {}

RollerCoasterRenderer::~RollerCoasterRenderer() = default;

bool RollerCoasterRenderer::initialize(const NativeSurface& surface,
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

    impl_->pipeline = NS::TransferPtr(
        makePipeline(impl_->device.get(), shaderDirectory + "/roller-coaster.metal", error));
    if (!impl_->pipeline) {
        pool->release();
        return false;
    }

    MTL::DepthStencilDescriptor* depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depthDescriptor->setDepthWriteEnabled(true);
    impl_->depthState = NS::TransferPtr(impl_->device->newDepthStencilState(depthDescriptor));
    depthDescriptor->release();

    resize(surface.pixelWidth, surface.pixelHeight);
    pool->release();
    return true;
}

void RollerCoasterRenderer::resize(int pixelWidth, int pixelHeight) {
    if (impl_->layer == nullptr || pixelWidth <= 0 || pixelHeight <= 0 ||
        (pixelWidth == impl_->pixelWidth && pixelHeight == impl_->pixelHeight)) {
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

bool RollerCoasterRenderer::draw(const TrackMesh& mesh, const Camera& camera, std::string& error) {
    if (impl_->pixelWidth <= 0 || impl_->pixelHeight <= 0 || !impl_->depthTexture) {
        return true;
    }

    if (!impl_->vertexBuffer || impl_->vertexCount != mesh.vertices.size()) {
        impl_->vertexBuffer = NS::TransferPtr(impl_->device->newBuffer(
            mesh.vertices.data(), mesh.vertices.size() * sizeof(TrackVertex),
            MTL::ResourceStorageModeShared));
        impl_->vertexCount = mesh.vertices.size();
        if (!impl_->vertexBuffer) {
            error = "Could not create the track vertex buffer.";
            return false;
        }
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    CA::MetalDrawable* drawable = impl_->layer->nextDrawable();
    if (drawable == nullptr) {
        pool->release();
        return true;
    }

    const float aspect =
        static_cast<float>(impl_->pixelWidth) / static_cast<float>(impl_->pixelHeight);
    const Mat4 viewProjection = multiply(perspective(camera.fovY, aspect, 0.1f, 600.0f),
                                         lookAt(camera.eye, camera.target, camera.up));

    CameraUniforms uniforms{};
    std::copy(std::begin(viewProjection.m), std::end(viewProjection.m),
              std::begin(uniforms.viewProjection));
    uniforms.cameraPosition[0] = camera.eye.x;
    uniforms.cameraPosition[1] = camera.eye.y;
    uniforms.cameraPosition[2] = camera.eye.z;

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
    color->setTexture(drawable->texture());
    color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore);
    color->setClearColor(MTL::ClearColor(0.56, 0.68, 0.82, 1.0));
    pass->depthAttachment()->setTexture(impl_->depthTexture.get());
    pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
    pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
    pass->depthAttachment()->setClearDepth(1.0);

    MTL::CommandBuffer* commandBuffer = impl_->commandQueue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    encoder->setRenderPipelineState(impl_->pipeline.get());
    encoder->setDepthStencilState(impl_->depthState.get());
    encoder->setCullMode(MTL::CullModeNone);
    encoder->setVertexBuffer(impl_->vertexBuffer.get(), 0, 2);
    encoder->setVertexBytes(&uniforms, sizeof(uniforms), 0);
    encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(impl_->vertexCount));
    encoder->endEncoding();
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();

    pass->release();
    pool->release();
    return true;
}

}  // namespace rollercoaster

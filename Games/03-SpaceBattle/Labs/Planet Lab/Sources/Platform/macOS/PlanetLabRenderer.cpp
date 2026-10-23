#include "Platform/macOS/PlanetLabRenderer.hpp"

#include "Demo/PlanetGenerator.hpp"
#include "Platform/macOS/Math.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <math.h>
#include <memory>
#include <string>
#include <vector>

namespace planetlab {
namespace {

constexpr int kSphereSlices = 128;
constexpr int kSphereStacks = 64;

struct MeshVertex {
    float position[4];
    float normal[4];
};

struct alignas(16) PlanetUniforms {
    float viewProjection[16];
    float model[16];
    float cameraPosition[4];
    float sunDirection[4];
    float planet[4];
    float surface[4];
    float appearance[4];
    float colorA[4];
    float colorB[4];
    float colorC[4];
    float colorD[4];
    float ring[4];
    float ringPlane[4];
    float ringCenter[4];
    float ringColorA[4];
    float ringColorB[4];
    float atmosphereColor[4];
    float detail[4];
};

struct alignas(16) RingUniforms {
    float viewProjection[16];
    float model[16];
    float cameraPosition[4];
    float sunDirection[4];
    float ring[4];
    float planetCenterRadius[4];
    float colorA[4];
    float colorB[4];
};

struct alignas(16) StarfieldUniforms {
    float resolution[2];
    float time;
    float seed;
    float padding[3];
};

static_assert(sizeof(PlanetUniforms) == 384, "PlanetUniforms must match Slang");
static_assert(sizeof(RingUniforms) == 224, "RingUniforms must match Slang");
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

MTL::Library* compileLibrary(MTL::Device* device, const std::string& path, std::string& error) {
    const std::string source = readTextFile(path);
    if (source.empty()) {
        error = "Could not read shader at " + path;
        return nullptr;
    }
    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(makeString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Shader compilation failed for " + path + ": " + describeError(metalError);
    }
    return library;
}

MTL::RenderPipelineState* makePipeline(MTL::Device* device, MTL::Library* library,
                                       const char* vertexName, const char* fragmentName,
                                       bool depthAttachment, bool additive, std::string& error) {
    MTL::Function* vertex = library->newFunction(makeString(vertexName));
    MTL::Function* fragment = library->newFunction(makeString(fragmentName));
    if (vertex == nullptr || fragment == nullptr) {
        error = std::string("Missing shader entry point: ") + vertexName + " / " + fragmentName;
        if (vertex != nullptr) {
            vertex->release();
        }
        if (fragment != nullptr) {
            fragment->release();
        }
        return nullptr;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(makeString(fragmentName));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    if (depthAttachment) {
        descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    }

    MTL::RenderPipelineColorAttachmentDescriptor* attachment =
        descriptor->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    if (additive) {
        attachment->setBlendingEnabled(true);
        attachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
        attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOne);
        attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
        attachment->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
        attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOne);
        attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);
    }

    NS::Error* metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = std::string("Could not create pipeline ") + fragmentName + ": " +
                describeError(metalError);
    }

    descriptor->release();
    vertex->release();
    fragment->release();
    return pipeline;
}

MTL::RenderPipelineState* makeRingPipeline(MTL::Device* device, MTL::Library* library,
                                           std::string& error) {
    MTL::Function* vertex = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragment = library->newFunction(makeString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "rings.metal is missing vertexMain or fragmentMain";
        return nullptr;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(makeString("Planet Lab Rings"));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineColorAttachmentDescriptor* attachment =
        descriptor->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    attachment->setBlendingEnabled(true);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);

    NS::Error* metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Could not create ring pipeline: " + describeError(metalError);
    }

    descriptor->release();
    vertex->release();
    fragment->release();
    return pipeline;
}

void appendSphere(std::vector<MeshVertex>& vertices, std::vector<std::uint32_t>& indices) {
    for (int stack = 0; stack <= kSphereStacks; ++stack) {
        const float latitude = static_cast<float>(M_PI) * static_cast<float>(stack) /
                               static_cast<float>(kSphereStacks);
        const float y = std::cos(latitude);
        const float radius = std::sin(latitude);
        for (int slice = 0; slice <= kSphereSlices; ++slice) {
            const float longitude = 2.0f * static_cast<float>(M_PI) * static_cast<float>(slice) /
                                    static_cast<float>(kSphereSlices);
            const float x = radius * std::cos(longitude);
            const float z = radius * std::sin(longitude);
            MeshVertex vertex{};
            vertex.position[0] = x;
            vertex.position[1] = y;
            vertex.position[2] = z;
            vertex.position[3] = 1.0f;
            vertex.normal[0] = x;
            vertex.normal[1] = y;
            vertex.normal[2] = z;
            vertex.normal[3] = 0.0f;
            vertices.push_back(vertex);
        }
    }

    for (int stack = 0; stack < kSphereStacks; ++stack) {
        for (int slice = 0; slice < kSphereSlices; ++slice) {
            const std::uint32_t a = static_cast<std::uint32_t>(stack * (kSphereSlices + 1) + slice);
            const std::uint32_t b = a + 1;
            const std::uint32_t c =
                static_cast<std::uint32_t>((stack + 1) * (kSphereSlices + 1) + slice);
            const std::uint32_t d = c + 1;
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }
}

Vec3 transformDirection(const Mat4& matrix, Vec3 direction) {
    return normalize(
        {matrix.m[0] * direction.x + matrix.m[4] * direction.y + matrix.m[8] * direction.z,
         matrix.m[1] * direction.x + matrix.m[5] * direction.y + matrix.m[9] * direction.z,
         matrix.m[2] * direction.x + matrix.m[6] * direction.y + matrix.m[10] * direction.z});
}

void copyMatrix(float target[16], const Mat4& source) {
    for (int index = 0; index < 16; ++index) {
        target[index] = source.m[index];
    }
}

void copyColor(float target[4], const float source[3]) {
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
    target[3] = 1.0f;
}

}  // namespace

struct PlanetLabRenderer::Impl {
    NS::SharedPtr<MTL::Device> device;
    NS::SharedPtr<MTL::CommandQueue> commandQueue;
    NS::SharedPtr<MTL::RenderPipelineState> planetPipeline;
    NS::SharedPtr<MTL::RenderPipelineState> atmospherePipeline;
    NS::SharedPtr<MTL::RenderPipelineState> ringPipeline;
    NS::SharedPtr<MTL::RenderPipelineState> starfieldPipeline;
    NS::SharedPtr<MTL::DepthStencilState> planetDepth;
    NS::SharedPtr<MTL::DepthStencilState> transparentDepth;
    NS::SharedPtr<MTL::DepthStencilState> noDepthTest;
    NS::SharedPtr<MTL::Buffer> vertexBuffer;
    NS::SharedPtr<MTL::Buffer> indexBuffer;
    NS::SharedPtr<MTL::Texture> depthTexture;
    CA::MetalLayer* layer{nullptr};
    std::uint32_t indexCount{0};
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

    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    appendSphere(vertices, indices);
    impl_->indexCount = static_cast<std::uint32_t>(indices.size());
    impl_->vertexBuffer = NS::TransferPtr(impl_->device->newBuffer(
        vertices.data(), vertices.size() * sizeof(MeshVertex), MTL::ResourceStorageModeShared));
    impl_->indexBuffer = NS::TransferPtr(impl_->device->newBuffer(
        indices.data(), indices.size() * sizeof(std::uint32_t), MTL::ResourceStorageModeShared));
    if (!impl_->vertexBuffer || !impl_->indexBuffer) {
        error = "Could not create planet mesh buffers.";
        pool->release();
        return false;
    }

    MTL::Library* planetLibrary =
        compileLibrary(impl_->device.get(), shaderDirectory + "/planet.metal", error);
    MTL::Library* ringLibrary =
        compileLibrary(impl_->device.get(), shaderDirectory + "/rings.metal", error);
    MTL::Library* starLibrary =
        compileLibrary(impl_->device.get(), shaderDirectory + "/starfield.metal", error);
    if (planetLibrary == nullptr || ringLibrary == nullptr || starLibrary == nullptr) {
        if (planetLibrary != nullptr) {
            planetLibrary->release();
        }
        if (ringLibrary != nullptr) {
            ringLibrary->release();
        }
        if (starLibrary != nullptr) {
            starLibrary->release();
        }
        pool->release();
        return false;
    }

    impl_->planetPipeline = NS::TransferPtr(makePipeline(
        impl_->device.get(), planetLibrary, "planetVertex", "planetFragment", true, false, error));
    impl_->atmospherePipeline =
        NS::TransferPtr(makePipeline(impl_->device.get(), planetLibrary, "planetVertex",
                                     "atmosphereFragment", true, true, error));
    impl_->ringPipeline =
        NS::TransferPtr(makeRingPipeline(impl_->device.get(), ringLibrary, error));
    impl_->starfieldPipeline = NS::TransferPtr(makePipeline(
        impl_->device.get(), starLibrary, "vertexMain", "fragmentMain", true, false, error));

    planetLibrary->release();
    ringLibrary->release();
    starLibrary->release();

    if (!impl_->planetPipeline || !impl_->atmospherePipeline || !impl_->ringPipeline ||
        !impl_->starfieldPipeline) {
        pool->release();
        return false;
    }

    MTL::DepthStencilDescriptor* depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depthDescriptor->setDepthWriteEnabled(true);
    impl_->planetDepth = NS::TransferPtr(impl_->device->newDepthStencilState(depthDescriptor));
    depthDescriptor->setDepthWriteEnabled(false);
    impl_->transparentDepth = NS::TransferPtr(impl_->device->newDepthStencilState(depthDescriptor));
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
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

bool PlanetLabRenderer::draw(const PlanetSpec& spec, float time, std::string& error) {
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

    const Mat4 tilt = rotationZ(spec.tiltZ) * rotationX(spec.tiltX);
    const Mat4 planetModel = tilt * rotationY(time * spec.spinSpeed) * scaling(spec.radius);
    const Mat4 atmosphereModel =
        tilt * rotationY(time * spec.spinSpeed) * scaling(spec.radius * 1.035f);
    const Mat4 ringModel = tilt;
    const Vec3 ringNormal = transformDirection(tilt, {0.0f, 1.0f, 0.0f});

    StarfieldUniforms starfield{};
    starfield.resolution[0] = static_cast<float>(impl_->pixelWidth);
    starfield.resolution[1] = static_cast<float>(impl_->pixelHeight);
    starfield.time = time;
    starfield.seed = static_cast<float>(spec.seed % 9973u);

    PlanetUniforms uniforms{};
    copyMatrix(uniforms.viewProjection, viewProjection);
    copyMatrix(uniforms.model, planetModel);
    uniforms.cameraPosition[0] = eye.x;
    uniforms.cameraPosition[1] = eye.y;
    uniforms.cameraPosition[2] = eye.z;
    uniforms.sunDirection[0] = spec.sunDirection[0];
    uniforms.sunDirection[1] = spec.sunDirection[1];
    uniforms.sunDirection[2] = spec.sunDirection[2];
    uniforms.planet[0] = spec.radius;
    uniforms.planet[1] = time;
    uniforms.planet[2] = static_cast<float>(spec.type);
    uniforms.planet[3] = static_cast<float>(spec.seed % 100000u);
    uniforms.surface[0] = spec.noiseScale;
    uniforms.surface[1] = spec.warp;
    uniforms.surface[2] = spec.iceCoverage;
    uniforms.surface[3] = spec.cloudCoverage;
    uniforms.appearance[0] = spec.specular;
    uniforms.appearance[1] = spec.cityLights;
    uniforms.appearance[2] = spec.volcanicActivity;
    uniforms.appearance[3] = spec.atmosphereDensity;
    copyColor(uniforms.colorA, spec.colorA);
    copyColor(uniforms.colorB, spec.colorB);
    copyColor(uniforms.colorC, spec.colorC);
    copyColor(uniforms.colorD, spec.colorD);
    uniforms.ring[0] = spec.ringInner;
    uniforms.ring[1] = spec.ringOuter;
    uniforms.ring[2] = spec.ringOpacity;
    uniforms.ring[3] = spec.hasRings ? 1.0f : 0.0f;
    uniforms.ringPlane[0] = ringNormal.x;
    uniforms.ringPlane[1] = ringNormal.y;
    uniforms.ringPlane[2] = ringNormal.z;
    uniforms.ringPlane[3] = spec.radius;
    uniforms.ringCenter[0] = 0.0f;
    uniforms.ringCenter[1] = 0.0f;
    uniforms.ringCenter[2] = 0.0f;
    copyColor(uniforms.ringColorA, spec.ringColorA);
    copyColor(uniforms.ringColorB, spec.ringColorB);
    copyColor(uniforms.atmosphereColor, spec.atmosphereColor);
    uniforms.detail[0] = spec.craterDensity;
    uniforms.detail[1] = spec.cloudSpeed;

    RingUniforms ringUniforms{};
    copyMatrix(ringUniforms.viewProjection, viewProjection);
    copyMatrix(ringUniforms.model, ringModel);
    ringUniforms.cameraPosition[0] = eye.x;
    ringUniforms.cameraPosition[1] = eye.y;
    ringUniforms.cameraPosition[2] = eye.z;
    ringUniforms.sunDirection[0] = spec.sunDirection[0];
    ringUniforms.sunDirection[1] = spec.sunDirection[1];
    ringUniforms.sunDirection[2] = spec.sunDirection[2];
    ringUniforms.ring[0] = spec.ringInner;
    ringUniforms.ring[1] = spec.ringOuter;
    ringUniforms.ring[2] = spec.ringOpacity;
    ringUniforms.ring[3] = static_cast<float>(spec.seed % 100000u);
    ringUniforms.planetCenterRadius[3] = spec.radius;
    copyColor(ringUniforms.colorA, spec.ringColorA);
    copyColor(ringUniforms.colorB, spec.ringColorB);

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);

    encoder->setRenderPipelineState(impl_->starfieldPipeline.get());
    encoder->setDepthStencilState(impl_->noDepthTest.get());
    encoder->setFragmentBytes(&starfield, sizeof(starfield), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(3));

    encoder->setRenderPipelineState(impl_->planetPipeline.get());
    encoder->setDepthStencilState(impl_->planetDepth.get());
    encoder->setCullMode(MTL::CullModeBack);
    encoder->setVertexBuffer(impl_->vertexBuffer.get(), 0, 0);
    encoder->setVertexBytes(&uniforms, sizeof(uniforms), 1);
    encoder->setFragmentBuffer(impl_->vertexBuffer.get(), 0, 0);
    encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 1);
    encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, impl_->indexCount,
                                   MTL::IndexTypeUInt32, impl_->indexBuffer.get(), 0);

    if (spec.atmosphereDensity > 0.001f) {
        encoder->setRenderPipelineState(impl_->atmospherePipeline.get());
        encoder->setDepthStencilState(impl_->transparentDepth.get());
        encoder->setCullMode(MTL::CullModeBack);
        PlanetUniforms atmosphereUniforms = uniforms;
        copyMatrix(atmosphereUniforms.model, atmosphereModel);
        encoder->setVertexBuffer(impl_->vertexBuffer.get(), 0, 0);
        encoder->setVertexBytes(&atmosphereUniforms, sizeof(atmosphereUniforms), 1);
        encoder->setFragmentBuffer(impl_->vertexBuffer.get(), 0, 0);
        encoder->setFragmentBytes(&atmosphereUniforms, sizeof(atmosphereUniforms), 1);
        encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, impl_->indexCount,
                                       MTL::IndexTypeUInt32, impl_->indexBuffer.get(), 0);
    }

    if (spec.hasRings) {
        encoder->setRenderPipelineState(impl_->ringPipeline.get());
        encoder->setDepthStencilState(impl_->transparentDepth.get());
        encoder->setCullMode(MTL::CullModeNone);
        encoder->setVertexBytes(&ringUniforms, sizeof(ringUniforms), 0);
        encoder->setFragmentBytes(&ringUniforms, sizeof(ringUniforms), 0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, static_cast<NS::UInteger>(0),
                                static_cast<NS::UInteger>(4));
    }

    encoder->endEncoding();
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();

    pass->release();
    pool->release();
    return true;
}

}  // namespace planetlab

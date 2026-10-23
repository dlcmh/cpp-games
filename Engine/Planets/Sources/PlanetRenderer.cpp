#include "Planets/PlanetRenderer.hpp"
#include "Planets/PlanetSpec.hpp"

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

namespace cppgames::planets {
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

static_assert(sizeof(PlanetUniforms) == 384, "PlanetUniforms must match Slang");
static_assert(sizeof(RingUniforms) == 224, "RingUniforms must match Slang");

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
                                           bool depthAttachment, std::string& error) {
    MTL::Function* vertex = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragment = library->newFunction(makeString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "rings.metal is missing vertexMain or fragmentMain";
        return nullptr;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(makeString("Planet Rings"));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    if (depthAttachment) {
        descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    }

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

void copyMatrix(float target[16], const float source[16]) {
    for (int index = 0; index < 16; ++index) {
        target[index] = source[index];
    }
}

void copyColor(float target[4], const float source[3]) {
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
    target[3] = 1.0f;
}

void scaleLinear(float matrix[16], float scale) {
    for (int row = 0; row < 3; ++row) {
        matrix[row] *= scale;
        matrix[4 + row] *= scale;
        matrix[8 + row] *= scale;
    }
}

}  // namespace

struct PlanetRenderer::Impl {
    NS::SharedPtr<MTL::RenderPipelineState> planetPipeline;
    NS::SharedPtr<MTL::RenderPipelineState> atmospherePipeline;
    NS::SharedPtr<MTL::RenderPipelineState> ringPipeline;
    NS::SharedPtr<MTL::DepthStencilState> planetDepth;
    NS::SharedPtr<MTL::DepthStencilState> transparentDepth;
    NS::SharedPtr<MTL::Buffer> vertexBuffer;
    NS::SharedPtr<MTL::Buffer> indexBuffer;
    std::uint32_t indexCount{0};
};

PlanetRenderer::PlanetRenderer() : impl_(std::make_unique<Impl>()) {}

PlanetRenderer::~PlanetRenderer() = default;

bool PlanetRenderer::build(MTL::Device* device, const std::string& shaderDirectory,
                           bool depthAttachment, std::string& error) {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    appendSphere(vertices, indices);
    impl_->indexCount = static_cast<std::uint32_t>(indices.size());
    impl_->vertexBuffer = NS::TransferPtr(device->newBuffer(
        vertices.data(), vertices.size() * sizeof(MeshVertex), MTL::ResourceStorageModeShared));
    impl_->indexBuffer = NS::TransferPtr(device->newBuffer(
        indices.data(), indices.size() * sizeof(std::uint32_t), MTL::ResourceStorageModeShared));
    if (!impl_->vertexBuffer || !impl_->indexBuffer) {
        error = "Could not create planet mesh buffers.";
        pool->release();
        return false;
    }

    MTL::Library* planetLibrary = compileLibrary(device, shaderDirectory + "/planet.metal", error);
    MTL::Library* ringLibrary = compileLibrary(device, shaderDirectory + "/rings.metal", error);
    if (planetLibrary == nullptr || ringLibrary == nullptr) {
        if (planetLibrary != nullptr) {
            planetLibrary->release();
        }
        if (ringLibrary != nullptr) {
            ringLibrary->release();
        }
        pool->release();
        return false;
    }

    impl_->planetPipeline = NS::TransferPtr(makePipeline(
        device, planetLibrary, "planetVertex", "planetFragment", depthAttachment, false, error));
    impl_->atmospherePipeline = NS::TransferPtr(makePipeline(
        device, planetLibrary, "planetVertex", "atmosphereFragment", depthAttachment, true, error));
    impl_->ringPipeline =
        NS::TransferPtr(makeRingPipeline(device, ringLibrary, depthAttachment, error));

    planetLibrary->release();
    ringLibrary->release();

    if (!impl_->planetPipeline || !impl_->atmospherePipeline || !impl_->ringPipeline) {
        pool->release();
        return false;
    }

    MTL::DepthStencilDescriptor* depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depthDescriptor->setDepthWriteEnabled(true);
    impl_->planetDepth = NS::TransferPtr(device->newDepthStencilState(depthDescriptor));
    depthDescriptor->setDepthWriteEnabled(false);
    impl_->transparentDepth = NS::TransferPtr(device->newDepthStencilState(depthDescriptor));
    depthDescriptor->release();

    pool->release();
    return true;
}

void PlanetRenderer::encode(MTL::RenderCommandEncoder* encoder, const PlanetSpec& spec,
                            const PlanetDraw& draw) const {
    if (!valid()) {
        return;
    }

    const float* viewProjection = draw.viewProjection;
    const float* model = draw.model;
    const float* ringModel = draw.ringModel;
    const float time = draw.time;

    const float ringNormal[3] = {ringModel[4], ringModel[5], ringModel[6]};
    const float ringLength =
        std::sqrt(ringNormal[0] * ringNormal[0] + ringNormal[1] * ringNormal[1] +
                  ringNormal[2] * ringNormal[2]);
    const float normalizedRing[3] = {
        ringLength > 0.0001f ? ringNormal[0] / ringLength : 0.0f,
        ringLength > 0.0001f ? ringNormal[1] / ringLength : 1.0f,
        ringLength > 0.0001f ? ringNormal[2] / ringLength : 0.0f,
    };

    PlanetUniforms uniforms{};
    copyMatrix(uniforms.viewProjection, viewProjection);
    copyMatrix(uniforms.model, model);
    uniforms.cameraPosition[0] = draw.cameraPosition[0];
    uniforms.cameraPosition[1] = draw.cameraPosition[1];
    uniforms.cameraPosition[2] = draw.cameraPosition[2];
    uniforms.sunDirection[0] = draw.sunDirection[0];
    uniforms.sunDirection[1] = draw.sunDirection[1];
    uniforms.sunDirection[2] = draw.sunDirection[2];
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
    uniforms.ring[3] = draw.drawRings ? 1.0f : 0.0f;
    uniforms.ringPlane[0] = normalizedRing[0];
    uniforms.ringPlane[1] = normalizedRing[1];
    uniforms.ringPlane[2] = normalizedRing[2];
    uniforms.ringPlane[3] = spec.radius;
    uniforms.ringCenter[0] = ringModel[12];
    uniforms.ringCenter[1] = ringModel[13];
    uniforms.ringCenter[2] = ringModel[14];
    copyColor(uniforms.ringColorA, spec.ringColorA);
    copyColor(uniforms.ringColorB, spec.ringColorB);
    copyColor(uniforms.atmosphereColor, spec.atmosphereColor);
    uniforms.detail[0] = spec.craterDensity;
    uniforms.detail[1] = spec.cloudSpeed;

    encoder->setRenderPipelineState(impl_->planetPipeline.get());
    encoder->setDepthStencilState(impl_->planetDepth.get());
    encoder->setCullMode(MTL::CullModeBack);
    encoder->setVertexBuffer(impl_->vertexBuffer.get(), 0, 0);
    encoder->setVertexBytes(&uniforms, sizeof(uniforms), 1);
    encoder->setFragmentBuffer(impl_->vertexBuffer.get(), 0, 0);
    encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 1);
    encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, impl_->indexCount,
                                   MTL::IndexTypeUInt32, impl_->indexBuffer.get(), 0);

    if (draw.drawAtmosphere && spec.atmosphereDensity > 0.001f) {
        PlanetUniforms atmosphere = uniforms;
        scaleLinear(atmosphere.model, 1.035f);
        encoder->setRenderPipelineState(impl_->atmospherePipeline.get());
        encoder->setDepthStencilState(impl_->transparentDepth.get());
        encoder->setCullMode(MTL::CullModeBack);
        encoder->setVertexBuffer(impl_->vertexBuffer.get(), 0, 0);
        encoder->setVertexBytes(&atmosphere, sizeof(atmosphere), 1);
        encoder->setFragmentBuffer(impl_->vertexBuffer.get(), 0, 0);
        encoder->setFragmentBytes(&atmosphere, sizeof(atmosphere), 1);
        encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, impl_->indexCount,
                                       MTL::IndexTypeUInt32, impl_->indexBuffer.get(), 0);
    }

    if (draw.drawRings && spec.hasRings) {
        RingUniforms ringUniforms{};
        copyMatrix(ringUniforms.viewProjection, viewProjection);
        copyMatrix(ringUniforms.model, ringModel);
        ringUniforms.cameraPosition[0] = draw.cameraPosition[0];
        ringUniforms.cameraPosition[1] = draw.cameraPosition[1];
        ringUniforms.cameraPosition[2] = draw.cameraPosition[2];
        ringUniforms.sunDirection[0] = draw.sunDirection[0];
        ringUniforms.sunDirection[1] = draw.sunDirection[1];
        ringUniforms.sunDirection[2] = draw.sunDirection[2];
        ringUniforms.ring[0] = spec.ringInner;
        ringUniforms.ring[1] = spec.ringOuter;
        ringUniforms.ring[2] = spec.ringOpacity;
        ringUniforms.ring[3] = static_cast<float>(spec.seed % 100000u);
        ringUniforms.planetCenterRadius[0] = ringModel[12];
        ringUniforms.planetCenterRadius[1] = ringModel[13];
        ringUniforms.planetCenterRadius[2] = ringModel[14];
        ringUniforms.planetCenterRadius[3] = spec.radius;
        copyColor(ringUniforms.colorA, spec.ringColorA);
        copyColor(ringUniforms.colorB, spec.ringColorB);

        encoder->setRenderPipelineState(impl_->ringPipeline.get());
        encoder->setDepthStencilState(impl_->transparentDepth.get());
        encoder->setCullMode(MTL::CullModeNone);
        encoder->setVertexBytes(&ringUniforms, sizeof(ringUniforms), 0);
        encoder->setFragmentBytes(&ringUniforms, sizeof(ringUniforms), 0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, static_cast<NS::UInteger>(0),
                                static_cast<NS::UInteger>(4));
    }
}

bool PlanetRenderer::valid() const {
    return impl_->planetPipeline && impl_->atmospherePipeline && impl_->ringPipeline &&
           impl_->vertexBuffer && impl_->indexBuffer;
}

}  // namespace cppgames::planets

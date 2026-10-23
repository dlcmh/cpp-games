#include "Cockpit/CockpitOverlay.hpp"
#include "Cockpit/CockpitState.hpp"

#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>

namespace cppgames::cockpit {
namespace {

// Must match CockpitUniforms in Shaders/cockpit.slang.
struct alignas(16) CockpitUniforms {
    float resolution[2];
    float time;
    float phase;
    float phaseTime;
    float flash;
    float jump;
    float steer;
    float firing;
    float showCanopy;
    float showDashboard;
    float showHud;
    float glassTint;
    float hudScale;
    float padding[2];
};

static_assert(sizeof(CockpitUniforms) == 64, "CockpitUniforms must match Slang");

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

}  // namespace

bool CockpitOverlay::build(MTL::Device* device, const std::string& shaderPath, bool depthAttachment,
                           std::string& error) {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {
        error = "Could not read cockpit shader at " + shaderPath;
        pool->release();
        return false;
    }

    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(makeString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Cockpit shader compilation failed: " + describeError(metalError);
        pool->release();
        return false;
    }

    MTL::Function* vertex = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragment = library->newFunction(makeString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "cockpit.metal is missing vertexMain or fragmentMain";
        if (vertex != nullptr) {
            vertex->release();
        }
        if (fragment != nullptr) {
            fragment->release();
        }
        library->release();
        pool->release();
        return false;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(makeString("Shared X-Wing Cockpit"));
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

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Could not create cockpit pipeline: " + describeError(metalError);
    } else {
        pipeline_ = NS::TransferPtr(pipeline);
    }

    descriptor->release();
    vertex->release();
    fragment->release();
    library->release();

    if (!pipeline_) {
        pool->release();
        return false;
    }

    MTL::DepthStencilDescriptor* depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
    depthDescriptor->setDepthWriteEnabled(false);
    depthState_ = NS::TransferPtr(device->newDepthStencilState(depthDescriptor));
    depthDescriptor->release();

    pool->release();
    return depthState_.get() != nullptr;
}

void CockpitOverlay::encode(MTL::RenderCommandEncoder* encoder, const CockpitState& state,
                            int pixelWidth, int pixelHeight) const {
    if (!pipeline_ || !depthState_) {
        return;
    }

    CockpitUniforms uniforms{};
    uniforms.resolution[0] = static_cast<float>(pixelWidth);
    uniforms.resolution[1] = static_cast<float>(pixelHeight);
    uniforms.time = state.time;
    uniforms.phase = state.phase;
    uniforms.phaseTime = state.phaseTime;
    uniforms.flash = state.flash;
    uniforms.jump = state.jump;
    uniforms.steer = state.steer;
    uniforms.firing = state.firing;
    uniforms.showCanopy = state.showCanopy ? 1.0f : 0.0f;
    uniforms.showDashboard = state.showDashboard ? 1.0f : 0.0f;
    uniforms.showHud = state.showHud ? 1.0f : 0.0f;
    uniforms.glassTint = state.glassTint;
    uniforms.hudScale = state.hudScale;

    encoder->setRenderPipelineState(pipeline_.get());
    encoder->setDepthStencilState(depthState_.get());
    encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(3));
}

bool CockpitOverlay::valid() const {
    return pipeline_ && depthState_;
}

}  // namespace cppgames::cockpit

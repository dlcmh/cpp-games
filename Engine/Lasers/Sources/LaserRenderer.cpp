#include "Lasers/LaserRenderer.hpp"
#include "Lasers/LaserTypes.hpp"

#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace cppgames::lasers {
namespace {

constexpr std::size_t kMaxBeams = 24;

struct alignas(16) ScreenBeam {
    float segment[4];
    float color[4];
    float params[4];
};

struct alignas(16) LaserUniforms {
    float screen[4];
    float counts[4];
    ScreenBeam beams[kMaxBeams];
};

static_assert(sizeof(LaserUniforms) == 16 + 16 + 48 * kMaxBeams, "LaserUniforms must match Slang");

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

bool LaserRenderer::build(MTL::Device* device, const std::string& shaderPath, bool depthAttachment,
                          std::string& error) {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {
        error = "Could not read laser shader at " + shaderPath;
        pool->release();
        return false;
    }

    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(makeString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Laser shader compilation failed: " + describeError(metalError);
        pool->release();
        return false;
    }

    MTL::Function* vertex = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragment = library->newFunction(makeString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "laser.metal is missing vertexMain or fragmentMain";
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
    descriptor->setLabel(makeString("Shared X-Wing Lasers"));
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
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOne);
    attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOne);
    attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Could not create laser pipeline: " + describeError(metalError);
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

void LaserRenderer::encode(MTL::RenderCommandEncoder* encoder, const LaserFrame& frame,
                           const std::vector<LaserBeam>& beams) const {
    if (!pipeline_ || !depthState_ || beams.empty()) {
        return;
    }

    LaserUniforms uniforms{};
    uniforms.screen[0] = frame.aspect;

    const float* m = frame.viewProjection;
    std::size_t count = 0;
    for (const LaserBeam& beam : beams) {
        if (count >= kMaxBeams) {
            break;
        }

        const auto project = [&](const float point[3], float& x, float& y, float& w) {
            const float px = point[0];
            const float py = point[1];
            const float pz = point[2];
            const float clipX = m[0] * px + m[4] * py + m[8] * pz + m[12];
            const float clipY = m[1] * px + m[5] * py + m[9] * pz + m[13];
            w = m[3] * px + m[7] * py + m[11] * pz + m[15];
            if (w <= 0.0001f) {
                return false;
            }
            x = (clipX / w) * frame.aspect;
            y = clipY / w;
            return true;
        };

        float startX = 0.0f;
        float startY = 0.0f;
        float startW = 0.0f;
        float endX = 0.0f;
        float endY = 0.0f;
        float endW = 0.0f;
        if (!project(beam.start, startX, startY, startW) || !project(beam.end, endX, endY, endW)) {
            continue;
        }

        ScreenBeam& target = uniforms.beams[count];
        target.segment[0] = startX;
        target.segment[1] = startY;
        target.segment[2] = endX;
        target.segment[3] = endY;
        target.color[0] = beam.color[0];
        target.color[1] = beam.color[1];
        target.color[2] = beam.color[2];
        target.color[3] = beam.seed;
        target.params[0] = beam.width * frame.projectionScale / startW;
        target.params[1] = beam.width * frame.projectionScale / endW;
        target.params[2] = static_cast<float>(beam.style);
        target.params[3] = beam.intensity;
        ++count;
    }

    if (count == 0) {
        return;
    }
    uniforms.counts[0] = static_cast<float>(count);

    encoder->setRenderPipelineState(pipeline_.get());
    encoder->setDepthStencilState(depthState_.get());
    encoder->setCullMode(MTL::CullModeNone);
    encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(3));
}

bool LaserRenderer::valid() const {
    return pipeline_ && depthState_;
}

}  // namespace cppgames::lasers

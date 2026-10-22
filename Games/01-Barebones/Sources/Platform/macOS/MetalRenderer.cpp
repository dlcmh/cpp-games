#include "Platform/macOS/MetalRenderer.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <CoreGraphics/CoreGraphics.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace barebones {
namespace {

// Must match Uniforms in Shaders/common.slang exactly. The buffer is 16 bytes:
// float2 (8) + float (4) + float (4).
struct Uniforms {
    float resolution[2];
    float time;
    float angle;
};
static_assert(sizeof(Uniforms) == 16, "Uniforms must match the Slang constant buffer layout");
static_assert(offsetof(Uniforms, time) == 8, "Uniforms layout drifted");

std::string readTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
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

NS::String* makeString(const char* text) {
    return NS::String::string(text, NS::UTF8StringEncoding);
}

class MetalRenderer final : public IRenderer {
public:
    MetalRenderer() = default;
    ~MetalRenderer() override = default;

    bool initialize(const NativeSurface& surface,
                    const std::string& shaderPath,
                    std::string& error) override;

    void resize(int pixelWidth, int pixelHeight) override;

    bool draw(const RenderFrame& frame, std::string& error) override;

private:
    bool buildPipeline(const std::string& shaderPath, std::string& error);

    NS::SharedPtr<MTL::Device> device_;
    NS::SharedPtr<MTL::CommandQueue> commandQueue_;
    NS::SharedPtr<MTL::RenderPipelineState> pipelineState_;
    CA::MetalLayer* layer_{nullptr};
    int pixelWidth_{0};
    int pixelHeight_{0};
};

bool MetalRenderer::initialize(const NativeSurface& surface,
                               const std::string& shaderPath,
                               std::string& error) {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    if (surface.metalLayer == nullptr) {
        error = "SDL did not provide a Metal layer for the window.";
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

    if (!buildPipeline(shaderPath, error)) {
        pool->release();
        return false;
    }

    pool->release();
    return true;
}

bool MetalRenderer::buildPipeline(const std::string& shaderPath, std::string& error) {
    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {
        error = "Could not read compiled shader at " + shaderPath;
        return false;
    }

    NS::Error* metalError = nullptr;
    MTL::Library* library = device_->newLibrary(makeString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Metal shader compilation failed: " + describeError(metalError);
        return false;
    }

    MTL::Function* vertexFunction = library->newFunction(makeString("vertexMain"));
    MTL::Function* fragmentFunction = library->newFunction(makeString("fragmentMain"));
    if (vertexFunction == nullptr || fragmentFunction == nullptr) {
        error = "Compiled shader is missing vertexMain or fragmentMain.";
        if (vertexFunction != nullptr) {
            vertexFunction->release();
        }
        if (fragmentFunction != nullptr) {
            fragmentFunction->release();
        }
        library->release();
        return false;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(makeString("Barebones Letter A"));
    descriptor->setVertexFunction(vertexFunction);
    descriptor->setFragmentFunction(fragmentFunction);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline =
        device_->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Could not create the render pipeline: " + describeError(metalError);
    } else {
        pipelineState_ = NS::TransferPtr(pipeline);
    }

    descriptor->release();
    vertexFunction->release();
    fragmentFunction->release();
    library->release();
    return pipelineState_.get() != nullptr;
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
    layer_->setDrawableSize(CGSizeMake(static_cast<CGFloat>(pixelWidth),
                                       static_cast<CGFloat>(pixelHeight)));
}

bool MetalRenderer::draw(const RenderFrame& frame, std::string& error) {
    if (frame.pixelWidth <= 0 || frame.pixelHeight <= 0) {
        return true;
    }

    resize(frame.pixelWidth, frame.pixelHeight);

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    CA::MetalDrawable* drawable = layer_->nextDrawable();
    if (drawable == nullptr) {
        // The window can be occluded or minimized; skip this frame.
        pool->release();
        return true;
    }

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* attachment = pass->colorAttachments()->object(0);
    attachment->setTexture(drawable->texture());
    attachment->setLoadAction(MTL::LoadActionClear);
    attachment->setStoreAction(MTL::StoreActionStore);
    attachment->setClearColor(MTL::ClearColor(0.012, 0.018, 0.045, 1.0));

    Uniforms uniforms{};
    uniforms.resolution[0] = static_cast<float>(pixelWidth_);
    uniforms.resolution[1] = static_cast<float>(pixelHeight_);
    uniforms.time = frame.elapsedSeconds;
    uniforms.angle = frame.angleRadians;

    MTL::CommandBuffer* commandBuffer = commandQueue_->commandBuffer();
    if (commandBuffer == nullptr) {
        pass->release();
        pool->release();
        error = "Could not create a Metal command buffer.";
        return false;
    }

    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
    encoder->setRenderPipelineState(pipelineState_.get());
    encoder->setVertexBytes(&uniforms, sizeof(uniforms), 0);
    encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                            static_cast<NS::UInteger>(0),
                            static_cast<NS::UInteger>(3));
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

}  // namespace barebones

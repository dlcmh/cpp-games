#include "Platform/macOS/ShaderPipeline.hpp"

#include <fstream>
#include <ios>
#include <iterator>
#include <string>

namespace rotatables {
namespace {

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

}  // namespace

bool ShaderPipeline::build(MTL::Device* device, const std::string& shaderPath, std::string& error) {
    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {
        error = "Could not read compiled shader at " + shaderPath;
        return false;
    }

    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(makeString(source.c_str()), nullptr, &metalError);
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
    descriptor->setLabel(makeString("Rotatables Skins"));
    descriptor->setVertexFunction(vertexFunction);
    descriptor->setFragmentFunction(fragmentFunction);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Could not create the render pipeline: " + describeError(metalError);
    } else {
        pipeline_ = NS::TransferPtr(pipeline);
    }

    descriptor->release();
    vertexFunction->release();
    fragmentFunction->release();
    library->release();
    return pipeline_.get() != nullptr;
}

MTL::RenderPipelineState* ShaderPipeline::state() const {
    return pipeline_.get();
}

}  // namespace rotatables

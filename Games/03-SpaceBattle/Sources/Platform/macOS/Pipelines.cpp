#include "Platform/macOS/Pipelines.hpp"

#include "Platform/macOS/MeshLibrary.hpp"

#include <fstream>
#include <ios>
#include <iterator>
#include <string>

namespace spacebattle {
namespace {

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

MTL::DepthStencilState* makeDepthState(MTL::Device* device, MTL::CompareFunction compare,
                                       bool write) {
    MTL::DepthStencilDescriptor* descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    descriptor->setDepthCompareFunction(compare);
    descriptor->setDepthWriteEnabled(write);
    MTL::DepthStencilState* state = device->newDepthStencilState(descriptor);
    descriptor->release();
    return state;
}

MTL::RenderPipelineState* makeScenePipeline(MTL::Device* device, MTL::Function* vertex,
                                            MTL::Function* fragment, bool additive,
                                            std::string& error) {
    MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    vertexDescriptor->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    vertexDescriptor->attributes()->object(0)->setOffset(0);
    vertexDescriptor->attributes()->object(0)->setBufferIndex(2);
    vertexDescriptor->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    vertexDescriptor->attributes()->object(1)->setOffset(sizeof(float) * 3);
    vertexDescriptor->attributes()->object(1)->setBufferIndex(2);
    vertexDescriptor->attributes()->object(2)->setFormat(MTL::VertexFormatFloat3);
    vertexDescriptor->attributes()->object(2)->setOffset(sizeof(float) * 6);
    vertexDescriptor->attributes()->object(2)->setBufferIndex(2);
    vertexDescriptor->layouts()->object(2)->setStride(sizeof(MeshVertex));
    vertexDescriptor->layouts()->object(2)->setStepFunction(MTL::VertexStepFunctionPerVertex);

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(makeString(additive ? "SpaceBattle Additive" : "SpaceBattle Scene"));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    descriptor->setVertexDescriptor(vertexDescriptor);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
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
        error = std::string("Could not create ") + (additive ? "additive" : "scene") +
                " pipeline: " + describeError(metalError);
    }

    descriptor->release();
    vertexDescriptor->release();
    return pipeline;
}

}  // namespace

bool Pipelines::build(MTL::Device* device, const std::string& shaderDirectory, std::string& error) {
    MTL::Library* sceneLibrary = compileLibrary(device, shaderDirectory + "/scene.metal", error);
    if (sceneLibrary == nullptr) {
        return false;
    }

    MTL::Function* sceneVertex = sceneLibrary->newFunction(makeString("vertexMain"));
    MTL::Function* sceneFragment = sceneLibrary->newFunction(makeString("fragmentMain"));
    if (sceneVertex == nullptr || sceneFragment == nullptr) {
        error = "scene.metal is missing vertexMain or fragmentMain";
        sceneLibrary->release();
        return false;
    }

    MTL::RenderPipelineState* opaquePipeline =
        makeScenePipeline(device, sceneVertex, sceneFragment, false, error);
    MTL::RenderPipelineState* additivePipeline =
        makeScenePipeline(device, sceneVertex, sceneFragment, true, error);
    sceneVertex->release();
    sceneFragment->release();
    sceneLibrary->release();

    if (opaquePipeline == nullptr || additivePipeline == nullptr) {
        if (opaquePipeline != nullptr) {
            opaquePipeline->release();
        }
        if (additivePipeline != nullptr) {
            additivePipeline->release();
        }
        return false;
    }
    opaque_ = NS::TransferPtr(opaquePipeline);
    additive_ = NS::TransferPtr(additivePipeline);

    sceneDepth_ = NS::TransferPtr(makeDepthState(device, MTL::CompareFunctionLess, true));
    effectDepth_ = NS::TransferPtr(makeDepthState(device, MTL::CompareFunctionLess, false));

    return sceneDepth_.get() != nullptr && effectDepth_.get() != nullptr;
}

MTL::RenderPipelineState* Pipelines::opaque() const {
    return opaque_.get();
}

MTL::RenderPipelineState* Pipelines::additive() const {
    return additive_.get();
}

MTL::DepthStencilState* Pipelines::sceneDepth() const {
    return sceneDepth_.get();
}

MTL::DepthStencilState* Pipelines::effectDepth() const {
    return effectDepth_.get();
}

}  // namespace spacebattle

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;

struct alignas(16) Uniforms {
    float resolution[2];
    float time;
    float padding;
};

static_assert(sizeof(Uniforms) == 16, "Uniforms must match the Slang declarations");

void logError(const char* message) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", message, SDL_GetError());
}

NS::String* nsString(const char* text) {
    return NS::String::string(text, NS::UTF8StringEncoding);
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string describeMetalError(NS::Error* error) {
    if (error == nullptr || error->localizedDescription() == nullptr) {
        return "unknown Metal error";
    }
    return error->localizedDescription()->utf8String();
}

std::filesystem::path shaderDirectory() {
    if (const char* overridePath = std::getenv("SHADERS_METAL_DIR")) {
        return overridePath;
    }
    if (const char* basePath = SDL_GetBasePath()) {
        std::error_code error;
        const std::filesystem::path candidate =
            std::filesystem::path(basePath) / ".." / "Resources" / "shaders";
        if (std::filesystem::is_directory(candidate, error)) {
            return candidate.lexically_normal();
        }
    }
    return "Shaders";
}

MTL::RenderPipelineState* buildPipeline(MTL::Device* device, const std::string& source,
                                        std::string& error) {
    NS::Error* metalError = nullptr;
    MTL::Library* library = device->newLibrary(nsString(source.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        error = "Shader compilation failed: " + describeMetalError(metalError);
        return nullptr;
    }

    MTL::Function* vertex = library->newFunction(nsString("vertexMain"));
    MTL::Function* fragment = library->newFunction(nsString("fragmentMain"));
    if (vertex == nullptr || fragment == nullptr) {
        error = "Shader is missing vertexMain or fragmentMain";
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
    descriptor->setLabel(nsString("Shaders Example"));
    descriptor->setVertexFunction(vertex);
    descriptor->setFragmentFunction(fragment);
    MTL::RenderPipelineColorAttachmentDescriptor* attachment =
        descriptor->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    attachment->setBlendingEnabled(true);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

    metalError = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &metalError);
    if (pipeline == nullptr) {
        error = "Pipeline creation failed: " + describeMetalError(metalError);
    }

    descriptor->release();
    vertex->release();
    fragment->release();
    library->release();
    return pipeline;
}

void setTitle(SDL_Window* window, const std::string& example) {
    const std::string title = "Shaders - " + example;
    SDL_SetWindowTitle(window, title.c_str());
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string example = argc > 1 ? argv[1] : "1";

    SDL_SetAppMetadata("Shaders", "0.1.0", "com.dlcmh.cppgames.shaders");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Shaders", kWindowWidth, kWindowHeight,
                         SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        logError("SDL_CreateWindow failed");
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 480, 360);

    SDL_MetalView metalView = SDL_Metal_CreateView(window);
    if (metalView == nullptr) {
        logError("SDL_Metal_CreateView failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    CA::MetalLayer* layer = static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(metalView));
    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    MTL::CommandQueue* queue = device != nullptr ? device->newCommandQueue() : nullptr;
    if (layer == nullptr || device == nullptr || queue == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create Metal device or queue.");
        SDL_Metal_DestroyView(metalView);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setFramebufferOnly(true);

    const std::filesystem::path shaderPath = shaderDirectory() / (example + ".metal");
    const std::string shaderSource = readTextFile(shaderPath);
    std::string pipelineError;
    MTL::RenderPipelineState* pipeline = buildPipeline(device, shaderSource, pipelineError);
    if (pipeline == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load %s: %s",
                     shaderPath.string().c_str(), pipelineError.c_str());
        queue->release();
        device->release();
        SDL_Metal_DestroyView(metalView);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    setTitle(window, example);

    bool running = true;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    float time = 0.0f;
    int pixelWidth = kWindowWidth;
    int pixelHeight = kWindowHeight;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_Q) {
                    running = false;
                } else if (event.key.key == SDLK_F || event.key.key == SDLK_F11) {
                    const bool fullscreen =
                        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                    SDL_SetWindowFullscreen(window, !fullscreen);
                }
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        time += static_cast<float>(now - previousCounter) / static_cast<float>(frequency);
        previousCounter = now;

        SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
        if (pixelWidth <= 0 || pixelHeight <= 0) {
            continue;
        }
        layer->setDrawableSize(
            CGSizeMake(static_cast<CGFloat>(pixelWidth), static_cast<CGFloat>(pixelHeight)));

        CA::MetalDrawable* drawable = layer->nextDrawable();
        if (drawable == nullptr) {
            continue;
        }

        Uniforms uniforms{};
        uniforms.resolution[0] = static_cast<float>(pixelWidth);
        uniforms.resolution[1] = static_cast<float>(pixelHeight);
        uniforms.time = time;

        MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
        MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
        color->setTexture(drawable->texture());
        color->setLoadAction(MTL::LoadActionClear);
        color->setStoreAction(MTL::StoreActionStore);
        color->setClearColor(MTL::ClearColor(0.015, 0.022, 0.035, 1.0));

        MTL::CommandBuffer* commandBuffer = queue->commandBuffer();
        MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
        encoder->setRenderPipelineState(pipeline);
        encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                                static_cast<NS::UInteger>(3));
        encoder->endEncoding();

        commandBuffer->presentDrawable(drawable);
        commandBuffer->commit();
        pass->release();
    }

    pipeline->release();
    queue->release();
    device->release();
    SDL_Metal_DestroyView(metalView);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

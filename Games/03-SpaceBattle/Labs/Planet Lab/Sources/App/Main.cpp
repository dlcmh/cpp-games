#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include "Planets/PlanetSpec.hpp"
#include "Platform/macOS/PlanetLabRenderer.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 720;

void logError(const std::string& context) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", context.c_str(), SDL_GetError());
}

std::filesystem::path resolveShaderDirectory() {
    if (const char* overridePath = std::getenv("PLANET_LAB_SHADER_DIR")) {
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

void updateWindowTitle(SDL_Window* window, const cppgames::planets::PlanetSpec& spec, bool paused) {
    char title[320];
    std::snprintf(title, sizeof(title),
                  "Planet Lab | #%016" PRIx64 " | %s | Rings: %s | %s | [Space] new planet",
                  spec.seed, cppgames::planets::planetTypeName(spec.type),
                  spec.hasRings ? "yes" : "no", paused ? "PAUSED" : "SPINNING");
    static std::string previousTitle;
    if (title != previousTitle) {
        SDL_SetWindowTitle(window, title);
        previousTitle = title;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("Planet Lab", "0.1.0", "com.dlcmh.cppgames.planetlab");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Planet Lab", kInitialWindowWidth, kInitialWindowHeight,
                         SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        logError("SDL_CreateWindow failed");
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 800, 500);

    SDL_MetalView metalView = SDL_Metal_CreateView(window);
    if (metalView == nullptr) {
        logError("SDL_Metal_CreateView failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    void* metalLayer = SDL_Metal_GetLayer(metalView);
    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);

    planetlab::PlanetLabRenderer renderer;
    std::string rendererError;
    const planetlab::NativeSurface surface{metalLayer, pixelWidth, pixelHeight};
    if (!renderer.initialize(surface, resolveShaderDirectory().string(), rendererError)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer initialization failed: %s",
                     rendererError.c_str());
        SDL_Metal_DestroyView(metalView);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    std::uint64_t seed = cppgames::planets::kShowcasePlanetSeed;
    cppgames::planets::PlanetSpec spec = cppgames::planets::generatePlanet(seed);
    float time = 0.0f;
    bool paused = false;
    bool running = true;
    updateWindowTitle(window, spec, paused);

    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const Uint64 counterFrequency = SDL_GetPerformanceFrequency();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                continue;
            }
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
                continue;
            }

            switch (event.key.key) {
                case SDLK_ESCAPE:
                case SDLK_Q:
                    running = false;
                    break;
                case SDLK_SPACE:
                    seed = cppgames::planets::nextSeed(seed);
                    spec = cppgames::planets::generatePlanet(seed);
                    time = 0.0f;
                    paused = false;
                    updateWindowTitle(window, spec, paused);
                    break;
                case SDLK_P:
                    paused = !paused;
                    updateWindowTitle(window, spec, paused);
                    break;
                case SDLK_F:
                case SDLK_F11: {
                    const bool isFullscreen =
                        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                    SDL_SetWindowFullscreen(window, !isFullscreen);
                    break;
                }
                default:
                    break;
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const double frameSeconds =
            static_cast<double>(now - previousCounter) / static_cast<double>(counterFrequency);
        previousCounter = now;
        const float deltaSeconds = static_cast<float>(std::min(frameSeconds, 0.1));
        if (!paused) {
            time += deltaSeconds;
        }

        if (!SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight)) {
            pixelWidth = 0;
            pixelHeight = 0;
        }
        renderer.resize(pixelWidth, pixelHeight);

        if (!renderer.draw(spec, time, rendererError)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render failed: %s", rendererError.c_str());
            running = false;
        }
    }

    SDL_Metal_DestroyView(metalView);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

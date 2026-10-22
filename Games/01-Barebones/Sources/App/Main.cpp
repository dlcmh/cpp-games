#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include "Core/Game.hpp"
#include "Core/GameLoop.hpp"
#include "Core/GameTypes.hpp"
#include "Platform/macOS/MetalRenderer.hpp"
#include "Render/IRenderer.hpp"
#include "Render/PipelineState.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr int kInitialWindowWidth = 960;
constexpr int kInitialWindowHeight = 720;

void logError(const std::string& context) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", context.c_str(), SDL_GetError());
}

std::filesystem::path resolveShaderPath() {
    if (const char* overridePath = std::getenv("BAREBONES_SHADER")) {
        return overridePath;
    }

    if (const char* basePath = SDL_GetBasePath()) {
        std::error_code error;
        std::filesystem::path candidate =
            std::filesystem::path(basePath) / ".." / "Resources" / "shaders" / "sprite.metal";
        if (std::filesystem::exists(candidate, error)) {
            return candidate.lexically_normal();
        }
    }

    return "Shaders/sprite.metal";
}

std::string windowTitle(const barebones::GameState& state) {
    std::string title = "Barebones - Rotating A";
    if (state.paused) {
        title += "  [paused]";
    }

    char speed[64];
    SDL_snprintf(speed, sizeof(speed), "  -  %.2f rad/s", static_cast<double>(state.rotationSpeed));
    title += speed;
    return title;
}

void updateWindowTitle(SDL_Window* window, const barebones::GameState& state) {
    static std::string previousTitle;
    std::string title = windowTitle(state);
    if (title != previousTitle) {
        SDL_SetWindowTitle(window, title.c_str());
        previousTitle = std::move(title);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("Barebones", "0.1.0", "com.dlcmh.cppgames.barebones");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Barebones - Rotating A", kInitialWindowWidth, kInitialWindowHeight,
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

    void* metalLayer = SDL_Metal_GetLayer(metalView);
    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);

    std::unique_ptr<barebones::IRenderer> renderer = barebones::createMetalRenderer();
    std::string rendererError;
    const barebones::NativeSurface surface{metalLayer, pixelWidth, pixelHeight};
    if (!renderer->initialize(surface, resolveShaderPath().string(), rendererError)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer initialization failed: %s",
                     rendererError.c_str());
        SDL_Metal_DestroyView(metalView);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    barebones::Game game;
    barebones::GameLoop gameLoop;
    updateWindowTitle(window, game.state());

    bool running = true;
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
                    game.togglePause();
                    break;
                case SDLK_UP:
                case SDLK_W:
                    game.increaseSpeed();
                    break;
                case SDLK_DOWN:
                case SDLK_S:
                    game.decreaseSpeed();
                    break;
                case SDLK_R:
                    game.reset();
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

        gameLoop.advance(frameSeconds, [&game](double fixedDelta) {
            game.update(static_cast<float>(fixedDelta));
        });

        if (!SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight)) {
            pixelWidth = 0;
            pixelHeight = 0;
        }

        const barebones::GameState& state = game.state();
        const float extraAngle =
            state.paused ? 0.0f
                         : state.rotationSpeed * static_cast<float>(gameLoop.remainderSeconds());
        const barebones::RenderFrame frame{
            state.angleRadians + extraAngle,
            state.elapsedSeconds + static_cast<float>(gameLoop.remainderSeconds()),
            pixelWidth,
            pixelHeight,
        };

        if (!renderer->draw(frame, rendererError)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render failed: %s", rendererError.c_str());
            running = false;
        }

        updateWindowTitle(window, state);
    }

    renderer.reset();
    SDL_Metal_DestroyView(metalView);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

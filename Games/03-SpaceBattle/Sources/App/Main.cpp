#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include "Core/BattleGame.hpp"
#include "Core/BattleState.hpp"
#include "Core/GameLoop.hpp"
#include "Platform/macOS/MetalRenderer.hpp"
#include "Render/IRenderer.hpp"
#include "Render/RenderFrame.hpp"
#include "Render/SceneBuilder.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 720;

void logError(const std::string& context) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", context.c_str(), SDL_GetError());
}

std::filesystem::path resolveShaderDirectory() {
    if (const char* overridePath = std::getenv("SPACEBATTLE_SHADER_DIR")) {
        return overridePath;
    }

    if (const char* basePath = SDL_GetBasePath()) {
        std::error_code error;
        std::filesystem::path candidate =
            std::filesystem::path(basePath) / ".." / "Resources" / "shaders";
        if (std::filesystem::is_directory(candidate, error)) {
            return candidate.lexically_normal();
        }
    }

    return "Shaders";
}

std::string windowTitle(const spacebattle::BattleState& state) {
    char title[192];
    SDL_snprintf(title, sizeof(title), "Space Battle - %s%s | Death Stars destroyed: %d",
                 std::string(spacebattle::phaseName(state.phase)).c_str(),
                 state.paused ? " [paused]" : "", state.deathStarsDestroyed);
    return title;
}

void updateWindowTitle(SDL_Window* window, const spacebattle::BattleState& state) {
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

    SDL_SetAppMetadata("Space Battle", "0.1.0", "com.dlcmh.cppgames.spacebattle");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Space Battle", kInitialWindowWidth, kInitialWindowHeight,
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

    std::unique_ptr<spacebattle::IRenderer> renderer = spacebattle::createMetalRenderer();
    std::string rendererError;
    const spacebattle::NativeSurface surface{metalLayer, pixelWidth, pixelHeight};
    if (!renderer->initialize(surface, resolveShaderDirectory().string(), rendererError)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer initialization failed: %s",
                     rendererError.c_str());
        SDL_Metal_DestroyView(metalView);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    spacebattle::BattleGame game;
    spacebattle::GameLoop gameLoop;
    spacebattle::SceneBuilder sceneBuilder;
    spacebattle::RenderFrame frame;
    frame.opaque.reserve(96);
    frame.additive.reserve(32);

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

            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_A ||
                    event.key.key == SDLK_RIGHT || event.key.key == SDLK_D) {
                    game.setSteer(0.0f);
                }
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
                case SDLK_LEFT:
                case SDLK_A:
                    game.setSteer(-1.0f);
                    break;
                case SDLK_RIGHT:
                case SDLK_D:
                    game.setSteer(1.0f);
                    break;
                case SDLK_SPACE:
                    game.fire();
                    break;
                case SDLK_P:
                    game.togglePause();
                    break;
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                case SDLK_5:
                    game.previewPhase(event.key.key - SDLK_0);
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

        const spacebattle::BattleState& state = game.state();
        spacebattle::BattleState renderState = state;
        const float remainder = static_cast<float>(gameLoop.remainderSeconds());
        renderState.phaseTime += remainder;
        renderState.elapsedSeconds += remainder;

        frame.planetSpec = game.planet();
        sceneBuilder.build(renderState, frame);
        frame.pixelWidth = pixelWidth;
        frame.pixelHeight = pixelHeight;

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

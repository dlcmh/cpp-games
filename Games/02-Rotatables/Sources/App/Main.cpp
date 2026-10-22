#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include "Core/Game.hpp"
#include "Core/GameLoop.hpp"
#include "Core/GameState.hpp"
#include "Core/Skins.hpp"
#include "Platform/macOS/MetalRenderer.hpp"
#include "Render/IRenderer.hpp"
#include "Render/RenderFrame.hpp"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace {

static_assert(rotatables::kGlyphCount == rotatables::kRenderGlyphCount,
              "Core and render character counts must stay in sync");

constexpr int kInitialWindowWidth = 1100;
constexpr int kInitialWindowHeight = 720;

void logError(const std::string& context) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", context.c_str(), SDL_GetError());
}

std::filesystem::path resolveShaderPath() {
    if (const char* overridePath = std::getenv("ROTATABLES_SHADER")) {
        return overridePath;
    }

    if (const char* basePath = SDL_GetBasePath()) {
        std::error_code error;
        std::filesystem::path candidate =
            std::filesystem::path(basePath) / ".." / "Resources" / "shaders" / "scene.metal";
        if (std::filesystem::exists(candidate, error)) {
            return candidate.lexically_normal();
        }
    }

    return "Shaders/scene.metal";
}

std::string windowTitle(const rotatables::GameState& state) {
    const rotatables::CharacterState& characterA = state.characters[0];
    const rotatables::CharacterState& characterB = state.characters[1];
    const char* selectedName = state.selectedIndex == 0 ? "A" : "B";
    const char* selectedSkin =
        rotatables::skinName(state.selectedIndex == 0 ? characterA.skin : characterB.skin);

    char title[192];
    SDL_snprintf(title, sizeof(title), "Rotatables - selected %s (%s) | A: %s | B: %s",
                 selectedName, selectedSkin, rotatables::skinName(characterA.skin),
                 rotatables::skinName(characterB.skin));
    return title;
}

void updateWindowTitle(SDL_Window* window, const rotatables::GameState& state) {
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

    SDL_SetAppMetadata("Rotatables", "0.1.0", "com.dlcmh.cppgames.rotatables");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Rotatables", kInitialWindowWidth, kInitialWindowHeight,
                         SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        logError("SDL_CreateWindow failed");
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowMinimumSize(window, 640, 420);

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

    std::unique_ptr<rotatables::IRenderer> renderer = rotatables::createMetalRenderer();
    std::string rendererError;
    const rotatables::NativeSurface surface{metalLayer, pixelWidth, pixelHeight};
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

    rotatables::Game game;
    rotatables::GameLoop gameLoop;
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
                case SDLK_LEFT:
                    game.select(rotatables::Glyph::A);
                    break;
                case SDLK_RIGHT:
                    game.select(rotatables::Glyph::B);
                    break;
                case SDLK_SPACE:
                    game.cycleSelectedSkin();
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

        const rotatables::GameState& state = game.state();
        const float remainder = static_cast<float>(gameLoop.remainderSeconds());

        rotatables::RenderFrame frame{};
        frame.elapsedSeconds = state.elapsedSeconds + remainder;
        frame.pixelWidth = pixelWidth;
        frame.pixelHeight = pixelHeight;
        frame.selectedIndex = state.selectedIndex;
        for (std::size_t index = 0; index < rotatables::kGlyphCount; ++index) {
            const rotatables::CharacterState& character = state.characters[index];
            frame.characters[index] = rotatables::CharacterFrame{
                character.angleRadians + character.rotationSpeed * remainder,
                static_cast<int>(character.skin),
            };
        }

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

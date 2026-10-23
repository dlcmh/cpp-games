#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include "Demo/CockpitDemo.hpp"
#include "Platform/macOS/PrototypeRenderer.hpp"

#include <algorithm>
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
    if (const char* overridePath = std::getenv("XWING_LAB_SHADER_DIR")) {
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

void updateWindowTitle(SDL_Window* window, const std::string& title) {
    static std::string previousTitle;
    if (title != previousTitle) {
        SDL_SetWindowTitle(window, title.c_str());
        previousTitle = title;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("X-Wing Cockpit Lab", "0.1.0", "com.dlcmh.cppgames.cockpitlab");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("X-Wing Cockpit Lab", kInitialWindowWidth, kInitialWindowHeight,
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

    cockpitlab::PrototypeRenderer renderer;
    std::string rendererError;
    const cockpitlab::NativeSurface surface{metalLayer, pixelWidth, pixelHeight};
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

    cockpitlab::CockpitDemo demo;
    updateWindowTitle(window, demo.windowTitle());

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
                    demo.setSteer(0.0f);
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
                    demo.setSteer(-1.0f);
                    break;
                case SDLK_RIGHT:
                case SDLK_D:
                    demo.setSteer(1.0f);
                    break;
                case SDLK_SPACE:
                    demo.fire();
                    break;
                case SDLK_J:
                    demo.triggerJump();
                    break;
                case SDLK_X:
                    demo.triggerFlash();
                    break;
                case SDLK_P:
                    demo.togglePause();
                    break;
                case SDLK_T:
                    demo.toggleAuto();
                    break;
                case SDLK_H:
                    demo.toggleHud();
                    break;
                case SDLK_C:
                    demo.toggleCanopy();
                    break;
                case SDLK_V:
                    demo.toggleDashboard();
                    break;
                case SDLK_LEFTBRACKET:
                    demo.adjustGlassTint(-0.05f);
                    break;
                case SDLK_RIGHTBRACKET:
                    demo.adjustGlassTint(0.05f);
                    break;
                case SDLK_MINUS:
                    demo.adjustHudScale(-0.05f);
                    break;
                case SDLK_EQUALS:
                    demo.adjustHudScale(0.05f);
                    break;
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                    demo.applyPreset(event.key.key - SDLK_0);
                    break;
                case SDLK_R:
                    demo.reset();
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
        demo.update(static_cast<float>(std::min(frameSeconds, 0.1)));

        if (!SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight)) {
            pixelWidth = 0;
            pixelHeight = 0;
        }
        renderer.resize(pixelWidth, pixelHeight);

        if (!renderer.draw(demo.state(), rendererError)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render failed: %s", rendererError.c_str());
            running = false;
        }

        updateWindowTitle(window, demo.windowTitle());
    }

    SDL_Metal_DestroyView(metalView);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include "Core/Track.hpp"
#include "Platform/macOS/RollerCoasterRenderer.hpp"
#include "Render/TrackMesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kFixedStep = 1.0f / 120.0f;

void logError(const char* context) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", context, SDL_GetError());
}

std::filesystem::path resolveShaderDirectory() {
    if (const char* overridePath = std::getenv("ROLLER_COASTER_SHADER_DIR")) {
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

float speedAt(const rollercoaster::Track& track, const rollercoaster::PathSample& sample) {
    return std::clamp(
        std::sqrt(2.0f * 9.81f * std::max(0.5f, track.startHeight - sample.position.y)), 5.0f,
        15.0f);
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("Roller Coaster", "0.1.0", "com.dlcmh.cppgames.rollercoaster");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Roller Coaster - first person", kWindowWidth, kWindowHeight,
                         SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        logError("SDL_CreateWindow failed");
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 640, 360);

    SDL_MetalView view = SDL_Metal_CreateView(window);
    if (view == nullptr) {
        logError("SDL_Metal_CreateView failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    void* metalLayer = SDL_Metal_GetLayer(view);
    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);

    const rollercoaster::Track track = rollercoaster::buildTrack();
    const rollercoaster::TrackMesh mesh = rollercoaster::buildTrackMesh(track);

    rollercoaster::RollerCoasterRenderer renderer;
    std::string rendererError;
    const rollercoaster::NativeSurface surface{metalLayer, pixelWidth, pixelHeight};
    if (!renderer.initialize(surface, resolveShaderDirectory().string(), rendererError)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer initialization failed: %s",
                     rendererError.c_str());
        SDL_Metal_DestroyView(view);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    float distance = 0.0f;
    float accumulator = 0.0f;
    bool paused = false;
    bool running = true;
    bool titlePaused = false;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                    case SDLK_Q:
                        running = false;
                        break;
                    case SDLK_P:
                        paused = !paused;
                        break;
                    case SDLK_R:
                        distance = 0.0f;
                        accumulator = 0.0f;
                        break;
                    case SDLK_F:
                    case SDLK_F11: {
                        const bool fullscreen =
                            (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                        SDL_SetWindowFullscreen(window, !fullscreen);
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const double frameSeconds =
            static_cast<double>(now - previousCounter) / static_cast<double>(frequency);
        previousCounter = now;

        if (!paused) {
            accumulator += static_cast<float>(std::min(frameSeconds, 0.1));
            int steps = 0;
            while (accumulator >= kFixedStep && steps < 12) {
                const rollercoaster::PathSample sample = rollercoaster::sampleAt(track, distance);
                distance = std::fmod(distance + speedAt(track, sample) * kFixedStep, track.length);
                accumulator -= kFixedStep;
                ++steps;
            }
            if (steps == 12) {
                accumulator = 0.0f;
            }
        }

        SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
        renderer.resize(pixelWidth, pixelHeight);

        const rollercoaster::PathSample current = rollercoaster::sampleAt(track, distance);
        const rollercoaster::PathSample ahead = rollercoaster::sampleAt(track, distance + 8.0f);
        const rollercoaster::Camera camera{
            current.position + current.up * 1.1f,
            ahead.position + ahead.up * 1.1f,
            current.up,
        };

        if (!renderer.draw(mesh, camera, rendererError)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render failed: %s", rendererError.c_str());
            running = false;
        }

        if (paused != titlePaused) {
            SDL_SetWindowTitle(window, paused ? "Roller Coaster - first person  [paused]"
                                              : "Roller Coaster - first person");
            titlePaused = paused;
        }
    }

    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

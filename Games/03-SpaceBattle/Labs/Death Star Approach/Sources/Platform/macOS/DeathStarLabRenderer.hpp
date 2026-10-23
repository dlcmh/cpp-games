#pragma once

#include <memory>
#include <string>

namespace deathstarlab {

struct NativeSurface {
    void* metalLayer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

enum class Phase {
    Approach,
    Trench,
};

struct RenderState {
    Phase phase{Phase::Approach};
    float time{0.0f};
    float phaseTime{0.0f};
    bool cockpitVisible{true};
};

class DeathStarLabRenderer {
public:
    DeathStarLabRenderer();
    ~DeathStarLabRenderer();
    DeathStarLabRenderer(const DeathStarLabRenderer&) = delete;
    DeathStarLabRenderer& operator=(const DeathStarLabRenderer&) = delete;

    bool initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                    std::string& error);
    void resize(int pixelWidth, int pixelHeight);
    bool draw(const RenderState& state, std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace deathstarlab

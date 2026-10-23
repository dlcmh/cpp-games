#pragma once

#include <memory>
#include <string>

#include "Planets/PlanetSpec.hpp"

namespace planetlab {

struct NativeSurface {
    void* metalLayer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

class PlanetLabRenderer {
public:
    PlanetLabRenderer();
    ~PlanetLabRenderer();
    PlanetLabRenderer(const PlanetLabRenderer&) = delete;
    PlanetLabRenderer& operator=(const PlanetLabRenderer&) = delete;

    bool initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                    std::string& error);
    void resize(int pixelWidth, int pixelHeight);
    bool draw(const cppgames::planets::PlanetSpec& spec, float time, std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace planetlab

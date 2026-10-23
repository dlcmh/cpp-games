#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Cockpit/CockpitState.hpp"
#include "Lasers/LaserTypes.hpp"

namespace laserlab {

struct NativeSurface {
    void* metalLayer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

// Standalone renderer for the laser lab. It has no game scene: a procedural
// starfield, the shared laser renderer, and the shared cockpit overlay.
class LaserLabRenderer {
public:
    LaserLabRenderer();
    ~LaserLabRenderer();
    LaserLabRenderer(const LaserLabRenderer&) = delete;
    LaserLabRenderer& operator=(const LaserLabRenderer&) = delete;

    bool initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                    std::string& error);
    void resize(int pixelWidth, int pixelHeight);
    bool draw(const cppgames::cockpit::CockpitState& cockpitState,
              const std::vector<cppgames::lasers::LaserBeam>& beams, std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace laserlab

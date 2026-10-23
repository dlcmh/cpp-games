#pragma once

#include <memory>
#include <string>

#include "Cockpit/CockpitState.hpp"

namespace cockpitlab {

struct NativeSurface {
    void* metalLayer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

// Small standalone renderer for the cockpit lab: a procedural starfield
// backdrop plus the shared cockpit overlay. It deliberately has no game scene,
// so the cockpit can be evaluated and iterated on in isolation.
class CockpitLabRenderer {
public:
    CockpitLabRenderer();
    ~CockpitLabRenderer();
    CockpitLabRenderer(const CockpitLabRenderer&) = delete;
    CockpitLabRenderer& operator=(const CockpitLabRenderer&) = delete;

    bool initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                    std::string& error);
    void resize(int pixelWidth, int pixelHeight);
    bool draw(const cppgames::cockpit::CockpitState& cockpitState, std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cockpitlab

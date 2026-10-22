#pragma once

#include <memory>
#include <string>

#include "Render/PipelineState.hpp"

namespace barebones {

// The engine seam. Platform renderers implement this interface; game code
// only ever sees RenderFrame.
struct NativeSurface {
    void* metalLayer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(const NativeSurface& surface,
                            const std::string& shaderPath,
                            std::string& error) = 0;

    virtual void resize(int pixelWidth, int pixelHeight) = 0;

    virtual bool draw(const RenderFrame& frame, std::string& error) = 0;
};

}  // namespace barebones

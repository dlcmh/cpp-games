#pragma once

#include <string>

#include "Render/RenderFrame.hpp"

namespace spacebattle {

struct NativeSurface {
    void* metalLayer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                            std::string& error) = 0;
    virtual void resize(int pixelWidth, int pixelHeight) = 0;
    virtual bool draw(const RenderFrame& frame, std::string& error) = 0;
};

}  // namespace spacebattle

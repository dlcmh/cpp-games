#pragma once

#include "Core/Math.hpp"
#include "Render/TrackMesh.hpp"

#include <memory>
#include <string>

namespace rollercoaster {

struct NativeSurface {
    void* metalLayer{nullptr};
    int pixelWidth{0};
    int pixelHeight{0};
};

struct Camera {
    Vec3 eye;
    Vec3 target;
    Vec3 up;
    float fovY{1.05f};
};

class RollerCoasterRenderer {
public:
    RollerCoasterRenderer();
    ~RollerCoasterRenderer();
    RollerCoasterRenderer(const RollerCoasterRenderer&) = delete;
    RollerCoasterRenderer& operator=(const RollerCoasterRenderer&) = delete;

    bool initialize(const NativeSurface& surface, const std::string& shaderDirectory,
                    std::string& error);
    void resize(int pixelWidth, int pixelHeight);
    bool draw(const TrackMesh& mesh, const Camera& camera, std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rollercoaster

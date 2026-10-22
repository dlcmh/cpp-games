#pragma once

#include <memory>

#include "Render/IRenderer.hpp"

namespace barebones {

// Defined only by the macOS platform layer. Metal headers never leak past this
// translation unit.
std::unique_ptr<IRenderer> createMetalRenderer();

}  // namespace barebones

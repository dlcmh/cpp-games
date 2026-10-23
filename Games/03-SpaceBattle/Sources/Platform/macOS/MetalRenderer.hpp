#pragma once

#include <memory>

#include "Render/IRenderer.hpp"

namespace spacebattle {

std::unique_ptr<IRenderer> createMetalRenderer();

}  // namespace spacebattle

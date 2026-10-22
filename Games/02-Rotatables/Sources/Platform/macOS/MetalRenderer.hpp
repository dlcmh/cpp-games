#pragma once

#include <memory>

#include "Render/IRenderer.hpp"

namespace rotatables {

std::unique_ptr<IRenderer> createMetalRenderer();

}  // namespace rotatables

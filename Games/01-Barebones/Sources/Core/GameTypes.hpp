#pragma once

namespace barebones {

// The complete, renderer-independent state of the game. Core never includes a
// graphics, windowing, or SDL header: it is plain C++ data.
struct GameState {
    float angleRadians{0.0f};
    float elapsedSeconds{0.0f};
    float rotationSpeed{1.8f};
    bool paused{false};
};

}  // namespace barebones

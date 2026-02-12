#pragma once

namespace engine::gameplay {

/// Player movement controller component.
///
/// Provides configurable player physics for platformer-style movement.
/// Works with PlayerSystem which applies forces to a physics body.
struct PlayerController {
    // Movement parameters
    float move_accel = 1500.0f;      // Horizontal acceleration when moving
    float max_move_speed = 100.0f;   // Maximum horizontal velocity
    float jump_velocity = -180.0f;   // Initial jump velocity (negative = up in pixel coords)
    float friction = 10.0f;          // Ground friction multiplier

    // Runtime state (set by PlayerInputSystem, read by PlayerSystem)
    int move_dir = 0;                // -1 = left, 0 = none, 1 = right
    bool jump_pressed = false;       // True when jump button is pressed
};

} // namespace engine::gameplay

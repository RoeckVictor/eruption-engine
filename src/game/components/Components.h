#pragma once

#include "game/world/Materials.h"
#include <cstdint>

namespace game {

/// Active tool mode for the ToolSystem.
enum class ToolMode {
    Material,    // Spawn/erase materials with brush (default)
    PasteSprite, // Paste a .pxg sprite as raw pixels
    SpawnBody    // Spawn a .pxg sprite as a physics rigidbody
};

// Singleton context component: persistent game-input state written by
// GameInputSystem and read by ToolSystem, PixelSimulationSystem.
// One-shot actions (quit, respawn) are published as events instead.
struct GameInputState {
    ToolMode tool_mode = ToolMode::Material;
    int selected_material = MAT_SAND;
    int brush_radius = 5;
    bool sim_paused = false;
    bool debug_draw = false;
};

struct Transform {
    float x = 0.0f;
    float y = 0.0f;
};

struct PlayerController {
    float move_accel = 1500.0f;
    float max_move_speed = 100.0f;
    float jump_velocity = -180.0f;
    float friction = 10.0f;
    int move_dir = 0;
    bool jump_pressed = false;
};

struct CameraTarget {
    float offset_y_fraction = 0.33f;
};

struct Renderable {
    float r = 1.0f;
    float g = 0.95f;
    float b = 0.3f;
    float a = 1.0f;
};

struct PlayerTag {};

} // namespace game

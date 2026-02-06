#pragma once

#include <cstdint>

namespace game {

// ============================================================================
// Input/UI Events
// ============================================================================

/// Fired when the player presses Escape.
struct QuitRequestedEvent {};

/// Fired when the player presses R to respawn.
struct RespawnRequestedEvent {};

/// Fired when the active tool mode changes.
struct ToolChangedEvent {
    int old_mode;  // ToolMode enum value
    int new_mode;
};

/// Fired when a different material is selected.
struct MaterialSelectedEvent {
    int old_material;
    int new_material;
};

/// Fired when brush size changes.
struct BrushSizeChangedEvent {
    int old_size;
    int new_size;
};

// ============================================================================
// Player Events
// ============================================================================

/// Fired when the player successfully jumps.
struct PlayerJumpedEvent {
    bool in_liquid;  // True if jumped while in liquid
};

/// Fired when the player lands on solid ground.
struct PlayerGroundedEvent {
    float fall_velocity;  // Velocity at moment of landing (for fall damage)
};

/// Fired when the player dies.
struct PlayerDiedEvent {
    enum Cause { Unknown, FallDamage, Lava, Drowned };
    Cause cause = Unknown;
};

/// Fired when the player is spawned or respawned.
struct PlayerSpawnedEvent {
    float x, y;
    bool is_respawn;  // False for initial spawn, true for respawn
};

// ============================================================================
// World Events
// ============================================================================

/// Fired when material is placed in the world.
struct MaterialPlacedEvent {
    int material_id;
    int x, y;
    int radius;
    int pixels_placed;  // Actual number of pixels modified
};

/// Fired when material is erased from the world.
struct MaterialErasedEvent {
    int x, y;
    int radius;
    int pixels_erased;
};

/// Fired when a physics rigidbody is spawned.
struct BodySpawnedEvent {
    float x, y;
    bool is_dynamic;
};

/// Fired when a rigidbody is destroyed (by damage or explicit removal).
struct BodyDestroyedEvent {
    float x, y;
    bool was_player;
};

// ============================================================================
// Simulation Events
// ============================================================================

/// Fired when simulation pause state changes.
struct SimulationPausedEvent {
    bool paused;
};

/// Fired when debug visualization mode changes.
struct DebugModeChangedEvent {
    bool enabled;
};

// ============================================================================
// Camera Events
// ============================================================================

/// Fired when camera zoom changes.
struct CameraZoomChangedEvent {
    float old_zoom;
    float new_zoom;
};

} // namespace game

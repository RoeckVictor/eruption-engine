#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MargolusSimulation.h"
#include "engine/physics/TerrainColliderManager.h"
#include "engine/graphics/RenderContext.h"
#include "engine/graphics/Texture.h"
#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/graphics/Shader.h"

namespace engine {
class Engine;

namespace physics {
class PhysicsWorld;
struct Rigidbody;
}
}

namespace editor {

struct SceneSettings;

/// State for a single active pixel simulation surface during play mode.
struct SimSurfaceState {
    engine::simulation::PixelGrid pixel_grid;
    engine::simulation::MargolusSimulation simulation;
    engine::graphics::Texture color_texture;              // RGBA8 for ImGui display
    engine::graphics::ShaderStorageBuffer palette_ssbo;   // Color palette for material→color
    std::unique_ptr<engine::physics::TerrainColliderManager> terrain_colliders; // Optional terrain physics
    entt::entity entity = entt::null;                     // editor entity ID
    int width = 0;
    int height = 0;
};

/// Editor/Runtime state machine.
enum class PlayState {
    Editing,    // Normal editor mode
    Playing,    // Game is running
    Paused      // Game is paused (can step frames)
};

/// Manages the runtime context for play mode.
/// Snapshots the scene on play, runs physics/simulation on the editor registry
/// directly, and restores the snapshot when play stops.
class RuntimeContext {
public:
    RuntimeContext();
    ~RuntimeContext();

    /// Initialize with the editor's scene registry.
    void init(entt::registry* editor_registry);

    /// Get the current play state.
    PlayState state() const { return m_state; }

    /// Check if we're in any play mode (Playing or Paused).
    bool is_playing() const { return m_state != PlayState::Editing; }

    /// Check if currently paused.
    bool is_paused() const { return m_state == PlayState::Paused; }

    /// Enter play mode - snapshots scene and starts runtime.
    /// Uses scene settings for physics gravity and scale.
    void play(const SceneSettings& settings);

    /// Pause the runtime.
    void pause();

    /// Resume from pause.
    void resume();

    /// Stop play mode - restores original scene state.
    void stop();

    /// Step one frame while paused.
    void step_frame();

    /// Update the runtime (called each frame when playing).
    void update(float dt);

    /// Get the live simulation color texture GL handle for an entity (during play mode).
    /// Returns 0 if entity has no active simulation.
    uint32_t get_sim_texture(entt::entity entity) const;

    /// Get time spent in current play session.
    float play_time() const { return m_play_time; }

    /// Get the frame count since play started.
    uint64_t frame_count() const { return m_frame_count; }

    /// Get the physics world (for debug queries like velocity).
    engine::physics::PhysicsWorld* physics_world() { return m_physics_world.get(); }
    const engine::physics::PhysicsWorld* physics_world() const { return m_physics_world.get(); }

    /// Access active simulation surfaces (for debug visualization).
    const std::vector<std::unique_ptr<SimSurfaceState>>& sim_surfaces() const { return m_sim_surfaces; }

private:
    void snapshot_scene();
    void restore_scene();

    void init_physics_bodies();
    void create_body_for_entity(entt::entity entity);
    void attach_collider_shapes(entt::entity entity, engine::physics::Rigidbody& rb);
    void sync_physics_to_transforms();

    void init_pixel_simulations();
    void shutdown_pixel_simulations();
    void update_pixel_simulations(float dt);

    entt::registry* m_editor_registry = nullptr;

    // Scene snapshot stored as serialized data
    std::string m_scene_snapshot;

    // Engine systems for runtime
    std::unique_ptr<engine::physics::PhysicsWorld> m_physics_world;

    // Pixel simulation state
    std::vector<std::unique_ptr<SimSurfaceState>> m_sim_surfaces;
    engine::graphics::RenderContext m_render_context;
    engine::graphics::Shader m_color_shader;  // Shared ssbo_to_color compute shader

    PlayState m_state = PlayState::Editing;
    float m_play_time = 0.0f;
    uint64_t m_frame_count = 0;
    bool m_step_requested = false;
};

} // namespace editor

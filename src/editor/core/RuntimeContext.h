#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MargolusSimulation.h"
#include "engine/physics/TerrainColliderManager.h"
#include "engine/graphics/RenderContext.h"
#include "engine/graphics/Texture.h"
#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/graphics/Shader.h"
#include "runtime/ComponentScript.h"

namespace engine {
class Engine;

namespace physics {
class PhysicsWorld;
struct Rigidbody;
}

namespace prefab {
class ComponentRegistry;
class PrefabManager;
}
}

namespace editor {

struct SceneSettings;
class ScriptManager;

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

    /// Initialize with the editor's scene registry and script manager.
    void init(entt::registry* editor_registry, ScriptManager* script_manager = nullptr);

    /// Set the engine pointer (called from EditorApplication::on_init).
    void set_engine(engine::Engine* engine) { m_engine = engine; }

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

    /// Queue an entity for deferred destruction (called by script host API).
    void queue_destroy(entt::entity entity) { m_deferred_destroys.push_back(entity); }

    /// Set the project assets path (for loading prefabs at runtime).
    void set_project_assets_path(const std::string& path) { m_project_assets_path = path; }

    /// Instantiate a prefab by name (called by host API callback).
    entt::entity instantiate_prefab_internal(const char* prefab_name);

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

    void init_scripts();
    void shutdown_scripts();
    void fixed_update_scripts();
    void update_scripts();
    void late_update_scripts();
    void check_enable_disable_scripts();

    entt::registry* m_editor_registry = nullptr;
    ScriptManager* m_script_manager = nullptr;
    engine::Engine* m_engine = nullptr;

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

    // Fixed timestep for on_fixed_update / physics
    float m_fixed_timestep = 1.0f / 60.0f;
    float m_fixed_time_accumulator = 0.0f;

    // Track entity enabled state for on_enable/on_disable callbacks
    std::unordered_set<entt::entity> m_previously_enabled_script_entities;

    // Script host API (shared instance for all scripts)
    runtime::ScriptHostAPI m_host_api;

    // Deferred entity destruction (queued by scripts, flushed at end of frame)
    std::vector<entt::entity> m_deferred_destroys;

    // Prefab system for script instantiation
    std::unique_ptr<engine::prefab::ComponentRegistry> m_component_registry;
    std::unique_ptr<engine::prefab::PrefabManager> m_prefab_manager;
    std::string m_project_assets_path;
};

} // namespace editor

#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

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
struct SimSurfaceState;
class ScriptManager;
class PhysicsPlayback;
class SimulationPlayback;

enum class PlayState {
    Editing,
    Playing,
    Paused
};

class RuntimeContext {
public:
    RuntimeContext();
    ~RuntimeContext();

    void init(entt::registry* editor_registry, ScriptManager* script_manager = nullptr);

    void set_engine(engine::Engine* engine) { m_engine = engine; m_host_api.engine_ctx = engine; }

    PlayState state() const { return m_state; }
    bool is_playing() const { return m_state != PlayState::Editing; }
    bool is_paused() const { return m_state == PlayState::Paused; }

    void play(const SceneSettings& settings);
    void pause();
    void resume();
    void stop();
    void step_frame();
    void update(float dt);

    uint32_t get_sim_texture(entt::entity entity) const;

    float play_time() const { return m_play_time; }
    uint64_t frame_count() const { return m_frame_count; }

    engine::physics::PhysicsWorld* physics_world() { return m_physics_world.get(); }
    const engine::physics::PhysicsWorld* physics_world() const { return m_physics_world.get(); }

    const std::vector<std::unique_ptr<SimSurfaceState>>& sim_surfaces() const;

    void queue_destroy(entt::entity entity) { m_deferred_destroys.push_back(entity); }

    void set_project_assets_path(const std::string& path) { m_project_assets_path = path; }

    entt::entity instantiate_prefab_internal(const char* prefab_name);

private:
    void snapshot_scene();
    void restore_scene();

    std::unique_ptr<PhysicsPlayback> m_physics_playback;
    std::unique_ptr<SimulationPlayback> m_sim_playback;

    void init_scripts();
    void shutdown_scripts();
    void fixed_update_scripts();
    void update_scripts();
    void late_update_scripts();
    void check_enable_disable_scripts();

    entt::registry* m_editor_registry = nullptr;
    ScriptManager* m_script_manager = nullptr;
    engine::Engine* m_engine = nullptr;

    std::string m_scene_snapshot;

    std::unique_ptr<engine::physics::PhysicsWorld> m_physics_world;

    PlayState m_state = PlayState::Editing;
    float m_play_time = 0.0f;
    uint64_t m_frame_count = 0;
    bool m_step_requested = false;

    float m_fixed_timestep = 1.0f / 60.0f;
    float m_fixed_time_accumulator = 0.0f;

    std::unordered_set<entt::entity> m_previously_enabled_script_entities;

    runtime::ScriptHostAPI m_host_api;

    std::vector<entt::entity> m_deferred_destroys;

    std::unique_ptr<engine::prefab::ComponentRegistry> m_component_registry;
    std::unique_ptr<engine::prefab::PrefabManager> m_prefab_manager;
    std::string m_project_assets_path;
};

}

#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <random>
#include <chrono>

#include "runtime/ComponentScript.h"
#include "runtime/ScriptEvents.h"
#include "runtime/ScriptCoroutine.h"
#include "runtime/ScriptEventDispatcher.h"

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
    // Friend declarations for host functions that need private member access
    friend runtime::CoroutineHandle host_start_coroutine(runtime::ScriptHostAPI*, entt::entity, void*);
    friend void host_stop_coroutine(runtime::ScriptHostAPI*, runtime::CoroutineHandle);
    friend void host_stop_all_coroutines(runtime::ScriptHostAPI*, entt::entity);
    friend bool host_is_coroutine_running(runtime::ScriptHostAPI*, runtime::CoroutineHandle);
    friend float host_random_float(runtime::ScriptHostAPI*);
    friend float host_random_range(runtime::ScriptHostAPI*, float, float);
    friend int host_random_int(runtime::ScriptHostAPI*, int, int);

public:
    RuntimeContext();
    ~RuntimeContext();

    void init(entt::registry* editor_registry, ScriptManager* script_manager = nullptr);

    void set_engine(engine::Engine* engine) { m_engine = engine; m_host_api.engine_ctx = engine; }
    engine::Engine* engine() { return m_engine; }
    const engine::Engine* engine() const { return m_engine; }

    PlayState state() const { return m_state; }
    bool is_playing() const { return m_state != PlayState::Editing; }
    bool is_paused() const { return m_state == PlayState::Paused; }

    void play(const SceneSettings& settings);
    void pause();
    void resume();
    void stop();
    void step_frame();
    void update(float dt);

    void* get_sim_texture(entt::entity entity) const;

    float play_time() const { return m_play_time; }
    uint64_t frame_count() const { return m_frame_count; }

    engine::physics::PhysicsWorld* physics_world() { return m_physics_world.get(); }
    const engine::physics::PhysicsWorld* physics_world() const { return m_physics_world.get(); }

    entt::registry* editor_registry() { return m_editor_registry; }
    const entt::registry* editor_registry() const { return m_editor_registry; }

    const std::vector<std::unique_ptr<SimSurfaceState>>& sim_surfaces() const;

    SimulationPlayback* sim_playback() { return m_sim_playback.get(); }
    const SimulationPlayback* sim_playback() const { return m_sim_playback.get(); }

    runtime::ScriptEventDispatcher& event_dispatcher() { return m_event_dispatcher; }

    void queue_destroy(entt::entity entity) { m_deferred_destroys.push_back(entity); }

    void set_project_assets_path(const std::string& path) { m_project_assets_path = path; }

    void set_viewport(float x, float y, float width, float height) {
        m_viewport_x = x;
        m_viewport_y = y;
        m_viewport_w = width;
        m_viewport_h = height;
    }

    float viewport_x() const { return m_viewport_x; }
    float viewport_y() const { return m_viewport_y; }
    float viewport_width() const { return m_viewport_w; }
    float viewport_height() const { return m_viewport_h; }

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

    // Viewport info for correct screen-to-world conversion in editor panels
    float m_viewport_x = 0.0f;
    float m_viewport_y = 0.0f;
    float m_viewport_w = 0.0f;
    float m_viewport_h = 0.0f;

    // Maps body -> entity for looking up entities from Box2D bodies
    std::unordered_map<uint64_t, entt::entity> m_body_to_entity;
    bool m_body_map_dirty = true;

    // Extended contact info stored per pair (keeps best data from multiple events)
    struct ContactData {
        runtime::ContactPair pair;
        runtime::CollisionInfo info_for_a;
        runtime::CollisionInfo info_for_b;
    };

    // Current frame contacts (pair -> extended info)
    std::unordered_map<runtime::ContactPair, ContactData, runtime::ContactPairHash> m_current_contacts;
    std::unordered_set<runtime::ContactPair, runtime::ContactPairHash> m_previous_contact_pairs;

    void process_collision_events();
    void build_body_to_entity_map();
    void invalidate_body_map() { m_body_map_dirty = true; }

    runtime::ScriptEventDispatcher m_event_dispatcher;

    std::vector<runtime::CoroutineInstance> m_coroutines;
    runtime::CoroutineHandle m_next_coroutine_handle = 1;

    void update_coroutines(float dt);
    void cleanup_entity_coroutines(entt::entity entity);

    /// Helper to safely destroy a coroutine handle and mark it inactive
    static void destroy_coroutine(runtime::CoroutineInstance& coro) {
        coro.active = false;
        if (coro.coro_handle) {
            coro.coro_handle.destroy();
            coro.coro_handle = nullptr;
        }
    }

    // Use random_device with time-based fallback (random_device may be deterministic on MinGW)
    static uint32_t generate_seed() {
        std::random_device rd;
        uint32_t seed = rd();
        static uint32_t last_seed = 0;
        if (seed == last_seed) {
            seed ^= static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        }
        last_seed = seed;
        return seed;
    }
    std::mt19937 m_random_engine{generate_seed()};
};

}

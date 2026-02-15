#include "RuntimeContext.h"
#include "PhysicsPlayback.h"
#include "SimulationPlayback.h"
#include "EditorComponents.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"
#include "engine/core/Engine.h"
#include "engine/platform/Input.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/animation/AnimationSystem.h"
#include "editor/scripting/ScriptManager.h"
#include "runtime/ScriptComponent.h"
#include "engine/prefab/PrefabManager.h"
#include "engine/prefab/ComponentRegistry.h"
#include "engine/EngineComponentRegistry.h"
#include <cmath>
#include <fstream>

namespace editor {

static bool host_is_key_held(runtime::ScriptHostAPI* api, int key) {
    auto* eng = static_cast<engine::Engine*>(api->engine_ctx);
    if (!eng) return false;
    return eng->input().is_held(static_cast<engine::platform::KeyCode>(key));
}

static bool host_is_key_pressed(runtime::ScriptHostAPI* api, int key) {
    auto* eng = static_cast<engine::Engine*>(api->engine_ctx);
    if (!eng) return false;
    return eng->input().is_pressed(static_cast<engine::platform::KeyCode>(key));
}

static bool host_is_key_released(runtime::ScriptHostAPI* api, int key) {
    auto* eng = static_cast<engine::Engine*>(api->engine_ctx);
    if (!eng) return false;
    return eng->input().is_released(static_cast<engine::platform::KeyCode>(key));
}

static bool host_is_mouse_held(runtime::ScriptHostAPI* api, int button) {
    auto* eng = static_cast<engine::Engine*>(api->engine_ctx);
    if (!eng) return false;
    return eng->input().is_mouse_held(static_cast<engine::platform::MouseButton>(button));
}

static bool host_is_mouse_pressed(runtime::ScriptHostAPI* api, int button) {
    auto* eng = static_cast<engine::Engine*>(api->engine_ctx);
    if (!eng) return false;
    return eng->input().is_mouse_pressed(static_cast<engine::platform::MouseButton>(button));
}

static double host_get_mouse_x(runtime::ScriptHostAPI* api) {
    auto* eng = static_cast<engine::Engine*>(api->engine_ctx);
    return eng ? eng->input().mouse_x() : 0.0;
}

static double host_get_mouse_y(runtime::ScriptHostAPI* api) {
    auto* eng = static_cast<engine::Engine*>(api->engine_ctx);
    return eng ? eng->input().mouse_y() : 0.0;
}

static void host_log_info(runtime::ScriptHostAPI* /*api*/, const char* msg) {
    engine::Logger::instance().info("Script", "%s", msg);
}

static void host_log_warning(runtime::ScriptHostAPI* /*api*/, const char* msg) {
    engine::Logger::instance().warning("Script", "%s", msg);
}

static void host_log_error(runtime::ScriptHostAPI* /*api*/, const char* msg) {
    engine::Logger::instance().error("Script", "%s", msg);
}

static void host_get_velocity(runtime::ScriptHostAPI* api, entt::registry* reg, entt::entity entity, float* vx, float* vy) {
    *vx = 0.0f; *vy = 0.0f;
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!reg || !reg->valid(entity) || !rt || !rt->physics_world()) return;
    if (!reg->all_of<engine::physics::Rigidbody>(entity)) return;
    auto& rb = reg->get<engine::physics::Rigidbody>(entity);
    if (!b2Body_IsValid(rb.body_id)) return;
    b2Vec2 vel = rt->physics_world()->get_body_linear_velocity(rb.body_id);
    *vx = vel.x; *vy = vel.y;
}

static void host_set_velocity(runtime::ScriptHostAPI* api, entt::registry* reg, entt::entity entity, float vx, float vy) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!reg || !reg->valid(entity) || !rt || !rt->physics_world()) return;
    if (!reg->all_of<engine::physics::Rigidbody>(entity)) return;
    auto& rb = reg->get<engine::physics::Rigidbody>(entity);
    if (!b2Body_IsValid(rb.body_id)) return;
    rt->physics_world()->set_body_linear_velocity(rb.body_id, vx, vy);
}

static void host_add_force(runtime::ScriptHostAPI* api, entt::registry* reg, entt::entity entity, float fx, float fy) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!reg || !reg->valid(entity) || !rt || !rt->physics_world()) return;
    if (!reg->all_of<engine::physics::Rigidbody>(entity)) return;
    auto& rb = reg->get<engine::physics::Rigidbody>(entity);
    if (!b2Body_IsValid(rb.body_id)) return;
    rt->physics_world()->apply_force(rb.body_id, fx, fy);
}

static void host_add_impulse(runtime::ScriptHostAPI* api, entt::registry* reg, entt::entity entity, float ix, float iy) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!reg || !reg->valid(entity) || !rt || !rt->physics_world()) return;
    if (!reg->all_of<engine::physics::Rigidbody>(entity)) return;
    auto& rb = reg->get<engine::physics::Rigidbody>(entity);
    if (!b2Body_IsValid(rb.body_id)) return;
    rt->physics_world()->apply_impulse(rb.body_id, ix, iy);
}

static entt::entity host_find_entity_by_name(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, const char* name) {
    if (!reg || !name) return entt::null;
    auto view = reg->view<EntityInfo>();
    for (auto entity : view) {
        if (view.get<EntityInfo>(entity).name == name) return entity;
    }
    return entt::null;
}

static void host_destroy_entity(runtime::ScriptHostAPI* api, entt::registry* reg, entt::entity entity) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!reg || !reg->valid(entity) || !rt) return;
    rt->queue_destroy(entity);
}

static void* host_get_component(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity, entt::id_type type_hash) {
    if (!reg || !reg->valid(entity)) return nullptr;
    auto* storage = reg->storage(type_hash);
    if (!storage || !storage->contains(entity)) return nullptr;
    return storage->value(entity);
}

static void* host_add_component(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity, entt::id_type type_hash) {
    if (!reg || !reg->valid(entity)) return nullptr;
    auto* storage = reg->storage(type_hash);
    if (!storage) return nullptr;
    if (storage->contains(entity)) return storage->value(entity);
    storage->push(entity);
    return storage->value(entity);
}

static void host_remove_component(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity, entt::id_type type_hash) {
    if (!reg || !reg->valid(entity)) return;
    auto* storage = reg->storage(type_hash);
    if (!storage || !storage->contains(entity)) return;
    storage->remove(entity);
}

static entt::entity host_instantiate_prefab(runtime::ScriptHostAPI* api, entt::registry* /*reg*/, const char* prefab_name) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!prefab_name || !rt) return entt::null;
    return rt->instantiate_prefab_internal(prefab_name);
}

RuntimeContext::RuntimeContext() = default;

RuntimeContext::~RuntimeContext() {
    if (m_state != PlayState::Editing) {
        stop();
    }
}

void RuntimeContext::init(entt::registry* editor_registry, ScriptManager* script_manager) {
    m_editor_registry = editor_registry;
    m_script_manager = script_manager;

    m_host_api.runtime_ctx = this;

    m_host_api.get_component = &host_get_component;
    m_host_api.is_key_held = &host_is_key_held;
    m_host_api.is_key_pressed = &host_is_key_pressed;
    m_host_api.is_key_released = &host_is_key_released;
    m_host_api.is_mouse_held = &host_is_mouse_held;
    m_host_api.is_mouse_pressed = &host_is_mouse_pressed;
    m_host_api.get_mouse_x = &host_get_mouse_x;
    m_host_api.get_mouse_y = &host_get_mouse_y;
    m_host_api.log_info = &host_log_info;
    m_host_api.log_warning = &host_log_warning;
    m_host_api.log_error = &host_log_error;
    m_host_api.get_velocity = &host_get_velocity;
    m_host_api.set_velocity = &host_set_velocity;
    m_host_api.add_force = &host_add_force;
    m_host_api.add_impulse = &host_add_impulse;
    m_host_api.find_entity_by_name = &host_find_entity_by_name;
    m_host_api.destroy_entity = &host_destroy_entity;
    m_host_api.add_component = &host_add_component;
    m_host_api.remove_component = &host_remove_component;
    m_host_api.instantiate_prefab = &host_instantiate_prefab;
    m_host_api.fixed_delta_time = m_fixed_timestep;
}

void RuntimeContext::play(const SceneSettings& settings) {
    if (m_state != PlayState::Editing) {
        return;
    }

    if (!m_editor_registry) {
        engine::Logger::instance().error("Runtime", "Cannot enter play mode: no registry set");
        return;
    }

    snapshot_scene();

    try {
        // Initialize physics world from scene settings.
        // SceneSettings gravity is in pixels/s² with positive Y = down convention.
        // Editor uses Y-up, so negate Y. PhysicsWorld expects m/s², so divide by ppm.
        float ppm = settings.pixels_per_meter;
        float gravity_x_m = settings.gravity_x / ppm;
        float gravity_y_m = -settings.gravity_y / ppm;
        m_physics_world = std::make_unique<engine::physics::PhysicsWorld>();
        auto result = m_physics_world->init(gravity_x_m, gravity_y_m, ppm);
        if (!result) {
            engine::Logger::instance().error("Runtime", "Failed to init physics world: %s",
                result.error().message.c_str());
            m_physics_world.reset();
            restore_scene();
            return;
        }

        m_physics_playback = std::make_unique<PhysicsPlayback>(*m_editor_registry, *m_physics_world);
        m_physics_playback->init_bodies();

        m_sim_playback = std::make_unique<SimulationPlayback>(*m_editor_registry);
        m_sim_playback->init(m_physics_world.get());

        m_component_registry = std::make_unique<engine::prefab::ComponentRegistry>();
        engine::register_engine_components(*m_component_registry);
        m_prefab_manager = std::make_unique<engine::prefab::PrefabManager>();
        m_prefab_manager->set_registry(*m_component_registry);

        // Instantiate component scripts from DLL
        init_scripts();
    } catch (const std::exception& e) {
        engine::Logger::instance().error("Runtime", "Exception during play init: %s", e.what());
        shutdown_scripts();
        m_previously_enabled_script_entities.clear();
        m_sim_playback.reset();
        m_physics_playback.reset();
        m_physics_world.reset();
        m_prefab_manager.reset();
        m_component_registry.reset();
        restore_scene();
        return;
    }

    m_state = PlayState::Playing;
    m_play_time = 0.0f;
    m_frame_count = 0;
    m_fixed_time_accumulator = 0.0f;

    engine::Logger::instance().info("Runtime", "Entered play mode with physics system");
}

void RuntimeContext::pause() {
    if (m_state != PlayState::Playing) {
        return;
    }

    m_state = PlayState::Paused;
    engine::Logger::instance().info("Runtime", "Paused (frame %llu, time %.2fs)", m_frame_count, m_play_time);
}

void RuntimeContext::resume() {
    if (m_state != PlayState::Paused) {
        return;
    }

    m_state = PlayState::Playing;
    engine::Logger::instance().info("Runtime", "Resumed");
}

void RuntimeContext::stop() {
    if (m_state == PlayState::Editing) {
        return;
    }

    shutdown_scripts();
    m_previously_enabled_script_entities.clear();
    m_deferred_destroys.clear();
    m_sim_playback.reset();
    m_physics_playback.reset();
    m_physics_world.reset();
    m_prefab_manager.reset();
    m_component_registry.reset();

    restore_scene();

    m_state = PlayState::Editing;
    engine::Logger::instance().info("Runtime", "Stopped play mode (ran for %.2fs, %llu frames)", m_play_time, m_frame_count);
}

void RuntimeContext::step_frame() {
    if (m_state != PlayState::Paused) {
        return;
    }

    m_step_requested = true;
}

void RuntimeContext::update(float dt) {
    if (m_state == PlayState::Editing) {
        return;
    }

    if (m_state == PlayState::Paused && !m_step_requested) {
        return;
    }

    m_step_requested = false;

    m_play_time += dt;
    m_frame_count++;

    if (!m_editor_registry) return;

    m_host_api.delta_time = dt;
    m_host_api.time = m_play_time;
    m_host_api.frame_count = m_frame_count;
    m_host_api.fixed_delta_time = m_fixed_timestep;

    m_fixed_time_accumulator += dt;

    // Cap accumulator to prevent spiral of death when frame rate drops
    static constexpr float MAX_ACCUMULATION = 0.25f;
    if (m_fixed_time_accumulator > MAX_ACCUMULATION) {
        m_fixed_time_accumulator = MAX_ACCUMULATION;
    }

    while (m_fixed_time_accumulator >= m_fixed_timestep) {
        m_fixed_time_accumulator -= m_fixed_timestep;

        fixed_update_scripts();

        if (m_physics_world) {
            m_physics_world->step(m_fixed_timestep, 4);
        }
    }

    if (m_physics_playback) {
        m_physics_playback->sync_to_transforms();
    }

    update_world_transforms(*m_editor_registry);

    check_enable_disable_scripts();
    update_scripts();
    late_update_scripts();

    if (m_sim_playback) {
        m_sim_playback->update(m_frame_count);
    }

    for (auto entity : m_deferred_destroys) {
        if (m_editor_registry->valid(entity)) {
            destroy_entity_recursive(*m_editor_registry, entity);
        }
    }
    m_deferred_destroys.clear();
}

void RuntimeContext::snapshot_scene() {
    if (!m_editor_registry) {
        return;
    }

    SceneSerializer serializer(*m_editor_registry);
    m_scene_snapshot = serializer.save_to_string();

    engine::Logger::instance().info("Runtime", "Scene snapshot created (%zu bytes)", m_scene_snapshot.size());
}

void RuntimeContext::restore_scene() {
    if (!m_editor_registry || m_scene_snapshot.empty()) {
        return;
    }

    m_editor_registry->clear();

    SceneSerializer serializer(*m_editor_registry);
    if (serializer.load_from_string(m_scene_snapshot)) {
        engine::Logger::instance().info("Runtime", "Scene restored from snapshot");
    } else {
        engine::Logger::instance().error("Runtime", "Failed to restore scene from snapshot");
    }

    m_scene_snapshot.clear();
}

uint32_t RuntimeContext::get_sim_texture(entt::entity entity) const {
    return m_sim_playback ? m_sim_playback->get_sim_texture(entity) : 0;
}

const std::vector<std::unique_ptr<SimSurfaceState>>& RuntimeContext::sim_surfaces() const {
    static const std::vector<std::unique_ptr<SimSurfaceState>> empty;
    return m_sim_playback ? m_sim_playback->surfaces() : empty;
}

void RuntimeContext::init_scripts() {
    if (!m_script_manager || !m_script_manager->are_scripts_loaded()) return;
    if (!m_editor_registry) return;

    auto& dll = m_script_manager->dll_manager();

    auto view = m_editor_registry->view<runtime::ScriptComponent>();
    int script_count = 0;
    for (auto entity : view) {
        auto& sc = view.get<runtime::ScriptComponent>(entity);

        for (const auto& type_name : sc.script_types) {
            auto* script = dll.create_script(type_name);
            if (script) {
                script->init_context(entity, m_editor_registry, m_engine, &m_host_api);
                sc.scripts.push_back(std::unique_ptr<runtime::ComponentScript>(script));
                script_count++;
            } else {
                engine::Logger::instance().warning("Runtime",
                    "Script type not found in DLL: %s", type_name.c_str());
            }
        }

        for (auto& script : sc.scripts) {
            if (script) script->on_create();
        }

        bool enabled = true;
        if (m_editor_registry->all_of<EntityInfo>(entity)) {
            enabled = m_editor_registry->get<EntityInfo>(entity).enabled_in_hierarchy;
        }
        if (enabled) {
            m_previously_enabled_script_entities.insert(entity);
        }
    }

    engine::Logger::instance().info("Runtime", "Scripts initialized (%d instances)", script_count);
}

void RuntimeContext::shutdown_scripts() {
    if (!m_editor_registry) return;

    auto view = m_editor_registry->view<runtime::ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<runtime::ScriptComponent>(entity);
        for (auto& script : sc.scripts) {
            if (script) script->on_destroy();
        }
        sc.scripts.clear();
    }
}

template<typename Fn>
static void for_each_enabled_script(entt::registry& registry, Fn&& fn) {
    auto view = registry.view<runtime::ScriptComponent>();
    for (auto entity : view) {
        if (registry.all_of<EntityInfo>(entity)) {
            if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) {
                continue;
            }
        }
        for (auto& script : view.get<runtime::ScriptComponent>(entity).scripts) {
            if (script) fn(*script);
        }
    }
}

void RuntimeContext::fixed_update_scripts() {
    if (!m_editor_registry) return;
    for_each_enabled_script(*m_editor_registry, [](runtime::ComponentScript& s) { s.on_fixed_update(); });
}

void RuntimeContext::update_scripts() {
    if (!m_editor_registry) return;
    for_each_enabled_script(*m_editor_registry, [](runtime::ComponentScript& s) { s.on_update(); });
}

void RuntimeContext::late_update_scripts() {
    if (!m_editor_registry) return;
    for_each_enabled_script(*m_editor_registry, [](runtime::ComponentScript& s) { s.on_late_update(); });
}

void RuntimeContext::check_enable_disable_scripts() {
    if (!m_editor_registry) return;

    auto view = m_editor_registry->view<runtime::ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<runtime::ScriptComponent>(entity);
        if (sc.scripts.empty()) continue;

        bool currently_enabled = true;
        if (m_editor_registry->all_of<EntityInfo>(entity)) {
            currently_enabled = m_editor_registry->get<EntityInfo>(entity).enabled_in_hierarchy;
        }

        bool was_enabled = m_previously_enabled_script_entities.count(entity) > 0;

        if (currently_enabled && !was_enabled) {
            for (auto& script : sc.scripts) {
                if (script) script->on_enable();
            }
            m_previously_enabled_script_entities.insert(entity);
        } else if (!currently_enabled && was_enabled) {
            for (auto& script : sc.scripts) {
                if (script) script->on_disable();
            }
            m_previously_enabled_script_entities.erase(entity);
        }
    }
}

entt::entity RuntimeContext::instantiate_prefab_internal(const char* prefab_name) {
    if (!m_prefab_manager || !m_editor_registry || !prefab_name) return entt::null;

    if (!m_prefab_manager->find(prefab_name)) {
        if (m_project_assets_path.empty()) {
            engine::Logger::instance().error("Runtime",
                "Cannot instantiate prefab '%s': no project assets path set", prefab_name);
            return entt::null;
        }

        std::string file_path = m_project_assets_path + "/" + std::string(prefab_name) + ".prefab";
        std::ifstream file(file_path);
        if (!file.is_open()) {
            engine::Logger::instance().error("Runtime",
                "Prefab file not found: %s", file_path.c_str());
            return entt::null;
        }

        std::string json_str((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
        if (!m_prefab_manager->load_prefab_from_string(json_str)) {
            engine::Logger::instance().error("Runtime",
                "Failed to parse prefab: %s", file_path.c_str());
            return entt::null;
        }
    }

    auto entity = m_prefab_manager->instantiate(prefab_name, *m_editor_registry);
    if (entity == entt::null) return entt::null;

    if (!m_editor_registry->all_of<EntityInfo>(entity)) {
        auto& info = m_editor_registry->emplace<EntityInfo>(entity);
        info.name = prefab_name;
        info.is_prefab_instance = true;
        info.prefab_path = std::string(prefab_name) + ".prefab";
    }
    if (!m_editor_registry->all_of<Hierarchy>(entity)) {
        m_editor_registry->emplace<Hierarchy>(entity);
    }

    engine::Logger::instance().info("Runtime",
        "Instantiated prefab '%s' as entity %u", prefab_name, static_cast<unsigned>(entity));

    return entity;
}

}

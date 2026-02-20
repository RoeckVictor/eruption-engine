#include "RuntimeContext.h"
#include "PhysicsPlayback.h"
#include "SimulationPlayback.h"
#include "EditorComponents.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"
#include "engine/core/Engine.h"
#include "engine/core/Hierarchy.h"
#include "engine/core/HierarchyUtils.h"
#include "engine/platform/Input.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/animation/AnimationSystem.h"
#include "engine/render/Camera2D.h"
#include "editor/scripting/ScriptManager.h"
#include "runtime/ScriptComponent.h"
#include "engine/prefab/PrefabManager.h"
#include "engine/prefab/ComponentRegistry.h"
#include "engine/EngineComponentRegistry.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <functional>

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

// ============================================================================
// Event System Host Functions
// ============================================================================

runtime::EventHandle host_subscribe_event(runtime::ScriptHostAPI* api, entt::entity owner, const char* event_name, void* callback) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt || !event_name || !callback) return 0;

    auto handle = rt->m_next_event_handle++;
    auto& subs = rt->m_event_subscriptions[event_name];
    subs.push_back({handle, owner, reinterpret_cast<void(*)(const runtime::EventData&)>(callback)});
    return handle;
}

void host_unsubscribe_event(runtime::ScriptHostAPI* api, runtime::EventHandle handle) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt || handle == 0) return;

    for (auto& [name, subs] : rt->m_event_subscriptions) {
        subs.erase(std::remove_if(subs.begin(), subs.end(),
            [handle](const auto& s) { return s.handle == handle; }), subs.end());
    }
}

void host_dispatch_event(runtime::ScriptHostAPI* api, const char* event_name, const runtime::EventData* data) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt || !event_name) return;

    auto it = rt->m_event_subscriptions.find(event_name);
    if (it == rt->m_event_subscriptions.end()) return;

    runtime::EventData empty_data;
    const runtime::EventData& event_data = data ? *data : empty_data;

    for (const auto& sub : it->second) {
        if (sub.callback) {
            sub.callback(event_data);
        }
    }
}

// ============================================================================
// Coroutine Host Functions
// ============================================================================

runtime::CoroutineHandle host_start_coroutine(runtime::ScriptHostAPI* api, entt::entity owner, void* coro_handle_ptr) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt || !coro_handle_ptr) return 0;

    // NOTE: from_address() cannot validate the pointer is a real coroutine handle.
    // This is safe because ComponentScript::start_coroutine() calls Coroutine::release()
    // which returns a handle from a properly constructed Coroutine object.
    // Direct calls with invalid pointers will cause undefined behavior.
    auto coro_handle = runtime::Coroutine::handle_type::from_address(coro_handle_ptr);

    // Basic sanity check - null handle or already finished
    if (!coro_handle) {
        engine::Logger::instance().warning("Coroutine", "Attempted to start null coroutine handle");
        return 0;
    }

    // Try to safely check if done (this can still crash on garbage pointers)
    try {
        if (coro_handle.done()) {
            engine::Logger::instance().warning("Coroutine", "Attempted to start already-finished coroutine");
            coro_handle.destroy();
            return 0;
        }
    } catch (...) {
        engine::Logger::instance().error("Coroutine", "Invalid coroutine handle passed to start_coroutine");
        return 0;
    }

    auto handle = rt->m_next_coroutine_handle++;

    runtime::CoroutineInstance instance;
    instance.handle = handle;
    instance.owner = owner;
    instance.coro_handle = coro_handle;
    instance.active = true;
    instance.started = false;
    instance.wait_timer = 0.0f;

    rt->m_coroutines.push_back(std::move(instance));
    return handle;
}

void host_stop_coroutine(runtime::ScriptHostAPI* api, runtime::CoroutineHandle handle) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt || handle == 0) return;

    for (auto& coro : rt->m_coroutines) {
        if (coro.handle == handle) {
            RuntimeContext::destroy_coroutine(coro);
            break;
        }
    }
}

void host_stop_all_coroutines(runtime::ScriptHostAPI* api, entt::entity owner) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt) return;

    for (auto& coro : rt->m_coroutines) {
        if (coro.owner == owner) {
            RuntimeContext::destroy_coroutine(coro);
        }
    }
}

bool host_is_coroutine_running(runtime::ScriptHostAPI* api, runtime::CoroutineHandle handle) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt || handle == 0) return false;

    for (const auto& coro : rt->m_coroutines) {
        if (coro.handle == handle) {
            return coro.active && coro.coro_handle && !coro.coro_handle.done();
        }
    }
    return false;
}

// ============================================================================
// Random Host Functions
// ============================================================================

float host_random_float(runtime::ScriptHostAPI* api) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt) return 0.0f;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rt->m_random_engine);
}

float host_random_range(runtime::ScriptHostAPI* api, float min, float max) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt) return min;
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rt->m_random_engine);
}

int host_random_int(runtime::ScriptHostAPI* api, int min, int max) {
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    if (!rt) return min;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rt->m_random_engine);
}

// ============================================================================
// Camera Host Functions
// ============================================================================

/// Helper to get the first Camera2D in the registry, or nullptr if none exists.
static engine::render::Camera2D* get_first_camera(RuntimeContext* rt) {
    if (!rt || !rt->editor_registry()) return nullptr;
    auto view = rt->editor_registry()->view<engine::render::Camera2D>();
    auto it = view.begin();
    if (it != view.end()) {
        return &view.get<engine::render::Camera2D>(*it);
    }
    return nullptr;
}

static void host_get_camera_position(runtime::ScriptHostAPI* api, float* x, float* y) {
    *x = 0.0f; *y = 0.0f;
    auto* camera = get_first_camera(static_cast<RuntimeContext*>(api->runtime_ctx));
    if (!camera) return;
    *x = camera->x;
    *y = camera->y;
}

static void host_set_camera_position(runtime::ScriptHostAPI* api, float x, float y) {
    auto* camera = get_first_camera(static_cast<RuntimeContext*>(api->runtime_ctx));
    if (!camera) return;
    camera->x = x;
    camera->y = y;
}

static float host_get_camera_zoom(runtime::ScriptHostAPI* api) {
    auto* camera = get_first_camera(static_cast<RuntimeContext*>(api->runtime_ctx));
    return camera ? camera->zoom : 1.0f;
}

static void host_set_camera_zoom(runtime::ScriptHostAPI* api, float zoom) {
    auto* camera = get_first_camera(static_cast<RuntimeContext*>(api->runtime_ctx));
    if (!camera) return;
    camera->zoom = (std::max)(camera->min_zoom, (std::min)(zoom, camera->max_zoom));
}

static void host_screen_to_world(runtime::ScriptHostAPI* api, float screen_x, float screen_y, float* world_x, float* world_y) {
    *world_x = screen_x; *world_y = screen_y;
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    auto* camera = get_first_camera(rt);
    if (!camera || !rt->engine()) return;

    float screen_w = static_cast<float>(rt->engine()->window().width());
    float screen_h = static_cast<float>(rt->engine()->window().height());
    engine::render::screen_to_world(*camera, screen_x, screen_y, screen_w, screen_h, *world_x, *world_y);
}

static void host_world_to_screen(runtime::ScriptHostAPI* api, float world_x, float world_y, float* screen_x, float* screen_y) {
    *screen_x = world_x; *screen_y = world_y;
    auto* rt = static_cast<RuntimeContext*>(api->runtime_ctx);
    auto* camera = get_first_camera(rt);
    if (!camera || !rt->engine()) return;

    float screen_w = static_cast<float>(rt->engine()->window().width());
    float screen_h = static_cast<float>(rt->engine()->window().height());

    // Inverse of screen_to_world
    *screen_x = (world_x - camera->x) * camera->zoom + screen_w / 2.0f;
    *screen_y = (world_y - camera->y) * camera->zoom + screen_h / 2.0f;
}

// ============================================================================
// Hierarchy Host Functions
// ============================================================================

static entt::entity host_get_parent(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity) {
    if (!reg || !reg->valid(entity)) return entt::null;
    if (!reg->all_of<engine::Hierarchy>(entity)) return entt::null;
    return reg->get<engine::Hierarchy>(entity).parent;
}

static void host_set_parent(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity child, entt::entity parent) {
    if (!reg || !reg->valid(child)) return;
    if (parent != entt::null && !reg->valid(parent)) return;

    // Prevent creating hierarchy cycles (e.g., A->B->A)
    if (engine::hierarchy::would_create_cycle(*reg, child, parent)) {
        engine::Logger::instance().warning("Script",
            "set_parent() rejected: would create hierarchy cycle");
        return;
    }

    engine::hierarchy::set_parent_internal(*reg, child, parent);
}

static int host_get_child_count(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity) {
    if (!reg || !reg->valid(entity)) return 0;
    if (!reg->all_of<engine::Hierarchy>(entity)) return 0;
    return static_cast<int>(reg->get<engine::Hierarchy>(entity).children.size());
}

static entt::entity host_get_child(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity parent, int index) {
    if (!reg || !reg->valid(parent)) return entt::null;
    if (!reg->all_of<engine::Hierarchy>(parent)) return entt::null;
    const auto& children = reg->get<engine::Hierarchy>(parent).children;
    if (index < 0 || index >= static_cast<int>(children.size())) return entt::null;
    return children[index];
}

// ============================================================================
// Entity Info Host Functions
// ============================================================================

static const char* host_get_entity_name(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity) {
    // Thread-local buffer to avoid data races; caller should copy if needed across calls
    thread_local std::string name_buffer;
    if (!reg || !reg->valid(entity)) return "";
    if (!reg->all_of<EntityInfo>(entity)) return "";
    name_buffer = reg->get<EntityInfo>(entity).name;
    return name_buffer.c_str();
}

static bool host_is_entity_active(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity) {
    if (!reg || !reg->valid(entity)) return false;
    if (!reg->all_of<EntityInfo>(entity)) return true;
    return reg->get<EntityInfo>(entity).enabled_in_hierarchy;
}

static void host_set_entity_active(runtime::ScriptHostAPI* /*api*/, entt::registry* reg, entt::entity entity, bool active) {
    if (!reg || !reg->valid(entity)) return;
    if (!reg->all_of<EntityInfo>(entity)) return;
    reg->get<EntityInfo>(entity).enabled_in_hierarchy = active;
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

    // Event system
    m_host_api.subscribe_event = &host_subscribe_event;
    m_host_api.unsubscribe_event = &host_unsubscribe_event;
    m_host_api.dispatch_event = &host_dispatch_event;

    // Coroutines
    m_host_api.start_coroutine = &host_start_coroutine;
    m_host_api.stop_coroutine = &host_stop_coroutine;
    m_host_api.stop_all_coroutines = &host_stop_all_coroutines;
    m_host_api.is_coroutine_running = &host_is_coroutine_running;

    // Random
    m_host_api.random_float = &host_random_float;
    m_host_api.random_range = &host_random_range;
    m_host_api.random_int = &host_random_int;

    // Camera
    m_host_api.get_camera_position = &host_get_camera_position;
    m_host_api.set_camera_position = &host_set_camera_position;
    m_host_api.get_camera_zoom = &host_get_camera_zoom;
    m_host_api.set_camera_zoom = &host_set_camera_zoom;
    m_host_api.screen_to_world = &host_screen_to_world;
    m_host_api.world_to_screen = &host_world_to_screen;

    // Hierarchy
    m_host_api.get_parent = &host_get_parent;
    m_host_api.set_parent = &host_set_parent;
    m_host_api.get_child_count = &host_get_child_count;
    m_host_api.get_child = &host_get_child;

    // Entity info
    m_host_api.get_entity_name = &host_get_entity_name;
    m_host_api.is_entity_active = &host_is_entity_active;
    m_host_api.set_entity_active = &host_set_entity_active;
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

    // Clean up coroutines
    for (auto& coro : m_coroutines) {
        destroy_coroutine(coro);
    }
    m_coroutines.clear();
    m_next_coroutine_handle = 1;

    // Clean up events
    m_event_subscriptions.clear();
    m_next_event_handle = 1;

    // Clean up collision tracking
    m_body_to_entity.clear();
    m_body_map_dirty = true;
    m_current_contacts.clear();
    m_previous_contact_pairs.clear();

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
            process_collision_events();
        }
    }

    if (m_physics_playback) {
        m_physics_playback->sync_to_transforms();
    }

    update_world_transforms(*m_editor_registry);

    update_coroutines(dt);
    check_enable_disable_scripts();
    update_scripts();
    late_update_scripts();

    if (m_sim_playback) {
        m_sim_playback->update(m_frame_count);
    }

    for (auto entity : m_deferred_destroys) {
        if (m_editor_registry->valid(entity)) {
            cleanup_entity_coroutines(entity);
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

    // Create physics bodies for instantiated entities with Rigidbody components
    if (m_physics_playback) {
        // Helper to create physics body for an entity and its children
        std::function<void(entt::entity)> create_bodies_recursive = [&](entt::entity e) {
            if (!m_editor_registry->valid(e)) return;

            if (m_editor_registry->all_of<engine::physics::Rigidbody>(e)) {
                auto& rb = m_editor_registry->get<engine::physics::Rigidbody>(e);
                if (rb.enabled && !b2Body_IsValid(rb.body_id)) {
                    m_physics_playback->create_body_for_entity(e);
                }
            }

            // Process children
            if (m_editor_registry->all_of<Hierarchy>(e)) {
                for (auto child : m_editor_registry->get<Hierarchy>(e).children) {
                    create_bodies_recursive(child);
                }
            }
        };

        create_bodies_recursive(entity);
        invalidate_body_map();
    }

    engine::Logger::instance().info("Runtime",
        "Instantiated prefab '%s' as entity %u", prefab_name, static_cast<unsigned>(entity));

    return entity;
}

void RuntimeContext::update_coroutines(float dt) {
    if (m_coroutines.empty()) return;

    for (auto& coro : m_coroutines) {
        if (!coro.active || !coro.coro_handle || coro.coro_handle.done()) {
            coro.active = false;
            continue;
        }

        // Check if owner entity is still valid
        if (coro.owner != entt::null && m_editor_registry && !m_editor_registry->valid(coro.owner)) {
            destroy_coroutine(coro);
            continue;
        }

        bool should_resume = false;

        if (!coro.started) {
            // First run - resume to start the coroutine
            should_resume = true;
            coro.started = true;
        } else {
            // Check yield instruction
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, runtime::WaitForNextFrame>) {
                    should_resume = true;
                } else if constexpr (std::is_same_v<T, runtime::WaitForSeconds>) {
                    coro.wait_timer -= dt;
                    if (coro.wait_timer <= 0.0f) {
                        should_resume = true;
                    }
                } else if constexpr (std::is_same_v<T, runtime::WaitUntil>) {
                    if (arg.condition && arg.condition()) {
                        should_resume = true;
                    }
                } else if constexpr (std::is_same_v<T, runtime::WaitWhile>) {
                    if (!arg.condition || !arg.condition()) {
                        should_resume = true;
                    }
                }
            }, coro.current_yield);
        }

        if (should_resume) {
            try {
                coro.coro_handle.resume();

                if (coro.coro_handle.done()) {
                    coro.active = false;
                } else {
                    // Get the new yield instruction
                    coro.current_yield = coro.coro_handle.promise().current_yield;

                    // Initialize wait timer for WaitForSeconds
                    if (auto* wait = std::get_if<runtime::WaitForSeconds>(&coro.current_yield)) {
                        coro.wait_timer = wait->seconds;
                    }
                }
            } catch (const std::exception& e) {
                engine::Logger::instance().error("Coroutine", "Exception in coroutine: %s", e.what());
                coro.active = false;
            }
        }
    }

    // Clean up and remove inactive coroutines
    for (auto& coro : m_coroutines) {
        if (!coro.active) {
            destroy_coroutine(coro);  // Safe to call even if already destroyed
        }
    }
    m_coroutines.erase(
        std::remove_if(m_coroutines.begin(), m_coroutines.end(),
            [](const runtime::CoroutineInstance& c) { return !c.active; }),
        m_coroutines.end());
}

void RuntimeContext::cleanup_entity_coroutines(entt::entity entity) {
    for (auto& coro : m_coroutines) {
        if (coro.owner == entity) {
            destroy_coroutine(coro);
        }
    }
}

void RuntimeContext::build_body_to_entity_map() {
    m_body_to_entity.clear();
    if (!m_editor_registry) return;

    auto view = m_editor_registry->view<engine::physics::Rigidbody>();
    for (auto entity : view) {
        auto& rb = view.get<engine::physics::Rigidbody>(entity);
        if (b2Body_IsValid(rb.body_id)) {
            // Use body ID as key (convert to uint64_t)
            uint64_t body_key = static_cast<uint64_t>(rb.body_id.index1) |
                               (static_cast<uint64_t>(rb.body_id.world0) << 32);
            m_body_to_entity[body_key] = entity;
        }
    }
}

void RuntimeContext::process_collision_events() {
    if (!m_physics_world || !m_editor_registry) return;

    // Rebuild body-to-entity map only when dirty (bodies added/removed)
    if (m_body_map_dirty) {
        build_body_to_entity_map();
        m_body_map_dirty = false;
    }

    // Store previous frame's contact pairs for enter/exit detection
    m_previous_contact_pairs.clear();
    for (const auto& [pair, data] : m_current_contacts) {
        m_previous_contact_pairs.insert(pair);
    }
    m_current_contacts.clear();

    // Phase 1: Collect all contacts (without firing callbacks)
    // Multiple events for the same pair will update the info (hit events have better data)
    m_physics_world->for_each_contact([this](const engine::physics::PhysicsWorld::ContactInfo& contact) {
        // Look up entities from bodies
        uint64_t body_a_key = static_cast<uint64_t>(contact.body_a.index1) |
                             (static_cast<uint64_t>(contact.body_a.world0) << 32);
        uint64_t body_b_key = static_cast<uint64_t>(contact.body_b.index1) |
                             (static_cast<uint64_t>(contact.body_b.world0) << 32);

        auto it_a = m_body_to_entity.find(body_a_key);
        auto it_b = m_body_to_entity.find(body_b_key);

        if (it_a == m_body_to_entity.end() || it_b == m_body_to_entity.end()) {
            return;  // One of the bodies doesn't have an entity
        }

        entt::entity entity_a = it_a->second;
        entt::entity entity_b = it_b->second;

        runtime::ContactPair pair;
        pair.entity_a = entity_a;
        pair.entity_b = entity_b;
        pair.is_trigger = contact.is_sensor;

        if (contact.is_touching) {
            auto& data = m_current_contacts[pair];
            data.pair = pair;

            // Update info - prefer data with impulse (hit events have this)
            bool should_update = (data.info_for_a.impulse == 0.0f && contact.impulse != 0.0f) ||
                                 data.info_for_a.other_entity == entt::null;
            if (should_update) {
                data.info_for_a.other_entity = entity_b;
                data.info_for_a.normal_x = contact.normal_x;
                data.info_for_a.normal_y = contact.normal_y;
                data.info_for_a.point_x = contact.point_x;
                data.info_for_a.point_y = contact.point_y;
                data.info_for_a.impulse = contact.impulse;

                data.info_for_b.other_entity = entity_a;
                data.info_for_b.normal_x = -contact.normal_x;
                data.info_for_b.normal_y = -contact.normal_y;
                data.info_for_b.point_x = contact.point_x;
                data.info_for_b.point_y = contact.point_y;
                data.info_for_b.impulse = contact.impulse;
            }
        }
    });

    // Phase 2: Fire callbacks for current contacts (each pair processed exactly once)
    for (const auto& [pair, data] : m_current_contacts) {
        bool was_touching = m_previous_contact_pairs.count(pair) > 0;

        auto call_collision_callbacks = [this, &pair, was_touching](entt::entity entity, const runtime::CollisionInfo& info) {
            if (!m_editor_registry->valid(entity)) return;
            if (!m_editor_registry->all_of<runtime::ScriptComponent>(entity)) return;
            auto& sc = m_editor_registry->get<runtime::ScriptComponent>(entity);

            for (auto& script : sc.scripts) {
                if (!script) continue;

                if (pair.is_trigger) {
                    if (!was_touching) {
                        script->on_trigger_enter(info);
                    } else {
                        script->on_trigger_stay(info);
                    }
                } else {
                    if (!was_touching) {
                        script->on_collision_enter(info);
                    } else {
                        script->on_collision_stay(info);
                    }
                }
            }
        };

        call_collision_callbacks(pair.entity_a, data.info_for_a);
        call_collision_callbacks(pair.entity_b, data.info_for_b);
    }

    // Phase 3: Fire exit callbacks for contacts that ended
    for (const auto& prev_pair : m_previous_contact_pairs) {
        if (m_current_contacts.count(prev_pair) == 0) {
            runtime::CollisionInfo info_for_a;
            info_for_a.other_entity = prev_pair.entity_b;

            runtime::CollisionInfo info_for_b;
            info_for_b.other_entity = prev_pair.entity_a;

            auto call_exit_callbacks = [this, &prev_pair](entt::entity entity, const runtime::CollisionInfo& info) {
                if (!m_editor_registry->valid(entity)) return;
                if (!m_editor_registry->all_of<runtime::ScriptComponent>(entity)) return;
                auto& sc = m_editor_registry->get<runtime::ScriptComponent>(entity);

                for (auto& script : sc.scripts) {
                    if (!script) continue;

                    if (prev_pair.is_trigger) {
                        script->on_trigger_exit(info);
                    } else {
                        script->on_collision_exit(info);
                    }
                }
            };

            if (m_editor_registry->valid(prev_pair.entity_a)) {
                call_exit_callbacks(prev_pair.entity_a, info_for_a);
            }
            if (m_editor_registry->valid(prev_pair.entity_b)) {
                call_exit_callbacks(prev_pair.entity_b, info_for_b);
            }
        }
    }
}

}

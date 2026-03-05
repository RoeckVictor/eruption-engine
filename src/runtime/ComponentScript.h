#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <nlohmann/json.hpp>

#include "engine/platform/KeyCode.h"
#include "engine/core/Transform.h"
#include "runtime/ScriptEvents.h"
#include "runtime/ScriptCoroutine.h"

namespace engine {
class Engine;
}

namespace editor {
class RuntimeContext;
class InspectorPanel;
}

namespace runtime {

// Simple 2D vector for script API return values.
struct Vec2 { float x, y; };

// Shared API struct passed from host (editor EXE) to scripts.
// Contains per-frame state values, host context pointers, and function pointers
// for host-side services.  One instance is created by RuntimeContext and shared
// by all scripts.
// Every callback receives a pointer back to this struct so it can recover the
// host context (Engine, RuntimeContext) without relying on global state
struct ScriptHostAPI {
    // Host Context (opaque pointers set by RuntimeContext)
    void* engine_ctx  = nullptr;
    void* runtime_ctx = nullptr;

    float delta_time = 0.0f;
    float fixed_delta_time = 1.0f / 60.0f;
    float time = 0.0f;
    uint64_t frame_count = 0;

    bool (*is_key_held)(ScriptHostAPI*, int key) = nullptr;
    bool (*is_key_pressed)(ScriptHostAPI*, int key) = nullptr;
    bool (*is_key_released)(ScriptHostAPI*, int key) = nullptr;
    bool (*is_mouse_held)(ScriptHostAPI*, int button) = nullptr;
    bool (*is_mouse_pressed)(ScriptHostAPI*, int button) = nullptr;
    double (*get_mouse_x)(ScriptHostAPI*) = nullptr;
    double (*get_mouse_y)(ScriptHostAPI*) = nullptr;

    void (*log_info)(ScriptHostAPI*, const char* msg) = nullptr;
    void (*log_warning)(ScriptHostAPI*, const char* msg) = nullptr;
    void (*log_error)(ScriptHostAPI*, const char* msg) = nullptr;

    void (*get_velocity)(ScriptHostAPI*, entt::registry*, entt::entity, float* vx, float* vy) = nullptr;
    void (*set_velocity)(ScriptHostAPI*, entt::registry*, entt::entity, float vx, float vy) = nullptr;
    void (*add_force)(ScriptHostAPI*, entt::registry*, entt::entity, float fx, float fy) = nullptr;
    void (*add_impulse)(ScriptHostAPI*, entt::registry*, entt::entity, float ix, float iy) = nullptr;

    entt::entity (*find_entity_by_name)(ScriptHostAPI*, entt::registry*, const char* name) = nullptr;
    entt::entity (*find_entity_by_guid)(ScriptHostAPI*, entt::registry*, const char* guid) = nullptr;
    void (*destroy_entity)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;

    void* (*get_component)(ScriptHostAPI*, entt::registry*, entt::entity, entt::id_type) = nullptr;

    void* (*add_component)(ScriptHostAPI*, entt::registry*, entt::entity, entt::id_type) = nullptr;
    void (*remove_component)(ScriptHostAPI*, entt::registry*, entt::entity, entt::id_type) = nullptr;

    entt::entity (*instantiate_prefab)(ScriptHostAPI*, entt::registry*, const char* prefab_name) = nullptr;

    EventHandle (*subscribe_event)(ScriptHostAPI*, entt::entity owner, const char* event_name, void* callback) = nullptr;
    void (*unsubscribe_event)(ScriptHostAPI*, EventHandle handle) = nullptr;
    void (*dispatch_event)(ScriptHostAPI*, const char* event_name, const EventData* data) = nullptr;

    CoroutineHandle (*start_coroutine)(ScriptHostAPI*, entt::entity owner, void* coro_handle) = nullptr;
    void (*stop_coroutine)(ScriptHostAPI*, CoroutineHandle handle) = nullptr;
    void (*stop_all_coroutines)(ScriptHostAPI*, entt::entity owner) = nullptr;
    bool (*is_coroutine_running)(ScriptHostAPI*, CoroutineHandle handle) = nullptr;

    float (*random_float)(ScriptHostAPI*) = nullptr;
    float (*random_range)(ScriptHostAPI*, float min, float max) = nullptr;
    int (*random_int)(ScriptHostAPI*, int min, int max) = nullptr;

    void (*get_camera_position)(ScriptHostAPI*, float* x, float* y) = nullptr;
    void (*set_camera_position)(ScriptHostAPI*, float x, float y) = nullptr;
    float (*get_camera_zoom)(ScriptHostAPI*) = nullptr;
    void (*set_camera_zoom)(ScriptHostAPI*, float zoom) = nullptr;
    void (*screen_to_world)(ScriptHostAPI*, float screen_x, float screen_y, float* world_x, float* world_y) = nullptr;
    void (*world_to_screen)(ScriptHostAPI*, float world_x, float world_y, float* screen_x, float* screen_y) = nullptr;

    entt::entity (*get_parent)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    void (*set_parent)(ScriptHostAPI*, entt::registry*, entt::entity child, entt::entity parent) = nullptr;
    int (*get_child_count)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    entt::entity (*get_child)(ScriptHostAPI*, entt::registry*, entt::entity parent, int index) = nullptr;

    const char* (*get_entity_name)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    bool (*is_entity_active)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    void (*set_entity_active)(ScriptHostAPI*, entt::registry*, entt::entity, bool active) = nullptr;

    bool (*spawn_pixels_at_world)(ScriptHostAPI*, float world_x, float world_y, int radius, int material_id, bool erase) = nullptr;

    void (*get_screen_size)(ScriptHostAPI*, float* width, float* height) = nullptr;
    void (*get_viewport_offset)(ScriptHostAPI*, float* x, float* y) = nullptr;

    bool (*raycast)(ScriptHostAPI*, float origin_x, float origin_y, float dir_x, float dir_y,
                    float max_distance, float* hit_x, float* hit_y, float* hit_normal_x, float* hit_normal_y) = nullptr;
};

inline ScriptHostAPI*& get_global_host_api();

// Base class for user-defined component scripts
// Scripts can be attached to entities to add custom behavior
// Scripts are hot-reloadable when compiled as a DLL
class ComponentScript {
public:
    virtual ~ComponentScript() = default;

    virtual void on_create() {}
    virtual void on_destroy() {}
    virtual void on_enable() {}
    virtual void on_disable() {}

    virtual void on_update() {}
    virtual void on_fixed_update() {}
    virtual void on_late_update() {}

    virtual void on_collision_enter(const CollisionInfo& info) { (void)info; }
    virtual void on_collision_stay(const CollisionInfo& info) { (void)info; }
    virtual void on_collision_exit(const CollisionInfo& info) { (void)info; }
    virtual void on_trigger_enter(const CollisionInfo& info) { (void)info; }
    virtual void on_trigger_stay(const CollisionInfo& info) { (void)info; }
    virtual void on_trigger_exit(const CollisionInfo& info) { (void)info; }

    virtual void on_render() {}
    virtual void on_inspector_gui(nlohmann::json& properties) { (void)properties; }
    virtual void on_gizmo() {}

    virtual void serialize_properties(nlohmann::json& out) const { (void)out; }
    virtual void deserialize_properties(const nlohmann::json& data) { (void)data; }

    float delta_time() const { return m_host_api ? m_host_api->delta_time : 0.0f; }
    float fixed_delta_time() const { return m_host_api ? m_host_api->fixed_delta_time : (1.0f / 60.0f); }
    float time() const { return m_host_api ? m_host_api->time : 0.0f; }
    uint64_t frame_count() const { return m_host_api ? m_host_api->frame_count : 0; }

    bool is_key_held(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_held &&
               m_host_api->is_key_held(m_host_api, static_cast<int>(key));
    }
    bool is_key_pressed(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_pressed &&
               m_host_api->is_key_pressed(m_host_api, static_cast<int>(key));
    }
    bool is_key_released(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_released &&
               m_host_api->is_key_released(m_host_api, static_cast<int>(key));
    }
    bool is_mouse_held(engine::platform::MouseButton btn) const {
        return m_host_api && m_host_api->is_mouse_held &&
               m_host_api->is_mouse_held(m_host_api, static_cast<int>(btn));
    }
    bool is_mouse_pressed(engine::platform::MouseButton btn) const {
        return m_host_api && m_host_api->is_mouse_pressed &&
               m_host_api->is_mouse_pressed(m_host_api, static_cast<int>(btn));
    }
    double mouse_x() const {
        return (m_host_api && m_host_api->get_mouse_x) ? m_host_api->get_mouse_x(m_host_api) : 0.0;
    }
    double mouse_y() const {
        return (m_host_api && m_host_api->get_mouse_y) ? m_host_api->get_mouse_y(m_host_api) : 0.0;
    }

    Vec2 get_position() {
        auto* t = get_component<engine::Transform>();
        return t ? Vec2{t->x, t->y} : Vec2{0.0f, 0.0f};
    }
    void set_position(float x, float y) {
        if (auto* t = get_component<engine::Transform>()) { t->x = x; t->y = y; }
    }
    void translate(float dx, float dy) {
        if (auto* t = get_component<engine::Transform>()) { t->x += dx; t->y += dy; }
    }
    float get_rotation() {
        auto* t = get_component<engine::Transform>();
        return t ? t->rotation : 0.0f;
    }
    void set_rotation(float angle) {
        if (auto* t = get_component<engine::Transform>()) t->rotation = angle;
    }
    void rotate(float delta) {
        if (auto* t = get_component<engine::Transform>()) t->rotation += delta;
    }
    Vec2 get_scale() {
        auto* t = get_component<engine::Transform>();
        return t ? Vec2{t->scale_x, t->scale_y} : Vec2{1.0f, 1.0f};
    }
    void set_scale(float sx, float sy) {
        if (auto* t = get_component<engine::Transform>()) { t->scale_x = sx; t->scale_y = sy; }
    }

    void log(const char* msg) {
        if (m_host_api && m_host_api->log_info) m_host_api->log_info(m_host_api, msg);
    }
    void log_warning(const char* msg) {
        if (m_host_api && m_host_api->log_warning) m_host_api->log_warning(m_host_api, msg);
    }
    void log_error(const char* msg) {
        if (m_host_api && m_host_api->log_error) m_host_api->log_error(m_host_api, msg);
    }

    Vec2 get_velocity() {
        Vec2 v{0.0f, 0.0f};
        if (m_host_api && m_host_api->get_velocity)
            m_host_api->get_velocity(m_host_api, m_registry, m_entity, &v.x, &v.y);
        return v;
    }
    void set_velocity(float vx, float vy) {
        if (m_host_api && m_host_api->set_velocity)
            m_host_api->set_velocity(m_host_api, m_registry, m_entity, vx, vy);
    }
    void add_force(float fx, float fy) {
        if (m_host_api && m_host_api->add_force)
            m_host_api->add_force(m_host_api, m_registry, m_entity, fx, fy);
    }
    void add_impulse(float ix, float iy) {
        if (m_host_api && m_host_api->add_impulse)
            m_host_api->add_impulse(m_host_api, m_registry, m_entity, ix, iy);
    }

    struct RaycastResult {
        bool hit = false;
        float point_x = 0.0f, point_y = 0.0f;
        float normal_x = 0.0f, normal_y = 0.0f;
    };

    RaycastResult raycast(float origin_x, float origin_y, float dir_x, float dir_y, float max_distance) {
        RaycastResult result;
        if (m_host_api && m_host_api->raycast) {
            result.hit = m_host_api->raycast(m_host_api, origin_x, origin_y, dir_x, dir_y,
                                              max_distance, &result.point_x, &result.point_y,
                                              &result.normal_x, &result.normal_y);
        }
        return result;
    }

    entt::entity find_entity(const char* name) {
        if (m_host_api && m_host_api->find_entity_by_name)
            return m_host_api->find_entity_by_name(m_host_api, m_registry, name);
        return entt::null;
    }
    entt::entity find_entity_by_guid(const char* guid) {
        if (m_host_api && m_host_api->find_entity_by_guid)
            return m_host_api->find_entity_by_guid(m_host_api, m_registry, guid);
        return entt::null;
    }
    void destroy_self() {
        if (m_host_api && m_host_api->destroy_entity)
            m_host_api->destroy_entity(m_host_api, m_registry, m_entity);
    }
    void destroy_entity(entt::entity target) {
        if (m_host_api && m_host_api->destroy_entity)
            m_host_api->destroy_entity(m_host_api, m_registry, target);
    }

    entt::entity entity() const { return m_entity; }
    entt::registry* registry() { return m_registry; }
    const entt::registry* registry() const { return m_registry; }

    engine::Engine* engine() { return m_engine; }
    const engine::Engine* engine() const { return m_engine; }

    virtual const char* type_name() const = 0;

    template<typename T>
    T* get_component() {
        if (!m_host_api || !m_host_api->get_component || !m_registry) return nullptr;
        return static_cast<T*>(m_host_api->get_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }
    template<typename T>
    const T* get_component() const {
        if (!m_host_api || !m_host_api->get_component || !m_registry) return nullptr;
        return static_cast<const T*>(m_host_api->get_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }
    template<typename T>
    bool has_component() const {
        return get_component<T>() != nullptr;
    }
    template<typename T>
    T* add_component() {
        if (!m_host_api || !m_host_api->add_component || !m_registry) return nullptr;
        return static_cast<T*>(m_host_api->add_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }
    template<typename T>
    void remove_component() {
        if (!m_host_api || !m_host_api->remove_component || !m_registry) return;
        m_host_api->remove_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value());
    }

    entt::entity instantiate(const char* prefab_name) {
        if (!m_host_api || !m_host_api->instantiate_prefab || !m_registry) return entt::null;
        return m_host_api->instantiate_prefab(m_host_api, m_registry, prefab_name);
    }

    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    static float inverse_lerp(float a, float b, float value) {
        if (std::abs(b - a) < 0.0001f) return 0.0f;
        return (value - a) / (b - a);
    }
    static float clamp(float value, float min, float max) {
        return value < min ? min : (value > max ? max : value);
    }
    static float clamp01(float value) {
        return clamp(value, 0.0f, 1.0f);
    }
    static float smoothstep(float edge0, float edge1, float x) {
        float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }
    static float move_towards(float current, float target, float max_delta) {
        float diff = target - current;
        if (std::abs(diff) <= max_delta) return target;
        return current + std::copysign(max_delta, diff);
    }
    static float rotate_towards(float current, float target, float max_delta) {
        float diff = angle_difference(target, current);
        if (std::abs(diff) <= max_delta) return target;
        return current + std::copysign(max_delta, diff);
    }
    static float angle_difference(float a, float b) {
        float diff = std::fmod(a - b + 180.0f, 360.0f) - 180.0f;
        return diff < -180.0f ? diff + 360.0f : diff;
    }
    static float sign(float value) {
        return static_cast<float>((value > 0.0f) - (value < 0.0f));
    }
    static Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
        return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
    }
    static float vec2_distance(Vec2 a, Vec2 b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    static float vec2_magnitude(Vec2 v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }
    static float vec2_sqr_magnitude(Vec2 v) {
        return v.x * v.x + v.y * v.y;
    }
    static Vec2 vec2_normalize(Vec2 v) {
        float len = vec2_magnitude(v);
        return len > 0.0001f ? Vec2{v.x / len, v.y / len} : Vec2{0.0f, 0.0f};
    }
    static float vec2_dot(Vec2 a, Vec2 b) {
        return a.x * b.x + a.y * b.y;
    }
    static Vec2 vec2_move_towards(Vec2 current, Vec2 target, float max_delta) {
        Vec2 diff = {target.x - current.x, target.y - current.y};
        float dist = vec2_magnitude(diff);
        if (dist <= max_delta || dist < 0.0001f) return target;
        return {current.x + diff.x / dist * max_delta, current.y + diff.y / dist * max_delta};
    }

    CoroutineHandle start_coroutine(Coroutine coro) {
        if (!m_host_api || !m_host_api->start_coroutine) return 0;
        auto handle = coro.release();
        return m_host_api->start_coroutine(m_host_api, m_entity, handle.address());
    }
    void stop_coroutine(CoroutineHandle handle) {
        if (m_host_api && m_host_api->stop_coroutine)
            m_host_api->stop_coroutine(m_host_api, handle);
    }
    void stop_all_coroutines() {
        if (m_host_api && m_host_api->stop_all_coroutines)
            m_host_api->stop_all_coroutines(m_host_api, m_entity);
    }
    bool is_coroutine_running(CoroutineHandle handle) const {
        if (!m_host_api || !m_host_api->is_coroutine_running) return false;
        return m_host_api->is_coroutine_running(m_host_api, handle);
    }

    EventHandle subscribe(const char* event_name, void (*callback)(const EventData&)) {
        if (!m_host_api || !m_host_api->subscribe_event) return 0;
        return m_host_api->subscribe_event(m_host_api, m_entity, event_name, reinterpret_cast<void*>(callback));
    }
    void unsubscribe(EventHandle handle) {
        if (m_host_api && m_host_api->unsubscribe_event)
            m_host_api->unsubscribe_event(m_host_api, handle);
    }
    void dispatch_event(const char* event_name, const EventData& data) {
        if (m_host_api && m_host_api->dispatch_event)
            m_host_api->dispatch_event(m_host_api, event_name, &data);
    }
    void dispatch_event(const char* event_name) {
        EventData empty;
        dispatch_event(event_name, empty);
    }


    float random_float() {
        if (!m_host_api || !m_host_api->random_float) return 0.0f;
        return m_host_api->random_float(m_host_api);
    }
    float random_range(float min, float max) {
        if (!m_host_api || !m_host_api->random_range) return min;
        return m_host_api->random_range(m_host_api, min, max);
    }
    int random_int(int min, int max) {
        if (!m_host_api || !m_host_api->random_int) return min;
        return m_host_api->random_int(m_host_api, min, max);
    }
    Vec2 random_point_in_circle(float radius) {
        float angle = random_range(0.0f, 6.28318530718f);
        float r = radius * std::sqrt(random_float());
        return {r * std::cos(angle), r * std::sin(angle)};
    }
    Vec2 random_direction() {
        float angle = random_range(0.0f, 6.28318530718f);
        return {std::cos(angle), std::sin(angle)};
    }

    Vec2 get_camera_position() {
        Vec2 pos{0.0f, 0.0f};
        if (m_host_api && m_host_api->get_camera_position)
            m_host_api->get_camera_position(m_host_api, &pos.x, &pos.y);
        return pos;
    }
    void set_camera_position(float x, float y) {
        if (m_host_api && m_host_api->set_camera_position)
            m_host_api->set_camera_position(m_host_api, x, y);
    }
    float get_camera_zoom() {
        if (!m_host_api || !m_host_api->get_camera_zoom) return 1.0f;
        return m_host_api->get_camera_zoom(m_host_api);
    }
    void set_camera_zoom(float zoom) {
        if (m_host_api && m_host_api->set_camera_zoom)
            m_host_api->set_camera_zoom(m_host_api, zoom);
    }
    Vec2 screen_to_world(float screen_x, float screen_y) {
        Vec2 world{0.0f, 0.0f};
        if (m_host_api && m_host_api->screen_to_world)
            m_host_api->screen_to_world(m_host_api, screen_x, screen_y, &world.x, &world.y);
        return world;
    }
    Vec2 world_to_screen(float world_x, float world_y) {
        Vec2 screen{0.0f, 0.0f};
        if (m_host_api && m_host_api->world_to_screen)
            m_host_api->world_to_screen(m_host_api, world_x, world_y, &screen.x, &screen.y);
        return screen;
    }

    entt::entity get_parent() {
        if (!m_host_api || !m_host_api->get_parent || !m_registry) return entt::null;
        return m_host_api->get_parent(m_host_api, m_registry, m_entity);
    }
    void set_parent(entt::entity parent) {
        if (m_host_api && m_host_api->set_parent && m_registry)
            m_host_api->set_parent(m_host_api, m_registry, m_entity, parent);
    }
    void detach_from_parent() {
        set_parent(entt::null);
    }
    int get_child_count() {
        if (!m_host_api || !m_host_api->get_child_count || !m_registry) return 0;
        return m_host_api->get_child_count(m_host_api, m_registry, m_entity);
    }
    entt::entity get_child(int index) {
        if (!m_host_api || !m_host_api->get_child || !m_registry) return entt::null;
        return m_host_api->get_child(m_host_api, m_registry, m_entity, index);
    }

    const char* get_name() {
        if (!m_host_api || !m_host_api->get_entity_name || !m_registry) return "";
        // Copy to local cache to avoid issues with thread-local buffer in host
        const char* name = m_host_api->get_entity_name(m_host_api, m_registry, m_entity);
        m_name_cache = name ? name : "";
        return m_name_cache.c_str();
    }
    bool is_active() {
        if (!m_host_api || !m_host_api->is_entity_active || !m_registry) return true;
        return m_host_api->is_entity_active(m_host_api, m_registry, m_entity);
    }
    void set_active(bool active) {
        if (m_host_api && m_host_api->set_entity_active && m_registry)
            m_host_api->set_entity_active(m_host_api, m_registry, m_entity, active);
    }

    bool spawn_pixels_at_world(float world_x, float world_y, int radius, int material_id) {
        if (!m_host_api || !m_host_api->spawn_pixels_at_world) return false;
        return m_host_api->spawn_pixels_at_world(m_host_api, world_x, world_y, radius, material_id, false);
    }
    bool erase_pixels_at_world(float world_x, float world_y, int radius) {
        if (!m_host_api || !m_host_api->spawn_pixels_at_world) return false;
        return m_host_api->spawn_pixels_at_world(m_host_api, world_x, world_y, radius, 0, true);
    }

    Vec2 get_screen_size() {
        Vec2 size{800.0f, 600.0f};
        if (m_host_api && m_host_api->get_screen_size)
            m_host_api->get_screen_size(m_host_api, &size.x, &size.y);
        return size;
    }
    Vec2 get_viewport_offset() {
        Vec2 offset{0.0f, 0.0f};
        if (m_host_api && m_host_api->get_viewport_offset)
            m_host_api->get_viewport_offset(m_host_api, &offset.x, &offset.y);
        return offset;
    }

protected:
    friend class ScriptSystem;
    friend class ScriptManager;
    friend class editor::RuntimeContext;
    friend class editor::InspectorPanel;

    entt::entity m_entity = entt::null;
    entt::registry* m_registry = nullptr;
    engine::Engine* m_engine = nullptr;
    ScriptHostAPI* m_host_api = nullptr;
    mutable std::string m_name_cache;

    void init_context(entt::entity entity, entt::registry* registry, engine::Engine* engine,
                      ScriptHostAPI* host_api = nullptr) {
        m_entity = entity;
        m_registry = registry;
        m_engine = engine;
        m_host_api = host_api;

        // Set global host API for static logging functions
        if (host_api) {
            get_global_host_api() = host_api;
        }
    }
};

using ScriptFactory = ComponentScript* (*)();

struct ScriptRegistryEntry {
    const char* name;
    ScriptFactory factory;
};

// Global auto-registration list. Scripts register themselves via REGISTER_COMPONENT_SCRIPT
// This works within a DLL because static initializers execute at DLL load time
inline std::vector<ScriptRegistryEntry>& get_script_registry() {
    static std::vector<ScriptRegistryEntry> registry;
    return registry;
}

// Helper struct for static-initializer-based registration.
struct ScriptAutoRegistrar {
    ScriptAutoRegistrar(const char* name, ScriptFactory factory) {
        get_script_registry().push_back({name, factory});
    }
};


inline ScriptHostAPI*& get_global_host_api() {
    static ScriptHostAPI* s_host_api = nullptr;
    return s_host_api;
}
inline void script_log(const char* msg) {
    auto* api = get_global_host_api();
    if (api && api->log_info) api->log_info(api, msg);
}
inline void script_log_warning(const char* msg) {
    auto* api = get_global_host_api();
    if (api && api->log_warning) api->log_warning(api, msg);
}
inline void script_log_error(const char* msg) {
    auto* api = get_global_host_api();
    if (api && api->log_error) api->log_error(api, msg);
}

// Macro to register a component script for DLL export
// Usage: Place REGISTER_COMPONENT_SCRIPT(MyScript) in your .cpp file
// The script will automatically appear in the editor's "Add Script" list
#define REGISTER_COMPONENT_SCRIPT(ClassName) \
    static ::runtime::ScriptAutoRegistrar g_register_##ClassName( \
        #ClassName, \
        []() -> ::runtime::ComponentScript* { return new ClassName(); } \
    );

}
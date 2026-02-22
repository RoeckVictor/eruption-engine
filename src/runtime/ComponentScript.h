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
}

namespace runtime {

/// Simple 2D vector for script API return values.
struct Vec2 { float x, y; };

/// Shared API struct passed from host (editor EXE) to scripts.
/// Contains per-frame state values, host context pointers, and function pointers
/// for host-side services.  One instance is created by RuntimeContext and shared
/// by all scripts.
///
/// Every callback receives a pointer back to this struct so it can recover the
/// host context (Engine, RuntimeContext) without relying on global state.
struct ScriptHostAPI {
    // --- Host Context (opaque pointers set by RuntimeContext) ---
    void* engine_ctx  = nullptr;  // engine::Engine*
    void* runtime_ctx = nullptr;  // editor::RuntimeContext*

    // --- Frame State (updated by host each frame) ---
    float delta_time = 0.0f;
    float fixed_delta_time = 1.0f / 60.0f;
    float time = 0.0f;
    uint64_t frame_count = 0;

    // --- Input ---
    bool (*is_key_held)(ScriptHostAPI*, int key) = nullptr;
    bool (*is_key_pressed)(ScriptHostAPI*, int key) = nullptr;
    bool (*is_key_released)(ScriptHostAPI*, int key) = nullptr;
    bool (*is_mouse_held)(ScriptHostAPI*, int button) = nullptr;
    bool (*is_mouse_pressed)(ScriptHostAPI*, int button) = nullptr;
    double (*get_mouse_x)(ScriptHostAPI*) = nullptr;
    double (*get_mouse_y)(ScriptHostAPI*) = nullptr;

    // --- Logging ---
    void (*log_info)(ScriptHostAPI*, const char* msg) = nullptr;
    void (*log_warning)(ScriptHostAPI*, const char* msg) = nullptr;
    void (*log_error)(ScriptHostAPI*, const char* msg) = nullptr;

    // --- Physics (registry + entity so host finds Rigidbody internally) ---
    void (*get_velocity)(ScriptHostAPI*, entt::registry*, entt::entity, float* vx, float* vy) = nullptr;
    void (*set_velocity)(ScriptHostAPI*, entt::registry*, entt::entity, float vx, float vy) = nullptr;
    void (*add_force)(ScriptHostAPI*, entt::registry*, entt::entity, float fx, float fy) = nullptr;
    void (*add_impulse)(ScriptHostAPI*, entt::registry*, entt::entity, float ix, float iy) = nullptr;
    bool (*is_grounded)(ScriptHostAPI*, entt::registry*, entt::entity, float tolerance) = nullptr;

    // --- Entity Operations ---
    entt::entity (*find_entity_by_name)(ScriptHostAPI*, entt::registry*, const char* name) = nullptr;
    void (*destroy_entity)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;

    // --- Component Access ---
    void* (*get_component)(ScriptHostAPI*, entt::registry*, entt::entity, entt::id_type) = nullptr;

    // --- Component Manipulation ---
    void* (*add_component)(ScriptHostAPI*, entt::registry*, entt::entity, entt::id_type) = nullptr;
    void (*remove_component)(ScriptHostAPI*, entt::registry*, entt::entity, entt::id_type) = nullptr;

    // --- Prefab Instantiation ---
    entt::entity (*instantiate_prefab)(ScriptHostAPI*, entt::registry*, const char* prefab_name) = nullptr;

    // --- Events ---
    EventHandle (*subscribe_event)(ScriptHostAPI*, entt::entity owner, const char* event_name, void* callback) = nullptr;
    void (*unsubscribe_event)(ScriptHostAPI*, EventHandle handle) = nullptr;
    void (*dispatch_event)(ScriptHostAPI*, const char* event_name, const EventData* data) = nullptr;

    // --- Coroutines ---
    CoroutineHandle (*start_coroutine)(ScriptHostAPI*, entt::entity owner, void* coro_handle) = nullptr;
    void (*stop_coroutine)(ScriptHostAPI*, CoroutineHandle handle) = nullptr;
    void (*stop_all_coroutines)(ScriptHostAPI*, entt::entity owner) = nullptr;
    bool (*is_coroutine_running)(ScriptHostAPI*, CoroutineHandle handle) = nullptr;

    // --- Random ---
    float (*random_float)(ScriptHostAPI*) = nullptr;
    float (*random_range)(ScriptHostAPI*, float min, float max) = nullptr;
    int (*random_int)(ScriptHostAPI*, int min, int max) = nullptr;

    // --- Camera ---
    void (*get_camera_position)(ScriptHostAPI*, float* x, float* y) = nullptr;
    void (*set_camera_position)(ScriptHostAPI*, float x, float y) = nullptr;
    float (*get_camera_zoom)(ScriptHostAPI*) = nullptr;
    void (*set_camera_zoom)(ScriptHostAPI*, float zoom) = nullptr;
    void (*screen_to_world)(ScriptHostAPI*, float screen_x, float screen_y, float* world_x, float* world_y) = nullptr;
    void (*world_to_screen)(ScriptHostAPI*, float world_x, float world_y, float* screen_x, float* screen_y) = nullptr;

    // --- Hierarchy ---
    entt::entity (*get_parent)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    void (*set_parent)(ScriptHostAPI*, entt::registry*, entt::entity child, entt::entity parent) = nullptr;
    int (*get_child_count)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    entt::entity (*get_child)(ScriptHostAPI*, entt::registry*, entt::entity parent, int index) = nullptr;

    // --- Entity Info ---
    const char* (*get_entity_name)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    bool (*is_entity_active)(ScriptHostAPI*, entt::registry*, entt::entity) = nullptr;
    void (*set_entity_active)(ScriptHostAPI*, entt::registry*, entt::entity, bool active) = nullptr;
};

// Forward declaration for global logging API (defined after ComponentScript)
inline ScriptHostAPI*& get_global_host_api();

/// Base class for user-defined component scripts.
/// Scripts can be attached to entities to add custom behavior.
/// Scripts are hot-reloadable when compiled as a DLL.
class ComponentScript {
public:
    virtual ~ComponentScript() = default;

    // =====================================================================
    // Lifecycle Events (override these in your script)
    // =====================================================================

    /// Called when the script is first created/attached.
    virtual void on_create() {}

    /// Called when the script is about to be destroyed.
    virtual void on_destroy() {}

    /// Called when the entity is enabled.
    virtual void on_enable() {}

    /// Called when the entity is disabled.
    virtual void on_disable() {}

    // =====================================================================
    // Update Events (override these in your script)
    // =====================================================================

    /// Called every frame. Use delta_time() for frame timing.
    virtual void on_update() {}

    /// Called at a fixed timestep (for physics). Use fixed_delta_time() for the step size.
    virtual void on_fixed_update() {}

    /// Called after all on_update() calls have finished.
    virtual void on_late_update() {}

    // =====================================================================
    // Physics Events (override these for collision/trigger handling)
    // =====================================================================

    /// Called when this entity first contacts another collider.
    virtual void on_collision_enter(const CollisionInfo& info) { (void)info; }

    /// Called every frame while this entity is in contact with another collider.
    virtual void on_collision_stay(const CollisionInfo& info) { (void)info; }

    /// Called when this entity stops contacting another collider.
    virtual void on_collision_exit(const CollisionInfo& info) { (void)info; }

    /// Called when this entity first enters a trigger volume.
    virtual void on_trigger_enter(const CollisionInfo& info) { (void)info; }

    /// Called every frame while this entity is inside a trigger volume.
    virtual void on_trigger_stay(const CollisionInfo& info) { (void)info; }

    /// Called when this entity exits a trigger volume.
    virtual void on_trigger_exit(const CollisionInfo& info) { (void)info; }

    // =====================================================================
    // Rendering (future hooks — not yet wired)
    // =====================================================================

    /// Called during rendering phase (not yet wired — future hook).
    virtual void on_render() {}

    // =====================================================================
    // Editor Hooks (override these for editor integration)
    // =====================================================================

    /// Custom inspector GUI. Called in both edit mode and play mode.
    /// @param properties JSON object storing this script's properties (read/write).
    /// In edit mode, changes are persisted to the scene.
    /// In play mode, changes apply to the live instance but are discarded on stop.
    virtual void on_inspector_gui(nlohmann::json& properties) { (void)properties; }

    /// Custom gizmo rendering in viewport (not yet wired — future hook).
    virtual void on_gizmo() {}

    // =====================================================================
    // Property Serialization (override these for save/load support)
    // =====================================================================

    /// Serialize script properties to JSON (for scene/prefab saving).
    /// Override this to save your script's custom properties.
    virtual void serialize_properties(nlohmann::json& out) const { (void)out; }

    /// Deserialize script properties from JSON (for scene/prefab loading).
    /// Override this to restore your script's custom properties.
    virtual void deserialize_properties(const nlohmann::json& data) { (void)data; }

    // =====================================================================
    // Time
    // =====================================================================

    /// Frame delta time in seconds.
    float delta_time() const { return m_host_api ? m_host_api->delta_time : 0.0f; }

    /// Fixed timestep value in seconds (default 1/60).
    float fixed_delta_time() const { return m_host_api ? m_host_api->fixed_delta_time : (1.0f / 60.0f); }

    /// Total time since play started in seconds.
    float time() const { return m_host_api ? m_host_api->time : 0.0f; }

    /// Total frames since play started.
    uint64_t frame_count() const { return m_host_api ? m_host_api->frame_count : 0; }

    // =====================================================================
    // Input
    // =====================================================================

    /// Check if a key is currently held down.
    bool is_key_held(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_held &&
               m_host_api->is_key_held(m_host_api, static_cast<int>(key));
    }

    /// Check if a key was just pressed this frame.
    bool is_key_pressed(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_pressed &&
               m_host_api->is_key_pressed(m_host_api, static_cast<int>(key));
    }

    /// Check if a key was just released this frame.
    bool is_key_released(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_released &&
               m_host_api->is_key_released(m_host_api, static_cast<int>(key));
    }

    /// Check if a mouse button is currently held down.
    bool is_mouse_held(engine::platform::MouseButton btn) const {
        return m_host_api && m_host_api->is_mouse_held &&
               m_host_api->is_mouse_held(m_host_api, static_cast<int>(btn));
    }

    /// Check if a mouse button was just pressed this frame.
    bool is_mouse_pressed(engine::platform::MouseButton btn) const {
        return m_host_api && m_host_api->is_mouse_pressed &&
               m_host_api->is_mouse_pressed(m_host_api, static_cast<int>(btn));
    }

    /// Get the mouse X position in screen coordinates.
    double mouse_x() const {
        return (m_host_api && m_host_api->get_mouse_x) ? m_host_api->get_mouse_x(m_host_api) : 0.0;
    }

    /// Get the mouse Y position in screen coordinates.
    double mouse_y() const {
        return (m_host_api && m_host_api->get_mouse_y) ? m_host_api->get_mouse_y(m_host_api) : 0.0;
    }

    // =====================================================================
    // Transform Shortcuts
    // =====================================================================

    /// Get the entity's position.
    Vec2 get_position() {
        auto* t = get_component<engine::Transform>();
        return t ? Vec2{t->x, t->y} : Vec2{0.0f, 0.0f};
    }

    /// Set the entity's position.
    void set_position(float x, float y) {
        if (auto* t = get_component<engine::Transform>()) { t->x = x; t->y = y; }
    }

    /// Move the entity by a delta.
    void translate(float dx, float dy) {
        if (auto* t = get_component<engine::Transform>()) { t->x += dx; t->y += dy; }
    }

    /// Get the entity's rotation in degrees.
    float get_rotation() {
        auto* t = get_component<engine::Transform>();
        return t ? t->rotation : 0.0f;
    }

    /// Set the entity's rotation in degrees.
    void set_rotation(float angle) {
        if (auto* t = get_component<engine::Transform>()) t->rotation = angle;
    }

    /// Rotate the entity by a delta in degrees.
    void rotate(float delta) {
        if (auto* t = get_component<engine::Transform>()) t->rotation += delta;
    }

    /// Get the entity's scale.
    Vec2 get_scale() {
        auto* t = get_component<engine::Transform>();
        return t ? Vec2{t->scale_x, t->scale_y} : Vec2{1.0f, 1.0f};
    }

    /// Set the entity's scale.
    void set_scale(float sx, float sy) {
        if (auto* t = get_component<engine::Transform>()) { t->scale_x = sx; t->scale_y = sy; }
    }

    // =====================================================================
    // Logging
    // =====================================================================

    /// Log an info message to the editor Console.
    void log(const char* msg) {
        if (m_host_api && m_host_api->log_info) m_host_api->log_info(m_host_api, msg);
    }

    /// Log a warning message to the editor Console.
    void log_warning(const char* msg) {
        if (m_host_api && m_host_api->log_warning) m_host_api->log_warning(m_host_api, msg);
    }

    /// Log an error message to the editor Console.
    void log_error(const char* msg) {
        if (m_host_api && m_host_api->log_error) m_host_api->log_error(m_host_api, msg);
    }

    // =====================================================================
    // Physics
    // =====================================================================

    /// Get the entity's linear velocity in pixels/sec (requires Rigidbody).
    Vec2 get_velocity() {
        Vec2 v{0.0f, 0.0f};
        if (m_host_api && m_host_api->get_velocity)
            m_host_api->get_velocity(m_host_api, m_registry, m_entity, &v.x, &v.y);
        return v;
    }

    /// Set the entity's linear velocity in pixels/sec (requires Rigidbody).
    void set_velocity(float vx, float vy) {
        if (m_host_api && m_host_api->set_velocity)
            m_host_api->set_velocity(m_host_api, m_registry, m_entity, vx, vy);
    }

    /// Apply a force in pixels/sec^2 (requires Rigidbody, applied over time).
    void add_force(float fx, float fy) {
        if (m_host_api && m_host_api->add_force)
            m_host_api->add_force(m_host_api, m_registry, m_entity, fx, fy);
    }

    /// Apply an instantaneous impulse in pixels/sec (requires Rigidbody).
    void add_impulse(float ix, float iy) {
        if (m_host_api && m_host_api->add_impulse)
            m_host_api->add_impulse(m_host_api, m_registry, m_entity, ix, iy);
    }

    /// Check if the entity is grounded (requires Rigidbody).
    /// @param tolerance Distance in pixels to check below the body (default 2.0).
    bool is_grounded(float tolerance = 2.0f) {
        if (m_host_api && m_host_api->is_grounded)
            return m_host_api->is_grounded(m_host_api, m_registry, m_entity, tolerance);
        return false;
    }

    // =====================================================================
    // Entity Operations
    // =====================================================================

    /// Find an entity by name. Returns entt::null if not found.
    entt::entity find_entity(const char* name) {
        if (m_host_api && m_host_api->find_entity_by_name)
            return m_host_api->find_entity_by_name(m_host_api, m_registry, name);
        return entt::null;
    }

    /// Destroy this entity (deferred to end of frame).
    void destroy_self() {
        if (m_host_api && m_host_api->destroy_entity)
            m_host_api->destroy_entity(m_host_api, m_registry, m_entity);
    }

    /// Destroy another entity (deferred to end of frame).
    void destroy_entity(entt::entity target) {
        if (m_host_api && m_host_api->destroy_entity)
            m_host_api->destroy_entity(m_host_api, m_registry, target);
    }

    // =====================================================================
    // Core Accessors
    // =====================================================================

    /// Get the entity this script is attached to.
    entt::entity entity() const { return m_entity; }

    /// Get the registry.
    entt::registry* registry() { return m_registry; }
    const entt::registry* registry() const { return m_registry; }

    /// Get the engine instance.
    engine::Engine* engine() { return m_engine; }
    const engine::Engine* engine() const { return m_engine; }

    /// Get the script's type name (for serialization).
    virtual const char* type_name() const = 0;

    // =====================================================================
    // Component Helpers
    // =====================================================================
    // These use a host-side callback to avoid instantiating entt templates in the DLL,
    // which would cause type ID mismatches across DLL boundaries.

    /// Get a component from this entity.
    template<typename T>
    T* get_component() {
        if (!m_host_api || !m_host_api->get_component || !m_registry) return nullptr;
        return static_cast<T*>(m_host_api->get_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }

    /// Get a component from this entity (const version).
    template<typename T>
    const T* get_component() const {
        if (!m_host_api || !m_host_api->get_component || !m_registry) return nullptr;
        return static_cast<const T*>(m_host_api->get_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }

    /// Check if this entity has a component.
    template<typename T>
    bool has_component() const {
        return get_component<T>() != nullptr;
    }

    /// Add a component to this entity (default-constructed). Returns pointer to the new component.
    /// If the component already exists, returns a pointer to the existing one.
    template<typename T>
    T* add_component() {
        if (!m_host_api || !m_host_api->add_component || !m_registry) return nullptr;
        return static_cast<T*>(m_host_api->add_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }

    /// Remove a component from this entity. No-op if the component doesn't exist.
    template<typename T>
    void remove_component() {
        if (!m_host_api || !m_host_api->remove_component || !m_registry) return;
        m_host_api->remove_component(
            m_host_api, m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value());
    }

    // =====================================================================
    // Prefab Instantiation
    // =====================================================================

    /// Instantiate a prefab by name, creating a new entity with all its components.
    /// Looks for <name>.prefab in the project Assets folder.
    /// Returns the new entity, or entt::null on failure.
    entt::entity instantiate(const char* prefab_name) {
        if (!m_host_api || !m_host_api->instantiate_prefab || !m_registry) return entt::null;
        return m_host_api->instantiate_prefab(m_host_api, m_registry, prefab_name);
    }

    // =====================================================================
    // Math Helpers (inline, no DLL boundary crossing)
    // =====================================================================

    /// Linear interpolation between two values.
    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    /// Inverse linear interpolation: find t such that lerp(a, b, t) = value.
    static float inverse_lerp(float a, float b, float value) {
        if (std::abs(b - a) < 0.0001f) return 0.0f;
        return (value - a) / (b - a);
    }

    /// Clamp a value between min and max.
    static float clamp(float value, float min, float max) {
        return value < min ? min : (value > max ? max : value);
    }

    /// Clamp a value between 0 and 1.
    static float clamp01(float value) {
        return clamp(value, 0.0f, 1.0f);
    }

    /// Smooth interpolation using smoothstep curve.
    static float smoothstep(float edge0, float edge1, float x) {
        float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    /// Move towards a target value by at most max_delta.
    static float move_towards(float current, float target, float max_delta) {
        float diff = target - current;
        if (std::abs(diff) <= max_delta) return target;
        return current + std::copysign(max_delta, diff);
    }

    /// Rotate towards a target angle (handles 360 degree wrapping).
    static float rotate_towards(float current, float target, float max_delta) {
        float diff = angle_difference(target, current);
        if (std::abs(diff) <= max_delta) return target;
        return current + std::copysign(max_delta, diff);
    }

    /// Calculate the shortest difference between two angles in degrees.
    static float angle_difference(float a, float b) {
        float diff = std::fmod(a - b + 180.0f, 360.0f) - 180.0f;
        return diff < -180.0f ? diff + 360.0f : diff;
    }

    /// Get the sign of a value (-1, 0, or 1).
    static float sign(float value) {
        return static_cast<float>((value > 0.0f) - (value < 0.0f));
    }

    /// Linear interpolation between two Vec2 values.
    static Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
        return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
    }

    /// Distance between two Vec2 points.
    static float vec2_distance(Vec2 a, Vec2 b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    /// Magnitude (length) of a Vec2.
    static float vec2_magnitude(Vec2 v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }

    /// Squared magnitude of a Vec2 (faster than magnitude, good for comparisons).
    static float vec2_sqr_magnitude(Vec2 v) {
        return v.x * v.x + v.y * v.y;
    }

    /// Normalize a Vec2 to unit length.
    static Vec2 vec2_normalize(Vec2 v) {
        float len = vec2_magnitude(v);
        return len > 0.0001f ? Vec2{v.x / len, v.y / len} : Vec2{0.0f, 0.0f};
    }

    /// Dot product of two Vec2 values.
    static float vec2_dot(Vec2 a, Vec2 b) {
        return a.x * b.x + a.y * b.y;
    }

    /// Move a Vec2 towards a target by at most max_delta distance.
    static Vec2 vec2_move_towards(Vec2 current, Vec2 target, float max_delta) {
        Vec2 diff = {target.x - current.x, target.y - current.y};
        float dist = vec2_magnitude(diff);
        if (dist <= max_delta || dist < 0.0001f) return target;
        return {current.x + diff.x / dist * max_delta, current.y + diff.y / dist * max_delta};
    }

    // =====================================================================
    // Coroutines
    // =====================================================================

    /// Start a coroutine. Returns a handle that can be used to stop it.
    CoroutineHandle start_coroutine(Coroutine coro) {
        if (!m_host_api || !m_host_api->start_coroutine) return 0;
        auto handle = coro.release();
        return m_host_api->start_coroutine(m_host_api, m_entity, handle.address());
    }

    /// Stop a running coroutine by handle.
    void stop_coroutine(CoroutineHandle handle) {
        if (m_host_api && m_host_api->stop_coroutine)
            m_host_api->stop_coroutine(m_host_api, handle);
    }

    /// Stop all coroutines owned by this script's entity.
    void stop_all_coroutines() {
        if (m_host_api && m_host_api->stop_all_coroutines)
            m_host_api->stop_all_coroutines(m_host_api, m_entity);
    }

    /// Check if a coroutine is still running.
    bool is_coroutine_running(CoroutineHandle handle) const {
        if (!m_host_api || !m_host_api->is_coroutine_running) return false;
        return m_host_api->is_coroutine_running(m_host_api, handle);
    }

    // =====================================================================
    // Custom Events
    // =====================================================================

    /// Subscribe to a custom event by name. Returns a handle for unsubscribing.
    /// The callback will be invoked when dispatch_event is called with matching name.
    EventHandle subscribe(const char* event_name, void (*callback)(const EventData&)) {
        if (!m_host_api || !m_host_api->subscribe_event) return 0;
        return m_host_api->subscribe_event(m_host_api, m_entity, event_name, reinterpret_cast<void*>(callback));
    }

    /// Unsubscribe from an event using the handle returned from subscribe().
    void unsubscribe(EventHandle handle) {
        if (m_host_api && m_host_api->unsubscribe_event)
            m_host_api->unsubscribe_event(m_host_api, handle);
    }

    /// Dispatch a custom event to all subscribers.
    void dispatch_event(const char* event_name, const EventData& data) {
        if (m_host_api && m_host_api->dispatch_event)
            m_host_api->dispatch_event(m_host_api, event_name, &data);
    }

    /// Dispatch a custom event with no data.
    void dispatch_event(const char* event_name) {
        EventData empty;
        dispatch_event(event_name, empty);
    }

    // =====================================================================
    // Random
    // =====================================================================

    /// Get a random float between 0.0 and 1.0.
    float random_float() {
        if (!m_host_api || !m_host_api->random_float) return 0.0f;
        return m_host_api->random_float(m_host_api);
    }

    /// Get a random float in the range [min, max].
    float random_range(float min, float max) {
        if (!m_host_api || !m_host_api->random_range) return min;
        return m_host_api->random_range(m_host_api, min, max);
    }

    /// Get a random integer in the range [min, max] (inclusive).
    int random_int(int min, int max) {
        if (!m_host_api || !m_host_api->random_int) return min;
        return m_host_api->random_int(m_host_api, min, max);
    }

    /// Get a random point inside a circle of the given radius centered at origin.
    Vec2 random_point_in_circle(float radius) {
        float angle = random_range(0.0f, 6.28318530718f);
        float r = radius * std::sqrt(random_float());
        return {r * std::cos(angle), r * std::sin(angle)};
    }

    /// Get a random unit direction vector.
    Vec2 random_direction() {
        float angle = random_range(0.0f, 6.28318530718f);
        return {std::cos(angle), std::sin(angle)};
    }

    // =====================================================================
    // Camera
    // =====================================================================

    /// Get the camera's world position.
    Vec2 get_camera_position() {
        Vec2 pos{0.0f, 0.0f};
        if (m_host_api && m_host_api->get_camera_position)
            m_host_api->get_camera_position(m_host_api, &pos.x, &pos.y);
        return pos;
    }

    /// Set the camera's world position.
    void set_camera_position(float x, float y) {
        if (m_host_api && m_host_api->set_camera_position)
            m_host_api->set_camera_position(m_host_api, x, y);
    }

    /// Get the camera's zoom level.
    float get_camera_zoom() {
        if (!m_host_api || !m_host_api->get_camera_zoom) return 1.0f;
        return m_host_api->get_camera_zoom(m_host_api);
    }

    /// Set the camera's zoom level.
    void set_camera_zoom(float zoom) {
        if (m_host_api && m_host_api->set_camera_zoom)
            m_host_api->set_camera_zoom(m_host_api, zoom);
    }

    /// Convert screen coordinates to world coordinates.
    Vec2 screen_to_world(float screen_x, float screen_y) {
        Vec2 world{0.0f, 0.0f};
        if (m_host_api && m_host_api->screen_to_world)
            m_host_api->screen_to_world(m_host_api, screen_x, screen_y, &world.x, &world.y);
        return world;
    }

    /// Convert world coordinates to screen coordinates.
    Vec2 world_to_screen(float world_x, float world_y) {
        Vec2 screen{0.0f, 0.0f};
        if (m_host_api && m_host_api->world_to_screen)
            m_host_api->world_to_screen(m_host_api, world_x, world_y, &screen.x, &screen.y);
        return screen;
    }

    // =====================================================================
    // Hierarchy
    // =====================================================================

    /// Get this entity's parent, or entt::null if it has no parent.
    entt::entity get_parent() {
        if (!m_host_api || !m_host_api->get_parent || !m_registry) return entt::null;
        return m_host_api->get_parent(m_host_api, m_registry, m_entity);
    }

    /// Set this entity's parent. Pass entt::null to detach from parent.
    void set_parent(entt::entity parent) {
        if (m_host_api && m_host_api->set_parent && m_registry)
            m_host_api->set_parent(m_host_api, m_registry, m_entity, parent);
    }

    /// Detach this entity from its parent.
    void detach_from_parent() {
        set_parent(entt::null);
    }

    /// Get the number of children this entity has.
    int get_child_count() {
        if (!m_host_api || !m_host_api->get_child_count || !m_registry) return 0;
        return m_host_api->get_child_count(m_host_api, m_registry, m_entity);
    }

    /// Get a child entity by index.
    entt::entity get_child(int index) {
        if (!m_host_api || !m_host_api->get_child || !m_registry) return entt::null;
        return m_host_api->get_child(m_host_api, m_registry, m_entity, index);
    }

    // =====================================================================
    // Entity Info
    // =====================================================================

    /// Get this entity's name.
    /// NOTE: The returned pointer is valid until the next call to get_name() on THIS script.
    /// Copy the result to a std::string if you need to store it.
    const char* get_name() {
        if (!m_host_api || !m_host_api->get_entity_name || !m_registry) return "";
        // Copy to local cache to avoid issues with thread-local buffer in host
        const char* name = m_host_api->get_entity_name(m_host_api, m_registry, m_entity);
        m_name_cache = name ? name : "";
        return m_name_cache.c_str();
    }

    /// Check if this entity is active (enabled in hierarchy).
    bool is_active() {
        if (!m_host_api || !m_host_api->is_entity_active || !m_registry) return true;
        return m_host_api->is_entity_active(m_host_api, m_registry, m_entity);
    }

    /// Set this entity's active state.
    void set_active(bool active) {
        if (m_host_api && m_host_api->set_entity_active && m_registry)
            m_host_api->set_entity_active(m_host_api, m_registry, m_entity, active);
    }

protected:
    friend class ScriptSystem;
    friend class ScriptManager;
    friend class editor::RuntimeContext;

    entt::entity m_entity = entt::null;
    entt::registry* m_registry = nullptr;
    engine::Engine* m_engine = nullptr;
    ScriptHostAPI* m_host_api = nullptr;
    mutable std::string m_name_cache;  // Cache for get_name() to avoid buffer issues

    /// Initialize the script context (called by ScriptSystem/RuntimeContext).
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

/// Factory function type for creating script instances.
using ScriptFactory = ComponentScript* (*)();

/// Auto-registration entry for component scripts.
struct ScriptRegistryEntry {
    const char* name;
    ScriptFactory factory;
};

/// Global auto-registration list. Scripts register themselves via REGISTER_COMPONENT_SCRIPT.
/// This works within a DLL because static initializers execute at DLL load time.
inline std::vector<ScriptRegistryEntry>& get_script_registry() {
    static std::vector<ScriptRegistryEntry> registry;
    return registry;
}

/// Helper struct for static-initializer-based registration.
struct ScriptAutoRegistrar {
    ScriptAutoRegistrar(const char* name, ScriptFactory factory) {
        get_script_registry().push_back({name, factory});
    }
};

// ============================================================================
// Global Script Logging API
// ============================================================================
// These free functions can be called from anywhere, including static callbacks.
// They use a global host API pointer that is set when scripts are initialized.

/// Global host API pointer for static logging functions.
/// Set automatically when any script is initialized.
inline ScriptHostAPI*& get_global_host_api() {
    static ScriptHostAPI* s_host_api = nullptr;
    return s_host_api;
}

/// Log an info message (usable from static callbacks or anywhere).
inline void script_log(const char* msg) {
    auto* api = get_global_host_api();
    if (api && api->log_info) api->log_info(api, msg);
}

/// Log a warning message (usable from static callbacks or anywhere).
inline void script_log_warning(const char* msg) {
    auto* api = get_global_host_api();
    if (api && api->log_warning) api->log_warning(api, msg);
}

/// Log an error message (usable from static callbacks or anywhere).
inline void script_log_error(const char* msg) {
    auto* api = get_global_host_api();
    if (api && api->log_error) api->log_error(api, msg);
}

/// Macro to register a component script for DLL export.
/// Usage: Place REGISTER_COMPONENT_SCRIPT(MyScript) in your .cpp file.
/// The script will automatically appear in the editor's "Add Script" list.
#define REGISTER_COMPONENT_SCRIPT(ClassName) \
    static ::runtime::ScriptAutoRegistrar g_register_##ClassName( \
        #ClassName, \
        []() -> ::runtime::ComponentScript* { return new ClassName(); } \
    );

} // namespace runtime

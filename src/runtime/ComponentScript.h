#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>

#include "engine/platform/KeyCode.h"
#include "engine/core/Transform.h"

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
/// Contains per-frame state values and function pointers for host-side services.
/// One instance is created by RuntimeContext and shared by all scripts.
struct ScriptHostAPI {
    // --- Frame State (updated by host each frame) ---
    float delta_time = 0.0f;
    float fixed_delta_time = 1.0f / 60.0f;
    float time = 0.0f;
    uint64_t frame_count = 0;

    // --- Input ---
    bool (*is_key_held)(int key) = nullptr;
    bool (*is_key_pressed)(int key) = nullptr;
    bool (*is_key_released)(int key) = nullptr;
    bool (*is_mouse_held)(int button) = nullptr;
    bool (*is_mouse_pressed)(int button) = nullptr;
    double (*get_mouse_x)() = nullptr;
    double (*get_mouse_y)() = nullptr;

    // --- Logging ---
    void (*log_info)(const char* msg) = nullptr;
    void (*log_warning)(const char* msg) = nullptr;
    void (*log_error)(const char* msg) = nullptr;

    // --- Physics (registry + entity so host finds Rigidbody internally) ---
    void (*get_velocity)(entt::registry*, entt::entity, float* vx, float* vy) = nullptr;
    void (*set_velocity)(entt::registry*, entt::entity, float vx, float vy) = nullptr;
    void (*add_force)(entt::registry*, entt::entity, float fx, float fy) = nullptr;
    void (*add_impulse)(entt::registry*, entt::entity, float ix, float iy) = nullptr;

    // --- Entity Operations ---
    entt::entity (*find_entity_by_name)(entt::registry*, const char* name) = nullptr;
    void (*destroy_entity)(entt::registry*, entt::entity) = nullptr;

    // --- Component Access ---
    void* (*get_component)(entt::registry*, entt::entity, entt::id_type) = nullptr;

    // --- Component Manipulation ---
    void* (*add_component)(entt::registry*, entt::entity, entt::id_type) = nullptr;
    void (*remove_component)(entt::registry*, entt::entity, entt::id_type) = nullptr;

    // --- Prefab Instantiation ---
    entt::entity (*instantiate_prefab)(entt::registry*, const char* prefab_name) = nullptr;
};

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
    // Rendering (future hooks — not yet wired)
    // =====================================================================

    /// Called during rendering phase (not yet wired — future hook).
    virtual void on_render() {}

    // =====================================================================
    // Editor Hooks (override these for editor integration)
    // =====================================================================

    /// Custom inspector GUI (called in editor only).
    virtual void on_inspector_gui() {}

    /// Custom gizmo rendering in viewport (not yet wired — future hook).
    virtual void on_gizmo() {}

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
        return m_host_api && m_host_api->is_key_held && m_host_api->is_key_held(static_cast<int>(key));
    }

    /// Check if a key was just pressed this frame.
    bool is_key_pressed(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_pressed && m_host_api->is_key_pressed(static_cast<int>(key));
    }

    /// Check if a key was just released this frame.
    bool is_key_released(engine::platform::KeyCode key) const {
        return m_host_api && m_host_api->is_key_released && m_host_api->is_key_released(static_cast<int>(key));
    }

    /// Check if a mouse button is currently held down.
    bool is_mouse_held(engine::platform::MouseButton btn) const {
        return m_host_api && m_host_api->is_mouse_held && m_host_api->is_mouse_held(static_cast<int>(btn));
    }

    /// Check if a mouse button was just pressed this frame.
    bool is_mouse_pressed(engine::platform::MouseButton btn) const {
        return m_host_api && m_host_api->is_mouse_pressed && m_host_api->is_mouse_pressed(static_cast<int>(btn));
    }

    /// Get the mouse X position in screen coordinates.
    double mouse_x() const {
        return (m_host_api && m_host_api->get_mouse_x) ? m_host_api->get_mouse_x() : 0.0;
    }

    /// Get the mouse Y position in screen coordinates.
    double mouse_y() const {
        return (m_host_api && m_host_api->get_mouse_y) ? m_host_api->get_mouse_y() : 0.0;
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
        if (m_host_api && m_host_api->log_info) m_host_api->log_info(msg);
    }

    /// Log a warning message to the editor Console.
    void log_warning(const char* msg) {
        if (m_host_api && m_host_api->log_warning) m_host_api->log_warning(msg);
    }

    /// Log an error message to the editor Console.
    void log_error(const char* msg) {
        if (m_host_api && m_host_api->log_error) m_host_api->log_error(msg);
    }

    // =====================================================================
    // Physics
    // =====================================================================

    /// Get the entity's linear velocity in pixels/sec (requires Rigidbody).
    Vec2 get_velocity() {
        Vec2 v{0.0f, 0.0f};
        if (m_host_api && m_host_api->get_velocity)
            m_host_api->get_velocity(m_registry, m_entity, &v.x, &v.y);
        return v;
    }

    /// Set the entity's linear velocity in pixels/sec (requires Rigidbody).
    void set_velocity(float vx, float vy) {
        if (m_host_api && m_host_api->set_velocity)
            m_host_api->set_velocity(m_registry, m_entity, vx, vy);
    }

    /// Apply a force in pixels/sec^2 (requires Rigidbody, applied over time).
    void add_force(float fx, float fy) {
        if (m_host_api && m_host_api->add_force)
            m_host_api->add_force(m_registry, m_entity, fx, fy);
    }

    /// Apply an instantaneous impulse in pixels/sec (requires Rigidbody).
    void add_impulse(float ix, float iy) {
        if (m_host_api && m_host_api->add_impulse)
            m_host_api->add_impulse(m_registry, m_entity, ix, iy);
    }

    // =====================================================================
    // Entity Operations
    // =====================================================================

    /// Find an entity by name. Returns entt::null if not found.
    entt::entity find_entity(const char* name) {
        if (m_host_api && m_host_api->find_entity_by_name)
            return m_host_api->find_entity_by_name(m_registry, name);
        return entt::null;
    }

    /// Destroy this entity (deferred to end of frame).
    void destroy_self() {
        if (m_host_api && m_host_api->destroy_entity)
            m_host_api->destroy_entity(m_registry, m_entity);
    }

    /// Destroy another entity (deferred to end of frame).
    void destroy_entity(entt::entity target) {
        if (m_host_api && m_host_api->destroy_entity)
            m_host_api->destroy_entity(m_registry, target);
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
            m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }

    /// Get a component from this entity (const version).
    template<typename T>
    const T* get_component() const {
        if (!m_host_api || !m_host_api->get_component || !m_registry) return nullptr;
        return static_cast<const T*>(m_host_api->get_component(
            m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
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
            m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value()));
    }

    /// Remove a component from this entity. No-op if the component doesn't exist.
    template<typename T>
    void remove_component() {
        if (!m_host_api || !m_host_api->remove_component || !m_registry) return;
        m_host_api->remove_component(
            m_registry, m_entity, entt::type_hash<std::remove_const_t<T>>::value());
    }

    // =====================================================================
    // Prefab Instantiation
    // =====================================================================

    /// Instantiate a prefab by name, creating a new entity with all its components.
    /// Looks for <name>.prefab in the project Assets folder.
    /// Returns the new entity, or entt::null on failure.
    entt::entity instantiate(const char* prefab_name) {
        if (!m_host_api || !m_host_api->instantiate_prefab || !m_registry) return entt::null;
        return m_host_api->instantiate_prefab(m_registry, prefab_name);
    }

protected:
    friend class ScriptSystem;
    friend class ScriptManager;
    friend class editor::RuntimeContext;

    entt::entity m_entity = entt::null;
    entt::registry* m_registry = nullptr;
    engine::Engine* m_engine = nullptr;
    ScriptHostAPI* m_host_api = nullptr;

    /// Initialize the script context (called by ScriptSystem/RuntimeContext).
    void init_context(entt::entity entity, entt::registry* registry, engine::Engine* engine,
                      ScriptHostAPI* host_api = nullptr) {
        m_entity = entity;
        m_registry = registry;
        m_engine = engine;
        m_host_api = host_api;
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

/// Macro to register a component script for DLL export.
/// Usage: Place REGISTER_COMPONENT_SCRIPT(MyScript) in your .cpp file.
/// The script will automatically appear in the editor's "Add Script" list.
#define REGISTER_COMPONENT_SCRIPT(ClassName) \
    static ::runtime::ScriptAutoRegistrar g_register_##ClassName( \
        #ClassName, \
        []() -> ::runtime::ComponentScript* { return new ClassName(); } \
    );

} // namespace runtime

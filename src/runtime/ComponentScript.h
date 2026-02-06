#pragma once

#include <entt/entt.hpp>
#include <string>

namespace engine {
class Engine;
}

namespace runtime {

/// Base class for user-defined component scripts.
/// Scripts can be attached to entities to add custom behavior.
/// Scripts are hot-reloadable when compiled as a DLL.
class ComponentScript {
public:
    virtual ~ComponentScript() = default;

    // --- Lifecycle Events ---

    /// Called when the script is first created/attached.
    virtual void on_create() {}

    /// Called when the script is about to be destroyed.
    virtual void on_destroy() {}

    /// Called when the entity is enabled.
    virtual void on_enable() {}

    /// Called when the entity is disabled.
    virtual void on_disable() {}

    // --- Update Events ---

    /// Called every frame.
    virtual void update(float dt) { (void)dt; }

    /// Called at a fixed timestep (for physics).
    virtual void fixed_update(float fixed_dt) { (void)fixed_dt; }

    /// Called after all update() calls have finished.
    virtual void late_update(float dt) { (void)dt; }

    // --- Rendering ---

    /// Called during rendering phase.
    virtual void render() {}

    // --- Editor Hooks ---

    /// Custom inspector GUI (called in editor only).
    virtual void on_inspector_gui() {}

    /// Custom gizmo rendering (called in editor only).
    virtual void on_gizmo() {}

    // --- Accessors ---

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

    // --- Component Helpers ---

    /// Get a component from this entity.
    template<typename T>
    T* get_component() {
        if (m_registry && m_registry->valid(m_entity) && m_registry->all_of<T>(m_entity)) {
            return &m_registry->get<T>(m_entity);
        }
        return nullptr;
    }

    /// Get a component from this entity (const version).
    template<typename T>
    const T* get_component() const {
        if (m_registry && m_registry->valid(m_entity) && m_registry->all_of<T>(m_entity)) {
            return &m_registry->get<T>(m_entity);
        }
        return nullptr;
    }

    /// Add a component to this entity.
    template<typename T, typename... Args>
    T& add_component(Args&&... args) {
        return m_registry->emplace<T>(m_entity, std::forward<Args>(args)...);
    }

    /// Check if this entity has a component.
    template<typename T>
    bool has_component() const {
        return m_registry && m_registry->valid(m_entity) && m_registry->all_of<T>(m_entity);
    }

    /// Remove a component from this entity.
    template<typename T>
    void remove_component() {
        if (has_component<T>()) {
            m_registry->remove<T>(m_entity);
        }
    }

protected:
    friend class ScriptSystem;
    friend class ScriptManager;

    entt::entity m_entity = entt::null;
    entt::registry* m_registry = nullptr;
    engine::Engine* m_engine = nullptr;

    /// Initialize the script context (called by ScriptSystem).
    void init_context(entt::entity entity, entt::registry* registry, engine::Engine* engine) {
        m_entity = entity;
        m_registry = registry;
        m_engine = engine;
    }
};

/// Factory function type for creating script instances.
using ScriptFactory = ComponentScript* (*)();

/// Macro to register a component script for DLL export.
/// Usage: REGISTER_COMPONENT_SCRIPT(MyScript)
#ifdef BUILDING_GAME_SCRIPTS
    #ifdef _WIN32
        #define SCRIPT_EXPORT __declspec(dllexport)
    #else
        #define SCRIPT_EXPORT __attribute__((visibility("default")))
    #endif
#else
    #ifdef _WIN32
        #define SCRIPT_EXPORT __declspec(dllimport)
    #else
        #define SCRIPT_EXPORT
    #endif
#endif

#define REGISTER_COMPONENT_SCRIPT(ClassName) \
    extern "C" SCRIPT_EXPORT runtime::ComponentScript* Create_##ClassName() { \
        return new ClassName(); \
    } \
    extern "C" SCRIPT_EXPORT const char* GetTypeName_##ClassName() { \
        return #ClassName; \
    }

} // namespace runtime

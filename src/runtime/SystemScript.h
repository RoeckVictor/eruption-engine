#pragma once

#include <entt/entt.hpp>
#include <string>

namespace engine {
class Engine;
}

namespace runtime {

/// Execution phase for systems.
enum class SystemPhase {
    PreUpdate,      // Before entity updates
    Update,         // Main update phase
    PostUpdate,     // After entity updates
    PrePhysics,     // Before physics simulation
    PostPhysics,    // After physics simulation
    PreRender,      // Before rendering
    Render,         // During rendering
    PostRender      // After rendering
};

/// Base class for user-defined system scripts.
/// Systems operate on all entities matching certain criteria.
/// Systems are hot-reloadable when compiled as a DLL.
class SystemScript {
public:
    virtual ~SystemScript() = default;

    // --- Lifecycle Events ---

    /// Called when the system is initialized.
    virtual void on_init() {}

    /// Called when the system is shutdown.
    virtual void on_shutdown() {}

    // --- Update ---

    /// Called every frame during this system's execution phase.
    virtual void execute(float dt) { (void)dt; }

    /// Called at fixed timestep (only for physics-related systems).
    virtual void fixed_execute(float fixed_dt) { (void)fixed_dt; }

    // --- Configuration ---

    /// Get the system's type name (for serialization).
    virtual const char* type_name() const = 0;

    /// Get the execution phase for this system.
    virtual SystemPhase phase() const { return SystemPhase::Update; }

    /// Get the execution priority within the phase (lower = earlier).
    virtual int priority() const { return 0; }

    // --- Accessors ---

    /// Get the registry.
    entt::registry* registry() { return m_registry; }
    const entt::registry* registry() const { return m_registry; }

    /// Get the engine instance.
    engine::Engine* engine() { return m_engine; }
    const engine::Engine* engine() const { return m_engine; }

protected:
    friend class ScriptSystem;
    friend class ScriptManager;

    entt::registry* m_registry = nullptr;
    engine::Engine* m_engine = nullptr;

    /// Initialize the system context (called by ScriptSystem).
    void init_context(entt::registry* registry, engine::Engine* engine) {
        m_registry = registry;
        m_engine = engine;
    }
};

/// Factory function type for creating system instances.
using SystemFactory = SystemScript* (*)();

/// Macro to register a system script for DLL export.
/// Usage: REGISTER_SYSTEM_SCRIPT(MySystem)
#ifdef BUILDING_GAME_SCRIPTS
    #ifdef _WIN32
        #define SYSTEM_EXPORT __declspec(dllexport)
    #else
        #define SYSTEM_EXPORT __attribute__((visibility("default")))
    #endif
#else
    #ifdef _WIN32
        #define SYSTEM_EXPORT __declspec(dllimport)
    #else
        #define SYSTEM_EXPORT
    #endif
#endif

#define REGISTER_SYSTEM_SCRIPT(ClassName) \
    extern "C" SYSTEM_EXPORT runtime::SystemScript* CreateSystem_##ClassName() { \
        return new ClassName(); \
    } \
    extern "C" SYSTEM_EXPORT const char* GetSystemTypeName_##ClassName() { \
        return #ClassName; \
    }

} // namespace runtime

#pragma once

#include "engine/core/Result.h"
#include "engine/core/SystemManager.h"
#include <entt/entt.hpp>

namespace engine {

class Engine;

namespace scene {

/// Base class for game scenes. Each scene has its own SystemManager and registry.
/// Scenes form a stack: only the top scene receives update/render calls.
/// Paused scenes (below top) retain their state but do not tick.
class Scene {
public:
    virtual ~Scene() = default;

    /// Human-readable name for debugging/logging.
    virtual const char* name() const = 0;

    /// Called when this scene becomes the active (top) scene.
    /// Register systems into systems() here.
    /// Returns error if initialization fails, causing the scene to be popped.
    virtual Result<void, ErrorInfo> on_enter(Engine& engine) = 0;

    /// Called when this scene is being removed from the stack entirely.
    virtual void on_exit(Engine& engine) = 0;

    /// Called when a new scene is pushed on top of this one.
    virtual void on_pause(Engine& /*engine*/) {}

    /// Called when the scene above this one is popped, making this active again.
    virtual void on_resume(Engine& /*engine*/) {}

    SystemManager& systems() { return m_systems; }
    entt::registry& registry() { return m_registry; }

protected:
    SystemManager m_systems;
    entt::registry m_registry;
};

} // namespace scene
} // namespace engine

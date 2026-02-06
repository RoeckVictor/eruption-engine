#pragma once

#include "engine/core/System.h"
#include <vector>

namespace engine {

class Engine;

/// Lightweight system registry with phase-based execution.
/// Systems are registered into one or more phases (update, fixed_update, render).
/// Execution order within a phase matches registration order.
///
/// **Ownership:** The SystemManager does NOT own the systems. The caller (typically
/// the Application subclass) must ensure that registered systems outlive the
/// SystemManager. Passing a reference (not pointer) reinforces non-null semantics.
class SystemManager {
public:
    void add_update_system(System& sys)       { m_update.push_back(&sys); }
    void add_fixed_update_system(System& sys) { m_fixed_update.push_back(&sys); }
    void add_render_system(System& sys)       { m_render.push_back(&sys); }

    bool init_all(Engine& engine);
    void shutdown_all();

    void update_all(Engine& engine, float dt);
    void fixed_update_all(Engine& engine, float dt);
    void render_all(Engine& engine);

private:
    std::vector<System*> m_update;
    std::vector<System*> m_fixed_update;
    std::vector<System*> m_render;

    // Track which systems were successfully initialized for safe shutdown
    std::vector<System*> m_initialized;
};

} // namespace engine

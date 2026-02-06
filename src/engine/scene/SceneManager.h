#pragma once

#include "engine/scene/Scene.h"
#include <memory>
#include <vector>

namespace engine {

class Engine;

namespace scene {

/// Manages a stack of scenes. Only the top scene is active.
/// Push/pop/replace operations are deferred to the start of the next frame
/// to avoid modifying the stack during iteration.
class SceneManager {
public:
    void push(std::unique_ptr<Scene> scene);
    void pop();
    void replace(std::unique_ptr<Scene> scene);

    bool has_active_scene() const { return !m_stack.empty(); }
    Scene* top() const;

    // Called by Engine main loop
    void process_pending(Engine& engine);
    void update(Engine& engine, float dt);
    void fixed_update(Engine& engine, float dt);
    void render(Engine& engine);
    void shutdown_all(Engine& engine);

private:
    std::vector<std::unique_ptr<Scene>> m_stack;

    enum class PendingOp { Push, Pop, Replace };
    struct PendingEntry {
        PendingOp op;
        std::unique_ptr<Scene> scene;
    };
    std::vector<PendingEntry> m_pending;
};

} // namespace scene
} // namespace engine

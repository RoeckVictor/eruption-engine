#include "engine/scene/SceneManager.h"
#include "engine/scene/Scene.h"
#include "engine/core/Engine.h"
#include "engine/profiler/Profiler.h"
#include "engine/core/Log.h"

namespace engine::scene {

void SceneManager::push(std::unique_ptr<Scene> scene) {
    PendingEntry entry;
    entry.op = PendingOp::Push;
    entry.scene = std::move(scene);
    m_pending.push_back(std::move(entry));
}

void SceneManager::pop() {
    PendingEntry entry;
    entry.op = PendingOp::Pop;
    m_pending.push_back(std::move(entry));
}

void SceneManager::replace(std::unique_ptr<Scene> scene) {
    PendingEntry entry;
    entry.op = PendingOp::Replace;
    entry.scene = std::move(scene);
    m_pending.push_back(std::move(entry));
}

Scene* SceneManager::top() const {
    return m_stack.empty() ? nullptr : m_stack.back().get();
}

void SceneManager::process_pending(Engine& engine) {
    if (m_pending.empty()) return;

    const int max_depth = engine.config().max_scene_stack_depth;

    for (auto& entry : m_pending) {
        switch (entry.op) {
        case PendingOp::Push: {
            // Check stack depth limit to prevent infinite recursion
            if (static_cast<int>(m_stack.size()) >= max_depth) {
                ENGINE_ERR("SceneManager: Stack depth limit (%d) reached, rejecting push of '%s'",
                           max_depth, entry.scene->name());
                break;
            }
            if (!m_stack.empty()) {
                ENGINE_LOG("SceneManager: Pausing '%s'", m_stack.back()->name());
                m_stack.back()->on_pause(engine);
            }
            ENGINE_LOG("SceneManager: Pushing '%s'", entry.scene->name());
            auto* scene = entry.scene.get();
            m_stack.push_back(std::move(entry.scene));
            auto result = scene->on_enter(engine);
            if (result.is_err()) {
                ENGINE_ERR("SceneManager: Scene '%s' failed to initialize: %s",
                           scene->name(), result.error().message.c_str());
                // Pop the failed scene
                m_stack.back()->on_exit(engine);
                m_stack.pop_back();
                // Resume the previous scene if one exists
                if (!m_stack.empty()) {
                    ENGINE_LOG("SceneManager: Resuming '%s' after failed push",
                               m_stack.back()->name());
                    m_stack.back()->on_resume(engine);
                }
                break;
            }
            scene->systems().init_all(engine);
            break;
        }
        case PendingOp::Pop: {
            if (m_stack.empty()) {
                ENGINE_ERR("SceneManager: Pop on empty stack");
                break;
            }
            ENGINE_LOG("SceneManager: Popping '%s'", m_stack.back()->name());
            m_stack.back()->systems().shutdown_all();
            m_stack.back()->on_exit(engine);
            m_stack.pop_back();
            if (!m_stack.empty()) {
                ENGINE_LOG("SceneManager: Resuming '%s'", m_stack.back()->name());
                m_stack.back()->on_resume(engine);
            }
            break;
        }
        case PendingOp::Replace: {
            if (!m_stack.empty()) {
                ENGINE_LOG("SceneManager: Replacing '%s'", m_stack.back()->name());
                m_stack.back()->systems().shutdown_all();
                m_stack.back()->on_exit(engine);
                m_stack.pop_back();
            }
            ENGINE_LOG("SceneManager: Pushing '%s' (replace)", entry.scene->name());
            auto* scene = entry.scene.get();
            m_stack.push_back(std::move(entry.scene));
            auto result = scene->on_enter(engine);
            if (result.is_err()) {
                ENGINE_ERR("SceneManager: Scene '%s' failed to initialize: %s",
                           scene->name(), result.error().message.c_str());
                // Pop the failed scene (stack is now empty since we replaced)
                m_stack.back()->on_exit(engine);
                m_stack.pop_back();
                break;
            }
            scene->systems().init_all(engine);
            break;
        }
        }
    }
    m_pending.clear();
}

void SceneManager::update(Engine& engine, float dt) {
    PROFILE_SCOPE("SceneManager::update");
    if (!m_stack.empty()) {
        m_stack.back()->systems().update_all(engine, dt);
    }
}

void SceneManager::fixed_update(Engine& engine, float dt) {
    PROFILE_SCOPE("SceneManager::fixed_update");
    if (!m_stack.empty()) {
        m_stack.back()->systems().fixed_update_all(engine, dt);
    }
}

void SceneManager::render(Engine& engine) {
    PROFILE_SCOPE("SceneManager::render");
    if (!m_stack.empty()) {
        m_stack.back()->systems().render_all(engine);
    }
}

void SceneManager::shutdown_all(Engine& engine) {
    while (!m_stack.empty()) {
        m_stack.back()->systems().shutdown_all();
        m_stack.back()->on_exit(engine);
        m_stack.pop_back();
    }
    m_pending.clear();
}

} // namespace engine::scene

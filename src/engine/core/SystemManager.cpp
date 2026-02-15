#include "engine/core/SystemManager.h"
#include "engine/core/Engine.h"
#include "engine/core/Log.h"
#include <typeinfo>
#include <unordered_set>

namespace engine {

bool SystemManager::init_all(Engine& engine) {
    if (m_is_initialized) {
        ENGINE_LOG("SystemManager::init_all() called again -- already initialized, skipping");
        return true;
    }

    // Collect unique systems across all phases, preserving registration order.
    // A system that appears in multiple phases is only initialized once,
    // at the position of its first registration.
    std::vector<System*> all;
    std::unordered_set<System*> seen;
    auto add_unique = [&](System* s) {
        if (seen.insert(s).second) all.push_back(s);
    };
    for (auto* s : m_update) add_unique(s);
    for (auto* s : m_fixed_update) add_unique(s);
    for (auto* s : m_render) add_unique(s);

    for (auto* sys : all) {
        if (!sys->init(engine)) {
            ENGINE_ERR("System init failed: %s", typeid(*sys).name());
            return false;
        }
        m_initialized.push_back(sys);
    }
    m_is_initialized = true;
    return true;
}

void SystemManager::shutdown_all() {
    // Shutdown in reverse init order
    for (auto it = m_initialized.rbegin(); it != m_initialized.rend(); ++it) {
        (*it)->shutdown();
    }
    m_initialized.clear();
    m_is_initialized = false;
}

void SystemManager::update_all(Engine& engine, float dt) {
    for (auto* sys : m_update) sys->update(engine, dt);
}

void SystemManager::fixed_update_all(Engine& engine, float dt) {
    for (auto* sys : m_fixed_update) sys->fixed_update(engine, dt);
}

void SystemManager::render_all(Engine& engine) {
    for (auto* sys : m_render) sys->render(engine);
}

} // namespace engine

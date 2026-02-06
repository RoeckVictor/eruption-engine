#include "engine/prefab/ComponentRegistry.h"
#include "engine/core/Log.h"

namespace engine::prefab {

void ComponentRegistry::register_component(const std::string& name, Factory factory) {
    m_factories[name] = std::move(factory);
}

const ComponentRegistry::Factory* ComponentRegistry::find(const std::string& name) const {
    auto it = m_factories.find(name);
    return (it != m_factories.end()) ? &it->second : nullptr;
}

bool ComponentRegistry::has(const std::string& name) const {
    return m_factories.find(name) != m_factories.end();
}

} // namespace engine::prefab

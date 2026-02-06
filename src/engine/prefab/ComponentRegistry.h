#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <nlohmann/json_fwd.hpp>
#include <entt/entt.hpp>

namespace engine::prefab {

/// Maps component type names to factory functions.
/// Games populate this registry; the PrefabManager uses it to instantiate entities.
class ComponentRegistry {
public:
    using Factory = std::function<void(entt::registry&, entt::entity, const nlohmann::json&)>;

    /// Register a component type with a name and a factory function.
    void register_component(const std::string& name, Factory factory);

    /// Look up a factory by name. Returns nullptr if not found.
    const Factory* find(const std::string& name) const;

    /// Convenience: register with a type and a deserializer lambda.
    template<typename T>
    void register_component(const std::string& name,
                            std::function<T(const nlohmann::json&)> deserialize) {
        register_component(name, [deser = std::move(deserialize)](
            entt::registry& reg, entt::entity e, const nlohmann::json& j) {
            reg.emplace<T>(e, deser(j));
        });
    }

    /// Register a tag component (no JSON data needed, emplaces with defaults).
    template<typename T>
    void register_tag(const std::string& name) {
        register_component(name, [](entt::registry& reg, entt::entity e,
                                     const nlohmann::json&) {
            reg.emplace<T>(e);
        });
    }

    /// Register a component that uses default construction (no JSON data).
    template<typename T>
    void register_default(const std::string& name) {
        register_component(name, [](entt::registry& reg, entt::entity e,
                                     const nlohmann::json&) {
            reg.emplace<T>(e);
        });
    }

    bool has(const std::string& name) const;
    size_t count() const { return m_factories.size(); }

private:
    std::unordered_map<std::string, Factory> m_factories;
};

} // namespace engine::prefab

#pragma once

#include "PropertyValue.h"
#include "engine/reflection/TypeRegistry.h"
#include "engine/reflection/PropertyInfo.h"
#include <entt/entt.hpp>
#include <string>
#include <optional>
#include <vector>
#include <functional>
#include <typeindex>

namespace engine::animation {

struct ComponentHandler {
    std::function<bool(entt::registry&, entt::entity)> has_component;
    std::function<void*(entt::registry&, entt::entity)> get_component;
    std::string full_type_name;
};

// Resolves property paths like "Transform.x" to actual memory locations
class PropertyResolver {
public:
    static PropertyResolver& instance();

    template<typename T>
    void register_component(const std::string& type_name, const std::string& full_type_name = "") {
        m_component_handlers[type_name] = ComponentHandler{
            [](entt::registry& reg, entt::entity entity) -> bool {
                return reg.all_of<T>(entity);
            },
            [](entt::registry& reg, entt::entity entity) -> void* {
                if (reg.all_of<T>(entity)) {
                    return &reg.get<T>(entity);
                }
                return nullptr;
            },
            full_type_name.empty() ? type_name : full_type_name
        };
        m_type_names[std::type_index(typeid(T))] = type_name;
    }

    static bool parse_path(const std::string& path,
                          std::string& out_component,
                          std::string& out_property);

    std::optional<PropertyValue> get_value(
        entt::registry& registry,
        entt::entity entity,
        const std::string& path) const;

    bool set_value(
        entt::registry& registry,
        entt::entity entity,
        const std::string& path,
        const PropertyValue& value) const;

    std::optional<PropertyValueType> get_property_type(const std::string& path) const;
    std::vector<std::string> get_animatable_properties(const std::string& component_name) const;
    std::vector<std::string> get_registered_components() const;

    bool is_component_registered(const std::string& component_name) const;

private:
    PropertyResolver() = default;

    void* get_component_ptr(entt::registry& registry, entt::entity entity,
                           const std::string& component_name) const;

    static std::optional<PropertyValueType> convert_property_type(reflection::PropertyType type);

    static std::optional<PropertyValue> read_value(const void* ptr, reflection::PropertyType type);
    static bool write_value(void* ptr, reflection::PropertyType type, const PropertyValue& value);

    std::unordered_map<std::string, ComponentHandler> m_component_handlers;
    std::unordered_map<std::type_index, std::string> m_type_names;
};

}

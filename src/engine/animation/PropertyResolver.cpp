#include "PropertyResolver.h"
#include "engine/reflection/TypeRegistry.h"
#include "engine/core/Logger.h"

namespace engine::animation {

// Helper to get a readable type name from the variant index
static const char* get_value_type_name(const PropertyValue& value) {
    static const char* names[] = {"bool", "int", "float", "Vec2", "Vec3", "Vec4", "string"};
    size_t idx = value.index();
    return idx < 7 ? names[idx] : "unknown";
}

// Helper to get a readable type name from reflection::PropertyType
static const char* get_property_type_name(reflection::PropertyType type) {
    switch (type) {
        case reflection::PropertyType::Bool:   return "bool";
        case reflection::PropertyType::Int:    return "int";
        case reflection::PropertyType::Float:  return "float";
        case reflection::PropertyType::Double: return "double";
        case reflection::PropertyType::Vec2:   return "Vec2";
        case reflection::PropertyType::Vec3:   return "Vec3";
        case reflection::PropertyType::Vec4:   return "Vec4";
        case reflection::PropertyType::Color:  return "Color";
        case reflection::PropertyType::String: return "string";
        default:                               return "unknown";
    }
}

PropertyResolver& PropertyResolver::instance() {
    static PropertyResolver resolver;
    return resolver;
}

bool PropertyResolver::parse_path(const std::string& path,
                                  std::string& out_component,
                                  std::string& out_property) {
    size_t dot_pos = path.find('.');
    if (dot_pos == std::string::npos || dot_pos == 0 || dot_pos == path.length() - 1) {
        return false;
    }

    out_component = path.substr(0, dot_pos);
    out_property = path.substr(dot_pos + 1);
    return true;
}

void* PropertyResolver::get_component_ptr(entt::registry& registry, entt::entity entity,
                                          const std::string& component_name) const {
    auto it = m_component_handlers.find(component_name);
    if (it == m_component_handlers.end()) {
        return nullptr;
    }

    if (!it->second.has_component(registry, entity)) {
        return nullptr;
    }

    return it->second.get_component(registry, entity);
}

std::optional<PropertyValueType> PropertyResolver::convert_property_type(reflection::PropertyType type) {
    switch (type) {
        case reflection::PropertyType::Bool:   return PropertyValueType::Bool;
        case reflection::PropertyType::Int:    return PropertyValueType::Int;
        case reflection::PropertyType::Float:  return PropertyValueType::Float;
        case reflection::PropertyType::Double: return PropertyValueType::Float; // Convert double to float
        case reflection::PropertyType::Vec2:   return PropertyValueType::Vec2;
        case reflection::PropertyType::Vec3:   return PropertyValueType::Vec3;
        case reflection::PropertyType::Vec4:   return PropertyValueType::Vec4;
        case reflection::PropertyType::Color:  return PropertyValueType::Color;
        case reflection::PropertyType::String: return PropertyValueType::String;
        default:                               return std::nullopt;
    }
}

std::optional<PropertyValue> PropertyResolver::read_value(const void* ptr, reflection::PropertyType type) {
    switch (type) {
        case reflection::PropertyType::Bool:
            return *static_cast<const bool*>(ptr);

        case reflection::PropertyType::Int:
            return *static_cast<const int*>(ptr);

        case reflection::PropertyType::Float:
            return *static_cast<const float*>(ptr);

        case reflection::PropertyType::Double:
            return static_cast<float>(*static_cast<const double*>(ptr));

        case reflection::PropertyType::Vec2: {
            const float* f = static_cast<const float*>(ptr);
            return Vec2{f[0], f[1]};
        }

        case reflection::PropertyType::Vec3: {
            const float* f = static_cast<const float*>(ptr);
            return Vec3{f[0], f[1], f[2]};
        }

        case reflection::PropertyType::Vec4:
        case reflection::PropertyType::Color: {
            const float* f = static_cast<const float*>(ptr);
            return Vec4{f[0], f[1], f[2], f[3]};
        }

        case reflection::PropertyType::String:
            return *static_cast<const std::string*>(ptr);

        default:
            return std::nullopt;
    }
}

bool PropertyResolver::write_value(void* ptr, reflection::PropertyType type, const PropertyValue& value) {
    switch (type) {
        case reflection::PropertyType::Bool:
            if (std::holds_alternative<bool>(value)) {
                *static_cast<bool*>(ptr) = std::get<bool>(value);
                return true;
            }
            break;

        case reflection::PropertyType::Int:
            if (std::holds_alternative<int>(value)) {
                *static_cast<int*>(ptr) = std::get<int>(value);
                return true;
            }
            break;

        case reflection::PropertyType::Float:
            if (std::holds_alternative<float>(value)) {
                *static_cast<float*>(ptr) = std::get<float>(value);
                return true;
            }
            break;

        case reflection::PropertyType::Double:
            if (std::holds_alternative<float>(value)) {
                *static_cast<double*>(ptr) = static_cast<double>(std::get<float>(value));
                return true;
            }
            break;

        case reflection::PropertyType::Vec2:
            if (std::holds_alternative<Vec2>(value)) {
                const Vec2& v = std::get<Vec2>(value);
                float* f = static_cast<float*>(ptr);
                f[0] = v.x;
                f[1] = v.y;
                return true;
            }
            break;

        case reflection::PropertyType::Vec3:
            if (std::holds_alternative<Vec3>(value)) {
                const Vec3& v = std::get<Vec3>(value);
                float* f = static_cast<float*>(ptr);
                f[0] = v.x;
                f[1] = v.y;
                f[2] = v.z;
                return true;
            }
            break;

        case reflection::PropertyType::Vec4:
        case reflection::PropertyType::Color:
            if (std::holds_alternative<Vec4>(value)) {
                const Vec4& v = std::get<Vec4>(value);
                float* f = static_cast<float*>(ptr);
                f[0] = v.x;
                f[1] = v.y;
                f[2] = v.z;
                f[3] = v.w;
                return true;
            }
            break;

        case reflection::PropertyType::String:
            if (std::holds_alternative<std::string>(value)) {
                *static_cast<std::string*>(ptr) = std::get<std::string>(value);
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

std::optional<PropertyValue> PropertyResolver::get_value(
    entt::registry& registry,
    entt::entity entity,
    const std::string& path) const
{
    std::string component_name, property_name;
    if (!parse_path(path, component_name, property_name)) {
        return std::nullopt;
    }

    // Find the component handler
    auto handler_it = m_component_handlers.find(component_name);
    if (handler_it == m_component_handlers.end()) {
        return std::nullopt;
    }

    // Get component pointer
    void* component_ptr = get_component_ptr(registry, entity, component_name);
    if (!component_ptr) {
        return std::nullopt;
    }

    // Get type info using full type name
    const auto* type_info = reflection::TypeRegistry::instance().get_by_name(handler_it->second.full_type_name);
    if (!type_info) {
        return std::nullopt;
    }

    // Get property info
    const auto* prop_info = type_info->get_property(property_name);
    if (!prop_info) {
        return std::nullopt;
    }

    // Read value from memory
    const void* prop_ptr = static_cast<const char*>(component_ptr) + prop_info->offset;
    return read_value(prop_ptr, prop_info->type);
}

bool PropertyResolver::set_value(
    entt::registry& registry,
    entt::entity entity,
    const std::string& path,
    const PropertyValue& value) const
{
    std::string component_name, property_name;
    if (!parse_path(path, component_name, property_name)) {
        return false;
    }

    // Find the component handler
    auto handler_it = m_component_handlers.find(component_name);
    if (handler_it == m_component_handlers.end()) {
        return false;
    }

    // Get component pointer
    void* component_ptr = get_component_ptr(registry, entity, component_name);
    if (!component_ptr) {
        return false;
    }

    // Get type info using full type name
    const auto* type_info = reflection::TypeRegistry::instance().get_by_name(handler_it->second.full_type_name);
    if (!type_info) {
        return false;
    }

    // Get property info
    const auto* prop_info = type_info->get_property(property_name);
    if (!prop_info) {
        return false;
    }

    // Check if read-only
    if (reflection::has_flag(prop_info->flags, reflection::PropertyFlags::ReadOnly)) {
        return false;
    }

    // Write value to memory
    void* prop_ptr = static_cast<char*>(component_ptr) + prop_info->offset;
    bool success = write_value(prop_ptr, prop_info->type, value);

    if (!success) {
        Logger::instance().warning("Animation",
            "Type mismatch writing property '%s': expected %s, got %s",
            path.c_str(),
            get_property_type_name(prop_info->type),
            get_value_type_name(value));
    }

    return success;
}

std::optional<PropertyValueType> PropertyResolver::get_property_type(const std::string& path) const {
    std::string component_name, property_name;
    if (!parse_path(path, component_name, property_name)) {
        return std::nullopt;
    }

    // Find the component handler
    auto handler_it = m_component_handlers.find(component_name);
    if (handler_it == m_component_handlers.end()) {
        return std::nullopt;
    }

    // Get type info using full type name
    const auto* type_info = reflection::TypeRegistry::instance().get_by_name(handler_it->second.full_type_name);
    if (!type_info) {
        return std::nullopt;
    }

    // Get property info
    const auto* prop_info = type_info->get_property(property_name);
    if (!prop_info) {
        return std::nullopt;
    }

    return convert_property_type(prop_info->type);
}

std::vector<std::string> PropertyResolver::get_animatable_properties(const std::string& component_name) const {
    std::vector<std::string> result;

    // Check if component is registered
    auto handler_it = m_component_handlers.find(component_name);
    if (handler_it == m_component_handlers.end()) {
        return result;
    }

    // Get type info using full type name
    const auto* type_info = reflection::TypeRegistry::instance().get_by_name(handler_it->second.full_type_name);
    if (!type_info) {
        return result;
    }

    // Get all animatable properties
    for (const auto& prop : type_info->properties()) {
        // Skip read-only and hidden properties
        if (reflection::has_flag(prop.flags, reflection::PropertyFlags::ReadOnly)) {
            continue;
        }
        if (reflection::has_flag(prop.flags, reflection::PropertyFlags::Hidden)) {
            continue;
        }

        // Check if the type is animatable
        auto value_type = convert_property_type(prop.type);
        if (value_type.has_value()) {
            result.push_back(component_name + "." + prop.name);
        }
    }

    return result;
}

std::vector<std::string> PropertyResolver::get_registered_components() const {
    std::vector<std::string> result;
    result.reserve(m_component_handlers.size());
    for (const auto& [name, handler] : m_component_handlers) {
        result.push_back(name);
    }
    return result;
}

bool PropertyResolver::is_component_registered(const std::string& component_name) const {
    return m_component_handlers.find(component_name) != m_component_handlers.end();
}

} // namespace engine::animation

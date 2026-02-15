#pragma once

#include "TypeInfo.h"
#include "TypeRegistry.h"
#include "engine/core/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace engine::reflection {

/// Serialize all reflected properties of an object to a flat JSON object.
/// Skips ReadOnly properties. Keys are the C++ member names.
inline void serialize_properties(const TypeInfo& type_info, const void* obj, nlohmann::json& out) {
    for (const auto& prop : type_info.properties()) {
        if (has_flag(prop.flags, PropertyFlags::ReadOnly)) continue;

        // Bounds check: ensure the property fits within the type's memory layout
        if (prop.offset + prop.size > type_info.size()) {
            Logger::instance().error("Reflection",
                "Property '%s' offset+size (%zu+%zu) exceeds type size (%zu), skipping",
                prop.name.c_str(), prop.offset, prop.size, type_info.size());
            continue;
        }

        const void* prop_ptr = static_cast<const char*>(obj) + prop.offset;

        switch (prop.type) {
            case PropertyType::Bool:
                out[prop.name] = *static_cast<const bool*>(prop_ptr);
                break;
            case PropertyType::Int:
                out[prop.name] = *static_cast<const int*>(prop_ptr);
                break;
            case PropertyType::Float:
                out[prop.name] = *static_cast<const float*>(prop_ptr);
                break;
            case PropertyType::Double:
                out[prop.name] = *static_cast<const double*>(prop_ptr);
                break;
            case PropertyType::String:
                out[prop.name] = *static_cast<const std::string*>(prop_ptr);
                break;
            case PropertyType::Enum:
                out[prop.name] = *static_cast<const int*>(prop_ptr);
                break;
            case PropertyType::Vec2: {
                const auto* f = static_cast<const float*>(prop_ptr);
                out[prop.name] = { f[0], f[1] };
                break;
            }
            case PropertyType::Vec3: {
                const auto* f = static_cast<const float*>(prop_ptr);
                out[prop.name] = { f[0], f[1], f[2] };
                break;
            }
            case PropertyType::Vec4:
            case PropertyType::Color: {
                const auto* f = static_cast<const float*>(prop_ptr);
                out[prop.name] = { f[0], f[1], f[2], f[3] };
                break;
            }
            default:
                break;
        }
    }
}

/// Deserialize reflected properties from a flat JSON object into an existing object.
/// Only writes properties present in the JSON. Skips ReadOnly properties.
/// Logs a warning on type mismatches.
inline void deserialize_properties(const TypeInfo& type_info, void* obj, const nlohmann::json& data) {
    for (const auto& prop : type_info.properties()) {
        if (has_flag(prop.flags, PropertyFlags::ReadOnly)) continue;
        if (!data.contains(prop.name)) continue;

        // Bounds check: ensure the property fits within the type's memory layout
        if (prop.offset + prop.size > type_info.size()) {
            Logger::instance().error("Reflection",
                "Property '%s' offset+size (%zu+%zu) exceeds type size (%zu), skipping",
                prop.name.c_str(), prop.offset, prop.size, type_info.size());
            continue;
        }

        void* prop_ptr = static_cast<char*>(obj) + prop.offset;

        try {
            const auto& val = data[prop.name];
            switch (prop.type) {
                case PropertyType::Bool:
                    *static_cast<bool*>(prop_ptr) = val.get<bool>();
                    break;
                case PropertyType::Int:
                    *static_cast<int*>(prop_ptr) = val.get<int>();
                    break;
                case PropertyType::Float:
                    *static_cast<float*>(prop_ptr) = val.get<float>();
                    break;
                case PropertyType::Double:
                    *static_cast<double*>(prop_ptr) = val.get<double>();
                    break;
                case PropertyType::String:
                    *static_cast<std::string*>(prop_ptr) = val.get<std::string>();
                    break;
                case PropertyType::Enum: {
                    int enum_val = val.get<int>();
                    if (!prop.enum_names.empty() &&
                        (enum_val < 0 || enum_val >= static_cast<int>(prop.enum_names.size()))) {
                        Logger::instance().warning("Reflection",
                            "Enum property '%s' of type '%s': value %d out of range [0, %d), clamping",
                            prop.name.c_str(), type_info.name().c_str(),
                            enum_val, static_cast<int>(prop.enum_names.size()));
                        enum_val = std::clamp(enum_val, 0, static_cast<int>(prop.enum_names.size()) - 1);
                    }
                    *static_cast<int*>(prop_ptr) = enum_val;
                    break;
                }
                case PropertyType::Vec2:
                    if (val.is_array() && val.size() >= 2) {
                        auto* f = static_cast<float*>(prop_ptr);
                        f[0] = val[0].get<float>();
                        f[1] = val[1].get<float>();
                    }
                    break;
                case PropertyType::Vec3:
                    if (val.is_array() && val.size() >= 3) {
                        auto* f = static_cast<float*>(prop_ptr);
                        f[0] = val[0].get<float>();
                        f[1] = val[1].get<float>();
                        f[2] = val[2].get<float>();
                    }
                    break;
                case PropertyType::Vec4:
                case PropertyType::Color:
                    if (val.is_array() && val.size() >= 4) {
                        auto* f = static_cast<float*>(prop_ptr);
                        f[0] = val[0].get<float>();
                        f[1] = val[1].get<float>();
                        f[2] = val[2].get<float>();
                        f[3] = val[3].get<float>();
                    }
                    break;
                default:
                    break;
            }
        } catch (const nlohmann::json::exception& e) {
            Logger::instance().warning("Reflection",
                "Failed to deserialize property '%s' of type '%s': %s",
                prop.name.c_str(), type_info.name().c_str(), e.what());
        }
    }
}

/// Convenience: default-construct T, then fill from JSON using reflection.
/// Returns a default T if the type is not registered in TypeRegistry.
template<typename T>
T deserialize_as(const nlohmann::json& data) {
    T obj{};
    const auto* type_info = TypeRegistry::instance().get<T>();
    if (type_info) {
        deserialize_properties(*type_info, &obj, data);
    }
    return obj;
}

/// Helper to register a reflected type with the TypeRegistry.
/// Reduces per-type boilerplate in init_engine_reflections() from 8 lines to 1.
/// Note: offsetof() is technically UB for non-standard-layout types, though most
/// compilers support it as an extension. A runtime warning is logged for such types.
template<typename T>
void register_type() {
    if constexpr (!std::is_standard_layout_v<T>) {
        Logger::instance().warning("Reflection",
            "Type '%s' is not standard-layout; offsetof() usage may be non-portable",
            TypeReflector<T>::name());
    }

    auto info = std::make_unique<TypeInfo>(
        TypeReflector<T>::name(),
        sizeof(T),
        std::type_index(typeid(T))
    );
    TypeReflector<T>::reflect(*info);
    TypeRegistry::instance().register_type(std::move(info));
}

} // namespace engine::reflection

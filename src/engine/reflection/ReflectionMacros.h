#pragma once

#include "TypeInfo.h"
#include "TypeRegistry.h"
#include <memory>

namespace engine::reflection {

/// Helper template for type reflection registration.
template<typename T>
struct TypeReflector;

/// Helper to register a type automatically at static initialization.
template<typename T>
struct TypeRegistrar {
    TypeRegistrar() {
        auto info = std::make_unique<TypeInfo>(
            TypeReflector<T>::name(),
            sizeof(T),
            std::type_index(typeid(T))
        );
        TypeReflector<T>::reflect(*info);
        TypeRegistry::instance().register_type(std::move(info));
    }
};

} // namespace engine::reflection

/// Begin reflecting a type.
/// Usage:
///   REFLECT_TYPE_BEGIN(MyComponent)
///       REFLECT_PROPERTY(x, "X Position")
///       REFLECT_PROPERTY(y, "Y Position")
///   REFLECT_TYPE_END()
#define REFLECT_TYPE_BEGIN(Type) \
    namespace engine::reflection { \
    template<> \
    struct TypeReflector<Type> { \
        using CurrentType = Type; \
        static const char* name() { return #Type; } \
        static void reflect(TypeInfo& info) {

/// Reflect a simple property.
#define REFLECT_PROPERTY(member, displayName) \
            { \
                PropertyInfo prop; \
                prop.name = #member; \
                prop.display_name = displayName; \
                prop.offset = offsetof(CurrentType, member); \
                prop.size = sizeof(CurrentType::member); \
                prop.type = get_property_type<decltype(CurrentType::member)>(); \
                prop.flags = PropertyFlags::None; \
                info.add_property(prop); \
            }

/// Reflect a property with flags.
#define REFLECT_PROPERTY_FLAGS(member, displayName, flags) \
            { \
                PropertyInfo prop; \
                prop.name = #member; \
                prop.display_name = displayName; \
                prop.offset = offsetof(CurrentType, member); \
                prop.size = sizeof(CurrentType::member); \
                prop.type = get_property_type<decltype(CurrentType::member)>(); \
                prop.flags = flags; \
                info.add_property(prop); \
            }

/// Reflect a property with range (for sliders).
#define REFLECT_PROPERTY_RANGE(member, displayName, minVal, maxVal, stepVal) \
            { \
                PropertyInfo prop; \
                prop.name = #member; \
                prop.display_name = displayName; \
                prop.offset = offsetof(CurrentType, member); \
                prop.size = sizeof(CurrentType::member); \
                prop.type = get_property_type<decltype(CurrentType::member)>(); \
                prop.flags = PropertyFlags::Slider; \
                prop.min_value = minVal; \
                prop.max_value = maxVal; \
                prop.step = stepVal; \
                info.add_property(prop); \
            }

/// Reflect a Vec2 property (two floats).
#define REFLECT_PROPERTY_VEC2(member, displayName) \
            { \
                PropertyInfo prop; \
                prop.name = #member; \
                prop.display_name = displayName; \
                prop.offset = offsetof(CurrentType, member); \
                prop.size = sizeof(float) * 2; \
                prop.type = PropertyType::Vec2; \
                prop.flags = PropertyFlags::None; \
                info.add_property(prop); \
            }

/// Reflect a color property (RGBA).
#define REFLECT_PROPERTY_COLOR(member, displayName) \
            { \
                PropertyInfo prop; \
                prop.name = #member; \
                prop.display_name = displayName; \
                prop.offset = offsetof(CurrentType, member); \
                prop.size = sizeof(float) * 4; \
                prop.type = PropertyType::Color; \
                prop.flags = PropertyFlags::Color; \
                info.add_property(prop); \
            }

/// End type reflection and register.
#define REFLECT_TYPE_END() \
        } \
    }; \
    } /* namespace engine::reflection */

/// Register the type (call this in a .cpp file).
/// This ensures the type is registered at static initialization.
#define REGISTER_REFLECTED_TYPE(Type) \
    static engine::reflection::TypeRegistrar<Type> s_##Type##_registrar;

/// Convenience macro to declare and register in header.
/// Use this after REFLECT_TYPE_END() in a .cpp file.
#define REFLECT_TYPE_REGISTER(Type) \
    REFLECT_TYPE_END() \
    REGISTER_REFLECTED_TYPE(Type)

#pragma once

#include "TypeInfo.h"
#include "TypeRegistry.h"
#include <memory>
#include <type_traits>

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
#ifdef _MSC_VER
// MSVC: suppress "offsetof used on non-standard-layout type" warning.
// offsetof() works correctly on MSVC/GCC/Clang for these types in practice.
#define REFLECT_OFFSETOF_PUSH __pragma(warning(push)) __pragma(warning(disable: 4116))
#define REFLECT_OFFSETOF_POP  __pragma(warning(pop))
#else
#define REFLECT_OFFSETOF_PUSH _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"")
#define REFLECT_OFFSETOF_POP  _Pragma("GCC diagnostic pop")
#endif

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
                REFLECT_OFFSETOF_PUSH \
                prop.offset = offsetof(CurrentType, member); \
                REFLECT_OFFSETOF_POP \
                prop.size = sizeof(CurrentType::member); \
                prop.type = get_property_type<decltype(CurrentType::member)>(); \
                prop.flags = PropertyFlags::None; \
                info.add_property(prop); \
            }

/// Reflect a property with flags.
#define REFLECT_PROPERTY_FLAGS(member, displayName, propFlags) \
            { \
                PropertyInfo prop; \
                prop.name = #member; \
                prop.display_name = displayName; \
                REFLECT_OFFSETOF_PUSH \
                prop.offset = offsetof(CurrentType, member); \
                REFLECT_OFFSETOF_POP \
                prop.size = sizeof(CurrentType::member); \
                prop.type = get_property_type<decltype(CurrentType::member)>(); \
                prop.flags = propFlags; \
                info.add_property(prop); \
            }

/// Reflect a property with range (for sliders).
#define REFLECT_PROPERTY_RANGE(member, displayName, minVal, maxVal, stepVal) \
            { \
                PropertyInfo prop; \
                prop.name = #member; \
                prop.display_name = displayName; \
                REFLECT_OFFSETOF_PUSH \
                prop.offset = offsetof(CurrentType, member); \
                REFLECT_OFFSETOF_POP \
                prop.size = sizeof(CurrentType::member); \
                prop.type = get_property_type<decltype(CurrentType::member)>(); \
                prop.flags = PropertyFlags::Slider; \
                prop.min_value = minVal; \
                prop.max_value = maxVal; \
                prop.step = stepVal; \
                info.add_property(prop); \
            }

/// End type reflection and register.
#define REFLECT_TYPE_END() \
        } \
    }; \
    } /* namespace engine::reflection */

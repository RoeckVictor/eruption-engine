#pragma once

#include <entt/entt.hpp>
#include <typeindex>
#include <unordered_map>
#include <functional>

namespace editor {

// Helper for dynamic component access via type_index.
// Maps type_index to functions that can check/get components at runtime.
class ComponentTypeRegistry {
public:
    using HasComponentFunc = std::function<bool(entt::registry&, entt::entity)>;
    using GetComponentFunc = std::function<void*(entt::registry&, entt::entity)>;
    using CopyComponentFunc = std::function<void(entt::registry& src, entt::entity src_entity, entt::registry& dst, entt::entity dst_entity)>;
    using CreateComponentFunc = std::function<void*(entt::registry&, entt::entity)>;
    using RemoveComponentFunc = std::function<void(entt::registry&, entt::entity)>;

    struct ComponentTypeHandler {
        HasComponentFunc has_component;
        GetComponentFunc get_component;
        CopyComponentFunc copy_component;
        CreateComponentFunc create_component;
        RemoveComponentFunc remove_component;
    };

    // Register a component type with its handler functions.
    template<typename T>
    void register_component() {
        auto type = std::type_index(typeid(T));

        m_handlers[type] = ComponentTypeHandler{
            // Check if entity has component
            [](entt::registry& reg, entt::entity entity) -> bool {
                return reg.all_of<T>(entity);
            },
            // Get component pointer
            [](entt::registry& reg, entt::entity entity) -> void* {
                if (reg.all_of<T>(entity)) {
                    return &reg.get<T>(entity);
                }
                return nullptr;
            },
            // Copy component from src to dst
            [](entt::registry& src, entt::entity src_entity, entt::registry& dst, entt::entity dst_entity) {
                if (src.all_of<T>(src_entity)) {
                    dst.emplace_or_replace<T>(dst_entity, src.get<T>(src_entity));
                }
            },
            // Create component and return pointer
            [](entt::registry& reg, entt::entity entity) -> void* {
                return &reg.emplace_or_replace<T>(entity);
            },
            // Remove component
            [](entt::registry& reg, entt::entity entity) {
                if (reg.all_of<T>(entity)) {
                    reg.remove<T>(entity);
                }
            }
        };
    }

    bool has_component(entt::registry& registry, entt::entity entity, std::type_index type) const {
        auto it = m_handlers.find(type);
        if (it != m_handlers.end()) {
            return it->second.has_component(registry, entity);
        }
        return false;
    }

    void* get_component(entt::registry& registry, entt::entity entity, std::type_index type) const {
        auto it = m_handlers.find(type);
        if (it != m_handlers.end()) {
            return it->second.get_component(registry, entity);
        }
        return nullptr;
    }

    void copy_all_components(entt::registry& src, entt::entity src_entity, entt::registry& dst, entt::entity dst_entity) const {
        for (const auto& [type, handler] : m_handlers) {
            handler.copy_component(src, src_entity, dst, dst_entity);
        }
    }

    void* create_component(entt::registry& registry, entt::entity entity, std::type_index type) const {
        auto it = m_handlers.find(type);
        if (it != m_handlers.end()) {
            return it->second.create_component(registry, entity);
        }
        return nullptr;
    }

    void remove_component(entt::registry& registry, entt::entity entity, std::type_index type) const {
        auto it = m_handlers.find(type);
        if (it != m_handlers.end()) {
            it->second.remove_component(registry, entity);
        }
    }

    static ComponentTypeRegistry& instance() {
        static ComponentTypeRegistry registry;
        return registry;
    }

private:
    ComponentTypeRegistry() = default;
    std::unordered_map<std::type_index, ComponentTypeHandler> m_handlers;
};

}

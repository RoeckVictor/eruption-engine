#pragma once

#include "engine/core/Log.h"
#include <any>
#include <functional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine {

/// A lightweight registry for engine subsystems.
/// Allows registration and lookup of subsystems by type, enabling:
/// - Extension without modifying Engine.h
/// - Systems requesting specific subsystems instead of entire Engine
/// - Runtime subsystem swapping (for testing, alternate implementations)
///
/// Usage:
///   // In Engine or Application init:
///   registry.register_subsystem<AudioSystem>(my_audio_system);
///
///   // In a system that needs audio:
///   auto* audio = registry.get<AudioSystem>();
///   if (audio) audio->play_sound("jump.wav");
///
class SubsystemRegistry {
public:
    /// Register a subsystem instance. The registry does NOT own the pointer.
    /// Caller must ensure the subsystem outlives the registry.
    template<typename T>
    void register_subsystem(T& subsystem) {
        auto key = std::type_index(typeid(T));
        m_subsystems[key] = &subsystem;
        ENGINE_LOG("SubsystemRegistry: Registered %s", typeid(T).name());
    }

    /// Register a subsystem with a custom name (for logging/debugging).
    template<typename T>
    void register_subsystem(T& subsystem, const std::string& name) {
        auto key = std::type_index(typeid(T));
        m_subsystems[key] = &subsystem;
        m_names[key] = name;
        ENGINE_LOG("SubsystemRegistry: Registered %s", name.c_str());
    }

    /// Unregister a subsystem by type.
    template<typename T>
    void unregister_subsystem() {
        auto key = std::type_index(typeid(T));
        m_subsystems.erase(key);
        m_names.erase(key);
    }

    /// Get a registered subsystem. Returns nullptr if not found.
    template<typename T>
    T* get() const {
        auto key = std::type_index(typeid(T));
        auto it = m_subsystems.find(key);
        if (it == m_subsystems.end()) {
            return nullptr;
        }
        return static_cast<T*>(it->second);
    }

    /// Get a registered subsystem, throwing if not found.
    template<typename T>
    T& require() const {
        T* ptr = get<T>();
        if (!ptr) {
            throw std::runtime_error(
                std::string("Required subsystem not registered: ") + typeid(T).name());
        }
        return *ptr;
    }

    /// Check if a subsystem is registered.
    template<typename T>
    bool has() const {
        auto key = std::type_index(typeid(T));
        return m_subsystems.find(key) != m_subsystems.end();
    }

    /// Clear all registered subsystems.
    void clear() {
        m_subsystems.clear();
        m_names.clear();
    }

private:
    std::unordered_map<std::type_index, void*> m_subsystems;
    std::unordered_map<std::type_index, std::string> m_names;
};

} // namespace engine

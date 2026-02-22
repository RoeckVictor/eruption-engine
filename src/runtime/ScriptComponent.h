#pragma once

#include "ComponentScript.h"
#include <vector>
#include <memory>
#include <string>

namespace runtime {

/// Component that holds script instances attached to an entity.
/// An entity can have multiple scripts attached.
struct ScriptComponent {
    /// List of attached scripts (only populated during play mode).
    std::vector<std::unique_ptr<ComponentScript>> scripts;

    /// Script type names (for serialization/recreation after hot-reload).
    std::vector<std::string> script_types;

    /// Serialized properties for each script (indexed parallel to script_types).
    /// Persists script configuration during edit mode when instances don't exist.
    std::vector<nlohmann::json> script_properties;

    ScriptComponent() = default;

    // Move-only (scripts are unique_ptr)
    ScriptComponent(ScriptComponent&&) = default;
    ScriptComponent& operator=(ScriptComponent&&) = default;
    ScriptComponent(const ScriptComponent&) = delete;
    ScriptComponent& operator=(const ScriptComponent&) = delete;

    /// Add a script to this component.
    template<typename T, typename... Args>
    T* add_script(Args&&... args) {
        static_assert(std::is_base_of_v<ComponentScript, T>, "T must derive from ComponentScript");
        auto script = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = script.get();
        scripts.push_back(std::move(script));
        script_types.push_back(ptr->type_name());
        script_properties.push_back(nlohmann::json::object());
        return ptr;
    }

    /// Add a script by raw pointer (takes ownership).
    void add_script(ComponentScript* script) {
        if (script) {
            script_types.push_back(script->type_name());
            scripts.push_back(std::unique_ptr<ComponentScript>(script));
            script_properties.push_back(nlohmann::json::object());
        }
    }

    /// Add a script by type name only (edit mode, no instance).
    void add_script_type(const std::string& type_name, const nlohmann::json& properties = nlohmann::json::object()) {
        script_types.push_back(type_name);
        script_properties.push_back(properties);
    }

    /// Get properties for a script by index, ensuring the vector is sized correctly.
    nlohmann::json& get_properties(size_t index) {
        while (script_properties.size() <= index) {
            script_properties.push_back(nlohmann::json::object());
        }
        return script_properties[index];
    }

    /// Get a script by type.
    template<typename T>
    T* get_script() {
        for (auto& script : scripts) {
            if (auto* ptr = dynamic_cast<T*>(script.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    /// Get a script by type (const version).
    template<typename T>
    const T* get_script() const {
        for (const auto& script : scripts) {
            if (auto* ptr = dynamic_cast<const T*>(script.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    /// Get a script by type name.
    ComponentScript* get_script_by_name(const std::string& type_name) {
        for (size_t i = 0; i < scripts.size(); ++i) {
            if (script_types[i] == type_name) {
                return scripts[i].get();
            }
        }
        return nullptr;
    }

    /// Remove a script by type.
    template<typename T>
    bool remove_script() {
        for (size_t i = 0; i < scripts.size(); ++i) {
            if (dynamic_cast<T*>(scripts[i].get())) {
                scripts[i]->on_destroy();
                scripts.erase(scripts.begin() + i);
                if (i < script_types.size()) {
                    script_types.erase(script_types.begin() + i);
                }
                if (i < script_properties.size()) {
                    script_properties.erase(script_properties.begin() + i);
                }
                return true;
            }
        }
        return false;
    }

    /// Remove a script by type name.
    bool remove_script_by_name(const std::string& type_name) {
        for (size_t i = 0; i < script_types.size(); ++i) {
            if (script_types[i] == type_name) {
                // Scripts instances may not exist (edit mode only has type names)
                if (i < scripts.size() && scripts[i]) {
                    scripts[i]->on_destroy();
                    scripts.erase(scripts.begin() + i);
                }
                script_types.erase(script_types.begin() + i);
                if (i < script_properties.size()) {
                    script_properties.erase(script_properties.begin() + i);
                }
                return true;
            }
        }
        return false;
    }

    /// Check if this component has any scripts (type names or live instances).
    bool empty() const { return script_types.empty(); }

    /// Get the number of scripts (type names, which exist in both edit and play mode).
    size_t size() const { return script_types.size(); }
};

} // namespace runtime

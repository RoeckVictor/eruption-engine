#pragma once

#include "ComponentScript.h"
#include <vector>
#include <memory>
#include <string>

namespace runtime {

/// Component that holds script instances attached to an entity.
/// An entity can have multiple scripts attached.
struct ScriptComponent {
    /// List of attached scripts.
    std::vector<std::unique_ptr<ComponentScript>> scripts;

    /// Script type names (for serialization/recreation after hot-reload).
    std::vector<std::string> script_types;

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
        return ptr;
    }

    /// Add a script by raw pointer (takes ownership).
    void add_script(ComponentScript* script) {
        if (script) {
            script_types.push_back(script->type_name());
            scripts.push_back(std::unique_ptr<ComponentScript>(script));
        }
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
                return true;
            }
        }
        return false;
    }

    /// Check if this component has any scripts (type names or live instances).
    bool empty() const { return script_types.empty(); }

    /// Get the number of scripts.
    size_t size() const { return scripts.size(); }
};

} // namespace runtime

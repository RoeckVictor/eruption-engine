#pragma once

#include "TypeInfo.h"
#include "engine/core/Logger.h"
#include <unordered_map>
#include <memory>
#include <typeindex>

namespace engine::reflection {

/// Global registry of all reflected types.
///
/// Thread safety: Registration must happen before any concurrent reads
/// (typically during init_engine_reflections() at startup). After that,
/// all const accessors (get, get_by_name, etc.) are safe to call from
/// any thread. Do not register new types after initialization.
class TypeRegistry {
public:
    /// Get the global instance.
    static TypeRegistry& instance() {
        static TypeRegistry registry;
        return registry;
    }

    /// Register a type. Logs a warning if the type is already registered.
    void register_type(std::unique_ptr<TypeInfo> info) {
        auto type_idx = info->type_index();
        auto name = info->name();

        if (m_types_by_index.find(type_idx) != m_types_by_index.end()) {
            Logger::instance().warning("TypeRegistry",
                "Duplicate registration for type '%s' -- previous registration will be overwritten",
                name.c_str());
        }

        m_types_by_index[type_idx] = info.get();
        m_types_by_name[name] = info.get();
        m_type_ptrs.push_back(info.get());
        m_owned_types.push_back(std::move(info));
    }

    /// Get type info by C++ type.
    template<typename T>
    const TypeInfo* get() const {
        return get_by_type_index(std::type_index(typeid(T)));
    }

    /// Get type info by type_index.
    const TypeInfo* get_by_type_index(std::type_index idx) const {
        auto it = m_types_by_index.find(idx);
        if (it != m_types_by_index.end()) {
            return it->second;
        }
        return nullptr;
    }

    /// Get type info by name.
    const TypeInfo* get_by_name(const std::string& name) const {
        auto it = m_types_by_name.find(name);
        if (it != m_types_by_name.end()) {
            return it->second;
        }
        return nullptr;
    }

    /// Get all registered types (no allocation — returns a reference to an internal cache).
    const std::vector<const TypeInfo*>& all_types() const { return m_type_ptrs; }

    /// Get all registered types (legacy — allocates a new vector each call).
    /// Prefer all_types() for hot paths.
    std::vector<const TypeInfo*> get_all_types() const {
        return m_type_ptrs;
    }

    /// Get all types in a category.
    std::vector<const TypeInfo*> get_types_in_category(const std::string& category) const {
        std::vector<const TypeInfo*> result;
        for (const auto& type : m_owned_types) {
            if (type->category() == category) {
                result.push_back(type.get());
            }
        }
        return result;
    }

    /// Clear all registered types (mainly for testing).
    void clear() {
        m_types_by_index.clear();
        m_types_by_name.clear();
        m_type_ptrs.clear();
        m_owned_types.clear();
    }

private:
    TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    std::vector<std::unique_ptr<TypeInfo>> m_owned_types;
    std::vector<const TypeInfo*> m_type_ptrs;  // Parallel cache for all_types()
    std::unordered_map<std::type_index, TypeInfo*> m_types_by_index;
    std::unordered_map<std::string, TypeInfo*> m_types_by_name;
};

} // namespace engine::reflection

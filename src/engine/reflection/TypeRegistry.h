#pragma once

#include "TypeInfo.h"
#include <unordered_map>
#include <memory>
#include <typeindex>

namespace engine::reflection {

/// Global registry of all reflected types.
/// Thread-safe for registration (typically done at static init time).
class TypeRegistry {
public:
    /// Get the global instance.
    static TypeRegistry& instance() {
        static TypeRegistry registry;
        return registry;
    }

    /// Register a type.
    void register_type(std::unique_ptr<TypeInfo> info) {
        auto type_idx = info->type_index();
        auto name = info->name();
        m_types_by_index[type_idx] = info.get();
        m_types_by_name[name] = info.get();
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

    /// Get all registered types.
    std::vector<const TypeInfo*> get_all_types() const {
        std::vector<const TypeInfo*> result;
        result.reserve(m_owned_types.size());
        for (const auto& type : m_owned_types) {
            result.push_back(type.get());
        }
        return result;
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
        m_owned_types.clear();
    }

private:
    TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    std::vector<std::unique_ptr<TypeInfo>> m_owned_types;
    std::unordered_map<std::type_index, TypeInfo*> m_types_by_index;
    std::unordered_map<std::string, TypeInfo*> m_types_by_name;
};

} // namespace engine::reflection

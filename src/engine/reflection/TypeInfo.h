#pragma once

#include "PropertyInfo.h"
#include <string>
#include <vector>
#include <functional>
#include <typeindex>

namespace engine::reflection {

/// Metadata for a reflected type (component, struct, etc.).
class TypeInfo {
public:
    TypeInfo() = default;

    TypeInfo(const std::string& name, size_t size, std::type_index type_index)
        : m_name(name)
        , m_size(size)
        , m_type_index(type_index)
    {}

    /// Get the type name.
    const std::string& name() const { return m_name; }

    /// Get the size of the type in bytes.
    size_t size() const { return m_size; }

    /// Get the C++ type_index.
    std::type_index type_index() const { return m_type_index; }

    /// Get all properties.
    const std::vector<PropertyInfo>& properties() const { return m_properties; }

    /// Get a property by name.
    const PropertyInfo* get_property(const std::string& name) const {
        for (const auto& prop : m_properties) {
            if (prop.name == name) {
                return &prop;
            }
        }
        return nullptr;
    }

    /// Add a property.
    void add_property(PropertyInfo prop) {
        m_properties.push_back(std::move(prop));
    }

    /// Set the factory function for creating instances.
    void set_factory(std::function<void*()> factory) {
        m_factory = std::move(factory);
    }

    /// Create a new instance of this type (heap allocated).
    void* create_instance() const {
        if (m_factory) {
            return m_factory();
        }
        return nullptr;
    }

    /// Set the category for grouping in UI.
    void set_category(const std::string& category) {
        m_category = category;
    }

    const std::string& category() const { return m_category; }

    /// Set the description/tooltip.
    void set_description(const std::string& desc) {
        m_description = desc;
    }

    const std::string& description() const { return m_description; }

private:
    std::string m_name;
    size_t m_size = 0;
    std::type_index m_type_index = std::type_index(typeid(void));
    std::vector<PropertyInfo> m_properties;
    std::function<void*()> m_factory;
    std::string m_category;
    std::string m_description;
};

} // namespace engine::reflection

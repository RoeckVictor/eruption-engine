#pragma once

#include <string>
#include <typeindex>

namespace editor {

// Manages clipboard data for entity and component copy/paste operations
class ClipboardContext {
public:
    ClipboardContext() = default;
    ~ClipboardContext() = default;

    ClipboardContext(const ClipboardContext&) = delete;
    ClipboardContext& operator=(const ClipboardContext&) = delete;

    void set_entity_clipboard(const std::string& json);
    const std::string& entity_clipboard() const { return m_entity_clipboard; }
    bool has_entity_clipboard() const { return !m_entity_clipboard.empty(); }
    void clear_entity_clipboard() { m_entity_clipboard.clear(); }
    void set_component_clipboard(const std::string& data, std::type_index type);
    const std::string& component_clipboard() const { return m_component_clipboard; }
    std::type_index component_clipboard_type() const { return m_component_clipboard_type; }
    bool has_component_clipboard() const { return !m_component_clipboard.empty(); }
    void clear_component_clipboard();

private:
    std::string m_entity_clipboard;
    std::string m_component_clipboard;
    std::type_index m_component_clipboard_type = std::type_index(typeid(void));
};

}

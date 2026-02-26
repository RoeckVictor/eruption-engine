#include "ClipboardContext.h"

namespace editor {

void ClipboardContext::set_entity_clipboard(const std::string& json) {
    m_entity_clipboard = json;
}

void ClipboardContext::set_component_clipboard(const std::string& data, std::type_index type) {
    m_component_clipboard = data;
    m_component_clipboard_type = type;
}

void ClipboardContext::clear_component_clipboard() {
    m_component_clipboard.clear();
    m_component_clipboard_type = std::type_index(typeid(void));
}

}

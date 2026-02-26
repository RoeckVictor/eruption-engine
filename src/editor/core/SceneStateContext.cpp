#include "SceneStateContext.h"

namespace editor {

void SceneStateContext::mark_dirty() {
    if (m_dirty_override) {
        m_dirty_override();
    } else {
        m_dirty = true;
    }
}

void SceneStateContext::set_dirty_override(DirtyOverrideCallback callback) {
    m_dirty_override = std::move(callback);
}

void SceneStateContext::clear_dirty_override() {
    m_dirty_override = nullptr;
}

}
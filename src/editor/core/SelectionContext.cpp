#include "SelectionContext.h"

namespace editor {

const std::vector<entt::entity>& SelectionContext::selection() const {
    if (m_override_selection) {
        return *m_override_selection;
    }
    return m_selection;
}

std::vector<entt::entity>& SelectionContext::current_selection() {
    if (m_override_selection) {
        return *m_override_selection;
    }
    return m_selection;
}

std::unordered_set<entt::entity>& SelectionContext::current_selection_set() {
    if (m_override_selection) {
        return m_override_selection_set;
    }
    return m_selection_set;
}

bool SelectionContext::is_selected(entt::entity entity) const {
    if (m_override_selection) {
        return m_override_selection_set.contains(entity);
    }
    return m_selection_set.contains(entity);
}

void SelectionContext::select(entt::entity entity) {
    auto& sel = current_selection();
    auto& sel_set = current_selection_set();

    sel.clear();
    sel_set.clear();

    if (entity != entt::null) {
        sel.push_back(entity);
        sel_set.insert(entity);
    }

    notify_selection_changed();
}

void SelectionContext::add_to_selection(entt::entity entity) {
    if (entity == entt::null) return;

    auto& sel_set = current_selection_set();
    if (sel_set.contains(entity)) return;

    current_selection().push_back(entity);
    sel_set.insert(entity);
    notify_selection_changed();
}

void SelectionContext::remove_from_selection(entt::entity entity) {
    auto& sel = current_selection();
    auto& sel_set = current_selection_set();

    if (!sel_set.contains(entity)) return;

    sel_set.erase(entity);
    sel.erase(std::remove(sel.begin(), sel.end(), entity), sel.end());
    notify_selection_changed();
}

void SelectionContext::clear_selection() {
    auto& sel = current_selection();
    auto& sel_set = current_selection_set();

    if (sel.empty()) return;

    sel.clear();
    sel_set.clear();
    notify_selection_changed();
}

void SelectionContext::select_multiple(const std::vector<entt::entity>& entities) {
    auto& sel = current_selection();
    auto& sel_set = current_selection_set();

    sel.clear();
    sel_set.clear();

    for (auto entity : entities) {
        if (entity != entt::null && !sel_set.contains(entity)) {
            sel.push_back(entity);
            sel_set.insert(entity);
        }
    }

    notify_selection_changed();
}

void SelectionContext::set_selection_changed_callback(SelectionChangedCallback callback) {
    m_callback = std::move(callback);
}

void SelectionContext::set_selection_override(std::vector<entt::entity>* override_selection) {
    m_override_selection = override_selection;

    // Rebuild the override selection set
    m_override_selection_set.clear();
    if (m_override_selection) {
        for (auto entity : *m_override_selection) {
            m_override_selection_set.insert(entity);
        }
    }
}

void SelectionContext::clear_selection_override() {
    m_override_selection = nullptr;
    m_override_selection_set.clear();
}

void SelectionContext::notify_selection_changed() {
    if (m_callback) {
        m_callback();
    }
}

}

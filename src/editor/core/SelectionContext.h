#pragma once

#include <entt/entt.hpp>
#include <vector>
#include <functional>
#include <unordered_set>

namespace editor {

// Manages entity selection with optional override support
// The override mechanism allows systems like the prefab editor to
// redirect selection operations to an isolated registry
class SelectionContext {
public:
    SelectionContext() = default;
    ~SelectionContext() = default;

    SelectionContext(const SelectionContext&) = delete;
    SelectionContext& operator=(const SelectionContext&) = delete;

    const std::vector<entt::entity>& selection() const;
    bool is_selected(entt::entity entity) const;
    void select(entt::entity entity);
    void add_to_selection(entt::entity entity);
    void remove_from_selection(entt::entity entity);
    void clear_selection();
    void select_multiple(const std::vector<entt::entity>& entities);

    using SelectionChangedCallback = std::function<void()>;
    void set_selection_changed_callback(SelectionChangedCallback callback);

    void set_selection_override(std::vector<entt::entity>* override_selection);
    void clear_selection_override();
    bool has_selection_override() const { return m_override_selection != nullptr; }

private:
    void notify_selection_changed();

    std::vector<entt::entity>& current_selection();
    std::unordered_set<entt::entity>& current_selection_set();

    std::vector<entt::entity> m_selection;
    std::unordered_set<entt::entity> m_selection_set;

    std::vector<entt::entity>* m_override_selection = nullptr;
    std::unordered_set<entt::entity> m_override_selection_set;

    SelectionChangedCallback m_callback;
};

}

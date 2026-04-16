#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace engine::ui {

/// Dropdown component for selecting from a list of options.
/// Requires UIInteractable component for interaction handling.
struct Dropdown {
    bool enabled = true;

    /// Currently selected option index (-1 = no selection)
    int selected_index = 0;

    /// List of option labels
    std::vector<std::string> options;

    /// Maximum visible items before scrolling (0 = show all)
    int max_visible_items = 5;

    /// Height of each option item in pixels
    float item_height = 32.0f;

    /// Child entity references
    entt::entity selected_text = entt::null;    // Text showing current selection
    entt::entity arrow = entt::null;            // Dropdown arrow image (optional)
    entt::entity options_panel = entt::null;    // Panel containing options (shown/hidden)
    entt::entity options_scrollview = entt::null; // ScrollView for scrolling options (optional)
    entt::entity options_content = entt::null;  // Container for option items

    // --- Runtime State (not serialized) ---
    bool _is_open = false;
    bool _options_dirty = true;  // Set true when options change, triggers rebuild of option items
    std::vector<entt::entity> _option_entities;  // Dynamically created option button entities

    /// Get the currently selected option label
    std::string get_selected_text() const {
        if (selected_index >= 0 && selected_index < static_cast<int>(options.size())) {
            return options[selected_index];
        }
        return "";
    }

    /// Set selection by label (returns true if found)
    bool set_selected(const std::string& label) {
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            if (options[i] == label) {
                selected_index = i;
                return true;
            }
        }
        return false;
    }
};

/// Internal component attached to dynamically created option entities.
/// Used by UIInteractionSystem to identify option clicks.
struct DropdownOption {
    entt::entity dropdown_entity = entt::null;
    int option_index = -1;
};

} // namespace engine::ui

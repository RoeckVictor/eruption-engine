#pragma once

#include <entt/entt.hpp>
#include <imgui.h>
#include <vector>

namespace editor {

class EditorContext;

/// Result of hit detection at a point
struct HitResult {
    std::vector<entt::entity> entities;  // All entities under cursor
};

/// State for click-to-select cycling through overlapping entities.
struct ClickCycleState {
    std::vector<entt::entity> last_hit_entities;
    int last_hit_index = -1;
    ImVec2 last_click_pos{0, 0};

    /// Reset the cycling state.
    void reset() {
        last_hit_entities.clear();
        last_hit_index = -1;
    }
};

/// Utility for detecting entities at screen positions in the viewport.
class EntityHitDetector {
public:
    /// Find all entities at the given screen position.
    /// @param registry The entity registry
    /// @param screen_pos Mouse position in screen coordinates
    /// @param viewport_pos Top-left of viewport
    /// @param viewport_size Size of viewport
    /// @param cam_x Camera X position
    /// @param cam_y Camera Y position
    /// @param zoom Camera zoom level
    /// @return HitResult containing all entities under the cursor
    static HitResult hit_test(
        entt::registry& registry,
        ImVec2 screen_pos,
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        float cam_x,
        float cam_y,
        float zoom
    );

    /// Process a click for entity selection with cycling support.
    /// Handles Ctrl/Shift modifiers for multi-selection.
    /// @param context Editor context for selection management
    /// @param hit Hit result from hit_test()
    /// @param mouse_pos Current mouse position
    /// @param cycle_state State for tracking click cycling (modified in place)
    /// @param ctrl_held Whether Ctrl key is held (toggle selection)
    /// @param shift_held Whether Shift key is held (add to selection)
    static void process_click_selection(
        EditorContext& context,
        const HitResult& hit,
        ImVec2 mouse_pos,
        ClickCycleState& cycle_state,
        bool ctrl_held,
        bool shift_held
    );
};

} // namespace editor

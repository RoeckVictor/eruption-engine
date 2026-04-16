#pragma once

#include "engine/core/System.h"
#include "engine/ui/UIInteractable.h"  // Need full definition for State enum
#include <entt/entt.hpp>
#include <vector>

namespace engine {
class Engine;
}

namespace engine::ui {

struct Button;
struct Slider;
struct Checkbox;
struct Panel;
struct ScrollView;
struct Dropdown;

/// System that handles UI interaction (hit testing, visual states, callbacks).
/// Processes UIInteractable components and their specialized widget types.
/// Supports both screen-space (ScreenRect) and world-space (Transform) UI.
class UIInteractionSystem : public System {
public:
    const char* name() const override { return "UIInteractionSystem"; }
    bool init(Engine& engine) override;
    void update(Engine& engine, float dt) override;
    void shutdown() override;

private:
    // Hit test result
    struct HitResult {
        entt::entity entity = entt::null;
        float local_x = 0.0f;  // Position within the element (0-1 normalized)
        float local_y = 0.0f;
        int layer = 0;         // Render layer for sorting
        bool is_screen_space = true;
    };

    /// Perform raycast against all UIInteractable elements
    /// Returns hits sorted by layer (highest layer = frontmost = first)
    std::vector<HitResult> raycast_ui(Engine& engine, float screen_x, float screen_y);

    /// Check if point is inside a screen-space rect
    bool point_in_screen_rect(entt::registry& reg, entt::entity entity,
                              float screen_x, float screen_y,
                              float& out_local_x, float& out_local_y);

    /// Check if point is inside a world-space element
    bool point_in_world_rect(Engine& engine, entt::registry& reg, entt::entity entity,
                             float screen_x, float screen_y,
                             float& out_local_x, float& out_local_y);

    /// Update visual state of a UIInteractable (handles color tint and sprite swap)
    void update_visual_state(entt::registry& reg, entt::entity entity,
                            UIInteractable& ui, UIInteractable::State new_state);

    /// Dispatch a callback to all scripts on the target entity.
    /// The callback_fn receives a reference to each script instance.
    template<typename Fn>
    void dispatch_to_scripts(entt::registry& reg, entt::entity ui_entity, Fn&& callback_fn);

    /// Typed dispatch helpers (delegate to dispatch_to_scripts)
    void dispatch_button_click(entt::registry& reg, entt::entity button_entity);
    void dispatch_button_press(entt::registry& reg, entt::entity button_entity);
    void dispatch_button_release(entt::registry& reg, entt::entity button_entity);
    void dispatch_pointer_enter(entt::registry& reg, entt::entity ui_entity);
    void dispatch_pointer_exit(entt::registry& reg, entt::entity ui_entity);
    void dispatch_slider_changed(entt::registry& reg, entt::entity slider_entity, float value);
    void dispatch_slider_drag_start(entt::registry& reg, entt::entity slider_entity);
    void dispatch_slider_drag_end(entt::registry& reg, entt::entity slider_entity);
    void dispatch_checkbox_changed(entt::registry& reg, entt::entity checkbox_entity, bool checked);
    void dispatch_scroll(entt::registry& reg, entt::entity scrollview_entity, float scroll_x, float scroll_y);
    void dispatch_panel_drag(entt::registry& reg, entt::entity panel_entity, float delta_x, float delta_y);
    void dispatch_dropdown_changed(entt::registry& reg, entt::entity dropdown_entity, int selected_index);

    /// Get the callback target entity (respects UIInteractable::callback_target)
    entt::entity get_callback_target(entt::registry& reg, entt::entity ui_entity);

    /// Handle slider dragging
    void update_slider_value(entt::registry& reg, entt::entity entity, Slider& slider,
                            float screen_x, float screen_y, const HitResult& original_hit);

    /// Handle checkbox toggle groups
    void handle_toggle_group(entt::registry& reg, entt::entity checkbox_entity, Checkbox& checkbox);

    /// Handle scrollview scrolling
    void update_scrollview(entt::registry& reg, entt::entity entity, ScrollView& sv, float dt);

    /// Handle panel dragging
    void update_panel_drag(entt::registry& reg, entt::entity entity, Panel& panel,
                          float screen_x, float screen_y);

    /// Handle dropdown open/close and selection
    void toggle_dropdown(entt::registry& reg, entt::entity entity, Dropdown& dropdown);
    void close_dropdown(entt::registry& reg, entt::entity entity, Dropdown& dropdown);
    void select_dropdown_option(entt::registry& reg, entt::entity dropdown_entity, Dropdown& dropdown, int index);
    void update_dropdown_visuals(entt::registry& reg, entt::entity entity, Dropdown& dropdown);
    void rebuild_dropdown_options(entt::registry& reg, entt::entity entity, Dropdown& dropdown);
    void clear_dropdown_options(entt::registry& reg, Dropdown& dropdown);

    // Interaction state
    entt::entity m_hovered_entity = entt::null;
    entt::entity m_pressed_entity = entt::null;
    entt::entity m_dragging_slider_entity = entt::null;  // The slider being dragged (may differ from pressed entity if handle was clicked)
    entt::entity m_dragging_scrollview_entity = entt::null;  // ScrollView being dragged (content or scrollbar handle)
    bool m_dragging_scrollbar_handle = false;  // True if dragging scrollbar handle, false if dragging content
    entt::entity m_open_dropdown_entity = entt::null;    // Currently open dropdown (only one can be open at a time)
    HitResult m_pressed_hit;  // Hit info when press started (for slider value calculation)

    float m_last_mouse_x = 0.0f;
    float m_last_mouse_y = 0.0f;

    // Viewport bounds (for editor where scene view is not at window origin)
    float m_viewport_x = 0.0f;
    float m_viewport_y = 0.0f;
    float m_viewport_w = 0.0f;  // 0 = use window size
    float m_viewport_h = 0.0f;

    // Registry to use (may differ from engine's scene stack in editor)
    entt::registry* m_registry = nullptr;

public:
    /// Set viewport bounds for converting window mouse coordinates to viewport-relative
    void set_viewport_offset(float x, float y) { m_viewport_x = x; m_viewport_y = y; }
    void set_viewport_size(float w, float h) { m_viewport_w = w; m_viewport_h = h; }

    /// Set the registry to query for UI entities (use this in editor mode)
    void set_registry(entt::registry* reg) { m_registry = reg; }
};

} // namespace engine::ui

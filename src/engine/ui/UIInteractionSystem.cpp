#include "engine/ui/UIInteractionSystem.h"
#include "engine/ui/UIComponents.h"
#include "engine/core/Engine.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/Transform.h"
#include "engine/core/Hierarchy.h"
#include "engine/render/Image.h"
#include "engine/render/Camera2D.h"
#include "engine/render/Text.h"
#include "engine/scene/SceneManager.h"
#include "engine/platform/Input.h"
#include "engine/platform/KeyCode.h"
#include "engine/profiler/Profiler.h"
#include "runtime/ScriptComponent.h"
#include <algorithm>
#include <cmath>

namespace engine::ui {

// Helper to recursively set visibility for an entity and all descendants
// Works for both screen-space (ScreenRect) and world-space (Transform + Image) entities
static void set_screenrect_enabled_recursive(entt::registry& reg, entt::entity entity, bool enabled) {
    if (!reg.valid(entity)) return;

    // Handle screen-space entities
    ScreenRect* rect = reg.try_get<ScreenRect>(entity);
    if (rect) {
        rect->enabled = enabled;
    }

    // Handle world-space entities (Transform + Image)
    // Only toggle Image if there's no ScreenRect (pure world-space entity)
    if (!rect) {
        render::Image* img = reg.try_get<render::Image>(entity);
        if (img) {
            img->enabled = enabled;
        }
        render::Text* txt = reg.try_get<render::Text>(entity);
        if (txt) {
            txt->enabled = enabled;
        }
    }

    Hierarchy* hierarchy = reg.try_get<Hierarchy>(entity);
    if (hierarchy) {
        for (auto child : hierarchy->children) {
            set_screenrect_enabled_recursive(reg, child, enabled);
        }
    }
}

// Helper to remove an entity from its parent's children list
static void remove_from_parent(entt::registry& reg, entt::entity entity) {
    if (!reg.valid(entity)) return;

    Hierarchy* hier = reg.try_get<Hierarchy>(entity);
    if (!hier || hier->parent == entt::null || !reg.valid(hier->parent)) return;

    Hierarchy* parent_hier = reg.try_get<Hierarchy>(hier->parent);
    if (parent_hier) {
        auto& children = parent_hier->children;
        children.erase(std::remove(children.begin(), children.end(), entity), children.end());
    }
    hier->parent = entt::null;
}

// Helper to destroy an entity and all its descendants recursively
static void destroy_entity_recursive(entt::registry& reg, entt::entity entity) {
    if (!reg.valid(entity)) return;

    // First destroy all children
    Hierarchy* hierarchy = reg.try_get<Hierarchy>(entity);
    if (hierarchy) {
        // Copy children list since we'll be modifying it
        auto children = hierarchy->children;
        for (auto child : children) {
            destroy_entity_recursive(reg, child);
        }
    }

    remove_from_parent(reg, entity);
    reg.destroy(entity);
}

bool UIInteractionSystem::init(Engine& engine) {
    (void)engine;

    // Reset all dropdown states to ensure clean initialization
    // This is important when re-entering play mode after stopping
    if (m_registry) {
        auto dropdown_view = m_registry->view<Dropdown>();
        for (auto entity : dropdown_view) {
            Dropdown& dropdown = dropdown_view.get<Dropdown>(entity);
            // Force options to be rebuilt and close any open dropdowns
            dropdown._options_dirty = true;
            dropdown._is_open = false;
            dropdown._option_entities.clear();
        }
    }

    return true;
}

void UIInteractionSystem::shutdown() {
    m_hovered_entity = entt::null;
    m_pressed_entity = entt::null;
    m_dragging_slider_entity = entt::null;
    m_dragging_scrollview_entity = entt::null;
    m_dragging_scrollbar_handle = false;
    m_open_dropdown_entity = entt::null;
}

void UIInteractionSystem::update(Engine& engine, float dt) {
    PROFILE_SCOPE("UIInteractionSystem::update");

    // Use explicitly set registry if available, otherwise fall back to engine's scene
    entt::registry* reg_ptr = m_registry;
    if (!reg_ptr) {
        scene::Scene* scene = engine.scenes().top();
        if (!scene) return;
        reg_ptr = &scene->registry();
    }
    entt::registry& reg = *reg_ptr;
    platform::Input& input = engine.input();

    // Convert window mouse coordinates to viewport-relative coordinates
    // ScreenRect positions use actual viewport pixels (no scaling)
    float mx = static_cast<float>(input.mouse_x()) - m_viewport_x;
    float my = static_cast<float>(input.mouse_y()) - m_viewport_y;
    bool mouse_pressed = input.is_mouse_pressed(platform::MouseButton::Left);
    bool mouse_held = input.is_mouse_held(platform::MouseButton::Left);
    bool mouse_released = input.is_mouse_released(platform::MouseButton::Left);
    float scroll = input.scroll_y();

    // Initialize visual state for any UIInteractable that hasn't been initialized yet
    // This applies the normal_color/normal_sprite on scene load
    {
        auto init_view = reg.view<UIInteractable>();
        for (auto entity : init_view) {
            UIInteractable& ui = init_view.get<UIInteractable>(entity);
            if (!ui._original_color_captured && ui.enabled) {
                // Force initial visual state application
                ui._current_state = UIInteractable::State::Pressed;  // Set to different state
                update_visual_state(reg, entity, ui,
                    ui.interactable ? UIInteractable::State::Normal : UIInteractable::State::Disabled);
            }
        }
    }

    // Initialize slider visuals (fill_rect and handle) to match current value
    {
        auto slider_view = reg.view<Slider>();
        for (auto entity : slider_view) {
            Slider& slider = slider_view.get<Slider>(entity);
            if (!slider._visuals_initialized && slider.enabled) {
                slider._visuals_initialized = true;

                // Get the slider's rect for calculating fill/handle sizes
                ScreenRect* slider_rect = reg.try_get<ScreenRect>(entity);
                if (!slider_rect) continue;

                float normalized_value = slider.value;

                // Update fill rect size
                if (slider.fill_rect != entt::null && reg.valid(slider.fill_rect)) {
                    ScreenRect* fill_screen_rect = reg.try_get<ScreenRect>(slider.fill_rect);
                    if (fill_screen_rect) {
                        if (slider.direction == Slider::Direction::LeftToRight ||
                            slider.direction == Slider::Direction::RightToLeft) {
                            fill_screen_rect->width = slider_rect->width * normalized_value;
                        } else {
                            fill_screen_rect->height = slider_rect->height * normalized_value;
                        }
                    }
                }

                // Update handle position
                if (slider.handle != entt::null && reg.valid(slider.handle)) {
                    ScreenRect* handle_rect = reg.try_get<ScreenRect>(slider.handle);
                    if (handle_rect) {
                        if (slider.direction == Slider::Direction::LeftToRight ||
                            slider.direction == Slider::Direction::RightToLeft) {
                            handle_rect->anchor_x = normalized_value;
                        } else {
                            handle_rect->anchor_y = normalized_value;
                        }
                    }
                }
            }
        }
    }

    // Initialize checkbox visuals (checkmark visibility matches checked state)
    {
        auto checkbox_view = reg.view<Checkbox>();
        for (auto entity : checkbox_view) {
            Checkbox& checkbox = checkbox_view.get<Checkbox>(entity);
            if (!checkbox._visuals_initialized && checkbox.enabled) {
                checkbox._visuals_initialized = true;

                // Update checkmark visibility to match initial checked state
                if (checkbox.checkmark != entt::null && reg.valid(checkbox.checkmark)) {
                    render::Image* checkmark_img = reg.try_get<render::Image>(checkbox.checkmark);
                    if (checkmark_img) {
                        checkmark_img->enabled = checkbox.checked;
                    }
                }
            }
        }
    }

    // Initialize ScrollView visuals (set clip_to for content children, compute content size)
    {
        auto scrollview_view = reg.view<ScrollView>();
        for (auto entity : scrollview_view) {
            ScrollView& sv = scrollview_view.get<ScrollView>(entity);
            if (!sv._visuals_initialized && sv.enabled) {
                sv._visuals_initialized = true;

                // Get viewport entity for clipping
                entt::entity viewport_entity = sv.viewport;
                if (viewport_entity == entt::null || !reg.valid(viewport_entity)) continue;

                // Get content entity
                if (sv.content == entt::null || !reg.valid(sv.content)) continue;

                Hierarchy* content_hier = reg.try_get<Hierarchy>(sv.content);
                if (!content_hier) continue;

                // Set clip_to for all content children and compute content bounds
                float min_y = 0.0f;
                float max_y = 0.0f;
                bool first_child = true;

                for (auto child : content_hier->children) {
                    if (!reg.valid(child)) continue;

                    ScreenRect* child_rect = reg.try_get<ScreenRect>(child);
                    if (child_rect) {
                        // Set clip_to to viewport
                        child_rect->clip_to = viewport_entity;

                        // Track bounds (using offset_y since anchor_y=0 means top)
                        float child_top = child_rect->offset_y;
                        float child_bottom = child_rect->offset_y + child_rect->height;
                        if (first_child) {
                            min_y = child_top;
                            max_y = child_bottom;
                            first_child = false;
                        } else {
                            if (child_top < min_y) min_y = child_top;
                            if (child_bottom > max_y) max_y = child_bottom;
                        }
                    }
                }

                // Set content height from computed bounds
                if (!first_child) {
                    sv.content_height = max_y - min_y;
                }

                // Compute content width similarly for horizontal scrolling
                if (sv.horizontal) {
                    float min_x = 0.0f;
                    float max_x = 0.0f;
                    first_child = true;
                    for (auto child : content_hier->children) {
                        if (!reg.valid(child)) continue;
                        ScreenRect* child_rect = reg.try_get<ScreenRect>(child);
                        if (child_rect) {
                            float child_left = child_rect->offset_x;
                            float child_right = child_rect->offset_x + child_rect->width;
                            if (first_child) {
                                min_x = child_left;
                                max_x = child_right;
                                first_child = false;
                            } else {
                                if (child_left < min_x) min_x = child_left;
                                if (child_right > max_x) max_x = child_right;
                            }
                        }
                    }
                    if (!first_child) {
                        sv.content_width = max_x - min_x;
                    }
                }
            }
        }
    }

    // Initialize dropdown visuals (selected text, hidden options panel, create options)
    {
        auto dropdown_view = reg.view<Dropdown>();
        for (auto entity : dropdown_view) {
            Dropdown& dropdown = dropdown_view.get<Dropdown>(entity);
            if (dropdown._options_dirty && dropdown.enabled) {
                dropdown._options_dirty = false;

                // Rebuild option entities when options change
                rebuild_dropdown_options(reg, entity, dropdown);

                // Update selected text display and panel sizing
                update_dropdown_visuals(reg, entity, dropdown);

                // Ensure options panel and all children are hidden initially
                if (!dropdown._is_open && dropdown.options_panel != entt::null && reg.valid(dropdown.options_panel)) {
                    set_screenrect_enabled_recursive(reg, dropdown.options_panel, false);
                }
            }
        }
    }

    // 1. Raycast to find UI elements under cursor
    std::vector<HitResult> hits = raycast_ui(engine, mx, my);
    entt::entity top_hit = hits.empty() ? entt::null : hits[0].entity;

    // 2. Handle hover enter/exit
    if (top_hit != m_hovered_entity) {
        // Exit previous
        if (m_hovered_entity != entt::null && reg.valid(m_hovered_entity)) {
            UIInteractable* ui = reg.try_get<UIInteractable>(m_hovered_entity);
            if (ui) {
                ui->_was_hovered = false;
                dispatch_pointer_exit(reg, m_hovered_entity);

                // Only change to Normal if not currently pressed
                if (m_pressed_entity != m_hovered_entity) {
                    update_visual_state(reg, m_hovered_entity, *ui,
                        ui->interactable ? UIInteractable::State::Normal : UIInteractable::State::Disabled);
                }
            }
        }

        // Enter new
        if (top_hit != entt::null) {
            UIInteractable* ui = reg.try_get<UIInteractable>(top_hit);
            if (ui) {
                ui->_was_hovered = true;
                dispatch_pointer_enter(reg, top_hit);

                // Only change to Hovered if not currently pressed
                if (m_pressed_entity != top_hit) {
                    update_visual_state(reg, top_hit, *ui,
                        ui->interactable ? UIInteractable::State::Hovered : UIInteractable::State::Disabled);
                }
            }
        }

        m_hovered_entity = top_hit;
    }

    // 3. Handle mouse press
    if (mouse_pressed) {
        // Close open dropdown if clicking outside of it
        if (m_open_dropdown_entity != entt::null && reg.valid(m_open_dropdown_entity)) {
            Dropdown* open_dropdown = reg.try_get<Dropdown>(m_open_dropdown_entity);
            if (open_dropdown && open_dropdown->_is_open) {
                // Check if click is on the dropdown itself or its options panel
                bool click_on_dropdown = (top_hit == m_open_dropdown_entity);
                bool click_on_options_panel = false;

                // Check if click is on options panel or its children
                if (open_dropdown->options_panel != entt::null) {
                    for (const auto& hit : hits) {
                        if (hit.entity == open_dropdown->options_panel) {
                            click_on_options_panel = true;
                            break;
                        }
                        // Check if hit entity is a child of options_panel
                        if (reg.all_of<Hierarchy>(hit.entity)) {
                            entt::entity parent = reg.get<Hierarchy>(hit.entity).parent;
                            while (parent != entt::null && reg.valid(parent)) {
                                if (parent == open_dropdown->options_panel ||
                                    parent == open_dropdown->options_scrollview ||
                                    parent == open_dropdown->options_content) {
                                    click_on_options_panel = true;
                                    break;
                                }
                                if (reg.all_of<Hierarchy>(parent)) {
                                    parent = reg.get<Hierarchy>(parent).parent;
                                } else {
                                    break;
                                }
                            }
                        }
                        if (click_on_options_panel) break;
                    }
                }

                if (!click_on_dropdown && !click_on_options_panel) {
                    close_dropdown(reg, m_open_dropdown_entity, *open_dropdown);
                }
            }
        }
    }

    if (mouse_pressed && top_hit != entt::null) {
        UIInteractable* ui = reg.try_get<UIInteractable>(top_hit);
        if (ui && ui->enabled && ui->interactable) {
            m_pressed_entity = top_hit;
            m_pressed_hit = hits[0];

            update_visual_state(reg, top_hit, *ui, UIInteractable::State::Pressed);

            // Button press
            if (reg.all_of<Button>(top_hit)) {
                Button& button = reg.get<Button>(top_hit);
                if (button.enabled) {
                    button._pressed_this_frame = true;
                    dispatch_button_press(reg, top_hit);
                }
            }

            // Slider drag start - check if top_hit is a slider or its handle/fill_rect
            Slider* slider = reg.try_get<Slider>(top_hit);
            entt::entity slider_entity = top_hit;

            if (!slider) {
                auto slider_view = reg.view<Slider>();
                for (auto e : slider_view) {
                    Slider& s = slider_view.get<Slider>(e);
                    if (s.handle == top_hit || s.fill_rect == top_hit) {
                        slider = &s;
                        slider_entity = e;
                        break;
                    }
                }
            }

            if (slider && slider->enabled) {
                slider->_is_dragging = true;
                slider->_drag_start_value = slider->value;
                m_dragging_slider_entity = slider_entity;
                dispatch_slider_drag_start(reg, slider_entity);
                update_slider_value(reg, slider_entity, *slider, mx, my, m_pressed_hit);
            }

            // Panel drag start
            Panel* panel = reg.try_get<Panel>(top_hit);
            if (panel && panel->enabled && panel->draggable) {
                // Check if we should start dragging (either no drag handle, or clicked on drag handle)
                bool can_drag = (panel->drag_handle == entt::null);
                if (!can_drag && panel->drag_handle != entt::null) {
                    // Check if the click is on the drag handle
                    for (const HitResult& hit : hits) {
                        if (hit.entity == panel->drag_handle) {
                            can_drag = true;
                            break;
                        }
                    }
                }
                if (can_drag) {
                    panel->_is_dragging = true;
                    panel->_drag_offset_x = mx;
                    panel->_drag_offset_y = my;
                }
            }

            // ScrollView drag start - search hierarchy for ScrollView or scrollbar handle
            ScrollView* sv = nullptr;
            entt::entity sv_entity = entt::null;
            bool is_scrollbar_handle = false;

            // First check if clicking on a scrollbar handle
            auto sv_view = reg.view<ScrollView>();
            for (auto e : sv_view) {
                ScrollView& s = sv_view.get<ScrollView>(e);
                if (!s.enabled) continue;

                // Check if click is on vertical scrollbar handle
                if (s.vertical_scrollbar != entt::null && reg.valid(s.vertical_scrollbar)) {
                    Hierarchy* sb_hier = reg.try_get<Hierarchy>(s.vertical_scrollbar);
                    if (sb_hier && !sb_hier->children.empty()) {
                        entt::entity handle = sb_hier->children[0];
                        if (handle == top_hit) {
                            sv = &s;
                            sv_entity = e;
                            is_scrollbar_handle = true;
                            break;
                        }
                    }
                }

                // Check if click is on horizontal scrollbar handle
                if (s.horizontal_scrollbar != entt::null && reg.valid(s.horizontal_scrollbar)) {
                    Hierarchy* sb_hier = reg.try_get<Hierarchy>(s.horizontal_scrollbar);
                    if (sb_hier && !sb_hier->children.empty()) {
                        entt::entity handle = sb_hier->children[0];
                        if (handle == top_hit) {
                            sv = &s;
                            sv_entity = e;
                            is_scrollbar_handle = true;
                            break;
                        }
                    }
                }
            }

            // If not a scrollbar handle, search for ScrollView in hit list
            if (!sv) {
                for (const HitResult& hit : hits) {
                    ScrollView* s = reg.try_get<ScrollView>(hit.entity);
                    if (s && s->enabled) {
                        sv = s;
                        sv_entity = hit.entity;
                        break;
                    }
                }
            }

            if (sv) {
                sv->_is_dragging = true;
                sv->_drag_start_x = mx;
                sv->_drag_start_y = my;
                sv->_drag_start_scroll_x = sv->scroll_x;
                sv->_drag_start_scroll_y = sv->scroll_y;
                sv->velocity_x = 0.0f;
                sv->velocity_y = 0.0f;
                m_dragging_scrollview_entity = sv_entity;
                m_dragging_scrollbar_handle = is_scrollbar_handle;
            }
        } else {
            // top_hit doesn't have UIInteractable - check if it's a slider handle/fill without UIInteractable
            auto slider_view = reg.view<Slider>();
            for (auto e : slider_view) {
                Slider& s = slider_view.get<Slider>(e);
                if (s.enabled && (s.handle == top_hit || s.fill_rect == top_hit)) {
                    m_pressed_entity = top_hit;
                    m_pressed_hit = hits[0];
                    s._is_dragging = true;
                    s._drag_start_value = s.value;
                    m_dragging_slider_entity = e;
                    dispatch_slider_drag_start(reg, e);
                    update_slider_value(reg, e, s, mx, my, m_pressed_hit);
                    break;
                }
            }
        }
    }

    // 4. Handle dragging (while mouse held)
    if (mouse_held && m_pressed_entity != entt::null && reg.valid(m_pressed_entity)) {
        // Slider dragging (use stored slider entity, which may differ from pressed entity if handle was clicked)
        if (m_dragging_slider_entity != entt::null && reg.valid(m_dragging_slider_entity)) {
            Slider* slider = reg.try_get<Slider>(m_dragging_slider_entity);
            if (slider && slider->_is_dragging) {
                update_slider_value(reg, m_dragging_slider_entity, *slider, mx, my, m_pressed_hit);
            }
        }

        // Panel dragging
        Panel* panel = reg.try_get<Panel>(m_pressed_entity);
        if (panel && panel->_is_dragging) {
            update_panel_drag(reg, m_pressed_entity, *panel, mx, my);
        }

        // ScrollView dragging (use stored scrollview entity)
        if (m_dragging_scrollview_entity != entt::null && reg.valid(m_dragging_scrollview_entity)) {
            ScrollView* sv = reg.try_get<ScrollView>(m_dragging_scrollview_entity);
            if (sv && sv->_is_dragging) {
                if (m_dragging_scrollbar_handle) {
                    // Dragging scrollbar handle - convert mouse position to scroll position
                    float viewport_height = 0.0f;
                    if (sv->viewport != entt::null && reg.valid(sv->viewport)) {
                        ScreenRect* vp_rect = reg.try_get<ScreenRect>(sv->viewport);
                        if (vp_rect) {
                            viewport_height = vp_rect->computed_height > 0 ? vp_rect->computed_height : vp_rect->height;
                        }
                    }

                    float max_scroll_y = std::max(0.0f, sv->content_height - viewport_height);

                    // Get scrollbar and handle rects for position calculation
                    if (sv->vertical && sv->vertical_scrollbar != entt::null && reg.valid(sv->vertical_scrollbar)) {
                        ScreenRect* sb_rect = reg.try_get<ScreenRect>(sv->vertical_scrollbar);
                        Hierarchy* sb_hier = reg.try_get<Hierarchy>(sv->vertical_scrollbar);
                        if (sb_rect && sb_hier && !sb_hier->children.empty()) {
                            entt::entity handle = sb_hier->children[0];
                            ScreenRect* handle_rect = reg.try_get<ScreenRect>(handle);
                            if (handle_rect && max_scroll_y > 0) {
                                // Calculate mouse position relative to scrollbar track
                                float sb_top = sb_rect->computed_y;
                                float track_height = sb_rect->computed_height - handle_rect->height;
                                if (track_height > 0) {
                                    float mouse_relative = my - sb_top - (handle_rect->height * 0.5f);
                                    float ratio = std::clamp(mouse_relative / track_height, 0.0f, 1.0f);
                                    sv->scroll_y = ratio * max_scroll_y;
                                }
                            }
                        }
                    }

                    // No velocity for scrollbar drag
                    sv->velocity_x = 0.0f;
                    sv->velocity_y = 0.0f;
                } else {
                    // Dragging content - scroll inversely to drag direction
                    float delta_x = mx - sv->_drag_start_x;
                    float delta_y = my - sv->_drag_start_y;

                    if (sv->horizontal) {
                        sv->scroll_x = sv->_drag_start_scroll_x - delta_x;
                    }
                    if (sv->vertical) {
                        sv->scroll_y = sv->_drag_start_scroll_y - delta_y;
                    }

                    // Track velocity for inertia
                    if (dt > 0.0f) {
                        sv->velocity_x = (mx - m_last_mouse_x) / dt;
                        sv->velocity_y = (my - m_last_mouse_y) / dt;
                    }
                }

                // Update content position immediately during drag
                if (sv->content != entt::null && reg.valid(sv->content)) {
                    ScreenRect* content_rect = reg.try_get<ScreenRect>(sv->content);
                    if (content_rect) {
                        content_rect->offset_x = -sv->scroll_x;
                        content_rect->offset_y = -sv->scroll_y;
                    }
                }

                // Update scrollbar handle position during drag
                float viewport_height = 0.0f;
                if (sv->viewport != entt::null && reg.valid(sv->viewport)) {
                    ScreenRect* vp_rect = reg.try_get<ScreenRect>(sv->viewport);
                    if (vp_rect) {
                        viewport_height = vp_rect->computed_height > 0 ? vp_rect->computed_height : vp_rect->height;
                    }
                }
                float max_scroll_y = std::max(0.0f, sv->content_height - viewport_height);

                if (sv->vertical_scrollbar != entt::null && reg.valid(sv->vertical_scrollbar) && max_scroll_y > 0) {
                    Hierarchy* sb_hier = reg.try_get<Hierarchy>(sv->vertical_scrollbar);
                    if (sb_hier && !sb_hier->children.empty()) {
                        entt::entity handle = sb_hier->children[0];
                        if (reg.valid(handle)) {
                            ScreenRect* handle_rect = reg.try_get<ScreenRect>(handle);
                            ScreenRect* scrollbar_rect = reg.try_get<ScreenRect>(sv->vertical_scrollbar);
                            if (handle_rect && scrollbar_rect) {
                                float ratio = viewport_height / sv->content_height;
                                float handle_height = std::max(20.0f, scrollbar_rect->height * ratio);
                                handle_rect->height = handle_height;

                                float scroll_ratio = sv->scroll_y / max_scroll_y;
                                float track_height = scrollbar_rect->height - handle_height;
                                handle_rect->offset_y = scroll_ratio * track_height;
                            }
                        }
                    }
                }
            }
        }
    }

    // 5. Handle mouse release
    if (mouse_released && m_pressed_entity != entt::null && reg.valid(m_pressed_entity)) {
        entt::entity pressed = m_pressed_entity;

        // Button click (only if released over the same button)
        Button* button = reg.try_get<Button>(pressed);
        if (button && button->enabled) {
            button->_released_this_frame = true;
            dispatch_button_release(reg, pressed);

            if (pressed == top_hit) {
                button->_clicked_this_frame = true;
                dispatch_button_click(reg, pressed);

                // Check if this button is a close button for any Panel
                auto panel_view = reg.view<Panel>();
                for (auto panel_entity : panel_view) {
                    Panel& panel = panel_view.get<Panel>(panel_entity);
                    if (panel.close_button == pressed) {
                        // Clear pressed entity since we're about to destroy it
                        m_pressed_entity = entt::null;
                        m_hovered_entity = entt::null;
                        // Destroy the panel and all its children
                        destroy_entity_recursive(reg, panel_entity);
                        break;
                    }
                }
            }
        }

        // Slider drag end (use stored slider entity)
        if (m_dragging_slider_entity != entt::null && reg.valid(m_dragging_slider_entity)) {
            Slider* slider = reg.try_get<Slider>(m_dragging_slider_entity);
            if (slider && slider->_is_dragging) {
                slider->_is_dragging = false;
                dispatch_slider_drag_end(reg, m_dragging_slider_entity);
            }
            m_dragging_slider_entity = entt::null;
        }

        // Checkbox toggle (only if released over the same checkbox)
        Checkbox* checkbox = reg.try_get<Checkbox>(pressed);
        if (checkbox && checkbox->enabled && pressed == top_hit) {
            checkbox->checked = !checkbox->checked;
            checkbox->_toggled_this_frame = true;
            handle_toggle_group(reg, pressed, *checkbox);
            dispatch_checkbox_changed(reg, pressed, checkbox->checked);

            // Update checkmark visibility
            if (checkbox->checkmark != entt::null && reg.valid(checkbox->checkmark)) {
                render::Image* checkmark_img = reg.try_get<render::Image>(checkbox->checkmark);
                if (checkmark_img) {
                    checkmark_img->enabled = checkbox->checked;
                }
            }
        }

        // Dropdown toggle (only if released over the same dropdown)
        Dropdown* dropdown = reg.try_get<Dropdown>(pressed);
        if (dropdown && dropdown->enabled && pressed == top_hit) {
            toggle_dropdown(reg, pressed, *dropdown);
        }

        // Dropdown option selection (clicked on an option item)
        DropdownOption* option = reg.try_get<DropdownOption>(pressed);
        if (option && pressed == top_hit) {
            if (option->dropdown_entity != entt::null && reg.valid(option->dropdown_entity)) {
                Dropdown* parent_dropdown = reg.try_get<Dropdown>(option->dropdown_entity);
                if (parent_dropdown && parent_dropdown->enabled) {
                    select_dropdown_option(reg, option->dropdown_entity, *parent_dropdown, option->option_index);
                    close_dropdown(reg, option->dropdown_entity, *parent_dropdown);
                }
            }
        }

        // Panel drag end
        Panel* panel = reg.try_get<Panel>(pressed);
        if (panel) {
            panel->_is_dragging = false;
        }

        // ScrollView drag end (start inertia)
        if (m_dragging_scrollview_entity != entt::null && reg.valid(m_dragging_scrollview_entity)) {
            ScrollView* sv = reg.try_get<ScrollView>(m_dragging_scrollview_entity);
            if (sv) {
                sv->_is_dragging = false;
                // Velocity is already set from dragging (unless scrollbar handle drag)
                if (m_dragging_scrollbar_handle) {
                    sv->velocity_x = 0.0f;
                    sv->velocity_y = 0.0f;
                }
            }
            m_dragging_scrollview_entity = entt::null;
            m_dragging_scrollbar_handle = false;
        }

        // Update visual state
        UIInteractable* ui = reg.try_get<UIInteractable>(pressed);
        if (ui) {
            UIInteractable::State new_state = UIInteractable::State::Normal;
            if (!ui->interactable) {
                new_state = UIInteractable::State::Disabled;
            } else if (pressed == top_hit) {
                new_state = UIInteractable::State::Hovered;
            }
            update_visual_state(reg, pressed, *ui, new_state);
        }

        m_pressed_entity = entt::null;
    }

    // 6. Handle scroll wheel
    if (scroll != 0.0f) {
        // Find scrollview in hit list or parent hierarchy
        for (const HitResult& hit : hits) {
            ScrollView* sv = reg.try_get<ScrollView>(hit.entity);
            if (sv && sv->enabled) {
                if (sv->vertical) {
                    sv->scroll_y += scroll * sv->scroll_sensitivity;
                } else if (sv->horizontal) {
                    sv->scroll_x += scroll * sv->scroll_sensitivity;
                }
                dispatch_scroll(reg, hit.entity, sv->scroll_x, sv->scroll_y);
                break;
            }
        }
    }

    // 7. Update scrollview inertia and bounds
    auto sv_view = reg.view<ScrollView>();
    for (auto entity : sv_view) {
        ScrollView& sv = sv_view.get<ScrollView>(entity);
        if (sv.enabled && !sv._is_dragging) {
            update_scrollview(reg, entity, sv, dt);
        }
    }

    // 8. Clear per-frame flags
    auto button_view = reg.view<Button>();
    for (auto entity : button_view) {
        Button& btn = button_view.get<Button>(entity);
        btn._clicked_this_frame = false;
        btn._pressed_this_frame = false;
        btn._released_this_frame = false;
    }

    auto checkbox_view = reg.view<Checkbox>();
    for (auto entity : checkbox_view) {
        Checkbox& cb = checkbox_view.get<Checkbox>(entity);
        cb._toggled_this_frame = false;
    }

    m_last_mouse_x = mx;
    m_last_mouse_y = my;
}

std::vector<UIInteractionSystem::HitResult> UIInteractionSystem::raycast_ui(
    Engine& engine, float screen_x, float screen_y)
{
    std::vector<HitResult> results;

    // Use explicitly set registry if available, otherwise fall back to engine's scene
    entt::registry* reg_ptr = m_registry;
    if (!reg_ptr) {
        scene::Scene* scene = engine.scenes().top();
        if (!scene) return results;
        reg_ptr = &scene->registry();
    }
    entt::registry& reg = *reg_ptr;

    // Query all UIInteractable entities
    auto view = reg.view<UIInteractable>();

    for (auto entity : view) {
        UIInteractable& ui = view.get<UIInteractable>(entity);
        if (!ui.enabled) continue;

        HitResult hit;
        hit.entity = entity;

        // Determine if screen-space or world-space
        if (reg.all_of<ScreenRect>(entity)) {
            hit.is_screen_space = true;
            if (point_in_screen_rect(reg, entity, screen_x, screen_y, hit.local_x, hit.local_y)) {
                // Get layer from Image if present
                render::Image* img = reg.try_get<render::Image>(entity);
                if (img) {
                    hit.layer = img->layer;
                }
                results.push_back(hit);
            }
        } else if (reg.all_of<Transform>(entity)) {
            hit.is_screen_space = false;
            if (point_in_world_rect(engine, reg, entity, screen_x, screen_y, hit.local_x, hit.local_y)) {
                render::Image* img = reg.try_get<render::Image>(entity);
                if (img) {
                    hit.layer = img->layer;
                }
                results.push_back(hit);
            }
        }
    }

    // Also check Panel components that block raycast
    auto panel_view = reg.view<Panel>();
    for (auto entity : panel_view) {
        Panel& panel = panel_view.get<Panel>(entity);
        if (!panel.enabled || !panel.blocks_raycast) continue;
        if (reg.all_of<UIInteractable>(entity)) continue;  // Already processed

        HitResult hit;
        hit.entity = entity;

        if (reg.all_of<ScreenRect>(entity)) {
            hit.is_screen_space = true;
            if (point_in_screen_rect(reg, entity, screen_x, screen_y, hit.local_x, hit.local_y)) {
                render::Image* img = reg.try_get<render::Image>(entity);
                if (img) {
                    hit.layer = img->layer;
                }
                results.push_back(hit);
            }
        } else if (reg.all_of<Transform>(entity)) {
            hit.is_screen_space = false;
            if (point_in_world_rect(engine, reg, entity, screen_x, screen_y, hit.local_x, hit.local_y)) {
                render::Image* img = reg.try_get<render::Image>(entity);
                if (img) {
                    hit.layer = img->layer;
                }
                results.push_back(hit);
            }
        }
    }

    // Also check Slider handles and fill_rects (they may not have UIInteractable but should still be clickable)
    auto slider_view = reg.view<Slider>();
    for (auto entity : slider_view) {
        Slider& slider = slider_view.get<Slider>(entity);
        if (!slider.enabled) continue;

        // Check handle
        if (slider.handle != entt::null && reg.valid(slider.handle) && !reg.all_of<UIInteractable>(slider.handle)) {
            HitResult hit;
            hit.entity = slider.handle;

            if (reg.all_of<ScreenRect>(slider.handle)) {
                hit.is_screen_space = true;
                if (point_in_screen_rect(reg, slider.handle, screen_x, screen_y, hit.local_x, hit.local_y)) {
                    render::Image* img = reg.try_get<render::Image>(slider.handle);
                    if (img) {
                        hit.layer = img->layer;
                    }
                    results.push_back(hit);
                }
            }
        }

        // Check fill_rect
        if (slider.fill_rect != entt::null && reg.valid(slider.fill_rect) && !reg.all_of<UIInteractable>(slider.fill_rect)) {
            HitResult hit;
            hit.entity = slider.fill_rect;

            if (reg.all_of<ScreenRect>(slider.fill_rect)) {
                hit.is_screen_space = true;
                if (point_in_screen_rect(reg, slider.fill_rect, screen_x, screen_y, hit.local_x, hit.local_y)) {
                    render::Image* img = reg.try_get<render::Image>(slider.fill_rect);
                    if (img) {
                        hit.layer = img->layer;
                    }
                    results.push_back(hit);
                }
            }
        }
    }

    // Sort by layer (highest first = frontmost)
    std::sort(results.begin(), results.end(), [](const HitResult& a, const HitResult& b) {
        return a.layer > b.layer;
    });

    return results;
}

bool UIInteractionSystem::point_in_screen_rect(
    entt::registry& reg, entt::entity entity,
    float screen_x, float screen_y,
    float& out_local_x, float& out_local_y)
{
    ScreenRect* rect = reg.try_get<ScreenRect>(entity);
    if (!rect || !rect->enabled) return false;

    // ScreenRectSystem already applies pivot offset, so computed_x/y is the top-left corner.
    // Using Y-down convention: Y=0 at top, Y increases downward.
    float left = rect->computed_x;
    float top = rect->computed_y;  // Smaller Y (top of rect)
    float right = left + rect->computed_width;
    float bottom = top + rect->computed_height;  // Larger Y (bottom of rect)

    if (screen_x >= left && screen_x <= right && screen_y >= top && screen_y <= bottom) {
        // Calculate local position (0-1 normalized)
        out_local_x = (rect->computed_width > 0.0f) ? (screen_x - left) / rect->computed_width : 0.0f;
        out_local_y = (rect->computed_height > 0.0f) ? (screen_y - top) / rect->computed_height : 0.0f;
        return true;
    }

    return false;
}

bool UIInteractionSystem::point_in_world_rect(
    Engine& engine, entt::registry& reg, entt::entity entity,
    float screen_x, float screen_y,
    float& out_local_x, float& out_local_y)
{
    Transform* transform = reg.try_get<Transform>(entity);
    if (!transform || !transform->enabled) return false;

    // Need camera to convert screen to world
    render::Camera2D* camera = nullptr;
    auto cam_view = reg.view<render::Camera2D>();
    for (auto cam_entity : cam_view) {
        render::Camera2D& cam = cam_view.get<render::Camera2D>(cam_entity);
        if (cam.enabled) {
            camera = &cam;
            break;
        }
    }
    if (!camera) return false;

    // Get screen dimensions from viewport (if set) or window
    float screen_w = m_viewport_w > 0 ? m_viewport_w : static_cast<float>(engine.window().width());
    float screen_h = m_viewport_h > 0 ? m_viewport_h : static_cast<float>(engine.window().height());

    // Convert screen to world coordinates using the utility function
    float world_x, world_y;
    render::screen_to_world(*camera, screen_x, screen_y, screen_w, screen_h, world_x, world_y);

    // Get bounds from Image component or Transform scale
    float half_w = 50.0f;  // Default size
    float half_h = 50.0f;

    render::Image* img = reg.try_get<render::Image>(entity);
    if (img && img->_cached_width > 0 && img->_cached_height > 0) {
        // Textured image: use texture size * scale
        half_w = img->_cached_width * 0.5f * transform->world_scale_x;
        half_h = img->_cached_height * 0.5f * transform->world_scale_y;
    } else {
        // Solid color or no image: use transform scale directly as size
        // (ImageRenderSystem uses 1x1 white texture * scale for solid colors)
        half_w = std::abs(transform->world_scale_x) * 0.5f;
        half_h = std::abs(transform->world_scale_y) * 0.5f;
    }

    // Simple AABB check (ignoring rotation for now)
    float left = transform->world_x - half_w;
    float right = transform->world_x + half_w;
    float bottom = transform->world_y - half_h;
    float top_bound = transform->world_y + half_h;

    if (world_x >= left && world_x <= right && world_y >= bottom && world_y <= top_bound) {
        out_local_x = (right - left > 0.0f) ? (world_x - left) / (right - left) : 0.0f;
        out_local_y = (top_bound - bottom > 0.0f) ? (world_y - bottom) / (top_bound - bottom) : 0.0f;
        return true;
    }

    return false;
}

void UIInteractionSystem::update_visual_state(
    entt::registry& reg, entt::entity entity,
    UIInteractable& ui, UIInteractable::State new_state)
{
    if (ui._current_state == new_state) return;

    ui._current_state = new_state;

    render::Image* img = reg.try_get<render::Image>(entity);
    if (!img) return;

    // Capture original color on first interaction
    if (!ui._original_color_captured) {
        ui._original_color = {img->color_r, img->color_g, img->color_b, img->color_a};
        ui._original_color_captured = true;
    }

    switch (ui.transition_mode) {
        case UIInteractable::TransitionMode::None:
            break;

        case UIInteractable::TransitionMode::ColorTint: {
            const std::array<float, 4>* tint = nullptr;
            switch (new_state) {
                case UIInteractable::State::Normal:   tint = &ui.normal_color; break;
                case UIInteractable::State::Hovered:  tint = &ui.hovered_color; break;
                case UIInteractable::State::Pressed:  tint = &ui.pressed_color; break;
                case UIInteractable::State::Disabled: tint = &ui.disabled_color; break;
            }
            if (tint) {
                img->color_r = ui._original_color[0] * (*tint)[0];
                img->color_g = ui._original_color[1] * (*tint)[1];
                img->color_b = ui._original_color[2] * (*tint)[2];
                img->color_a = ui._original_color[3] * (*tint)[3];
            }
            break;
        }

        case UIInteractable::TransitionMode::SpriteSwap: {
            const std::string* sprite = nullptr;
            switch (new_state) {
                case UIInteractable::State::Normal:   sprite = &ui.normal_sprite; break;
                case UIInteractable::State::Hovered:  sprite = &ui.hovered_sprite; break;
                case UIInteractable::State::Pressed:  sprite = &ui.pressed_sprite; break;
                case UIInteractable::State::Disabled: sprite = &ui.disabled_sprite; break;
            }
            if (sprite && !sprite->empty()) {
                img->sprite_path = *sprite;
                img->_texture_loaded = false;  // Force reload
            }
            break;
        }
    }
}

entt::entity UIInteractionSystem::get_callback_target(entt::registry& reg, entt::entity ui_entity) {
    UIInteractable* ui = reg.try_get<UIInteractable>(ui_entity);
    if (ui && ui->callback_target != entt::null && reg.valid(ui->callback_target)) {
        return ui->callback_target;
    }
    return ui_entity;
}

template<typename Fn>
void UIInteractionSystem::dispatch_to_scripts(entt::registry& reg, entt::entity ui_entity, Fn&& callback_fn) {
    entt::entity target = get_callback_target(reg, ui_entity);
    runtime::ScriptComponent* sc = reg.try_get<runtime::ScriptComponent>(target);
    if (sc) {
        for (auto& script : sc->scripts) {
            if (script) callback_fn(*script);
        }
    }
}

void UIInteractionSystem::dispatch_button_click(entt::registry& reg, entt::entity e) {
    dispatch_to_scripts(reg, e, [e](auto& s) { s.on_button_click(e); });
}
void UIInteractionSystem::dispatch_button_press(entt::registry& reg, entt::entity e) {
    dispatch_to_scripts(reg, e, [e](auto& s) { s.on_button_press(e); });
}
void UIInteractionSystem::dispatch_button_release(entt::registry& reg, entt::entity e) {
    dispatch_to_scripts(reg, e, [e](auto& s) { s.on_button_release(e); });
}
void UIInteractionSystem::dispatch_pointer_enter(entt::registry& reg, entt::entity e) {
    dispatch_to_scripts(reg, e, [e](auto& s) { s.on_pointer_enter(e); });
}
void UIInteractionSystem::dispatch_pointer_exit(entt::registry& reg, entt::entity e) {
    dispatch_to_scripts(reg, e, [e](auto& s) { s.on_pointer_exit(e); });
}
void UIInteractionSystem::dispatch_slider_changed(entt::registry& reg, entt::entity e, float value) {
    dispatch_to_scripts(reg, e, [e, value](auto& s) { s.on_slider_changed(e, value); });
}
void UIInteractionSystem::dispatch_slider_drag_start(entt::registry& reg, entt::entity e) {
    dispatch_to_scripts(reg, e, [e](auto& s) { s.on_slider_drag_start(e); });
}
void UIInteractionSystem::dispatch_slider_drag_end(entt::registry& reg, entt::entity e) {
    dispatch_to_scripts(reg, e, [e](auto& s) { s.on_slider_drag_end(e); });
}
void UIInteractionSystem::dispatch_checkbox_changed(entt::registry& reg, entt::entity e, bool checked) {
    dispatch_to_scripts(reg, e, [e, checked](auto& s) { s.on_checkbox_changed(e, checked); });
}
void UIInteractionSystem::dispatch_scroll(entt::registry& reg, entt::entity e, float scroll_x, float scroll_y) {
    dispatch_to_scripts(reg, e, [e, scroll_x, scroll_y](auto& s) { s.on_scroll(e, scroll_x, scroll_y); });
}
void UIInteractionSystem::dispatch_panel_drag(entt::registry& reg, entt::entity e, float delta_x, float delta_y) {
    dispatch_to_scripts(reg, e, [e, delta_x, delta_y](auto& s) { s.on_panel_drag(e, delta_x, delta_y); });
}

void UIInteractionSystem::update_slider_value(
    entt::registry& reg, entt::entity entity, Slider& slider,
    float screen_x, float screen_y, const HitResult& original_hit)
{
    (void)original_hit; // May be used for world-space sliders later

    // Get the slider's rect
    float local_x = 0.0f;
    float local_y = 0.0f;

    ScreenRect* rect = reg.try_get<ScreenRect>(entity);
    if (rect) {
        // computed_x/y is already the top-left corner (pivot offset applied by ScreenRectSystem)
        float left = rect->computed_x;
        float top = rect->computed_y;

        local_x = (rect->computed_width > 0.0f) ? (screen_x - left) / rect->computed_width : 0.0f;
        local_y = (rect->computed_height > 0.0f) ? (screen_y - top) / rect->computed_height : 0.0f;
    } else {
        // World space - use the local coordinates from hit result
        local_x = original_hit.local_x;
        local_y = original_hit.local_y;
    }

    // Clamp to 0-1
    local_x = local_x < 0.0f ? 0.0f : (local_x > 1.0f ? 1.0f : local_x);
    local_y = local_y < 0.0f ? 0.0f : (local_y > 1.0f ? 1.0f : local_y);

    // Calculate new value based on direction
    float new_value = 0.0f;
    switch (slider.direction) {
        case Slider::Direction::LeftToRight:
            new_value = local_x;
            break;
        case Slider::Direction::RightToLeft:
            new_value = 1.0f - local_x;
            break;
        case Slider::Direction::BottomToTop:
            new_value = local_y;
            break;
        case Slider::Direction::TopToBottom:
            new_value = 1.0f - local_y;
            break;
    }

    float old_value = slider.value;
    slider.value = new_value;

    // Update fill rect if present
    if (slider.fill_rect != entt::null && reg.valid(slider.fill_rect)) {
        ScreenRect* fill_screen_rect = reg.try_get<ScreenRect>(slider.fill_rect);
        if (fill_screen_rect && rect) {
            // Adjust fill width/height based on direction
            if (slider.direction == Slider::Direction::LeftToRight ||
                slider.direction == Slider::Direction::RightToLeft) {
                fill_screen_rect->width = rect->computed_width * new_value;
            } else {
                fill_screen_rect->height = rect->computed_height * new_value;
            }
        }
    }

    // Update handle position if present
    if (slider.handle != entt::null && reg.valid(slider.handle)) {
        ScreenRect* handle_rect = reg.try_get<ScreenRect>(slider.handle);
        if (handle_rect) {
            if (slider.direction == Slider::Direction::LeftToRight ||
                slider.direction == Slider::Direction::RightToLeft) {
                handle_rect->anchor_x = new_value;
            } else {
                handle_rect->anchor_y = new_value;
            }
        }
    }

    // Dispatch callback if value changed
    if (old_value != slider.value) {
        dispatch_slider_changed(reg, entity, slider.get_value());
    }
}

void UIInteractionSystem::handle_toggle_group(entt::registry& reg, entt::entity checkbox_entity, Checkbox& checkbox) {
    if (checkbox.toggle_group.empty() || !checkbox.checked) return;

    // Uncheck all other checkboxes in the same group
    auto view = reg.view<Checkbox>();
    for (auto entity : view) {
        if (entity == checkbox_entity) continue;

        Checkbox& other = view.get<Checkbox>(entity);
        if (other.toggle_group == checkbox.toggle_group && other.checked) {
            other.checked = false;

            // Update checkmark visibility
            if (other.checkmark != entt::null && reg.valid(other.checkmark)) {
                render::Image* checkmark_img = reg.try_get<render::Image>(other.checkmark);
                if (checkmark_img) {
                    checkmark_img->enabled = false;
                }
            }

            dispatch_checkbox_changed(reg, entity, false);
        }
    }
}

void UIInteractionSystem::update_scrollview(entt::registry& reg, entt::entity entity, ScrollView& sv, float dt) {
    (void)entity;

    // Apply inertia
    if (sv.inertia) {
        if (sv.horizontal && sv.velocity_x != 0.0f) {
            sv.scroll_x -= sv.velocity_x * dt;
            sv.velocity_x *= (1.0f - sv.deceleration_rate);
            if (std::abs(sv.velocity_x) < 1.0f) sv.velocity_x = 0.0f;
        }
        if (sv.vertical && sv.velocity_y != 0.0f) {
            sv.scroll_y -= sv.velocity_y * dt;
            sv.velocity_y *= (1.0f - sv.deceleration_rate);
            if (std::abs(sv.velocity_y) < 1.0f) sv.velocity_y = 0.0f;
        }
    }

    // Calculate viewport size for proper scroll bounds
    float viewport_width = 0.0f;
    float viewport_height = 0.0f;
    if (sv.viewport != entt::null && reg.valid(sv.viewport)) {
        ScreenRect* viewport_rect = reg.try_get<ScreenRect>(sv.viewport);
        if (viewport_rect) {
            viewport_width = viewport_rect->computed_height > 0 ? viewport_rect->computed_width : viewport_rect->width;
            viewport_height = viewport_rect->computed_height > 0 ? viewport_rect->computed_height : viewport_rect->height;
        }
    }

    // Apply elastic bounds
    // max_scroll is content size minus viewport size (how much can be scrolled)
    float max_scroll_x = std::max(0.0f, sv.content_width - viewport_width);
    float max_scroll_y = std::max(0.0f, sv.content_height - viewport_height);

    if (sv.elastic) {
        // Spring back if out of bounds
        if (sv.scroll_x < 0) {
            sv.scroll_x += (-sv.scroll_x) * sv.elasticity;
            if (sv.scroll_x > -0.5f) sv.scroll_x = 0;
        }
        if (sv.scroll_y < 0) {
            sv.scroll_y += (-sv.scroll_y) * sv.elasticity;
            if (sv.scroll_y > -0.5f) sv.scroll_y = 0;
        }
        if (sv.scroll_x > max_scroll_x) {
            sv.scroll_x -= (sv.scroll_x - max_scroll_x) * sv.elasticity;
        }
        if (sv.scroll_y > max_scroll_y) {
            sv.scroll_y -= (sv.scroll_y - max_scroll_y) * sv.elasticity;
        }
    } else {
        // Hard clamp
        if (sv.scroll_x < 0) sv.scroll_x = 0;
        if (sv.scroll_y < 0) sv.scroll_y = 0;
        if (sv.scroll_x > max_scroll_x) sv.scroll_x = max_scroll_x;
        if (sv.scroll_y > max_scroll_y) sv.scroll_y = max_scroll_y;
    }

    // Update content position (scroll_y moves content up, revealing items below)
    if (sv.content != entt::null && reg.valid(sv.content)) {
        ScreenRect* content_rect = reg.try_get<ScreenRect>(sv.content);
        if (content_rect) {
            content_rect->offset_x = -sv.scroll_x;
            content_rect->offset_y = -sv.scroll_y;  // Negative offset moves content up
        }
    }

    // Update vertical scrollbar handle position
    if (sv.vertical_scrollbar != entt::null && reg.valid(sv.vertical_scrollbar) && max_scroll_y > 0) {
        // Find the Handle child of the scrollbar
        Hierarchy* sb_hierarchy = reg.try_get<Hierarchy>(sv.vertical_scrollbar);
        if (sb_hierarchy && !sb_hierarchy->children.empty()) {
            // Assume first child is the handle
            entt::entity handle = sb_hierarchy->children[0];
            if (reg.valid(handle)) {
                ScreenRect* handle_rect = reg.try_get<ScreenRect>(handle);
                ScreenRect* scrollbar_rect = reg.try_get<ScreenRect>(sv.vertical_scrollbar);
                if (handle_rect && scrollbar_rect) {
                    // Calculate handle size based on viewport/content ratio
                    float ratio = viewport_height / sv.content_height;
                    float handle_height = std::max(20.0f, scrollbar_rect->height * ratio);
                    handle_rect->height = handle_height;

                    // Position handle based on scroll position
                    // With anchor_y=0 (TOP), positive offset moves handle down
                    float scroll_ratio = sv.scroll_y / max_scroll_y;
                    float track_height = scrollbar_rect->height - handle_height;
                    handle_rect->offset_y = scroll_ratio * track_height;
                }
            }
        }
    }
}

void UIInteractionSystem::update_panel_drag(
    entt::registry& reg, entt::entity entity, Panel& panel,
    float screen_x, float screen_y)
{
    float delta_x = screen_x - panel._drag_offset_x;
    float delta_y = screen_y - panel._drag_offset_y;

    // Move the panel
    ScreenRect* rect = reg.try_get<ScreenRect>(entity);
    if (rect) {
        rect->offset_x += delta_x;
        rect->offset_y += delta_y;
    } else {
        Transform* transform = reg.try_get<Transform>(entity);
        if (transform) {
            transform->x += delta_x;
            transform->y += delta_y;
        }
    }

    panel._drag_offset_x = screen_x;
    panel._drag_offset_y = screen_y;

    dispatch_panel_drag(reg, entity, delta_x, delta_y);
}

void UIInteractionSystem::dispatch_dropdown_changed(entt::registry& reg, entt::entity e, int selected_index) {
    dispatch_to_scripts(reg, e, [e, selected_index](auto& s) { s.on_dropdown_changed(e, selected_index); });
}

void UIInteractionSystem::toggle_dropdown(entt::registry& reg, entt::entity entity, Dropdown& dropdown) {
    if (dropdown._is_open) {
        close_dropdown(reg, entity, dropdown);
    } else {
        // Close any other open dropdown first
        if (m_open_dropdown_entity != entt::null && m_open_dropdown_entity != entity) {
            Dropdown* other_dropdown = reg.try_get<Dropdown>(m_open_dropdown_entity);
            if (other_dropdown && other_dropdown->_is_open) {
                close_dropdown(reg, m_open_dropdown_entity, *other_dropdown);
            }
        }

        dropdown._is_open = true;
        m_open_dropdown_entity = entity;

        // Show options panel and all its children
        if (dropdown.options_panel != entt::null && reg.valid(dropdown.options_panel)) {
            set_screenrect_enabled_recursive(reg, dropdown.options_panel, true);
        }

        update_dropdown_visuals(reg, entity, dropdown);
    }
}

void UIInteractionSystem::close_dropdown(entt::registry& reg, entt::entity entity, Dropdown& dropdown) {
    dropdown._is_open = false;
    if (m_open_dropdown_entity == entity) {
        m_open_dropdown_entity = entt::null;
    }

    // Hide options panel and all its children
    if (dropdown.options_panel != entt::null && reg.valid(dropdown.options_panel)) {
        set_screenrect_enabled_recursive(reg, dropdown.options_panel, false);
    }
}

void UIInteractionSystem::select_dropdown_option(entt::registry& reg, entt::entity dropdown_entity, Dropdown& dropdown, int index) {
    if (index < 0 || index >= static_cast<int>(dropdown.options.size())) return;

    int old_index = dropdown.selected_index;
    dropdown.selected_index = index;

    // Update selected text display
    if (dropdown.selected_text != entt::null && reg.valid(dropdown.selected_text)) {
        render::Text* text = reg.try_get<render::Text>(dropdown.selected_text);
        if (text) {
            text->content = dropdown.options[index];
        }
    }

    close_dropdown(reg, dropdown_entity, dropdown);

    if (old_index != index) {
        dispatch_dropdown_changed(reg, dropdown_entity, index);
    }
}

void UIInteractionSystem::update_dropdown_visuals(entt::registry& reg, entt::entity entity, Dropdown& dropdown) {
    // Update selected text
    if (dropdown.selected_text != entt::null && reg.valid(dropdown.selected_text)) {
        render::Text* text = reg.try_get<render::Text>(dropdown.selected_text);
        if (text && dropdown.selected_index >= 0 && dropdown.selected_index < static_cast<int>(dropdown.options.size())) {
            text->content = dropdown.options[dropdown.selected_index];
        }
    }

    // Determine if scrolling is needed
    int option_count = static_cast<int>(dropdown.options.size());
    int max_visible = dropdown.max_visible_items > 0 ? dropdown.max_visible_items : option_count;
    bool needs_scrolling = option_count > max_visible;
    int visible_items = needs_scrolling ? max_visible : option_count;
    float panel_height = visible_items * dropdown.item_height;

    // Check if this is a world-space dropdown
    bool is_world_space = false;
    float dropdown_scale_y = 1.0f;
    Transform* dropdown_transform = reg.try_get<Transform>(entity);
    if (dropdown_transform && !reg.all_of<ScreenRect>(entity)) {
        is_world_space = true;
        dropdown_scale_y = dropdown_transform->scale_y;
    }

    // Calculate options panel size based on max_visible_items
    if (dropdown.options_panel != entt::null && reg.valid(dropdown.options_panel)) {
        ScreenRect* panel_rect = reg.try_get<ScreenRect>(dropdown.options_panel);
        if (panel_rect) {
            panel_rect->height = panel_height;
        }

        // World-space: update Transform scale and position
        Transform* panel_transform = reg.try_get<Transform>(dropdown.options_panel);
        if (panel_transform && is_world_space) {
            // Scale: panel_height in world units, normalized by dropdown scale
            float normalized_panel_height = panel_height / dropdown_scale_y;
            panel_transform->scale_y = normalized_panel_height;

            // Position: place panel just below dropdown button (Y-UP convention)
            // Panel center should be at: -0.5 (dropdown bottom) - panel_height/2
            float panel_center_y = -0.5f - normalized_panel_height * 0.5f;
            panel_transform->y = panel_center_y;
        }
    }

    // Update scrollview content_height for scroll bounds
    if (dropdown.options_scrollview != entt::null && reg.valid(dropdown.options_scrollview)) {
        ScrollView* sv = reg.try_get<ScrollView>(dropdown.options_scrollview);
        if (sv) {
            sv->content_height = option_count * dropdown.item_height;
        }
    }

    // Update viewport height (parent of Content in prefab structure)
    if (dropdown.options_content != entt::null && reg.valid(dropdown.options_content)) {
        Hierarchy* content_hier = reg.try_get<Hierarchy>(dropdown.options_content);
        if (content_hier && content_hier->parent != entt::null && reg.valid(content_hier->parent)) {
            ScreenRect* viewport_rect = reg.try_get<ScreenRect>(content_hier->parent);
            if (viewport_rect) {
                viewport_rect->height = panel_height;
            }
        }
    }

    // Update scrollbar height (second child of OptionsPanel in prefab structure)
    if (dropdown.options_scrollview != entt::null && reg.valid(dropdown.options_scrollview)) {
        Hierarchy* panel_hier = reg.try_get<Hierarchy>(dropdown.options_scrollview);
        if (panel_hier && panel_hier->children.size() >= 2) {
            entt::entity scrollbar_ent = panel_hier->children[1];
            if (reg.valid(scrollbar_ent)) {
                ScreenRect* scrollbar_rect = reg.try_get<ScreenRect>(scrollbar_ent);
                if (scrollbar_rect) {
                    scrollbar_rect->height = panel_height;
                }
                set_screenrect_enabled_recursive(reg, scrollbar_ent, needs_scrolling && dropdown._is_open);
            }
        }
    }

    // Update scrollview content size if present
    if (dropdown.options_content != entt::null && reg.valid(dropdown.options_content)) {
        ScreenRect* content_rect = reg.try_get<ScreenRect>(dropdown.options_content);
        if (content_rect) {
            content_rect->height = option_count * dropdown.item_height;
        }
    }

    (void)entity;  // May be used for future enhancements
}

void UIInteractionSystem::clear_dropdown_options(entt::registry& reg, Dropdown& dropdown) {
    // Destroy all existing option entities
    for (auto option_entity : dropdown._option_entities) {
        if (reg.valid(option_entity)) {
            // Remove from parent's hierarchy
            if (dropdown.options_content != entt::null && reg.valid(dropdown.options_content)) {
                Hierarchy* content_hierarchy = reg.try_get<Hierarchy>(dropdown.options_content);
                if (content_hierarchy) {
                    auto& children = content_hierarchy->children;
                    children.erase(
                        std::remove(children.begin(), children.end(), option_entity),
                        children.end()
                    );
                }
            }
            reg.destroy(option_entity);
        }
    }
    dropdown._option_entities.clear();
}

void UIInteractionSystem::rebuild_dropdown_options(entt::registry& reg, entt::entity entity, Dropdown& dropdown) {
    // First, clear existing options
    clear_dropdown_options(reg, dropdown);

    // Can't create options without a content container
    if (dropdown.options_content == entt::null || !reg.valid(dropdown.options_content)) {
        return;
    }

    // Get content container's hierarchy (or create one)
    if (!reg.all_of<Hierarchy>(dropdown.options_content)) {
        reg.emplace<Hierarchy>(dropdown.options_content);
    }
    Hierarchy& content_hierarchy = reg.get<Hierarchy>(dropdown.options_content);

    // Check if this is a world-space (Transform) or screen-space (ScreenRect) dropdown
    bool is_world_space = reg.all_of<Transform>(dropdown.options_content) &&
                          !reg.all_of<ScreenRect>(dropdown.options_content);

    // Get viewport entity for clipping (parent of Content in prefab structure)
    // Structure: OptionsPanel -> Viewport -> Content
    entt::entity viewport_entity = entt::null;
    Hierarchy* content_hier = reg.try_get<Hierarchy>(dropdown.options_content);
    if (content_hier && content_hier->parent != entt::null && reg.valid(content_hier->parent)) {
        if (reg.all_of<ScreenRect>(content_hier->parent)) {
            viewport_entity = content_hier->parent;
        }
    }

    // Get parent dimensions for calculating item width
    float item_width = 160.0f;  // Default width
    ScreenRect* content_rect = reg.try_get<ScreenRect>(dropdown.options_content);
    if (content_rect) {
        item_width = content_rect->width;
    }

    // For world-space, compute the panel's effective world scale directly
    // This avoids depending on precomputed world transforms which may be stale
    float panel_scale_x = 1.0f;
    float panel_scale_y = 1.0f;
    if (is_world_space) {
        // Get dropdown transform to compute effective world scale
        Transform* dropdown_transform = reg.try_get<Transform>(entity);
        if (dropdown_transform) {
            // Compute the panel height the same way as update_dropdown_visuals
            // This ensures consistent calculations regardless of world transform state
            int option_count = static_cast<int>(dropdown.options.size());
            int max_visible = dropdown.max_visible_items > 0 ? dropdown.max_visible_items : option_count;
            int visible_items = option_count > max_visible ? max_visible : option_count;
            float panel_height = visible_items * dropdown.item_height;

            // Panel world scale = dropdown scale * panel local scale
            // Panel local scale is set to (panel_height / dropdown_scale_y) by update_dropdown_visuals
            // So panel world scale = dropdown_scale_y * (panel_height / dropdown_scale_y) = panel_height
            panel_scale_y = panel_height;
            panel_scale_x = dropdown_transform->scale_x;  // Assume panel matches dropdown width

            // Ensure we don't divide by zero
            if (panel_scale_y < 0.001f) panel_scale_y = 1.0f;
            if (panel_scale_x < 0.001f) panel_scale_x = 1.0f;
        }
    }

    // Create an entity for each option
    for (int i = 0; i < static_cast<int>(dropdown.options.size()); ++i) {
        entt::entity option_entity = reg.create();

        if (is_world_space) {
            // World-space: use Transform for positioning
            // Options are stacked vertically from the top of the content area
            // Using Y-up convention: first option at top (highest Y), subsequent below
            Transform& transform = reg.emplace<Transform>(option_entity);
            transform.enabled = true;
            transform.x = 0.0f;

            // Normalize item_height by panel scale to get proper local coordinates
            float normalized_item_height = dropdown.item_height / panel_scale_y;

            // Stack items from top (y=0.5) going down (negative Y direction)
            // First item centered at y = 0.5 - half_item_height
            // Each subsequent item is one item_height below
            float first_item_y = 0.5f - normalized_item_height * 0.5f;
            transform.y = first_item_y - i * normalized_item_height;

            // Full width, item height
            transform.scale_x = 1.0f;
            transform.scale_y = normalized_item_height;
        } else {
            // Screen-space: use ScreenRect for positioning
            // anchor_y=0 means TOP, anchor_y=1 means BOTTOM (Y-down convention)
            // pivot_y=0 means top edge is at anchor position
            // Positive offset_y moves DOWN from the anchor point
            ScreenRect& rect = reg.emplace<ScreenRect>(option_entity);
            rect.enabled = true;
            rect.anchor_x = 0.5f;
            rect.anchor_y = 0.0f;  // Anchor to TOP of content
            rect.pivot_x = 0.5f;
            rect.pivot_y = 0.0f;   // Top edge at anchor
            rect.width = item_width;
            rect.height = dropdown.item_height;
            rect.offset_x = 0.0f;
            rect.offset_y = i * dropdown.item_height;  // Stack downward (positive = down)
            rect.clip_to = viewport_entity;  // Clip to viewport bounds
        }

        // Background Image
        render::Image& img = reg.emplace<render::Image>(option_entity);
        img.enabled = true;
        img.layer = 2;  // Above panel background
        img.color_r = 0.3f;
        img.color_g = 0.3f;
        img.color_b = 0.3f;
        img.color_a = 1.0f;

        // Text label
        render::Text& text = reg.emplace<render::Text>(option_entity);
        text.enabled = true;
        text.content = dropdown.options[i];
        text.font_path = "fonts/OpenSans-Regular.ttf";
        text.font_size = is_world_space ? 16.0f : 14.0f;
        text.color_r = 1.0f;
        text.color_g = 1.0f;
        text.color_b = 1.0f;
        text.color_a = 1.0f;
        // World-space: center text since Transform is at center
        // Screen-space: left align within the ScreenRect bounds
        text.h_align = is_world_space ? render::TextHAlign::Center : render::TextHAlign::Left;
        text.v_align = render::TextVAlign::Middle;
        text.layer = 3;

        // UIInteractable for hover/press states
        UIInteractable& ui = reg.emplace<UIInteractable>(option_entity);
        ui.enabled = true;
        ui.interactable = true;
        ui.transition_mode = UIInteractable::TransitionMode::ColorTint;
        ui.normal_color = {1.0f, 1.0f, 1.0f, 1.0f};
        ui.hovered_color = {1.2f, 1.2f, 1.2f, 1.0f};
        ui.pressed_color = {0.8f, 0.8f, 0.8f, 1.0f};
        ui.disabled_color = {0.5f, 0.5f, 0.5f, 0.5f};

        // DropdownOption marker for click detection
        DropdownOption& opt = reg.emplace<DropdownOption>(option_entity);
        opt.dropdown_entity = entity;
        opt.option_index = i;

        // Hierarchy: child of content container
        Hierarchy& opt_hierarchy = reg.emplace<Hierarchy>(option_entity);
        opt_hierarchy.parent = dropdown.options_content;
        content_hierarchy.children.push_back(option_entity);

        dropdown._option_entities.push_back(option_entity);
    }
}

} // namespace engine::ui

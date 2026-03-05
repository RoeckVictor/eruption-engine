#include "AnimationPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "editor/EditorFileDialogs.h"
#include "engine/animation/PropertyResolver.h"
#include "engine/animation/Interpolation.h"
#include "engine/asset/loaders/AnimationClipLoader.h"
#include "engine/core/Engine.h"
#include "engine/core/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace editor {

AnimationPanel::AnimationPanel(EditorContext& context)
    : Panel("Animation", PanelVisibilityMode::OnDemand)
    , m_context(context)
{
}

AnimationPanel::~AnimationPanel() = default;

void AnimationPanel::on_open() {
    m_properties_cache_dirty = true;

    // Set animated property check callback
    m_context.set_animated_property_check([this](entt::entity entity, const std::string& property_path) {
        return is_property_animated(entity, property_path);
    });

    // Register selection change callback to restore values when selection changes
    m_context.selection().set_selection_changed_callback([this]() {
        on_selection_changed();
    });
}

void AnimationPanel::on_close() {
    stop();

    // Restore original values before closing
    restore_original_values();

    // Clear callbacks
    m_context.set_animated_property_check(nullptr);
    m_context.selection().set_selection_changed_callback(nullptr);
}

void AnimationPanel::on_gui() {
    render_toolbar();

    // Layout: track list | timeline | properties
    float available_width = ImGui::GetContentRegionAvail().x;
    float timeline_width = std::max(available_width - m_track_list_width - m_properties_width - 16.0f, 100.0f);
    m_timeline_width = timeline_width;

    // Track list
    ImGui::BeginChild("TrackList", ImVec2(m_track_list_width, 0), true);
    render_track_list();
    ImGui::EndChild();

    ImGui::SameLine();

    // Timeline
    ImGui::BeginChild("Timeline", ImVec2(timeline_width, 0), true);
    render_timeline();
    ImGui::EndChild();

    ImGui::SameLine();

    // Properties panel
    ImGui::BeginChild("KeyframeProperties", ImVec2(m_properties_width, 0), true);
    render_keyframe_properties();
    ImGui::EndChild();

    // Add track dialog
    render_add_track_dialog();

    // Add keyframe dialog
    render_add_keyframe_dialog();

    // Update playback if playing
    if (m_is_playing) {
        float current_time = static_cast<float>(ImGui::GetTime());
        float dt = current_time - m_last_frame_time;
        m_last_frame_time = current_time;
        update_preview(dt);
    }
}

void AnimationPanel::render_toolbar() {
    // File operations
    if (ImGui::Button(ICON_FA_FILE " New")) {
        new_clip();
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open")) {
        auto path = open_animation_clip();
        if (!path.empty()) {
            open_clip(path);
        }
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
        save_clip();
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FILE_EXPORT " Save As")) {
        auto initial_dir = get_assets_directory(m_context.scene_state().project_path());
        auto path = save_animation_clip_as(initial_dir);
        if (!path.empty()) {
            save_clip_as(path);
        }
    }
    ImGui::SameLine();

    ImGui::Separator();
    ImGui::SameLine();

    // Playback controls
    if (m_is_playing) {
        if (ImGui::Button(ICON_FA_PAUSE " Pause")) {
            pause();
        }
    } else {
        if (ImGui::Button(ICON_FA_PLAY " Play")) {
            play();
        }
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_STOP " Stop")) {
        stop();
    }
    ImGui::SameLine();

    // Recording
    if (m_is_recording) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(ICON_FA_CIRCLE " Recording")) {
            stop_recording();
        }
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button(ICON_FA_CIRCLE " Record")) {
            start_recording();
        }
    }
    ImGui::SameLine();

    ImGui::Separator();
    ImGui::SameLine();

    // Time display - editable via DragFloat
    ImGui::Text("Time:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    float time = m_playhead_time;
    if (ImGui::DragFloat("##Time", &time, 0.01f, 0.0f, m_clip.duration, "%.2fs")) {
        set_time(time);
    }
    ImGui::SameLine();
    ImGui::Text("/ %.2fs", m_clip.duration);
    ImGui::SameLine();

    // Duration
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("Duration", &m_clip.duration, 0.1f, 0.1f, 60.0f)) {
        // Clamp playhead if duration shortened
        if (m_playhead_time > m_clip.duration) {
            m_playhead_time = m_clip.duration;
        }
        mark_dirty();
    }
    ImGui::SameLine();

    // Looping
    if (ImGui::Checkbox("Loop", &m_clip.looping)) {
        mark_dirty();
    }
    ImGui::SameLine();

    // Zoom
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Zoom", &m_timeline_zoom, 5.0f, 20.0f, 500.0f, "%.0f px/s");

    ImGui::Separator();

    // File info
    if (!m_current_path.empty()) {
        std::string filename = m_current_path.substr(m_current_path.find_last_of("/\\") + 1);
        ImGui::Text("%s%s", filename.c_str(), m_has_unsaved_changes ? "*" : "");
    } else {
        ImGui::Text("Untitled%s", m_has_unsaved_changes ? "*" : "");
    }

    ImGui::Separator();
}

void AnimationPanel::render_track_list() {
    ImGui::Text(ICON_FA_LIST " Tracks");
    ImGui::Separator();

    if (ImGui::Button(ICON_FA_PLUS " Add Track")) {
        m_show_add_track_dialog = true;
    }

    ImGui::Separator();

    for (size_t i = 0; i < m_clip.tracks.size(); ++i) {
        const auto& track = m_clip.tracks[i];

        ImGui::PushID(static_cast<int>(i));

        bool selected = (m_selected_track == static_cast<int>(i));
        if (ImGui::Selectable(track.property_path.c_str(), selected)) {
            m_selected_track = static_cast<int>(i);
            m_selected_keyframe = -1;
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Track")) {
                remove_track(i);
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

void AnimationPanel::render_timeline() {
    m_timeline_canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    m_timeline_height = canvas_size.y;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(m_timeline_canvas_pos, ImVec2(m_timeline_canvas_pos.x + canvas_size.x, m_timeline_canvas_pos.y + canvas_size.y),
                              IM_COL32(30, 30, 35, 255));

    // Ruler area
    float ruler_height = 30.0f;
    draw_list->AddRectFilled(m_timeline_canvas_pos, ImVec2(m_timeline_canvas_pos.x + canvas_size.x, m_timeline_canvas_pos.y + ruler_height),
                              IM_COL32(40, 40, 50, 255));

    // Draw ruler
    draw_timeline_ruler();

    // Draw tracks and keyframes
    draw_tracks();
    draw_keyframes();

    // Gray out area outside clip duration
    float clip_end_x = time_to_screen_x(m_clip.duration) + m_timeline_canvas_pos.x - m_timeline_scroll;
    if (clip_end_x < m_timeline_canvas_pos.x + canvas_size.x) {
        // Draw semi-transparent dark overlay for out-of-bounds area
        float overlay_start = std::max(clip_end_x, m_timeline_canvas_pos.x);
        draw_list->AddRectFilled(
            ImVec2(overlay_start, m_timeline_canvas_pos.y + ruler_height),
            ImVec2(m_timeline_canvas_pos.x + canvas_size.x, m_timeline_canvas_pos.y + canvas_size.y),
            IM_COL32(0, 0, 0, 120));

        // Also darken the ruler area
        draw_list->AddRectFilled(
            ImVec2(overlay_start, m_timeline_canvas_pos.y),
            ImVec2(m_timeline_canvas_pos.x + canvas_size.x, m_timeline_canvas_pos.y + ruler_height),
            IM_COL32(0, 0, 0, 80));

        // Draw a vertical line at clip end
        draw_list->AddLine(
            ImVec2(clip_end_x, m_timeline_canvas_pos.y),
            ImVec2(clip_end_x, m_timeline_canvas_pos.y + canvas_size.y),
            IM_COL32(100, 100, 120, 200),
            1.0f);
    }

    // Draw playhead (uses foreground draw list to render on top)
    draw_playhead();

    // Handle input - reset cursor position for input handling
    ImGui::SetCursorScreenPos(m_timeline_canvas_pos);
    ImVec2 button_size = ImVec2(std::max(canvas_size.x, 1.0f), std::max(canvas_size.y, 1.0f));
    ImGui::InvisibleButton("timeline_canvas", button_size);

    bool is_hovered = ImGui::IsItemHovered();
    bool is_active = ImGui::IsItemActive();

    if (is_hovered) {
        // Zoom with mouse wheel
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_timeline_zoom *= (1.0f + wheel * 0.1f);
            m_timeline_zoom = std::clamp(m_timeline_zoom, 20.0f, 500.0f);
        }
    }

    // Check if clicking on playhead handle (top 20px of timeline)
    ImVec2 mouse_pos = ImGui::GetMousePos();
    float playhead_x = time_to_screen_x(m_playhead_time) + m_timeline_canvas_pos.x - m_timeline_scroll;
    bool on_playhead = std::abs(mouse_pos.x - playhead_x) < 10.0f &&
                       mouse_pos.y >= m_timeline_canvas_pos.y && mouse_pos.y <= m_timeline_canvas_pos.y + 20.0f;

    // Start dragging playhead
    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (on_playhead) {
            m_dragging_playhead = true;
        } else {
            // Click anywhere to set playhead position
            float time = screen_x_to_time(mouse_pos.x - m_timeline_canvas_pos.x + m_timeline_scroll);
            set_time(time);
            m_dragging_playhead = true;  // Allow immediate dragging after click
        }
    }

    // Continue dragging playhead
    if (m_dragging_playhead && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float time = screen_x_to_time(mouse_pos.x - m_timeline_canvas_pos.x + m_timeline_scroll);
        set_time(time);
    }

    // Stop dragging
    if (m_dragging_playhead && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_dragging_playhead = false;
    }

    // Pan with middle mouse
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        m_timeline_scroll -= ImGui::GetIO().MouseDelta.x;
        m_timeline_scroll = std::max(0.0f, m_timeline_scroll);
    }
}

void AnimationPanel::draw_timeline_ruler() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float ruler_height = 30.0f;

    // Calculate tick spacing based on zoom
    float seconds_per_tick = 0.1f;
    if (m_timeline_zoom < 50) seconds_per_tick = 1.0f;
    else if (m_timeline_zoom < 100) seconds_per_tick = 0.5f;
    else if (m_timeline_zoom < 200) seconds_per_tick = 0.25f;

    float pixels_per_tick = seconds_per_tick * m_timeline_zoom;
    int tick_count = static_cast<int>((m_timeline_width + m_timeline_scroll) / pixels_per_tick) + 1;

    for (int i = 0; i < tick_count; ++i) {
        float time = i * seconds_per_tick;
        float x = time_to_screen_x(time) + m_timeline_canvas_pos.x - m_timeline_scroll;

        if (x < m_timeline_canvas_pos.x || x > m_timeline_canvas_pos.x + m_timeline_width) continue;

        // Major tick every second
        bool major = std::fmod(time, 1.0f) < 0.001f;
        float tick_height = major ? ruler_height * 0.6f : ruler_height * 0.3f;

        draw_list->AddLine(
            ImVec2(x, m_timeline_canvas_pos.y + ruler_height - tick_height),
            ImVec2(x, m_timeline_canvas_pos.y + ruler_height),
            IM_COL32(150, 150, 150, 255));

        // Time label for major ticks
        if (major) {
            char label[16];
            snprintf(label, sizeof(label), "%.1fs", time);
            draw_list->AddText(ImVec2(x + 2, m_timeline_canvas_pos.y + 2), IM_COL32(200, 200, 200, 255), label);
        }
    }
}

void AnimationPanel::draw_tracks() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float ruler_height = 30.0f;
    float y = m_timeline_canvas_pos.y + ruler_height;

    for (size_t i = 0; i < m_clip.tracks.size(); ++i) {
        bool selected = (m_selected_track == static_cast<int>(i));

        // Track background
        ImU32 bg_color = selected ? IM_COL32(60, 80, 100, 255) : IM_COL32(40, 40, 45, 255);
        draw_list->AddRectFilled(
            ImVec2(m_timeline_canvas_pos.x, y),
            ImVec2(m_timeline_canvas_pos.x + m_timeline_width, y + m_track_height),
            bg_color);

        // Track border
        draw_list->AddLine(
            ImVec2(m_timeline_canvas_pos.x, y + m_track_height),
            ImVec2(m_timeline_canvas_pos.x + m_timeline_width, y + m_track_height),
            IM_COL32(60, 60, 70, 255));

        y += m_track_height;
    }
}

void AnimationPanel::draw_keyframes() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float ruler_height = 30.0f;
    float keyframe_radius = 6.0f;

    for (size_t track_idx = 0; track_idx < m_clip.tracks.size(); ++track_idx) {
        const auto& track = m_clip.tracks[track_idx];
        float track_y = m_timeline_canvas_pos.y + ruler_height + track_idx * m_track_height + m_track_height * 0.5f;

        for (size_t kf_idx = 0; kf_idx < track.keyframes.size(); ++kf_idx) {
            const auto& kf = track.keyframes[kf_idx];
            float x = time_to_screen_x(kf.time) + m_timeline_canvas_pos.x - m_timeline_scroll;

            if (x < m_timeline_canvas_pos.x || x > m_timeline_canvas_pos.x + m_timeline_width) continue;

            bool selected = (m_selected_track == static_cast<int>(track_idx) &&
                            m_selected_keyframe == static_cast<int>(kf_idx));

            ImU32 fill_color = selected ? IM_COL32(255, 200, 100, 255) : IM_COL32(200, 150, 80, 255);

            // Draw diamond shape
            draw_list->AddQuadFilled(
                ImVec2(x, track_y - keyframe_radius),
                ImVec2(x + keyframe_radius, track_y),
                ImVec2(x, track_y + keyframe_radius),
                ImVec2(x - keyframe_radius, track_y),
                fill_color);

            // Draw border for selected keyframe (white outline)
            if (selected) {
                draw_list->AddQuad(
                    ImVec2(x, track_y - keyframe_radius - 1),
                    ImVec2(x + keyframe_radius + 1, track_y),
                    ImVec2(x, track_y + keyframe_radius + 1),
                    ImVec2(x - keyframe_radius - 1, track_y),
                    IM_COL32(255, 255, 255, 255),
                    2.0f);
            }

            // Handle click
            ImGui::SetCursorScreenPos(ImVec2(x - keyframe_radius, track_y - keyframe_radius));
            ImGui::InvisibleButton(("kf_" + std::to_string(track_idx) + "_" + std::to_string(kf_idx)).c_str(),
                                   ImVec2(keyframe_radius * 2, keyframe_radius * 2));

            if (ImGui::IsItemClicked()) {
                m_selected_track = static_cast<int>(track_idx);
                m_selected_keyframe = static_cast<int>(kf_idx);
            }
        }
    }
}

void AnimationPanel::draw_playhead() {
    // Use foreground draw list to render playhead on top of everything
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    float x = time_to_screen_x(m_playhead_time) + m_timeline_canvas_pos.x - m_timeline_scroll;

    if (x >= m_timeline_canvas_pos.x && x <= m_timeline_canvas_pos.x + m_timeline_width) {
        // Playhead line
        draw_list->AddLine(
            ImVec2(x, m_timeline_canvas_pos.y),
            ImVec2(x, m_timeline_canvas_pos.y + m_timeline_height),
            IM_COL32(255, 100, 100, 255),
            2.0f);

        // Playhead handle
        draw_list->AddTriangleFilled(
            ImVec2(x - 8, m_timeline_canvas_pos.y),
            ImVec2(x + 8, m_timeline_canvas_pos.y),
            ImVec2(x, m_timeline_canvas_pos.y + 15),
            IM_COL32(255, 100, 100, 255));
    }
}

float AnimationPanel::time_to_screen_x(float time) const {
    return time * m_timeline_zoom;
}

float AnimationPanel::screen_x_to_time(float x) const {
    return x / m_timeline_zoom;
}

void AnimationPanel::render_keyframe_properties() {
    ImGui::Text(ICON_FA_KEY " Keyframe Properties");
    ImGui::Separator();

    if (m_selected_track >= 0 && m_selected_track < static_cast<int>(m_clip.tracks.size())) {
        auto& track = m_clip.tracks[m_selected_track];

        if (m_selected_keyframe >= 0 && m_selected_keyframe < static_cast<int>(track.keyframes.size())) {
            auto& kf = track.keyframes[m_selected_keyframe];

            // Time
            float time = kf.time;
            if (ImGui::DragFloat("Time", &time, 0.01f, 0.0f, m_clip.duration)) {
                move_keyframe(m_selected_track, m_selected_keyframe, time);
            }

            // Interpolation
            const char* interp_names[] = { "Step", "Linear", "EaseIn", "EaseOut", "EaseInOut" };
            int interp = static_cast<int>(kf.interpolation);
            if (ImGui::Combo("Interpolation", &interp, interp_names, 5)) {
                kf.interpolation = static_cast<engine::animation::InterpolationType>(interp);
                mark_dirty();
            }

            ImGui::Separator();
            ImGui::Text("Value:");

            // Value editor based on type
            std::visit([this, &kf](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, bool>) {
                    bool v = val;
                    if (ImGui::Checkbox("Value", &v)) {
                        kf.value = v;
                        mark_dirty();
                    }
                } else if constexpr (std::is_same_v<T, int>) {
                    int v = val;
                    if (ImGui::InputInt("Value", &v)) {
                        kf.value = v;
                        mark_dirty();
                    }
                } else if constexpr (std::is_same_v<T, float>) {
                    float v = val;
                    if (ImGui::DragFloat("Value", &v, 0.01f)) {
                        kf.value = v;
                        mark_dirty();
                    }
                } else if constexpr (std::is_same_v<T, engine::animation::Vec2>) {
                    float v[2] = { val.x, val.y };
                    if (ImGui::DragFloat2("Value", v, 0.01f)) {
                        kf.value = engine::animation::Vec2{ v[0], v[1] };
                        mark_dirty();
                    }
                } else if constexpr (std::is_same_v<T, engine::animation::Vec3>) {
                    float v[3] = { val.x, val.y, val.z };
                    if (ImGui::DragFloat3("Value", v, 0.01f)) {
                        kf.value = engine::animation::Vec3{ v[0], v[1], v[2] };
                        mark_dirty();
                    }
                } else if constexpr (std::is_same_v<T, engine::animation::Vec4>) {
                    float v[4] = { val.x, val.y, val.z, val.w };
                    if (ImGui::DragFloat4("Value", v, 0.01f)) {
                        kf.value = engine::animation::Vec4{ v[0], v[1], v[2], v[3] };
                        mark_dirty();
                    }
                } else if constexpr (std::is_same_v<T, std::string>) {
                    char buf[256];
                    strncpy(buf, val.c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';  // Ensure null termination
                    if (ImGui::InputText("Value", buf, sizeof(buf))) {
                        kf.value = std::string(buf);
                        mark_dirty();
                    }
                }
            }, kf.value);

            ImGui::Separator();

            // Delete keyframe button
            if (ImGui::Button(ICON_FA_TRASH_CAN " Delete Keyframe")) {
                remove_keyframe(m_selected_track, m_selected_keyframe);
            }
        } else {
            ImGui::TextDisabled("Select a keyframe");

            // Add keyframe button
            if (ImGui::Button(ICON_FA_PLUS " Add Keyframe at Current Time")) {
                m_show_add_keyframe_dialog = true;
                m_new_keyframe_time = m_playhead_time;
            }
        }
    } else {
        ImGui::TextDisabled("Select a track");
    }
}

void AnimationPanel::render_add_track_dialog() {
    if (m_show_add_track_dialog) {
        ImGui::OpenPopup("Add Track");
        m_show_add_track_dialog = false;

        // Refresh animatable properties (now stores path + type pairs)
        if (m_properties_cache_dirty) {
            m_animatable_properties.clear();
            auto& resolver = engine::animation::PropertyResolver::instance();
            auto components = resolver.get_registered_components();
            for (const auto& comp : components) {
                auto props = resolver.get_animatable_properties(comp);
                for (const auto& prop : props) {
                    auto prop_type = resolver.get_property_type(prop);
                    if (prop_type) {
                        m_animatable_properties.emplace_back(prop, *prop_type);
                    }
                }
            }
            m_properties_cache_dirty = false;
        }
    }

    if (ImGui::BeginPopupModal("Add Track", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Add a new animation track");
        ImGui::Separator();

        // Property path input or dropdown
        ImGui::InputText("Property Path", m_new_track_property, sizeof(m_new_track_property));

        // Quick select from available properties (auto-selects type)
        if (ImGui::BeginCombo("Available Properties", "Select...")) {
            for (const auto& [prop_path, prop_type] : m_animatable_properties) {
                if (ImGui::Selectable(prop_path.c_str())) {
                    strncpy(m_new_track_property, prop_path.c_str(), sizeof(m_new_track_property) - 1);
                    // Auto-select the correct type
                    m_new_track_type = static_cast<int>(prop_type);
                }
            }
            ImGui::EndCombo();
        }

        // Value type (now auto-filled when selecting from dropdown)
        const char* type_names[] = { "Bool", "Int", "Float", "Vec2", "Vec3", "Vec4", "Color", "String" };
        ImGui::Combo("Value Type", &m_new_track_type, type_names, 8);

        ImGui::Separator();
        if (ImGui::Button("Add", ImVec2(120, 0))) {
            if (strlen(m_new_track_property) > 0) {
                add_track(m_new_track_property, static_cast<engine::animation::PropertyValueType>(m_new_track_type));
                m_new_track_property[0] = '\0';
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AnimationPanel::render_add_keyframe_dialog() {
    if (m_show_add_keyframe_dialog) {
        ImGui::OpenPopup("Add Keyframe");
        m_show_add_keyframe_dialog = false;
    }

    if (ImGui::BeginPopupModal("Add Keyframe", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Add a keyframe to track: %s",
                    m_selected_track >= 0 ? m_clip.tracks[m_selected_track].property_path.c_str() : "");
        ImGui::Separator();

        ImGui::DragFloat("Time", &m_new_keyframe_time, 0.01f, 0.0f, m_clip.duration);

        ImGui::Separator();
        if (ImGui::Button("Add", ImVec2(120, 0))) {
            if (m_selected_track >= 0) {
                // Get default value based on track type
                auto& track = m_clip.tracks[m_selected_track];
                engine::animation::PropertyValue default_value;
                switch (track.value_type) {
                    case engine::animation::PropertyValueType::Bool: default_value = false; break;
                    case engine::animation::PropertyValueType::Int: default_value = 0; break;
                    case engine::animation::PropertyValueType::Float: default_value = 0.0f; break;
                    case engine::animation::PropertyValueType::Vec2:
                        default_value = engine::animation::Vec2{0.0f, 0.0f}; break;
                    case engine::animation::PropertyValueType::Vec3:
                        default_value = engine::animation::Vec3{0.0f, 0.0f, 0.0f}; break;
                    case engine::animation::PropertyValueType::Vec4:
                    case engine::animation::PropertyValueType::Color:
                        default_value = engine::animation::Vec4{0.0f, 0.0f, 0.0f, 1.0f}; break;
                    case engine::animation::PropertyValueType::String:
                        default_value = std::string(); break;
                }
                add_keyframe(m_selected_track, m_new_keyframe_time, default_value);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AnimationPanel::add_track(const std::string& property_path, engine::animation::PropertyValueType type) {
    engine::animation::AnimationTrack track;
    track.property_path = property_path;
    track.value_type = type;
    m_clip.tracks.push_back(std::move(track));
    mark_dirty();
}

void AnimationPanel::remove_track(size_t index) {
    if (index < m_clip.tracks.size()) {
        m_clip.tracks.erase(m_clip.tracks.begin() + index);
        if (m_selected_track == static_cast<int>(index)) {
            m_selected_track = -1;
            m_selected_keyframe = -1;
        } else if (m_selected_track > static_cast<int>(index)) {
            m_selected_track--;
        }
        mark_dirty();
    }
}

void AnimationPanel::add_keyframe(size_t track_index, float time, const engine::animation::PropertyValue& value) {
    if (track_index < m_clip.tracks.size()) {
        auto& track = m_clip.tracks[track_index];

        engine::animation::Keyframe kf;
        kf.time = time;
        kf.value = value;

        // Use Step interpolation for types that can't be linearly interpolated (String, Bool)
        if (track.value_type == engine::animation::PropertyValueType::String ||
            track.value_type == engine::animation::PropertyValueType::Bool) {
            kf.interpolation = engine::animation::InterpolationType::Step;
        } else {
            kf.interpolation = engine::animation::InterpolationType::Linear;
        }

        track.keyframes.push_back(std::move(kf));
        track.sort_keyframes();
        mark_dirty();
    }
}

void AnimationPanel::remove_keyframe(size_t track_index, size_t keyframe_index) {
    if (track_index < m_clip.tracks.size()) {
        auto& track = m_clip.tracks[track_index];
        if (keyframe_index < track.keyframes.size()) {
            track.keyframes.erase(track.keyframes.begin() + keyframe_index);
            m_selected_keyframe = -1;
            mark_dirty();
        }
    }
}

void AnimationPanel::move_keyframe(size_t track_index, size_t keyframe_index, float new_time) {
    if (track_index < m_clip.tracks.size()) {
        auto& track = m_clip.tracks[track_index];
        if (keyframe_index < track.keyframes.size()) {
            track.keyframes[keyframe_index].time = new_time;
            track.sort_keyframes();
            mark_dirty();
        }
    }
}

void AnimationPanel::play() {
    m_is_playing = true;
    m_last_frame_time = static_cast<float>(ImGui::GetTime());
}

void AnimationPanel::pause() {
    m_is_playing = false;
}

void AnimationPanel::stop() {
    m_is_playing = false;
    m_playhead_time = 0.0f;
}

void AnimationPanel::set_time(float time) {
    m_playhead_time = std::clamp(time, 0.0f, m_clip.duration);
    apply_preview_values();
}

void AnimationPanel::update_preview(float dt) {
    m_playhead_time += dt;

    if (m_playhead_time > m_clip.duration) {
        if (m_clip.looping) {
            m_playhead_time = std::fmod(m_playhead_time, m_clip.duration);
        } else {
            m_playhead_time = m_clip.duration;
            m_is_playing = false;
        }
    }

    apply_preview_values();
}

void AnimationPanel::apply_preview_values() {
    // Get the target entity (use selected entity if no target set)
    entt::entity target = m_target_entity;
    if (target == entt::null) {
        const auto& sel = m_context.selection().selection();
        if (!sel.empty()) {
            target = sel[0];
        }
    }

    if (target == entt::null) {
        return;
    }

    // Get the registry (uses editing override if in prefab editor)
    auto* registry = m_context.registry();
    if (!registry) {
        return;
    }

    // Check if entity is valid
    if (!registry->valid(target)) {
        return;
    }

    // If entity changed or we haven't captured values yet, capture them now
    if (target != m_preview_entity || !m_has_captured_values) {
        // Restore old entity's values first if different entity
        if (m_preview_entity != entt::null && m_preview_entity != target) {
            restore_original_values();
        }
        m_preview_entity = target;
        capture_original_values();
    }

    // Apply each track's sampled value to the entity
    auto& resolver = engine::animation::PropertyResolver::instance();

    for (const auto& track : m_clip.tracks) {
        if (track.keyframes.empty()) {
            continue;
        }

        // Sample the track at current playhead time
        engine::animation::PropertyValue value = track.sample(m_playhead_time);

        // Apply the value to the entity
        resolver.set_value(*registry, target, track.property_path, value);
    }
}

void AnimationPanel::capture_original_values() {
    if (m_preview_entity == entt::null || m_clip.tracks.empty()) {
        return;
    }

    // Get the registry (uses editing override if in prefab editor)
    auto* registry = m_context.registry();
    if (!registry || !registry->valid(m_preview_entity)) {
        return;
    }

    m_original_values.clear();
    auto& resolver = engine::animation::PropertyResolver::instance();

    for (const auto& track : m_clip.tracks) {
        auto value = resolver.get_value(*registry, m_preview_entity, track.property_path);
        if (value) {
            m_original_values[track.property_path] = *value;
        }
    }

    m_has_captured_values = true;
}

void AnimationPanel::restore_original_values() {
    if (m_preview_entity == entt::null || m_original_values.empty()) {
        clear_original_values();
        return;
    }

    // Get the registry (uses editing override if in prefab editor)
    auto* registry = m_context.registry();
    if (!registry || !registry->valid(m_preview_entity)) {
        clear_original_values();
        return;
    }

    auto& resolver = engine::animation::PropertyResolver::instance();

    for (const auto& [property_path, value] : m_original_values) {
        resolver.set_value(*registry, m_preview_entity, property_path, value);
    }

    clear_original_values();
}

void AnimationPanel::clear_original_values() {
    m_original_values.clear();
    m_preview_entity = entt::null;
    m_has_captured_values = false;
}

void AnimationPanel::on_selection_changed() {
    // If the selection changed and we have captured values, restore them
    const auto& sel = m_context.selection().selection();
    entt::entity new_selection = sel.empty() ? entt::null : sel[0];

    // If selection changed away from our preview entity, restore original values
    if (m_has_captured_values && m_preview_entity != entt::null && m_preview_entity != new_selection) {
        restore_original_values();
    }
}

void AnimationPanel::start_recording() {
    // Use currently selected entity as recording target
    const auto& sel = m_context.selection().selection();
    if (!sel.empty()) {
        m_target_entity = sel[0];
    }

    if (m_target_entity == entt::null) {
        engine::Logger::instance().warning("Animation", "Cannot start recording: no entity selected");
        return;
    }

    m_is_recording = true;

    // Register recording callbacks with EditorContext
    m_context.set_animation_recording_check([this](entt::entity entity) {
        return m_is_recording && entity == m_target_entity && entity != entt::null;
    });

    m_context.set_animation_record_callback([this](entt::entity entity,
                                                    const std::string& property_path,
                                                    const engine::animation::PropertyValue& value,
                                                    engine::animation::PropertyValueType type) {
        return record_property_change(entity, property_path, value, type);
    });

    engine::Logger::instance().info("Animation", "Recording started for entity %u", static_cast<uint32_t>(m_target_entity));
}

void AnimationPanel::stop_recording() {
    m_is_recording = false;

    // Clear recording callbacks
    m_context.set_animation_recording_check(nullptr);
    m_context.set_animation_record_callback(nullptr);
}

bool AnimationPanel::record_property_change(entt::entity entity, const std::string& property_path,
                                            const engine::animation::PropertyValue& value,
                                            engine::animation::PropertyValueType type) {
    // Only record if recording is active and entity matches
    if (!m_is_recording || entity != m_target_entity || entity == entt::null) {
        return false;
    }

    // Find or create track for this property
    int track_index = -1;
    for (size_t i = 0; i < m_clip.tracks.size(); ++i) {
        if (m_clip.tracks[i].property_path == property_path) {
            track_index = static_cast<int>(i);
            break;
        }
    }

    // Create track if it doesn't exist
    if (track_index < 0) {
        engine::animation::AnimationTrack new_track;
        new_track.property_path = property_path;
        new_track.value_type = type;
        m_clip.tracks.push_back(std::move(new_track));
        track_index = static_cast<int>(m_clip.tracks.size() - 1);
    }

    auto& track = m_clip.tracks[track_index];

    // Check if there's already a keyframe at this exact time
    bool found_existing = false;
    for (auto& kf : track.keyframes) {
        if (std::abs(kf.time - m_playhead_time) < 0.001f) {
            // Update existing keyframe
            kf.value = value;
            found_existing = true;
            break;
        }
    }

    // Add new keyframe if none exists at this time
    if (!found_existing) {
        engine::animation::Keyframe kf;
        kf.time = m_playhead_time;
        kf.value = value;
        kf.interpolation = engine::animation::InterpolationType::Linear;
        track.keyframes.push_back(std::move(kf));
        track.sort_keyframes();
    }

    mark_dirty();
    return true;
}

void AnimationPanel::open_clip(const std::string& path) {
    // Restore original values before loading new clip
    restore_original_values();

    auto* runtime = m_context.runtime();
    if (!runtime || !runtime->engine()) {
        engine::Logger::instance().error("Animation", "Cannot open clip: engine not available");
        return;
    }
    auto& assets = runtime->engine()->assets();
    auto handle = assets.load<engine::animation::AnimationClip>(path);
    const auto* clip = assets.get(handle);

    if (clip) {
        m_clip = *clip;
        m_current_path = path;
        m_has_unsaved_changes = false;
        m_selected_track = -1;
        m_selected_keyframe = -1;
        m_playhead_time = 0.0f;
        engine::Logger::instance().info("Animation", "Opened clip: %s", path.c_str());
    } else {
        engine::Logger::instance().error("Animation", "Failed to load clip: %s", path.c_str());
    }
}

void AnimationPanel::new_clip() {
    // Restore original values before creating new clip
    restore_original_values();

    m_clip = engine::animation::AnimationClip();
    m_clip.duration = 1.0f;
    m_current_path.clear();
    m_has_unsaved_changes = false;
    m_selected_track = -1;
    m_selected_keyframe = -1;
    m_playhead_time = 0.0f;
}

bool AnimationPanel::save_clip() {
    if (m_current_path.empty()) {
        auto initial_dir = get_assets_directory(m_context.scene_state().project_path());
        auto path = save_animation_clip(initial_dir);
        if (path.empty()) return false;
        return save_clip_as(path);
    }
    return save_clip_as(m_current_path);
}

bool AnimationPanel::save_clip_as(const std::string& path) {
    bool success = engine::animation::save_clip(m_clip, path);
    if (success) {
        m_current_path = path;
        m_has_unsaved_changes = false;
        engine::Logger::instance().info("Animation", "Saved clip: %s", path.c_str());
    } else {
        engine::Logger::instance().error("Animation", "Failed to save clip: %s", path.c_str());
    }
    return success;
}

void AnimationPanel::mark_dirty() {
    m_has_unsaved_changes = true;
}

bool AnimationPanel::is_property_animated(entt::entity entity, const std::string& property_path) const {
    // Check if this entity is the target (or selected if no target)
    entt::entity target = m_target_entity;
    if (target == entt::null) {
        const auto& sel = m_context.selection().selection();
        if (!sel.empty()) {
            target = sel[0];
        }
    }

    // Only highlight properties for the target entity
    if (entity != target || entity == entt::null) {
        return false;
    }

    // Check if any track in the clip has this property path
    for (const auto& track : m_clip.tracks) {
        if (track.property_path == property_path) {
            return true;
        }
    }

    return false;
}

} // namespace editor

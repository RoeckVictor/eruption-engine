#pragma once

#include "Panel.h"
#include "engine/animation/AnimationClip.h"
#include "engine/animation/AnimationTrack.h"
#include "engine/animation/PropertyValue.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <utility>
#include <entt/entt.hpp>

namespace editor {

class EditorContext;

// Animation Clip editor panel for editing .anim files
class AnimationPanel : public Panel {
public:
    explicit AnimationPanel(EditorContext& context);
    ~AnimationPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    void open_clip(const std::string& path);
    void new_clip();

    bool save_clip();
    bool save_clip_as(const std::string& path);
    bool has_unsaved_changes() const { return m_has_unsaved_changes; }

    const std::string& current_path() const { return m_current_path; }

    void set_target_entity(entt::entity entity) { m_target_entity = entity; }
    entt::entity target_entity() const { return m_target_entity; }

    bool is_recording() const { return m_is_recording; }
    bool is_property_animated(entt::entity entity, const std::string& property_path) const;

    bool record_property_change(entt::entity entity, const std::string& property_path,
                                const engine::animation::PropertyValue& value,
                                engine::animation::PropertyValueType type);

private:
    void render_toolbar();
    void render_track_list();
    void render_timeline();
    void render_keyframe_properties();
    void render_add_track_dialog();
    void render_add_keyframe_dialog();

    float time_to_screen_x(float time) const;
    float screen_x_to_time(float x) const;
    void draw_timeline_ruler();
    void draw_tracks();
    void draw_keyframes();
    void draw_playhead();

    void add_track(const std::string& property_path, engine::animation::PropertyValueType type);
    void remove_track(size_t index);

    void add_keyframe(size_t track_index, float time, const engine::animation::PropertyValue& value);
    void remove_keyframe(size_t track_index, size_t keyframe_index);
    void move_keyframe(size_t track_index, size_t keyframe_index, float new_time);

    void play();
    void pause();
    void stop();
    void set_time(float time);
    void update_preview(float dt);
    void apply_preview_values();

    void capture_original_values();
    void restore_original_values();
    void clear_original_values();
    void on_selection_changed();

    void start_recording();
    void stop_recording();

    void mark_dirty();

    EditorContext& m_context;

    engine::animation::AnimationClip m_clip;
    std::string m_current_path;
    bool m_has_unsaved_changes = false;

    bool m_is_playing = false;
    bool m_is_recording = false;
    float m_playhead_time = 0.0f;
    float m_last_frame_time = 0.0f;

    float m_timeline_scroll = 0.0f;
    float m_timeline_zoom = 100.0f;
    float m_timeline_width = 800.0f;
    float m_timeline_height = 300.0f;
    float m_track_height = 24.0f;
    ImVec2 m_timeline_canvas_pos;

    int m_selected_track = -1;
    int m_selected_keyframe = -1;

    bool m_dragging_playhead = false;

    entt::entity m_target_entity = entt::null;

    bool m_show_add_track_dialog = false;
    char m_new_track_property[256] = "";
    int m_new_track_type = 0;

    bool m_show_add_keyframe_dialog = false;
    float m_new_keyframe_time = 0.0f;

    engine::animation::PropertyValue m_editing_value;
    int m_editing_interpolation = 0;

    float m_track_list_width = 200.0f;
    float m_properties_width = 250.0f;

    std::vector<std::pair<std::string, engine::animation::PropertyValueType>> m_animatable_properties;
    bool m_properties_cache_dirty = true;

    std::unordered_map<std::string, engine::animation::PropertyValue> m_original_values;
    entt::entity m_preview_entity = entt::null;
    bool m_has_captured_values = false;
};

}

#pragma once

#include "engine/animation/PropertyValue.h"
#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <imgui.h>

namespace editor {

// Unified recording context for animation keyframe capture in inspectors.
// Used by all inspector types (AutoInspector, AnimatorInspector, PixelGridComponentInspector, etc.)
struct RecordingContext {
    entt::entity entity = entt::null;
    entt::registry* registry = nullptr;  // For entity ref dropdown access

    std::string component_name;

    bool is_recording = false;

    std::function<bool(entt::entity, const std::string&,
                       const engine::animation::PropertyValue&,
                       engine::animation::PropertyValueType)> record_callback;
    std::function<bool(entt::entity, const std::string&)> is_animated_callback;

    bool active() const { return is_recording && record_callback != nullptr; }
    bool is_animated(const std::string& property_path) const {
        return is_animated_callback && is_animated_callback(entity, property_path);
    }
    bool record(const std::string& property_path,
                const engine::animation::PropertyValue& value,
                engine::animation::PropertyValueType type) const {
        if (active()) {
            return record_callback(entity, property_path, value, type);
        }
        return false;
    }
};

// RAII helper for animated property styling in ImGui
class ScopedAnimatedStyle {
public:
    explicit ScopedAnimatedStyle(bool is_animated,
                                  ImVec4 frame_bg = ImVec4(0.5f, 0.2f, 0.2f, 1.0f))
        : m_count(0) {
        if (is_animated) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, frame_bg);
            m_count = 1;
        }
    }

    ScopedAnimatedStyle(bool is_animated, bool include_button_colors)
        : m_count(0) {
        if (is_animated) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
            if (include_button_colors) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
                m_count = 3;
            } else {
                m_count = 1;
            }
        }
    }

    ~ScopedAnimatedStyle() {
        if (m_count > 0) {
            ImGui::PopStyleColor(m_count);
        }
    }

    ScopedAnimatedStyle(const ScopedAnimatedStyle&) = delete;
    ScopedAnimatedStyle& operator=(const ScopedAnimatedStyle&) = delete;

private:
    int m_count;
};

}

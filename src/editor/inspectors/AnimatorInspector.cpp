#include "AnimatorInspector.h"
#include "InspectorUtils.h"
#include "AssetPicker.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "engine/animation/Animator.h"
#include "engine/platform/PlatformUtils.h"
#include "engine/core/Logger.h"
#include <imgui.h>
#include <filesystem>

namespace editor {

// Animator controller file extensions
static const std::vector<std::string> ANIMSTATE_EXTENSIONS = {
    ".animstate"
};

bool AnimatorInspector::draw(engine::animation::Animator& animator, const std::string& project_path) {
    RecordingContext empty_ctx;
    return draw(animator, project_path, empty_ctx);
}

bool AnimatorInspector::draw(engine::animation::Animator& animator, const std::string& project_path,
                              const RecordingContext& rec_ctx) {
    bool changed = false;

    // Helper to check if a property is animated (for red background)
    auto is_animated = [&](const std::string& prop_name) {
        return rec_ctx.is_animated("Animator." + prop_name);
    };

    // Helper to record a property change
    auto record_property = [&](const std::string& prop_name, const engine::animation::PropertyValue& value,
                               engine::animation::PropertyValueType type) {
        rec_ctx.record("Animator." + prop_name, value, type);
    };

    // Enabled checkbox
    {
        ScopedAnimatedStyle style(is_animated("enabled"));
        if (EnabledCheckbox(&animator.enabled)) {
            changed = true;
            record_property("enabled", animator.enabled, engine::animation::PropertyValueType::Bool);
        }
    }

    SectionSeparator();

    // Controller path field with asset picker
    ImGui::Text("Animator Controller");

    {
        ScopedAnimatedStyle style(is_animated("controller_path"), true);

        AssetPickerConfig config;
        config.popup_id = "AnimatorControllerPicker";
        config.title = "Select Animator Controller (.animstate)";
        config.extensions = ANIMSTATE_EXTENSIONS;
        config.empty_message = "No .animstate files found in Assets folder";
        config.clear_button_label = "Clear Selection";
        config.button_tooltip = "Click to select an .animstate file\nOr drag & drop from File Browser panel";

        auto result = AssetPicker::draw_button(animator.controller_path, project_path, config);
        if (result.changed) {
            animator.controller_path = result.selected_path;
            animator.reset();  // Reset state when controller changes
            changed = true;
            record_property("controller_path", result.selected_path, engine::animation::PropertyValueType::String);
        }
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(ANIMSTATE_EXTENSIONS);
        if (!dropped.empty()) {
            animator.controller_path = dropped;
            animator.reset();
            changed = true;
            record_property("controller_path", dropped, engine::animation::PropertyValueType::String);
        }
        ImGui::EndDragDropTarget();
    }

    // If no controller assigned, show help text
    if (animator.controller_path.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No controller assigned.");
        ImGui::TextDisabled("Assign a .animstate file to enable animation.");
        return changed;
    }

    SectionSeparator();

    // Runtime State Section (read-only display)
    ImGui::Text("Runtime State");

    // Current state
    ImGui::BulletText("Current State: %s",
        animator.current_state.empty() ? "(initializing)" : animator.current_state.c_str());

    // State time
    ImGui::BulletText("State Time: %.2fs", animator.state_time);

    // Blending status
    if (animator.is_blending) {
        ImGui::BulletText("Blending: %.0f%% from %s",
            animator.blend_progress * 100.0f,
            animator.blend_from_state.c_str());
    }

    // Initialized status
    if (!animator.initialized) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Not initialized (will initialize on play)");
    }

    SectionSeparator();

    // Parameters Section
    if (ImGui::TreeNode("Parameters")) {
        if (animator.bool_params.empty() &&
            animator.int_params.empty() &&
            animator.float_params.empty() &&
            animator.trigger_params.empty()) {
            ImGui::TextDisabled("No parameters (load controller to populate)");
        } else {
            // Bool parameters
            for (auto& [name, value] : animator.bool_params) {
                if (ImGui::Checkbox(name.c_str(), &value)) {
                    // Value is modified in-place
                }
            }

            // Int parameters
            for (auto& [name, value] : animator.int_params) {
                ImGui::SetNextItemWidth(100.0f);
                ImGui::DragInt(name.c_str(), &value);
            }

            // Float parameters
            for (auto& [name, value] : animator.float_params) {
                ImGui::SetNextItemWidth(100.0f);
                ImGui::DragFloat(name.c_str(), &value, 0.01f);
            }

            // Trigger parameters
            for (auto& [name, value] : animator.trigger_params) {
                if (ImGui::Button(name.c_str())) {
                    animator.set_trigger(name);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(trigger)");
            }
        }
        ImGui::TreePop();
    }

    // Events Section
    if (!animator.pending_events.empty()) {
        SectionSeparator();
        ImGui::Text("Pending Events");
        for (const auto& event : animator.pending_events) {
            ImGui::BulletText("%s", event.c_str());
        }
    }

    SectionSeparator();

    // Reset button
    if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset Animator")) {
        animator.reset();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Reset state machine");

    return changed;
}

}

#include "AnimatorInspector.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "engine/animation/Animator.h"
#include <imgui.h>

namespace editor {

bool AnimatorInspector::draw(engine::animation::Animator& animator) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &animator.enabled)) {
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Playback controls
    ImGui::Text("Playback");

    bool is_playing = animator.playing;
    if (ImGui::Checkbox("Playing", &is_playing)) {
        animator.playing = is_playing;
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_ARROW_ROTATE_LEFT " Reset")) {
        animator.reset();
        changed = true;
    }

    ImGui::Spacing();

    // Current clip selection
    ImGui::Text("Current Clip");

    if (ImGui::BeginCombo("##CurrentClip", animator.current_clip.empty() ? "(None)" : animator.current_clip.c_str())) {
        // Show option for no clip
        if (ImGui::Selectable("(None)", animator.current_clip.empty())) {
            animator.current_clip = "";
            changed = true;
        }

        // Show all available clips
        for (const auto& [name, clip] : animator.clips) {
            bool is_selected = (animator.current_clip == name);
            if (ImGui::Selectable(name.c_str(), is_selected)) {
                animator.play(name);
                changed = true;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // Clip information
    if (!animator.current_clip.empty()) {
        auto it = animator.clips.find(animator.current_clip);
        if (it != animator.clips.end()) {
            const auto& clip = it->second;

            ImGui::Text("Clip Info");
            ImGui::BulletText("Frames: %zu", clip.frames.size());
            float fps = clip.frame_duration > 0.0f ? (1.0f / clip.frame_duration) : 0.0f;
            ImGui::BulletText("FPS: %.1f (frame duration: %.3fs)", fps, clip.frame_duration);
            ImGui::BulletText("Loop: %s", clip.looping ? "Yes" : "No");

            ImGui::Spacing();

            // Playback state
            ImGui::Text("Playback State");
            ImGui::BulletText("Current Frame: %d / %zu", animator.current_frame, clip.frames.size());
            ImGui::BulletText("Elapsed: %.2fs", animator.elapsed);

            if (animator.finished) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Animation Finished");
            }
        }
    }

    ImGui::Spacing();

    // Available clips list
    if (ImGui::TreeNode("Available Clips")) {
        if (animator.clips.empty()) {
            ImGui::TextDisabled("No clips loaded");
        } else {
            for (const auto& [name, clip] : animator.clips) {
                float fps = clip.frame_duration > 0.0f ? (1.0f / clip.frame_duration) : 0.0f;
                ImGui::BulletText("%s (%zu frames, %.1f fps)",
                    name.c_str(), clip.frames.size(), fps);
            }
        }
        ImGui::TreePop();
    }

    // TODO: Add UI for loading new clips from asset browser
    // TODO: Add UI for creating/editing clips in editor

    return changed;
}

} // namespace editor

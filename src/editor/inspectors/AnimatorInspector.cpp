#include "AnimatorInspector.h"
#include "InspectorUtils.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "engine/animation/Animator.h"
#include "engine/asset/loaders/AnimationClipLoader.h"
#include "engine/platform/PlatformUtils.h"
#include "engine/core/Logger.h"
#include <imgui.h>
#include <fstream>
#include <filesystem>

namespace editor {

namespace {

// Helper to load animation clips from a JSON file into an animator
// Returns the number of clips successfully loaded
int load_clips_from_file(const std::string& path, engine::animation::Animator& animator) {
    std::ifstream file(path);
    if (!file) {
        engine::Logger::instance().warning("Editor", "AnimatorInspector: Failed to open file: %s", path.c_str());
        return 0;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    int loaded_count = 0;
    try {
        auto json = nlohmann::json::parse(content);
        if (json.is_array()) {
            for (const auto& item : json) {
                auto clip = engine::asset::AnimationClipLoader::parse_clip(item);
                if (clip) {
                    animator.clips[clip->name] = std::move(*clip);
                    ++loaded_count;
                }
            }
        } else if (json.is_object()) {
            auto clip = engine::asset::AnimationClipLoader::parse_clip(json);
            if (clip) {
                animator.clips[clip->name] = std::move(*clip);
                ++loaded_count;
            }
        } else {
            engine::Logger::instance().warning("Editor", "AnimatorInspector: Invalid JSON format in file: %s", path.c_str());
        }
    } catch (const nlohmann::json::exception& e) {
        engine::Logger::instance().warning("Editor", "AnimatorInspector: JSON parse error in '%s': %s", path.c_str(), e.what());
    }

    if (loaded_count > 0) {
        engine::Logger::instance().info("Editor", "AnimatorInspector: Loaded %d animation clip(s) from '%s'", loaded_count, path.c_str());
    }

    return loaded_count;
}

}

bool AnimatorInspector::draw(engine::animation::Animator& animator) {
    bool changed = false;

    // Enabled checkbox
    if (EnabledCheckbox(&animator.enabled)) {
        changed = true;
    }

    SectionSeparator();

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

    SectionSeparator();

    // Load clips from file
    ImGui::Text("Load Clips");
    if (ImGui::Button(ICON_FA_FILE_IMPORT " Load from File...")) {
        std::string path = engine::platform::open_file_dialog(
            "Select Animation Clip",
            {{"Animation Clips (*.json)", "*.json"}, {"All Files (*.*)", "*.*"}});
        if (!path.empty()) {
            if (load_clips_from_file(path, animator) > 0) {
                changed = true;
            }
        }
    }

    // Drag-drop target for loading clips
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string path(static_cast<const char*>(payload->Data), payload->DataSize - 1);
            if (std::filesystem::path(path).extension() == ".json") {
                if (load_clips_from_file(path, animator) > 0) {
                    changed = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::TextDisabled("Drag .json animation files here or use the button above");

    return changed;
}

}

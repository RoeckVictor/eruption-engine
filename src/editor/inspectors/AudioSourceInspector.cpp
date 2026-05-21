#include "AudioSourceInspector.h"
#include "AssetPicker.h"
#include "InspectorUtils.h"
#include "engine/audio/AudioSource.h"
#include "engine/audio/AudioEngine.h"
#include <imgui.h>

namespace editor {

// Audio file extensions supported by miniaudio
static const std::vector<std::string> AUDIO_EXTENSIONS = {
    ".wav", ".mp3", ".flac", ".WAV", ".MP3", ".FLAC"
};

// Preview handle shared across inspector redraws (editor-only playback)
static uint64_t s_preview_handle = 0;

bool AudioSourceInspector::draw(engine::audio::AudioSource& source,
                                 const std::string& project_path,
                                 engine::audio::AudioEngine* audio_engine) {
    bool changed = false;

    // Enabled checkbox
    if (EnabledCheckbox(&source.enabled)) changed = true;

    SectionSeparator();

    // Audio clip with asset picker
    ImGui::Text("Audio Clip");

    AssetPickerConfig config;
    config.popup_id = "AudioClipAssetPicker";
    config.title = "Select Audio Asset (.wav, .mp3, .flac)";
    config.extensions = AUDIO_EXTENSIONS;
    config.empty_message = "No audio files found in Assets folder";
    config.clear_button_label = "Clear";
    config.button_tooltip = "Click to select an audio file (.wav, .mp3, .flac)\n"
                            "Or drag & drop from File Browser panel";

    auto result = AssetPicker::draw_button(source.clip_path, project_path, config);
    if (result.changed) {
        source.clip_path = result.selected_path;
        changed = true;
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(AUDIO_EXTENSIONS);
        if (!dropped.empty()) {
            source.clip_path = dropped;
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    // Preview buttons (only if we have an audio engine and a clip)
    if (audio_engine && !source.clip_path.empty()) {
        bool currently_previewing = (s_preview_handle != 0 && audio_engine->is_playing(s_preview_handle));
        if (!currently_previewing) {
            if (ImGui::Button("Preview")) {
                engine::audio::PlayParams params;
                params.volume = source.volume;
                params.pitch  = source.pitch;
                params.pan    = source.pan;
                params.loop   = false;
                s_preview_handle = audio_engine->play(source.clip_path, params);
            }
        } else {
            if (ImGui::Button("Stop")) {
                audio_engine->stop(s_preview_handle);
                s_preview_handle = 0;
            }
        }
    }

    SectionSeparator();

    // Volume
    ImGui::Text("Volume");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##Volume", &source.volume, 0.0f, 1.0f, "%.2f")) changed = true;

    // Pitch
    ImGui::Text("Pitch");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##Pitch", &source.pitch, 0.1f, 3.0f, "%.2f")) changed = true;

    // Pan
    ImGui::Text("Pan");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##Pan", &source.pan, -1.0f, 1.0f, "%.2f")) changed = true;

    SectionSeparator();

    // Flags
    if (ImGui::Checkbox("Loop", &source.loop)) changed = true;
    if (ImGui::Checkbox("Play On Start", &source.play_on_start)) changed = true;

    SectionSeparator();

    // Channel group
    ImGui::Text("Channel");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    const char* groups[] = { "Master", "SFX", "Music", "UI" };
    if (ImGui::Combo("##ChannelGroup", &source.channel_group, groups, 4)) changed = true;

    SectionSeparator();

    // Spatial audio
    if (ImGui::Checkbox("Spatial", &source.spatial)) changed = true;

    if (source.spatial) {
        ImGui::Text("Min Distance");
        ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##MinDist", &source.min_distance, 1.0f, 1.0f, 1000.0f, "%.0f px"))
            changed = true;

        ImGui::Text("Max Distance");
        ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##MaxDist", &source.max_distance, 10.0f, 10.0f, 5000.0f, "%.0f px"))
            changed = true;
    }

    return changed;
}

}

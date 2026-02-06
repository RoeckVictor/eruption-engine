#pragma once

#include "engine/animation/AnimationClip.h"
#include "engine/asset/VFS.h"
#include "engine/core/Log.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace engine::asset {

/// Loads AnimationClip definitions from JSON files.
///
/// JSON format:
/// {
///   "name": "player_walk",
///   "frame_duration": 0.12,
///   "looping": true,
///   "frames": [
///     { "u0": 0.0, "v0": 0.0, "u1": 0.0625, "v1": 0.0625 },
///     ...
///   ]
/// }
///
/// Or an array of clips in a single file:
/// [
///   { "name": "idle", "frame_duration": 0.2, "looping": true, "frames": [...] },
///   { "name": "walk", "frame_duration": 0.12, "looping": true, "frames": [...] }
/// ]
struct AnimationClipLoader {
    static std::optional<animation::AnimationClip> parse_clip(const nlohmann::json& j) {
        animation::AnimationClip clip;

        if (!j.contains("name") || !j["name"].is_string()) return std::nullopt;
        clip.name = j["name"].get<std::string>();
        clip.frame_duration = j.value("frame_duration", 0.1f);
        clip.looping = j.value("looping", true);

        if (!j.contains("frames") || !j["frames"].is_array()) return std::nullopt;
        for (const auto& frame : j["frames"]) {
            animation::FrameRect rect;
            rect.u0 = frame.value("u0", 0.0f);
            rect.v0 = frame.value("v0", 0.0f);
            rect.u1 = frame.value("u1", 1.0f);
            rect.v1 = frame.value("v1", 1.0f);
            clip.frames.push_back(rect);
        }

        return clip;
    }

    static std::vector<animation::AnimationClip> load(const VFS& vfs,
                                                       const std::string& virtual_path) {
        std::vector<animation::AnimationClip> clips;

        auto text = vfs.read_text(virtual_path);
        if (text.is_err()) {
            ENGINE_ERR("AnimationClipLoader: Cannot read '%s': %s",
                       virtual_path.c_str(), text.error().message.c_str());
            return clips;
        }

        try {
            auto root = nlohmann::json::parse(text.value());

            if (root.is_array()) {
                for (const auto& item : root) {
                    auto clip = parse_clip(item);
                    if (clip) clips.push_back(std::move(*clip));
                }
            } else if (root.is_object()) {
                auto clip = parse_clip(root);
                if (clip) clips.push_back(std::move(*clip));
            }
        } catch (const nlohmann::json::exception& e) {
            ENGINE_ERR("AnimationClipLoader: JSON error in '%s': %s",
                       virtual_path.c_str(), e.what());
        }

        return clips;
    }
};

} // namespace engine::asset

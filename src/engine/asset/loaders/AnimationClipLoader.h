#pragma once

#include "engine/asset/AssetLoader.h"
#include "engine/asset/VFS.h"
#include "engine/animation/AnimationClip.h"
#include "engine/animation/PropertyValueJson.h"
#include "engine/core/Log.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <fstream>

namespace engine::asset {

// AnimationClip loader specialization
template<>
struct AssetLoader<animation::AnimationClip> {
    static std::unique_ptr<animation::AnimationClip> load(const VFS& vfs, const std::string& virtual_path) {
        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) {
            ENGINE_ERR("AnimationClipLoader: Cannot read '%s': %s",
                       virtual_path.c_str(), file_data.error().message.c_str());
            return nullptr;
        }

        try {
            nlohmann::json j = nlohmann::json::parse(file_data.value().bytes);
            auto clip = std::make_unique<animation::AnimationClip>();
            parse_clip(j, *clip);

            ENGINE_LOG("AnimationClipLoader: Loaded '%s' (%zu tracks, %.2fs)",
                       virtual_path.c_str(), clip->tracks.size(), clip->duration);
            return clip;
        } catch (const nlohmann::json::exception& e) {
            ENGINE_ERR("AnimationClipLoader: JSON parse error in '%s': %s",
                       virtual_path.c_str(), e.what());
            return nullptr;
        }
    }

    static bool reload(animation::AnimationClip& clip, const VFS& vfs, const std::string& virtual_path) {
        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) return false;

        try {
            nlohmann::json j = nlohmann::json::parse(file_data.value().bytes);

            // Clear existing data
            clip.tracks.clear();
            clip.events.clear();

            parse_clip(j, clip);
            return true;
        } catch (const nlohmann::json::exception&) {
            return false;
        }
    }

private:
    static void parse_clip(const nlohmann::json& j, animation::AnimationClip& clip) {
        if (j.contains("name")) clip.name = j["name"].get<std::string>();
        if (j.contains("duration")) clip.duration = j["duration"].get<float>();
        if (j.contains("looping")) clip.looping = j["looping"].get<bool>();

        // Parse tracks
        if (j.contains("tracks") && j["tracks"].is_array()) {
            for (const auto& track_json : j["tracks"]) {
                animation::AnimationTrack track;
                parse_track(track_json, track);
                clip.tracks.push_back(std::move(track));
            }
        }

        // Parse events
        if (j.contains("events") && j["events"].is_array()) {
            for (const auto& event_json : j["events"]) {
                animation::AnimationEvent event;
                if (event_json.contains("time")) event.time = event_json["time"].get<float>();
                if (event_json.contains("name")) event.name = event_json["name"].get<std::string>();
                if (event_json.contains("parameter")) event.parameter = event_json["parameter"].get<std::string>();
                clip.events.push_back(std::move(event));
            }
        }
        clip.sort_events();
    }

    static void parse_track(const nlohmann::json& j, animation::AnimationTrack& track) {
        if (j.contains("property")) track.property_path = j["property"].get<std::string>();
        if (j.contains("type")) {
            track.value_type = animation::property_value_type_from_string(j["type"].get<std::string>());
        }

        // Parse keyframes
        if (j.contains("keyframes") && j["keyframes"].is_array()) {
            for (const auto& kf_json : j["keyframes"]) {
                animation::Keyframe kf;
                parse_keyframe(kf_json, kf, track.value_type);
                track.keyframes.push_back(std::move(kf));
            }
        }
        track.sort_keyframes();
    }

    static void parse_keyframe(const nlohmann::json& j, animation::Keyframe& kf,
                               animation::PropertyValueType type) {
        if (j.contains("time")) kf.time = j["time"].get<float>();
        if (j.contains("interpolation")) {
            kf.interpolation = animation::interpolation_type_from_string(
                j["interpolation"].get<std::string>());
        }

        if (j.contains("value")) {
            kf.value = animation::parse_property_value(j["value"], type);
        }
    }
};

}

// Serialization helpers for saving clips
namespace engine::animation {

inline void to_json(nlohmann::json& j, const AnimationClip& clip) {
    j["name"] = clip.name;
    j["duration"] = clip.duration;
    j["looping"] = clip.looping;

    j["tracks"] = nlohmann::json::array();
    for (const auto& track : clip.tracks) {
        nlohmann::json track_json;
        track_json["property"] = track.property_path;
        track_json["type"] = property_value_type_to_string(track.value_type);

        track_json["keyframes"] = nlohmann::json::array();
        for (const auto& kf : track.keyframes) {
            nlohmann::json kf_json;
            kf_json["time"] = kf.time;
            kf_json["interpolation"] = interpolation_type_to_string(kf.interpolation);

            // Serialize value using shared utility
            kf_json["value"] = property_value_to_json(kf.value);

            track_json["keyframes"].push_back(kf_json);
        }
        j["tracks"].push_back(track_json);
    }

    j["events"] = nlohmann::json::array();
    for (const auto& event : clip.events) {
        nlohmann::json event_json;
        event_json["time"] = event.time;
        event_json["name"] = event.name;
        if (!event.parameter.empty()) {
            event_json["parameter"] = event.parameter;
        }
        j["events"].push_back(event_json);
    }
}

inline bool save_clip(const AnimationClip& clip, const std::string& path) {
    try {
        nlohmann::json j;
        to_json(j, clip);

        std::ofstream file(path);
        if (!file.is_open()) {
            ENGINE_ERR("save_clip: Failed to open file for writing: %s", path.c_str());
            return false;
        }

        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        ENGINE_ERR("save_clip: Failed to save '%s': %s", path.c_str(), e.what());
        return false;
    } catch (...) {
        ENGINE_ERR("save_clip: Failed to save '%s': unknown error", path.c_str());
        return false;
    }
}

}

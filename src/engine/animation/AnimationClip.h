#pragma once

#include "AnimationTrack.h"
#include "AnimationEvent.h"
#include <deque>
#include <vector>
#include <string>
#include <unordered_map>

namespace engine::animation {

// A complete animation clip containing multiple property tracks and events
struct AnimationClip {
    std::string name;
    float duration = 1.0f;
    bool looping = true;
    std::deque<AnimationTrack> tracks;
    std::vector<AnimationEvent> events;

    AnimationClip() = default;
    AnimationClip(std::string clip_name, float dur = 1.0f, bool loop = true)
        : name(std::move(clip_name)), duration(dur), looping(loop) {}

    AnimationTrack* add_track(const std::string& property_path, PropertyValueType type);
    AnimationTrack* get_track(const std::string& property_path);
    const AnimationTrack* get_track(const std::string& property_path) const;
    bool remove_track(const std::string& property_path);
    bool has_track(const std::string& property_path) const;

    void add_event(const AnimationEvent& event);
    void add_event(float time, const std::string& event_name, const std::string& param = "");
    void remove_event(size_t index);
    void sort_events();

    std::vector<const AnimationEvent*> get_events_in_range(float prev_time, float curr_time) const;

    std::unordered_map<std::string, PropertyValue> sample_all(float time) const;

    void recalculate_duration();

    bool empty() const { return tracks.empty() && events.empty(); }

    size_t track_count() const { return tracks.size(); }
    size_t event_count() const { return events.size(); }
};

}

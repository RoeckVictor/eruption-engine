#include "AnimationClip.h"
#include <algorithm>
#include <unordered_map>

namespace engine::animation {

AnimationTrack* AnimationClip::add_track(const std::string& property_path, PropertyValueType type) {
    // Check if track already exists
    if (auto* existing = get_track(property_path)) {
        return existing;
    }

    tracks.emplace_back(property_path, type);
    return &tracks.back();
}

AnimationTrack* AnimationClip::get_track(const std::string& property_path) {
    for (auto& track : tracks) {
        if (track.property_path == property_path) {
            return &track;
        }
    }
    return nullptr;
}

const AnimationTrack* AnimationClip::get_track(const std::string& property_path) const {
    for (const auto& track : tracks) {
        if (track.property_path == property_path) {
            return &track;
        }
    }
    return nullptr;
}

bool AnimationClip::remove_track(const std::string& property_path) {
    auto it = std::find_if(tracks.begin(), tracks.end(),
        [&](const AnimationTrack& t) { return t.property_path == property_path; });

    if (it != tracks.end()) {
        tracks.erase(it);
        return true;
    }
    return false;
}

bool AnimationClip::has_track(const std::string& property_path) const {
    return get_track(property_path) != nullptr;
}

void AnimationClip::add_event(const AnimationEvent& event) {
    // Insert in sorted order
    auto it = std::lower_bound(events.begin(), events.end(), event,
        [](const AnimationEvent& a, const AnimationEvent& b) { return a.time < b.time; });
    events.insert(it, event);
}

void AnimationClip::add_event(float time, const std::string& event_name, const std::string& param) {
    add_event(AnimationEvent(time, event_name, param));
}

void AnimationClip::remove_event(size_t index) {
    if (index < events.size()) {
        events.erase(events.begin() + static_cast<ptrdiff_t>(index));
    }
}

void AnimationClip::sort_events() {
    std::sort(events.begin(), events.end(),
        [](const AnimationEvent& a, const AnimationEvent& b) { return a.time < b.time; });
}

std::vector<const AnimationEvent*> AnimationClip::get_events_in_range(float prev_time, float curr_time) const {
    std::vector<const AnimationEvent*> result;

    if (events.empty()) {
        return result;
    }

    if (curr_time >= prev_time) {
        // Normal case: no wrap
        for (const auto& event : events) {
            if (event.time > prev_time && event.time <= curr_time) {
                result.push_back(&event);
            }
        }
    } else {
        // Looping case: time wrapped around
        // Get events from prev_time to end, then from start to curr_time
        for (const auto& event : events) {
            if (event.time > prev_time && event.time <= duration) {
                result.push_back(&event);
            }
        }
        for (const auto& event : events) {
            if (event.time >= 0.0f && event.time <= curr_time) {
                result.push_back(&event);
            }
        }
    }

    return result;
}

std::unordered_map<std::string, PropertyValue> AnimationClip::sample_all(float time) const {
    std::unordered_map<std::string, PropertyValue> result;
    for (const auto& track : tracks) {
        result[track.property_path] = track.sample(time);
    }
    return result;
}

void AnimationClip::recalculate_duration() {
    float max_time = 0.0f;

    for (const auto& track : tracks) {
        float track_end = track.get_end_time();
        if (track_end > max_time) {
            max_time = track_end;
        }
    }

    for (const auto& event : events) {
        if (event.time > max_time) {
            max_time = event.time;
        }
    }

    if (max_time > 0.0f) {
        duration = max_time;
    }
}

} // namespace engine::animation

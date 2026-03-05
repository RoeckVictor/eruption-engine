#pragma once

#include "Keyframe.h"
#include <vector>
#include <string>

namespace engine::animation {

// A track that animates a single property over time
struct AnimationTrack {
    std::string property_path;
    PropertyValueType value_type;
    std::vector<Keyframe> keyframes;

    AnimationTrack() = default;
    AnimationTrack(std::string path, PropertyValueType type)
        : property_path(std::move(path)), value_type(type) {}

    void sort_keyframes();
    void add_keyframe(const Keyframe& kf);
    void set_keyframe(float time, const PropertyValue& value,
                     InterpolationType interp = InterpolationType::Linear,
                     float tolerance = 0.001f);
    void remove_keyframe(size_t index);

    bool remove_keyframe_at_time(float time, float tolerance = 0.001f);
    int find_keyframe_at_time(float time, float tolerance = 0.001f) const;

    PropertyValue sample(float time) const;

    float get_start_time() const;
    float get_end_time() const;

    bool empty() const { return keyframes.empty(); }

    size_t keyframe_count() const { return keyframes.size(); }
};

}

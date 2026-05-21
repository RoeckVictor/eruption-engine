#pragma once

#include <cstdint>
#include <string>

namespace engine::audio {

// ECS component for entities that emit sound.
// Pure data struct -- all logic lives in AudioSystem.
struct AudioSource {
    bool enabled = true;

    std::string clip_path;
    float volume   = 1.0f;
    float pitch    = 1.0f;
    float pan      = 0.0f;
    bool  loop     = false;
    bool  play_on_start = false;

    bool  spatial       = false;
    float min_distance  = 50.0f;
    float max_distance  = 500.0f;

    int   channel_group = 0;

    uint64_t _playback_handle = 0;
    bool     _is_playing      = false;
    bool     _needs_start     = false;
};

}

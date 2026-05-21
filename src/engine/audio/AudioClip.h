#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace engine::audio {

struct AudioClip {
    std::string file_path;
    uint32_t    sample_rate = 0;
    uint32_t    channels    = 0;
    uint64_t    frame_count = 0;
    float       duration    = 0.0f;

    std::vector<uint8_t> pcm_data;

    bool is_streaming() const { return pcm_data.empty(); }
};

}
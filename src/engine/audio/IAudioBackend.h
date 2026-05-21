#pragma once

#include <cstdint>
#include <string>

namespace engine::audio {

// Parameters for a single sound playback instance.
struct PlayParams {
    float volume = 1.0f;
    float pitch  = 1.0f;
    float pan    = 0.0f;
    bool  loop   = false;
    int   group  = 0;
};

// Abstract audio backend interface.
// Concrete implementations (MiniaudioBackend, etc.) live behind this
// so the engine can swap backends without touching game code.
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual bool init(int sample_rate) = 0;
    virtual void shutdown() = 0;

    virtual uint64_t play(const std::string& file_path, const PlayParams& params) = 0;
    virtual void stop(uint64_t handle) = 0;
    virtual void stop_all() = 0;
    virtual bool is_playing(uint64_t handle) const = 0;

    virtual void set_volume(uint64_t handle, float volume) = 0;
    virtual void set_pitch(uint64_t handle, float pitch) = 0;
    virtual void set_pan(uint64_t handle, float pan) = 0;

    virtual void set_master_volume(float volume) = 0;
    virtual void set_group_volume(int group, float volume) = 0;
};

}

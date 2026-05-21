#pragma once

#include "engine/audio/IAudioBackend.h"
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <string>

// We need the actual miniaudio types here since they are typedefs, not plain structs.
// miniaudio.h is huge but this header is only included by MiniaudioBackend.cpp
// and anyone who explicitly needs the backend (rare).
#include "miniaudio.h"

namespace engine::audio {

// Concrete IAudioBackend using the miniaudio library
class MiniaudioBackend : public IAudioBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;

    bool init(int sample_rate) override;
    void shutdown() override;

    uint64_t play(const std::string& file_path, const PlayParams& params) override;
    void stop(uint64_t handle) override;
    void stop_all() override;
    bool is_playing(uint64_t handle) const override;

    void set_volume(uint64_t handle, float volume) override;
    void set_pitch(uint64_t handle, float pitch) override;
    void set_pan(uint64_t handle, float pan) override;

    void set_master_volume(float volume) override;
    void set_group_volume(int group, float volume) override;

private:
    struct SoundSlot {
        ma_sound* sound = nullptr;
        int       group = 0;
    };

    ma_sound* find_sound(uint64_t handle) const;
    void cleanup_finished();

    ma_context*     m_context = nullptr;
    ma_engine*      m_engine = nullptr;
    ma_sound_group* m_groups = nullptr;
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, SoundSlot> m_sounds;
    uint64_t m_next_handle = 1;

    static constexpr int GROUP_COUNT = 4;
};

}
#pragma once

#include "engine/audio/IAudioBackend.h"
#include "engine/audio/AudioMixer.h"
#include <memory>
#include <string>

namespace engine::audio {

// High-level audio engine facade.
// Owns the backend and mixer; registered as a subsystem on Engine.
class AudioEngine {
public:
    bool init(int sample_rate = 44100);
    void shutdown();

    uint64_t play(const std::string& clip_path, const PlayParams& params = {});
    void stop(uint64_t handle);
    void stop_all();
    bool is_playing(uint64_t handle) const;

    void set_volume(uint64_t handle, float volume);
    void set_pitch(uint64_t handle, float pitch);
    void set_pan(uint64_t handle, float pan);

    void set_master_volume(float volume);
    void set_group_volume(int group, float volume);
    AudioMixer& mixer() { return m_mixer; }
    const AudioMixer& mixer() const { return m_mixer; }

    IAudioBackend* backend() { return m_backend.get(); }

private:
    std::unique_ptr<IAudioBackend> m_backend;
    AudioMixer m_mixer;
};

}
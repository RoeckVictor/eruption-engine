#include "engine/audio/AudioEngine.h"
#include "engine/audio/MiniaudioBackend.h"
#include "engine/core/Log.h"

namespace engine::audio {

bool AudioEngine::init(int sample_rate) {
    m_backend = std::make_unique<MiniaudioBackend>();
    if (!m_backend->init(sample_rate)) {
        ENGINE_ERR("AudioEngine: Failed to initialize audio backend");
        m_backend.reset();
        return false;
    }
    ENGINE_LOG("AudioEngine: Initialized (sample rate: %d)", sample_rate);
    return true;
}

void AudioEngine::shutdown() {
    if (m_backend) {
        m_backend->stop_all();
        m_backend->shutdown();
        m_backend.reset();
    }
    ENGINE_LOG("AudioEngine: Shut down");
}

uint64_t AudioEngine::play(const std::string& clip_path, const PlayParams& params) {
    if (!m_backend) return 0;

    PlayParams adjusted = params;
    adjusted.volume = m_mixer.effective_volume(params.group, params.volume);

    return m_backend->play(clip_path, adjusted);
}

void AudioEngine::stop(uint64_t handle) {
    if (m_backend) m_backend->stop(handle);
}

void AudioEngine::stop_all() {
    if (m_backend) m_backend->stop_all();
}

bool AudioEngine::is_playing(uint64_t handle) const {
    return m_backend ? m_backend->is_playing(handle) : false;
}

void AudioEngine::set_volume(uint64_t handle, float volume) {
    if (m_backend) m_backend->set_volume(handle, volume);
}

void AudioEngine::set_pitch(uint64_t handle, float pitch) {
    if (m_backend) m_backend->set_pitch(handle, pitch);
}

void AudioEngine::set_pan(uint64_t handle, float pan) {
    if (m_backend) m_backend->set_pan(handle, pan);
}

void AudioEngine::set_master_volume(float volume) {
    m_mixer.set_master_volume(volume);
    if (m_backend) m_backend->set_master_volume(volume);
}

void AudioEngine::set_group_volume(int group, float volume) {
    m_mixer.set_group_volume(group, volume);
    if (m_backend) m_backend->set_group_volume(group, volume);
}

} // namespace engine::audio

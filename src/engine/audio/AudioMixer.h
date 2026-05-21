#pragma once

#include <array>

namespace engine::audio {

// Simple channel-group mixer.
// Each group has its own volume; final volume = master * group * per-sound.
class AudioMixer {
public:
    static constexpr int GROUP_MASTER = 0;
    static constexpr int GROUP_SFX    = 1;
    static constexpr int GROUP_MUSIC  = 2;
    static constexpr int GROUP_UI     = 3;
    static constexpr int GROUP_COUNT  = 4;

    void set_master_volume(float v) { m_volumes[GROUP_MASTER] = clamp01(v); }
    float master_volume() const     { return m_volumes[GROUP_MASTER]; }

    void set_group_volume(int group, float v) {
        if (group >= 0 && group < GROUP_COUNT)
            m_volumes[group] = clamp01(v);
    }
    float group_volume(int group) const {
        return (group >= 0 && group < GROUP_COUNT) ? m_volumes[group] : 0.0f;
    }

    /// Compute the effective volume for a sound in the given group.
    float effective_volume(int group, float sound_volume) const {
        float gv = (group >= 0 && group < GROUP_COUNT) ? m_volumes[group] : 1.0f;
        return m_volumes[GROUP_MASTER] * gv * sound_volume;
    }

private:
    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    std::array<float, GROUP_COUNT> m_volumes = { 1.0f, 1.0f, 1.0f, 1.0f };
};

}
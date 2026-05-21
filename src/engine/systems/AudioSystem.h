#pragma once

#include "engine/core/System.h"

namespace engine::audio {
class AudioEngine;
}

namespace engine {

// System that drives AudioSource components each frame.
// - Starts sounds that have _needs_start set
// - Updates spatial audio (pan/attenuation) for sources with spatial=true
// - Syncs _is_playing state
// - Triggers play_on_start sources when play mode begins
class AudioSystem : public System {
public:
    const char* name() const override { return "AudioSystem"; }
    bool init(Engine& engine) override;
    void update(Engine& engine, float dt) override;
    void shutdown() override;

    void trigger_play_on_start(Engine& engine);

private:
    audio::AudioEngine* m_audio = nullptr;
    bool m_play_on_start_triggered = false;
};

}
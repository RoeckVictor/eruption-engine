#include "engine/systems/AudioSystem.h"
#include "engine/core/Engine.h"
#include "engine/audio/AudioEngine.h"
#include "engine/audio/AudioSource.h"
#include "engine/audio/AudioListener.h"
#include "engine/audio/IAudioBackend.h"
#include "engine/core/Transform.h"
#include "engine/core/Log.h"
#include "engine/scene/SceneManager.h"
#include "engine/profiler/Profiler.h"
#include <entt/entt.hpp>
#include <cmath>

namespace engine {

bool AudioSystem::init(Engine& engine) {
    m_audio = engine.subsystems().get<audio::AudioEngine>();
    if (!m_audio) {
        ENGINE_LOG_WARN("AudioSystem: No AudioEngine registered -- audio disabled");
    }
    m_play_on_start_triggered = false;
    return true;
}

void AudioSystem::update(Engine& engine, float /*dt*/) {
    PROFILE_SCOPE("AudioSystem::update");
    if (!m_audio) return;

    auto* scene = engine.scenes().top();
    if (!scene) return;

    auto& registry = scene->registry();

    // Find the active listener position (if any)
    float listener_x = 0.0f, listener_y = 0.0f;
    bool has_listener = false;
    {
        auto view = registry.view<audio::AudioListener, Transform>();
        for (auto entity : view) {
            auto& listener = view.get<audio::AudioListener>(entity);
            if (!listener.enabled) continue;
            auto& t = view.get<Transform>(entity);
            listener_x = t.world_x;
            listener_y = t.world_y;
            has_listener = true;
            break; // Only first active listener
        }
    }

    // Update all AudioSource entities
    auto view = registry.view<audio::AudioSource>();
    for (auto entity : view) {
        auto& src = view.get<audio::AudioSource>(entity);
        if (!src.enabled) continue;

        // Start pending sounds
        if (src._needs_start && !src.clip_path.empty()) {
            audio::PlayParams params;
            params.volume = src.volume;
            params.pitch  = src.pitch;
            params.pan    = src.pan;
            params.loop   = src.loop;
            params.group  = src.channel_group;

            src._playback_handle = m_audio->play(src.clip_path, params);
            src._is_playing = (src._playback_handle != 0);
            src._needs_start = false;
        }

        // Update spatial audio
        if (src._is_playing && src.spatial && has_listener) {
            auto* t_ptr = registry.try_get<Transform>(entity);
            if (t_ptr) {
                float dx = t_ptr->world_x - listener_x;
                float dy = t_ptr->world_y - listener_y;
                float dist = std::sqrt(dx * dx + dy * dy);

                // Pan based on horizontal offset (-1 to 1)
                float max_pan_dist = src.max_distance > 0.0f ? src.max_distance : 1.0f;
                float pan = dx / max_pan_dist;
                pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);

                // Volume attenuation
                float attenuation = 1.0f;
                if (dist > src.min_distance) {
                    float range = src.max_distance - src.min_distance;
                    if (range > 0.0f) {
                        attenuation = 1.0f - ((dist - src.min_distance) / range);
                        attenuation = attenuation < 0.0f ? 0.0f : attenuation;
                    } else {
                        attenuation = 0.0f;
                    }
                }

                m_audio->set_pan(src._playback_handle, pan);
                m_audio->set_volume(src._playback_handle, src.volume * attenuation);
            }
        }

        // Sync playing state
        if (src._is_playing && src._playback_handle != 0) {
            if (!m_audio->is_playing(src._playback_handle)) {
                src._is_playing = false;
                src._playback_handle = 0;
            }
        }
    }
}

void AudioSystem::trigger_play_on_start(Engine& engine) {
    if (!m_audio || m_play_on_start_triggered) return;
    m_play_on_start_triggered = true;

    auto* scene = engine.scenes().top();
    if (!scene) return;

    auto& registry = scene->registry();
    auto view = registry.view<audio::AudioSource>();
    for (auto entity : view) {
        auto& src = view.get<audio::AudioSource>(entity);
        if (src.enabled && src.play_on_start && !src.clip_path.empty()) {
            src._needs_start = true;
        }
    }
}

void AudioSystem::shutdown() {
    if (m_audio) {
        m_audio->stop_all();
    }
    m_audio = nullptr;
    m_play_on_start_triggered = false;
}

} // namespace engine

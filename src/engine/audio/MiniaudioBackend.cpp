// The MINIAUDIO_IMPLEMENTATION define must appear in exactly one translation unit.
// We do it here, before including our header (which includes miniaudio.h).
#define MINIAUDIO_IMPLEMENTATION
#include "engine/audio/MiniaudioBackend.h"
#include "engine/core/Log.h"

#include <algorithm>

namespace engine::audio {

MiniaudioBackend::MiniaudioBackend() = default;

MiniaudioBackend::~MiniaudioBackend() {
    shutdown();
}

bool MiniaudioBackend::init(int sample_rate) {
    // First create a context with an explicit backend choice.
    // On Windows, WASAPI's CoInitializeEx(COINIT_MULTITHREADED) conflicts with
    // GLFW's OleInitialize (apartment-threaded COM), which breaks the window
    // message pump and causes flickering. We use DirectSound instead.
    m_context = new ma_context();

#ifdef _WIN32
    ma_backend backends[] = { ma_backend_dsound, ma_backend_winmm };
    ma_uint32 backend_count = 2;
#else
    ma_backend backends[] = { ma_backend_pulseaudio, ma_backend_alsa };
    ma_uint32 backend_count = 2;
#endif

    ma_context_config ctx_config = ma_context_config_init();
    ma_result result = ma_context_init(backends, backend_count, &ctx_config, m_context);
    if (result != MA_SUCCESS) {
        ENGINE_ERR("MiniaudioBackend: ma_context_init failed (%d)", static_cast<int>(result));
        delete m_context;
        m_context = nullptr;
        return false;
    }

    // Now create the engine using our context
    m_engine = new ma_engine();

    ma_engine_config config = ma_engine_config_init();
    config.pContext = m_context;
    config.sampleRate = static_cast<ma_uint32>(sample_rate);

    result = ma_engine_init(&config, m_engine);
    if (result != MA_SUCCESS) {
        ENGINE_ERR("MiniaudioBackend: ma_engine_init failed (%d)", static_cast<int>(result));
        delete m_engine;
        m_engine = nullptr;
        ma_context_uninit(m_context);
        delete m_context;
        m_context = nullptr;
        return false;
    }

    // Create sound groups
    m_groups = new ma_sound_group[GROUP_COUNT];
    for (int i = 0; i < GROUP_COUNT; ++i) {
        result = ma_sound_group_init(m_engine, 0, nullptr, &m_groups[i]);
        if (result != MA_SUCCESS) {
            ENGINE_ERR("MiniaudioBackend: Failed to create sound group %d (%d)", i, static_cast<int>(result));
        }
    }

    ENGINE_LOG("MiniaudioBackend: Initialized");
    return true;
}

void MiniaudioBackend::shutdown() {
    if (!m_engine) return;

    // Stop and free all active sounds
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [handle, slot] : m_sounds) {
            if (slot.sound) {
                ma_sound_uninit(slot.sound);
                delete slot.sound;
            }
        }
        m_sounds.clear();
    }

    // Uninit groups
    if (m_groups) {
        for (int i = 0; i < GROUP_COUNT; ++i) {
            ma_sound_group_uninit(&m_groups[i]);
        }
        delete[] m_groups;
        m_groups = nullptr;
    }

    ma_engine_uninit(m_engine);
    delete m_engine;
    m_engine = nullptr;

    if (m_context) {
        ma_context_uninit(m_context);
        delete m_context;
        m_context = nullptr;
    }
}

uint64_t MiniaudioBackend::play(const std::string& file_path, const PlayParams& params) {
    if (!m_engine) return 0;

    // Periodically clean up finished sounds
    cleanup_finished();

    auto* sound = new ma_sound();

    int group_index = (params.group >= 0 && params.group < GROUP_COUNT) ? params.group : 0;
    ma_sound_group* group = m_groups ? &m_groups[group_index] : nullptr;

    ma_uint32 flags = MA_SOUND_FLAG_DECODE; // decode upfront for short sounds
    ma_result result = ma_sound_init_from_file(m_engine, file_path.c_str(), flags, group, nullptr, sound);
    if (result != MA_SUCCESS) {
        ENGINE_ERR("MiniaudioBackend: Failed to load '%s' (%d)", file_path.c_str(), static_cast<int>(result));
        delete sound;
        return 0;
    }

    ma_sound_set_volume(sound, params.volume);
    ma_sound_set_pitch(sound, params.pitch);
    ma_sound_set_pan(sound, params.pan);
    ma_sound_set_looping(sound, params.loop ? MA_TRUE : MA_FALSE);

    result = ma_sound_start(sound);
    if (result != MA_SUCCESS) {
        ENGINE_ERR("MiniaudioBackend: Failed to start '%s' (%d)", file_path.c_str(), static_cast<int>(result));
        ma_sound_uninit(sound);
        delete sound;
        return 0;
    }

    uint64_t handle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handle = m_next_handle++;
        m_sounds[handle] = { sound, group_index };
    }

    return handle;
}

void MiniaudioBackend::stop(uint64_t handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sounds.find(handle);
    if (it == m_sounds.end()) return;

    if (it->second.sound) {
        ma_sound_stop(it->second.sound);
        ma_sound_uninit(it->second.sound);
        delete it->second.sound;
    }
    m_sounds.erase(it);
}

void MiniaudioBackend::stop_all() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [h, slot] : m_sounds) {
        if (slot.sound) {
            ma_sound_stop(slot.sound);
            ma_sound_uninit(slot.sound);
            delete slot.sound;
        }
    }
    m_sounds.clear();
}

bool MiniaudioBackend::is_playing(uint64_t handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sounds.find(handle);
    if (it == m_sounds.end()) return false;
    return it->second.sound && ma_sound_is_playing(it->second.sound);
}

void MiniaudioBackend::set_volume(uint64_t handle, float volume) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* s = find_sound(handle);
    if (s) ma_sound_set_volume(s, volume);
}

void MiniaudioBackend::set_pitch(uint64_t handle, float pitch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* s = find_sound(handle);
    if (s) ma_sound_set_pitch(s, pitch);
}

void MiniaudioBackend::set_pan(uint64_t handle, float pan) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* s = find_sound(handle);
    if (s) ma_sound_set_pan(s, pan);
}

void MiniaudioBackend::set_master_volume(float volume) {
    if (m_engine) {
        ma_engine_set_volume(m_engine, volume);
    }
}

void MiniaudioBackend::set_group_volume(int group, float volume) {
    if (m_groups && group >= 0 && group < GROUP_COUNT) {
        ma_sound_group_set_volume(&m_groups[group], volume);
    }
}

ma_sound* MiniaudioBackend::find_sound(uint64_t handle) const {
    auto it = m_sounds.find(handle);
    return (it != m_sounds.end()) ? it->second.sound : nullptr;
}

void MiniaudioBackend::cleanup_finished() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove finished non-looping sounds to avoid leaking memory
    for (auto it = m_sounds.begin(); it != m_sounds.end(); ) {
        auto* s = it->second.sound;
        if (s && !ma_sound_is_playing(s) && !ma_sound_is_looping(s)) {
            ma_sound_uninit(s);
            delete s;
            it = m_sounds.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace engine::audio

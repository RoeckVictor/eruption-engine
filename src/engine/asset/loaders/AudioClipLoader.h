#pragma once

#include "engine/asset/AssetLoader.h"
#include "engine/audio/AudioClip.h"
#include "engine/core/Log.h"

namespace engine::asset {
template<>
struct AssetLoader<audio::AudioClip> {
    static constexpr size_t STREAMING_THRESHOLD = 2 * 1024 * 1024; // 2 MB

    static std::unique_ptr<audio::AudioClip> load(const VFS& vfs, const std::string& virtual_path) {
        auto resolved = vfs.resolve(virtual_path);
        if (resolved.empty()) {
            ENGINE_ERR("AudioClipLoader: Cannot resolve '%s'", virtual_path.c_str());
            return nullptr;
        }

        auto clip = std::make_unique<audio::AudioClip>();
        clip->file_path = resolved;

        // We store the physical path; actual decoding is handled by miniaudio
        // at playback time. Metadata could be extracted here via ma_decoder,
        // but that would require including miniaudio.h in this header.
        // Instead we keep it lightweight -- AudioEngine plays from file path.

        ENGINE_LOG("AudioClipLoader: Loaded '%s'", virtual_path.c_str());
        return clip;
    }

    static bool reload(audio::AudioClip& clip, const VFS& vfs, const std::string& virtual_path) {
        auto resolved = vfs.resolve(virtual_path);
        if (resolved.empty()) return false;
        clip.file_path = resolved;
        return true;
    }
};

}

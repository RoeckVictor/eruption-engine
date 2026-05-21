#pragma once

#include <string>

namespace engine::audio {
struct AudioSource;
class AudioEngine;
}

namespace editor {
class AudioSourceInspector {
public:
    static bool draw(engine::audio::AudioSource& source,
                     const std::string& project_path,
                     engine::audio::AudioEngine* audio_engine);
};

}

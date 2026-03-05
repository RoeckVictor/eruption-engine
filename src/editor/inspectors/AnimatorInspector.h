#pragma once

#include "RecordingContext.h"
#include <string>

namespace engine::animation {
struct Animator;
}

namespace editor {

// Custom inspector for Animator component
// Provides UI for managing animation controller and runtime state
class AnimatorInspector {
public:
    static bool draw(engine::animation::Animator& animator, const std::string& project_path);

    static bool draw(engine::animation::Animator& animator, const std::string& project_path,
                     const RecordingContext& rec_ctx);
};

}

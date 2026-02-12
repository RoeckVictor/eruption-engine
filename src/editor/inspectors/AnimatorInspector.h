#pragma once

namespace engine::animation {
struct Animator;
}

namespace editor {

/// Custom inspector for Animator component.
/// Provides UI for managing animation clips and playback control.
class AnimatorInspector {
public:
    /// Draw inspector UI for Animator component.
    /// Returns true if any value was modified.
    static bool draw(engine::animation::Animator& animator);
};

} // namespace editor

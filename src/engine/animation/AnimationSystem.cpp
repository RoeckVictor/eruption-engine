#include "engine/animation/AnimationSystem.h"
#include "engine/animation/Animator.h"
#include "engine/profiler/Profiler.h"
#include <entt/entt.hpp>

namespace engine::animation {

void AnimationSystem::update(Engine& /*engine*/, float dt) {
    if (!m_registry) return;
    PROFILE_SCOPE("AnimationSystem::update");

    auto view = m_registry->view<Animator>();
    for (auto [entity, animator] : view.each()) {
        if (!animator.playing || animator.finished) continue;

        auto it = animator.clips.find(animator.current_clip);
        if (it == animator.clips.end() || it->second.frames.empty()) continue;

        const auto& clip = it->second;
        animator.elapsed += dt;

        if (animator.elapsed >= clip.frame_duration) {
            int advance = static_cast<int>(animator.elapsed / clip.frame_duration);
            animator.elapsed -= advance * clip.frame_duration;
            animator.current_frame += advance;

            int frame_count = static_cast<int>(clip.frames.size());
            if (clip.looping) {
                animator.current_frame %= frame_count;
            } else if (animator.current_frame >= frame_count) {
                animator.current_frame = frame_count - 1;
                animator.finished = true;
                animator.playing = false;
            }
        }
    }
}

} // namespace engine::animation

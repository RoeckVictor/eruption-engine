#pragma once

#include "engine/core/System.h"
#include "engine/animation/AnimationClip.h"
#include "engine/animation/AnimatorController.h"
#include "engine/animation/Animator.h"
#include <entt/fwd.hpp>

namespace engine::animation {

// Drives all Animator components each frame
class AnimationSystem : public engine::System {
public:
    const char* name() const override { return "AnimationSystem"; }
    void set_registry(entt::registry* reg) { m_registry = reg; }

    void update(engine::Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;

    void initialize_animator(Animator& animator, const AnimatorController& controller);

    void check_transitions(Animator& animator, const AnimatorController& controller,
                          const AnimationClip* current_clip);

    bool evaluate_transition(const Animator& animator, const StateTransition& transition,
                            const AnimationClip* current_clip) const;
    bool evaluate_condition(const Animator& animator, const TransitionCondition& condition) const;

    void apply_animation(entt::entity entity, const AnimationClip& clip, float time, float weight);
    void apply_blended_animation(entt::entity entity,
                                 const AnimationClip* from_clip, float from_time,
                                 const AnimationClip* to_clip, float to_time,
                                 float blend_weight);

    void process_events(Animator& animator, const AnimationClip& clip,
                       float prev_time, float curr_time);
};

}

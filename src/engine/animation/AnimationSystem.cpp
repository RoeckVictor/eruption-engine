#include "engine/animation/AnimationSystem.h"
#include "engine/animation/PropertyResolver.h"
#include "engine/animation/Interpolation.h"
#include "engine/asset/AssetDatabase.h"
#include "engine/asset/loaders/AnimationClipLoader.h"
#include "engine/asset/loaders/AnimatorControllerLoader.h"
#include "engine/profiler/Profiler.h"
#include "engine/core/Engine.h"
#include <entt/entt.hpp>
#include <algorithm>

namespace engine::animation {

void AnimationSystem::update(Engine& engine, float dt) {
    if (!m_registry) return;
    PROFILE_SCOPE("AnimationSystem::update");

    auto& assets = engine.assets();
    auto& resolver = PropertyResolver::instance();

    auto view = m_registry->view<Animator>();
    for (auto [entity, animator] : view.each()) {
        if (!animator.enabled || animator.controller_path.empty()) {
            continue;
        }

        // Load controller
        auto controller_handle = assets.load<AnimatorController>(animator.controller_path);
        const auto* controller = assets.get(controller_handle);
        if (!controller || controller->states.empty()) {
            continue;
        }

        // Initialize animator if needed
        if (!animator.initialized) {
            initialize_animator(animator, *controller);
        }

        // Get current state
        const auto* current_state = controller->get_state(animator.current_state);
        if (!current_state) {
            // Invalid state, reset to default
            animator.current_state = controller->default_state;
            animator.state_time = 0.0f;
            current_state = controller->get_state(animator.current_state);
            if (!current_state) continue;
        }

        // Load current animation clip
        const AnimationClip* current_clip = nullptr;
        if (!current_state->clip_path.empty()) {
            auto clip_handle = assets.load<AnimationClip>(current_state->clip_path);
            current_clip = assets.get(clip_handle);
        }

        // Store previous time for event detection
        animator.previous_state_time = animator.state_time;

        // Check transitions before advancing time
        check_transitions(animator, *controller, current_clip);

        // After transition check, current state might have changed
        current_state = controller->get_state(animator.current_state);
        if (!current_state) continue;

        // Reload clip if state changed
        if (!current_state->clip_path.empty()) {
            auto clip_handle = assets.load<AnimationClip>(current_state->clip_path);
            current_clip = assets.get(clip_handle);
        } else {
            current_clip = nullptr;
        }

        // Advance state time
        if (current_clip) {
            animator.state_time += dt * current_state->speed;

            // Handle looping
            if (current_clip->looping) {
                while (animator.state_time >= current_clip->duration) {
                    animator.state_time -= current_clip->duration;
                }
            } else {
                animator.state_time = std::min(animator.state_time, current_clip->duration);
            }

            // Process events
            process_events(animator, *current_clip, animator.previous_state_time, animator.state_time);
        }

        // Update blend progress
        if (animator.is_blending) {
            // Guard against division by zero: if blend_duration is <= 0, complete immediately
            if (animator.blend_duration <= 0.0f) {
                animator.blend_progress = 1.0f;
            } else {
                animator.blend_progress += dt / animator.blend_duration;
            }
            if (animator.blend_progress >= 1.0f) {
                animator.blend_progress = 1.0f;
                animator.is_blending = false;
                animator.blend_from_state.clear();
            }
        }

        // Apply animation
        if (animator.is_blending && !animator.blend_from_state.empty()) {
            // Get the "from" animation clip
            const auto* from_state = controller->get_state(animator.blend_from_state);
            const AnimationClip* from_clip = nullptr;
            if (from_state && !from_state->clip_path.empty()) {
                auto from_clip_handle = assets.load<AnimationClip>(from_state->clip_path);
                from_clip = assets.get(from_clip_handle);
            }

            apply_blended_animation(entity,
                                   from_clip, animator.blend_from_time,
                                   current_clip, animator.state_time,
                                   animator.blend_progress);
        } else if (current_clip) {
            apply_animation(entity, *current_clip, animator.state_time, 1.0f);
        }

        // Reset triggers at end of frame
        animator.reset_triggers();

        // Clear events from previous frame
        animator.clear_events();
    }
}

void AnimationSystem::initialize_animator(Animator& animator, const AnimatorController& controller) {
    // Set default state
    animator.current_state = controller.default_state;
    animator.state_time = 0.0f;
    animator.previous_state_time = 0.0f;

    // Initialize parameters with defaults
    animator.bool_params.clear();
    animator.int_params.clear();
    animator.float_params.clear();
    animator.trigger_params.clear();

    for (const auto& param : controller.parameters) {
        switch (param.type) {
            case ParameterType::Bool:
                animator.bool_params[param.name] = param.default_bool;
                break;
            case ParameterType::Int:
                animator.int_params[param.name] = param.default_int;
                break;
            case ParameterType::Float:
                animator.float_params[param.name] = param.default_float;
                break;
            case ParameterType::Trigger:
                animator.trigger_params[param.name] = false;
                break;
        }
    }

    animator.initialized = true;
}

void AnimationSystem::check_transitions(Animator& animator, const AnimatorController& controller,
                                        const AnimationClip* current_clip) {
    // Get all transitions from current state (including Any State transitions)
    auto transitions = controller.get_transitions_from(animator.current_state);

    for (const auto* transition : transitions) {
        if (evaluate_transition(animator, *transition, current_clip)) {
            // Transition triggered
            animator.start_blend(transition->to_state, transition->blend_duration);
            break;  // Only process one transition per frame
        }
    }
}

bool AnimationSystem::evaluate_transition(const Animator& animator, const StateTransition& transition,
                                          const AnimationClip* current_clip) const {
    // Check exit time requirement
    if (transition.has_exit_time && current_clip) {
        // Guard against division by zero for zero-duration clips
        if (current_clip->duration <= 0.0f) {
            // Zero-duration clips are considered "complete" immediately
            // so exit_time conditions pass
        } else {
            float normalized_time = animator.state_time / current_clip->duration;
            if (normalized_time < transition.exit_time) {
                return false;
            }
        }
    }

    // Evaluate all conditions (AND logic)
    for (const auto& condition : transition.conditions) {
        if (!evaluate_condition(animator, condition)) {
            return false;
        }
    }

    // All conditions passed (or no conditions)
    return true;
}

bool AnimationSystem::evaluate_condition(const Animator& animator, const TransitionCondition& condition) const {
    // Check bool parameters
    auto bool_it = animator.bool_params.find(condition.parameter_name);
    if (bool_it != animator.bool_params.end()) {
        return condition.evaluate(bool_it->second);
    }

    // Check int parameters
    auto int_it = animator.int_params.find(condition.parameter_name);
    if (int_it != animator.int_params.end()) {
        return condition.evaluate(int_it->second);
    }

    // Check float parameters
    auto float_it = animator.float_params.find(condition.parameter_name);
    if (float_it != animator.float_params.end()) {
        return condition.evaluate(float_it->second);
    }

    // Check trigger parameters
    auto trigger_it = animator.trigger_params.find(condition.parameter_name);
    if (trigger_it != animator.trigger_params.end()) {
        return condition.evaluate(trigger_it->second);
    }

    // Parameter not found
    return false;
}

void AnimationSystem::apply_animation(entt::entity entity,
                                      const AnimationClip& clip, float time, float weight) {
    auto& resolver = PropertyResolver::instance();

    for (const auto& track : clip.tracks) {
        // Sample the track at current time
        PropertyValue value = track.sample(time);

        // Apply value to entity component
        if (weight >= 1.0f) {
            resolver.set_value(*m_registry, entity, track.property_path, value);
        } else {
            // Partial weight: blend with current value
            auto current = resolver.get_value(*m_registry, entity, track.property_path);
            if (current.has_value()) {
                PropertyValue blended = interpolate(*current, value, weight, InterpolationType::Linear);
                resolver.set_value(*m_registry, entity, track.property_path, blended);
            }
        }
    }
}

void AnimationSystem::apply_blended_animation(entt::entity entity,
                                              const AnimationClip* from_clip, float from_time,
                                              const AnimationClip* to_clip, float to_time,
                                              float blend_weight) {
    auto& resolver = PropertyResolver::instance();

    // Collect all property paths from both clips
    std::unordered_map<std::string, std::pair<const AnimationTrack*, const AnimationTrack*>> tracks;

    if (from_clip) {
        for (const auto& track : from_clip->tracks) {
            tracks[track.property_path].first = &track;
        }
    }
    if (to_clip) {
        for (const auto& track : to_clip->tracks) {
            tracks[track.property_path].second = &track;
        }
    }

    // Blend each property
    for (const auto& [path, track_pair] : tracks) {
        const auto* from_track = track_pair.first;
        const auto* to_track = track_pair.second;

        PropertyValue final_value;

        if (from_track && to_track) {
            // Both clips have this track - blend between them
            PropertyValue from_value = from_track->sample(from_time);
            PropertyValue to_value = to_track->sample(to_time);
            final_value = interpolate(from_value, to_value, blend_weight, InterpolationType::Linear);
        } else if (to_track) {
            // Only "to" clip has this track - blend from current value
            auto current = resolver.get_value(*m_registry, entity, path);
            PropertyValue to_value = to_track->sample(to_time);
            if (current.has_value()) {
                final_value = interpolate(*current, to_value, blend_weight, InterpolationType::Linear);
            } else {
                final_value = to_value;
            }
        } else if (from_track) {
            // Only "from" clip has this track - blend to current value
            PropertyValue from_value = from_track->sample(from_time);
            auto current = resolver.get_value(*m_registry, entity, path);
            if (current.has_value()) {
                final_value = interpolate(from_value, *current, blend_weight, InterpolationType::Linear);
            } else {
                final_value = from_value;
            }
        }

        resolver.set_value(*m_registry, entity, path, final_value);
    }
}

void AnimationSystem::process_events(Animator& animator, const AnimationClip& clip,
                                     float prev_time, float curr_time) {
    auto fired_events = clip.get_events_in_range(prev_time, curr_time);
    for (const auto* event : fired_events) {
        animator.pending_events.push_back(event->name);
    }
}

} // namespace engine::animation

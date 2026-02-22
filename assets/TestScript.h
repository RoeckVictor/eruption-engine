#pragma once

#include "runtime/ComponentScript.h"

/// Test script demonstrating all new scripting features:
/// - Collision/trigger callbacks
/// - Coroutines (WaitForSeconds, WaitForNextFrame, WaitUntil)
/// - Custom events (subscribe, dispatch)
/// - Math helpers (lerp, clamp, smoothstep, etc.)
/// - Random utilities
/// - Camera access
/// - Hierarchy access
/// - Entity info
class TestScript : public runtime::ComponentScript {
public:
    const char* type_name() const override { return "TestScript"; }

    // Lifecycle
    void on_create() override;
    void on_destroy() override;
    void on_enable() override;
    void on_disable() override;
    void on_update() override;

    // Collision callbacks
    void on_collision_enter(const runtime::CollisionInfo& info) override;
    void on_collision_stay(const runtime::CollisionInfo& info) override;
    void on_collision_exit(const runtime::CollisionInfo& info) override;
    void on_trigger_enter(const runtime::CollisionInfo& info) override;
    void on_trigger_exit(const runtime::CollisionInfo& info) override;

    // Custom inspector GUI (shows debug info, not serializable properties)
    void on_inspector_gui(nlohmann::json& properties) override;

private:
    // Coroutine examples
    runtime::Coroutine countdown_coroutine();
    runtime::Coroutine camera_shake_coroutine(float duration, float intensity);
    runtime::Coroutine patrol_coroutine();

    // Event callback (must be static for function pointer)
    static void on_player_damaged(const runtime::EventData& data);

    // State
    runtime::CoroutineHandle m_countdown_handle = 0;
    runtime::CoroutineHandle m_patrol_handle = 0;
    runtime::EventHandle m_damage_event_handle = 0;

    float m_move_speed = 100.0f;
    float m_time_alive = 0.0f;
    int m_collision_count = 0;
    bool m_is_patrolling = false;

    // Patrol waypoints
    runtime::Vec2 m_patrol_start{0.0f, 0.0f};
    runtime::Vec2 m_patrol_end{200.0f, 0.0f};
};

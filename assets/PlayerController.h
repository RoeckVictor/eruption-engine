#pragma once

#include "runtime/ComponentScript.h"

/// Simple 2D platformer player controller.
/// Handles horizontal movement with acceleration/friction and jumping when grounded.
///
/// Controls:
/// - A/Left Arrow: Move left
/// - D/Right Arrow: Move right
/// - Space: Jump (when grounded)
///
/// Requires: RigidBody (Dynamic) + BoxCollider
class PlayerController : public runtime::ComponentScript {
public:
    const char* type_name() const override { return "PlayerController"; }

    void on_fixed_update() override;

    // Serialization and inspector
    void serialize_properties(nlohmann::json& out) const override;
    void deserialize_properties(const nlohmann::json& data) override;
    void on_inspector_gui(nlohmann::json& properties) override;

    // Movement parameters (configurable in inspector)
    float move_accel = 1500.0f;
    float max_move_speed = 100.0f;
    float jump_velocity = 180.0f;  // Positive = up in Box2D Y-up coordinates
    float friction = 10.0f;

private:
    bool m_jump_consumed = false;  // Prevents repeated jumps while holding Space
};

#include "PlayerController.h"
#include "engine/platform/KeyCode.h"
#include <imgui.h>
#include <algorithm>
#include <string>

REGISTER_COMPONENT_SCRIPT(PlayerController)

void PlayerController::on_fixed_update() {
    using namespace engine::platform;
    float dt = fixed_delta_time();

    int move_dir = 0;
    if (is_key_held(KeyCode::A) || is_key_held(KeyCode::Left)) move_dir -= 1;
    if (is_key_held(KeyCode::D) || is_key_held(KeyCode::Right)) move_dir += 1;
    bool jump = is_key_held(KeyCode::Space);

    auto vel = get_velocity();

    if (move_dir != 0) {
        float target_vx = static_cast<float>(move_dir) * max_move_speed;
        float accel = move_accel * dt;
        if (vel.x < target_vx) {
            vel.x = std::min(vel.x + accel, target_vx);
        } else if (vel.x > target_vx) {
            vel.x = std::max(vel.x - accel, target_vx);
        }
    } else {
        float friction_decel = friction * max_move_speed * dt;
        if (vel.x > 0) {
            vel.x = std::max(0.0f, vel.x - friction_decel);
        } else if (vel.x < 0) {
            vel.x = std::min(0.0f, vel.x + friction_decel);
        }
    }

    bool grounded = is_grounded();

    if (!jump) {
        m_jump_consumed = false;
    }
    if (jump && grounded && !m_jump_consumed) {
        vel.y = jump_velocity;
        m_jump_consumed = true;
    }

    set_velocity(vel.x, vel.y);
}

void PlayerController::serialize_properties(nlohmann::json& out) const {
    out["move_accel"] = move_accel;
    out["max_move_speed"] = max_move_speed;
    out["jump_velocity"] = jump_velocity;
    out["friction"] = friction;
}

void PlayerController::deserialize_properties(const nlohmann::json& data) {
    if (data.contains("move_accel")) move_accel = data["move_accel"].get<float>();
    if (data.contains("max_move_speed")) max_move_speed = data["max_move_speed"].get<float>();
    if (data.contains("jump_velocity")) jump_velocity = data["jump_velocity"].get<float>();
    if (data.contains("friction")) friction = data["friction"].get<float>();
}

void PlayerController::on_inspector_gui(nlohmann::json& props) {
    float accel = props.value("move_accel", 1500.0f);
    float max_speed = props.value("max_move_speed", 100.0f);
    float jump_vel = props.value("jump_velocity", 180.0f);
    float fric = props.value("friction", 10.0f);

    ImGui::DragFloat("Move Accel", &accel, 10.0f, 0.0f, 5000.0f);
    ImGui::DragFloat("Max Speed", &max_speed, 5.0f, 0.0f, 500.0f);
    ImGui::DragFloat("Jump Velocity", &jump_vel, 5.0f, 0.0f, 500.0f);
    ImGui::DragFloat("Friction", &fric, 0.5f, 0.0f, 50.0f);

    props["move_accel"] = accel;
    props["max_move_speed"] = max_speed;
    props["jump_velocity"] = jump_vel;
    props["friction"] = fric;
}

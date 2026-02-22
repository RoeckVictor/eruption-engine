#include "TestScript.h"
#include <string>
#include <cmath>
#include <imgui.h>

// Register the script with the engine
REGISTER_COMPONENT_SCRIPT(TestScript)

// ============================================================================
// Lifecycle
// ============================================================================

void TestScript::on_create() {
    log("TestScript::on_create() - Hello from TestScript!");

    // Store initial position for patrol
    m_patrol_start = get_position();
    m_patrol_end = {m_patrol_start.x + 200.0f, m_patrol_start.y};

    // Subscribe to custom events
    m_damage_event_handle = subscribe("player_damaged", &TestScript::on_player_damaged);

    // Start the countdown coroutine
    m_countdown_handle = start_coroutine(countdown_coroutine());

    // Log some entity info
    log(("Entity name: " + std::string(get_name())).c_str());
    log(("Is active: " + std::string(is_active() ? "true" : "false")).c_str());

    // Test random utilities
    float rand_val = random_float();
    float rand_range = random_range(-10.0f, 10.0f);
    int rand_int = random_int(1, 100);
    log(("Random float: " + std::to_string(rand_val)).c_str());
    log(("Random range [-10, 10]: " + std::to_string(rand_range)).c_str());
    log(("Random int [1, 100]: " + std::to_string(rand_int)).c_str());

    // Test hierarchy
    auto parent = get_parent();
    if (parent != entt::null) {
        log("This entity has a parent");
    }
    int child_count = get_child_count();
    log(("Child count: " + std::to_string(child_count)).c_str());
}

void TestScript::on_destroy() {
    log("TestScript::on_destroy() - Goodbye!");

    // Unsubscribe from events
    if (m_damage_event_handle != 0) {
        unsubscribe(m_damage_event_handle);
    }

    // Stop any running coroutines (they'll be cleaned up anyway, but good practice)
    stop_all_coroutines();
}

void TestScript::on_enable() {
    log("TestScript::on_enable() - Entity enabled");
}

void TestScript::on_disable() {
    log("TestScript::on_disable() - Entity disabled");
}

void TestScript::on_update() {
    float dt = delta_time();
    m_time_alive += dt;

    // Test input and movement
    runtime::Vec2 move_dir = {0.0f, 0.0f};

    if (is_key_held(engine::platform::KeyCode::W) || is_key_held(engine::platform::KeyCode::Up)) {
        move_dir.y -= 1.0f;
    }
    if (is_key_held(engine::platform::KeyCode::S) || is_key_held(engine::platform::KeyCode::Down)) {
        move_dir.y += 1.0f;
    }
    if (is_key_held(engine::platform::KeyCode::A) || is_key_held(engine::platform::KeyCode::Left)) {
        move_dir.x -= 1.0f;
    }
    if (is_key_held(engine::platform::KeyCode::D) || is_key_held(engine::platform::KeyCode::Right)) {
        move_dir.x += 1.0f;
    }

    // Normalize and apply movement using math helpers
    if (vec2_sqr_magnitude(move_dir) > 0.0f) {
        move_dir = vec2_normalize(move_dir);
        translate(move_dir.x * m_move_speed * dt, move_dir.y * m_move_speed * dt);
    }

    // Test key press events
    if (is_key_pressed(engine::platform::KeyCode::Space)) {
        log("Space pressed! Starting camera shake...");
        start_coroutine(camera_shake_coroutine(0.3f, 5.0f));
    }

    if (is_key_pressed(engine::platform::KeyCode::P)) {
        if (!m_is_patrolling) {
            log("Starting patrol...");
            m_patrol_start = get_position();
            m_patrol_end = {m_patrol_start.x + 200.0f, m_patrol_start.y};
            m_patrol_handle = start_coroutine(patrol_coroutine());
            m_is_patrolling = true;
        } else {
            log("Stopping patrol...");
            stop_coroutine(m_patrol_handle);
            m_is_patrolling = false;
        }
    }

    if (is_key_pressed(engine::platform::KeyCode::E)) {
        // Dispatch a custom event
        runtime::EventData data;
        data.set("damage", 25.0f);
        data.set("source", std::string("TestScript"));
        dispatch_event("player_damaged", data);
    }

    // Test mouse input
    if (is_mouse_pressed(engine::platform::MouseButton::Left)) {
        float mx = static_cast<float>(mouse_x());
        float my = static_cast<float>(mouse_y());
        runtime::Vec2 world_pos = screen_to_world(mx, my);
        log(("Mouse clicked at screen (" + std::to_string(mx) + ", " + std::to_string(my) +
             ") -> world (" + std::to_string(world_pos.x) + ", " + std::to_string(world_pos.y) + ")").c_str());
    }

    // Test smoothstep for smooth oscillation
    float t = std::fmod(m_time_alive, 2.0f) / 2.0f;  // 0 to 1 over 2 seconds
    float smooth_t = smoothstep(0.0f, 1.0f, t);

    // Camera zoom test with number keys
    if (is_key_pressed(engine::platform::KeyCode::Num1)) {
        set_camera_zoom(1.0f);
        log("Camera zoom set to 1.0");
    }
    if (is_key_pressed(engine::platform::KeyCode::Num2)) {
        set_camera_zoom(2.0f);
        log("Camera zoom set to 2.0");
    }
    if (is_key_pressed(engine::platform::KeyCode::Num3)) {
        set_camera_zoom(0.5f);
        log("Camera zoom set to 0.5");
    }
}

// ============================================================================
// Collision Callbacks
// ============================================================================

void TestScript::on_collision_enter(const runtime::CollisionInfo& info) {
    m_collision_count++;
    log(("Collision ENTER with entity " + std::to_string(static_cast<uint32_t>(info.other_entity)) +
         " - Normal: (" + std::to_string(info.normal_x) + ", " + std::to_string(info.normal_y) + ")" +
         " - Total collisions: " + std::to_string(m_collision_count)).c_str());
}

void TestScript::on_collision_stay(const runtime::CollisionInfo& info) {
    // Called every frame while in contact - usually don't want to log this
    (void)info;
}

void TestScript::on_collision_exit(const runtime::CollisionInfo& info) {
    m_collision_count--;
    log(("Collision EXIT with entity " + std::to_string(static_cast<uint32_t>(info.other_entity)) +
         " - Total collisions: " + std::to_string(m_collision_count)).c_str());
}

void TestScript::on_trigger_enter(const runtime::CollisionInfo& info) {
    log(("Trigger ENTER with entity " + std::to_string(static_cast<uint32_t>(info.other_entity))).c_str());
}

void TestScript::on_trigger_exit(const runtime::CollisionInfo& info) {
    log(("Trigger EXIT with entity " + std::to_string(static_cast<uint32_t>(info.other_entity))).c_str());
}

// ============================================================================
// Coroutines
// ============================================================================

runtime::Coroutine TestScript::countdown_coroutine() {
    log("Countdown starting in 1 second...");
    co_yield runtime::WaitForSeconds(1.0f);

    for (int i = 3; i > 0; --i) {
        log(("Countdown: " + std::to_string(i)).c_str());
        co_yield runtime::WaitForSeconds(1.0f);
    }

    log("GO!");

    // Wait one frame
    co_yield runtime::WaitForNextFrame();

    log("Countdown coroutine finished!");
}

runtime::Coroutine TestScript::camera_shake_coroutine(float duration, float intensity) {
    runtime::Vec2 original_pos = get_camera_position();
    float elapsed = 0.0f;

    while (elapsed < duration) {
        // Random offset for shake
        float offset_x = random_range(-intensity, intensity);
        float offset_y = random_range(-intensity, intensity);

        // Decay intensity over time
        float decay = 1.0f - (elapsed / duration);
        set_camera_position(original_pos.x + offset_x * decay, original_pos.y + offset_y * decay);

        co_yield runtime::WaitForNextFrame();
        elapsed += delta_time();
    }

    // Restore original position
    set_camera_position(original_pos.x, original_pos.y);
    log("Camera shake finished");
}

runtime::Coroutine TestScript::patrol_coroutine() {
    while (true) {
        // Move to end point
        runtime::Vec2 current = get_position();
        while (vec2_distance(current, m_patrol_end) > 5.0f) {
            current = get_position();
            runtime::Vec2 new_pos = vec2_move_towards(current, m_patrol_end, m_move_speed * delta_time());
            set_position(new_pos.x, new_pos.y);
            co_yield runtime::WaitForNextFrame();
        }

        log("Reached patrol end, waiting 1 second...");
        co_yield runtime::WaitForSeconds(1.0f);

        // Move back to start point
        current = get_position();
        while (vec2_distance(current, m_patrol_start) > 5.0f) {
            current = get_position();
            runtime::Vec2 new_pos = vec2_move_towards(current, m_patrol_start, m_move_speed * delta_time());
            set_position(new_pos.x, new_pos.y);
            co_yield runtime::WaitForNextFrame();
        }

        log("Reached patrol start, waiting 1 second...");
        co_yield runtime::WaitForSeconds(1.0f);
    }
}

// ============================================================================
// Events
// ============================================================================

void TestScript::on_player_damaged(const runtime::EventData& data) {
    float damage = data.get<float>("damage", 0.0f);
    std::string source = data.get<std::string>("source", "unknown");

    // Use the global script_log function for static callbacks
    std::string msg = "Player damaged! Damage: " + std::to_string(damage) + ", Source: " + source;
    runtime::script_log(msg.c_str());
}

// ============================================================================
// Inspector GUI
// ============================================================================

void TestScript::on_inspector_gui(nlohmann::json& properties) {
    (void)properties;  // TestScript shows debug info, not serializable properties
    ImGui::Text("=== TestScript Debug Info ===");
    ImGui::Separator();

    ImGui::Text("Time alive: %.2f seconds", m_time_alive);
    ImGui::Text("Active collisions: %d", m_collision_count);
    ImGui::Text("Is patrolling: %s", m_is_patrolling ? "Yes" : "No");

    ImGui::Separator();
    ImGui::SliderFloat("Move Speed", &m_move_speed, 10.0f, 500.0f);

    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::BulletText("WASD/Arrows: Move");
    ImGui::BulletText("Space: Camera shake");
    ImGui::BulletText("P: Toggle patrol");
    ImGui::BulletText("E: Dispatch damage event");
    ImGui::BulletText("1/2/3: Set camera zoom");
    ImGui::BulletText("Left Click: Log mouse position");

    ImGui::Separator();
    runtime::Vec2 pos = get_position();
    ImGui::Text("Position: (%.1f, %.1f)", pos.x, pos.y);

    runtime::Vec2 cam_pos = get_camera_position();
    float cam_zoom = get_camera_zoom();
    ImGui::Text("Camera: (%.1f, %.1f) zoom: %.2f", cam_pos.x, cam_pos.y, cam_zoom);

    if (ImGui::Button("Spawn Random Position")) {
        runtime::Vec2 rand_pos = random_point_in_circle(100.0f);
        set_position(pos.x + rand_pos.x, pos.y + rand_pos.y);
    }

    if (ImGui::Button("Test Math Helpers")) {
        log("Testing math helpers...");
        log(("lerp(0, 100, 0.5) = " + std::to_string(lerp(0.0f, 100.0f, 0.5f))).c_str());
        log(("clamp(150, 0, 100) = " + std::to_string(clamp(150.0f, 0.0f, 100.0f))).c_str());
        log(("smoothstep(0, 1, 0.5) = " + std::to_string(smoothstep(0.0f, 1.0f, 0.5f))).c_str());
        log(("angle_difference(350, 10) = " + std::to_string(angle_difference(350.0f, 10.0f))).c_str());
        log(("sign(-5) = " + std::to_string(sign(-5.0f))).c_str());

        runtime::Vec2 a = {3.0f, 4.0f};
        log(("vec2_magnitude({3,4}) = " + std::to_string(vec2_magnitude(a))).c_str());

        runtime::Vec2 norm = vec2_normalize(a);
        log(("vec2_normalize({3,4}) = (" + std::to_string(norm.x) + ", " + std::to_string(norm.y) + ")").c_str());
    }
}

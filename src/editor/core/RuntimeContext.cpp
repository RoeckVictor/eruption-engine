#include "RuntimeContext.h"
#include "EditorComponents.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"

namespace editor {

RuntimeContext::RuntimeContext() = default;
RuntimeContext::~RuntimeContext() = default;

void RuntimeContext::init(entt::registry* editor_registry) {
    m_editor_registry = editor_registry;
}

void RuntimeContext::play() {
    if (m_state != PlayState::Editing) {
        return; // Already playing
    }

    if (!m_editor_registry) {
        engine::Logger::instance().error("Runtime", "Cannot enter play mode: no registry set");
        return;
    }

    // Snapshot the current scene state
    snapshot_scene();

    // Create a runtime copy of the scene
    create_runtime_copy();

    m_state = PlayState::Playing;
    m_play_time = 0.0f;
    m_frame_count = 0;

    engine::Logger::instance().info("Runtime", "Entered play mode");
}

void RuntimeContext::pause() {
    if (m_state != PlayState::Playing) {
        return;
    }

    m_state = PlayState::Paused;
    engine::Logger::instance().info("Runtime", "Paused (frame %llu, time %.2fs)", m_frame_count, m_play_time);
}

void RuntimeContext::resume() {
    if (m_state != PlayState::Paused) {
        return;
    }

    m_state = PlayState::Playing;
    engine::Logger::instance().info("Runtime", "Resumed");
}

void RuntimeContext::stop() {
    if (m_state == PlayState::Editing) {
        return; // Not playing
    }

    // Restore the original scene state
    restore_scene();

    // Clean up runtime registry
    m_runtime_registry.reset();

    m_state = PlayState::Editing;
    engine::Logger::instance().info("Runtime", "Stopped play mode (ran for %.2fs, %llu frames)", m_play_time, m_frame_count);
}

void RuntimeContext::step_frame() {
    if (m_state != PlayState::Paused) {
        return;
    }

    m_step_requested = true;
}

void RuntimeContext::update(float dt) {
    if (m_state == PlayState::Editing) {
        return;
    }

    // If paused and no step requested, don't update
    if (m_state == PlayState::Paused && !m_step_requested) {
        return;
    }

    // Clear step request
    m_step_requested = false;

    // Update play time and frame count
    m_play_time += dt;
    m_frame_count++;

    // TODO: Run game systems on m_runtime_registry
    // This is where user scripts and game logic would execute
}

void RuntimeContext::snapshot_scene() {
    if (!m_editor_registry) {
        return;
    }

    // Serialize the scene to a string (JSON)
    SceneSerializer serializer(*m_editor_registry);
    m_scene_snapshot = serializer.save_to_string();

    engine::Logger::instance().info("Runtime", "Scene snapshot created (%zu bytes)", m_scene_snapshot.size());
}

void RuntimeContext::restore_scene() {
    if (!m_editor_registry || m_scene_snapshot.empty()) {
        return;
    }

    // Clear the editor registry
    m_editor_registry->clear();

    // Restore from snapshot
    SceneSerializer serializer(*m_editor_registry);
    if (serializer.load_from_string(m_scene_snapshot)) {
        engine::Logger::instance().info("Runtime", "Scene restored from snapshot");
    } else {
        engine::Logger::instance().error("Runtime", "Failed to restore scene from snapshot");
    }

    m_scene_snapshot.clear();
}

void RuntimeContext::create_runtime_copy() {
    if (!m_editor_registry) {
        return;
    }

    // Create a new registry for runtime
    m_runtime_registry = std::make_unique<entt::registry>();

    // Copy all entities and components from editor to runtime
    // We iterate through all entities and copy their components

    auto view = m_editor_registry->view<EntityInfo>();
    for (auto entity : view) {
        // Create corresponding entity in runtime registry
        auto runtime_entity = m_runtime_registry->create();

        // Copy EntityInfo
        if (m_editor_registry->all_of<EntityInfo>(entity)) {
            m_runtime_registry->emplace<EntityInfo>(runtime_entity, m_editor_registry->get<EntityInfo>(entity));
        }

        // Copy Transform
        if (m_editor_registry->all_of<Transform>(entity)) {
            m_runtime_registry->emplace<Transform>(runtime_entity, m_editor_registry->get<Transform>(entity));
        }

        // Copy Hierarchy
        if (m_editor_registry->all_of<Hierarchy>(entity)) {
            m_runtime_registry->emplace<Hierarchy>(runtime_entity, m_editor_registry->get<Hierarchy>(entity));
        }

        // TODO: Copy other component types as needed
    }

    engine::Logger::instance().info("Runtime", "Created runtime copy with %zu entities",
        static_cast<size_t>(m_runtime_registry->view<EntityInfo>().size()));
}

} // namespace editor

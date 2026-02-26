#pragma once

#include <string>
#include <functional>

namespace editor {

// Manages scene dirty state and file paths with optional override support
// The override mechanism allows systems like the prefab editor to
// redirect dirty marking to the prefab instead of the main scene
class SceneStateContext {
public:
    SceneStateContext() = default;
    ~SceneStateContext() = default;

    SceneStateContext(const SceneStateContext&) = delete;
    SceneStateContext& operator=(const SceneStateContext&) = delete;

    bool is_dirty() const { return m_dirty; }
    void mark_dirty();
    void clear_dirty() { m_dirty = false; }

    using DirtyOverrideCallback = std::function<void()>;
    void set_dirty_override(DirtyOverrideCallback callback);
    void clear_dirty_override();
    bool has_dirty_override() const { return m_dirty_override != nullptr; }

    const std::string& scene_path() const { return m_scene_path; }
    void set_scene_path(const std::string& path) { m_scene_path = path; }

    const std::string& project_path() const { return m_project_path; }
    void set_project_path(const std::string& path) { m_project_path = path; }

private:
    bool m_dirty = false;
    std::string m_scene_path;
    std::string m_project_path;

    DirtyOverrideCallback m_dirty_override;
};

}

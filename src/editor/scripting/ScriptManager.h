#pragma once

#include "DLLManager.h"
#include "ScriptCompiler.h"
#include "ScriptWatcher.h"
#include <memory>
#include <string>

namespace editor {

/// Coordinates script compilation, loading, and hot-reload.
class ScriptManager {
public:
    ScriptManager();
    ~ScriptManager();

    /// Initialize the script manager for a project.
    void init(const std::string& project_path, const std::string& engine_src_path, const std::string& engine_build_path);

    /// Shutdown and cleanup.
    void shutdown();

    /// Update (call every frame to handle async operations).
    void update();

    /// Trigger a rebuild of scripts.
    void rebuild();

    /// Check if scripts are currently building.
    bool is_building() const { return m_compiler.is_building(); }

    /// Check if scripts are loaded.
    bool are_scripts_loaded() const { return m_dll_manager.is_loaded(); }

    /// Get the DLL manager for creating script instances.
    DLLManager& dll_manager() { return m_dll_manager; }
    const DLLManager& dll_manager() const { return m_dll_manager; }

    /// Get the script compiler.
    ScriptCompiler& compiler() { return m_compiler; }
    const ScriptCompiler& compiler() const { return m_compiler; }

    /// Get build status text for UI.
    std::string status_text() const;

    /// Enable/disable auto-reload on file changes.
    void set_auto_reload(bool enabled) { m_auto_reload = enabled; }
    bool auto_reload() const { return m_auto_reload; }

private:
    void on_build_complete(bool success);
    void on_scripts_changed();

    DLLManager m_dll_manager;
    ScriptCompiler m_compiler;
    ScriptWatcher m_watcher;

    std::string m_project_path;
    bool m_auto_reload = true;
    bool m_reload_pending = false;
};

} // namespace editor

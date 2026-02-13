#include "ScriptManager.h"
#include "engine/core/Logger.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace editor {

ScriptManager::ScriptManager() = default;

ScriptManager::~ScriptManager() {
    shutdown();
}

void ScriptManager::init(const std::string& project_path, const std::string& engine_src_path, const std::string& engine_build_path) {
    m_project_path = project_path;

    // Set up compiler
    m_compiler.set_project_path(project_path);
    m_compiler.set_engine_include_path(engine_src_path);
    m_compiler.set_engine_build_path(engine_build_path);
    m_compiler.set_build_complete_callback([this](bool success) {
        on_build_complete(success);
    });

    // Set up watcher - watch Assets folder for script changes
    std::string assets_path = (fs::path(project_path) / "Assets").string();
    m_watcher.set_watch_path(assets_path);
    m_watcher.set_changed_callback([this]() {
        on_scripts_changed();
    });

    // Generate initial CMakeLists if needed
    m_compiler.generate_cmake();

    // Start watching for changes
    m_watcher.start();

    // Try to load existing DLL if it exists
    std::string dll_path = m_compiler.dll_path();
    if (fs::exists(dll_path)) {
        if (m_dll_manager.load(dll_path)) {
            engine::Logger::instance().info("ScriptManager", "Loaded existing scripts DLL");
        }
    } else {
        // No DLL exists yet - do initial build
        engine::Logger::instance().info("ScriptManager", "No scripts DLL found, building...");
        rebuild();
    }
}

void ScriptManager::shutdown() {
    m_watcher.stop();
    m_dll_manager.unload();
}

void ScriptManager::update() {
    // Poll for file changes
    m_watcher.poll();

    // Update compiler (check for async build completion)
    m_compiler.update();

    // Handle pending reload
    if (m_reload_pending && !m_compiler.is_building()) {
        m_reload_pending = false;
        rebuild();
    }
}

void ScriptManager::rebuild() {
    if (m_compiler.is_building()) {
        engine::Logger::instance().warning("ScriptManager", "Build already in progress");
        return;
    }

    engine::Logger::instance().info("ScriptManager", "Starting script build...");
    m_compiler.start_build();
}

std::string ScriptManager::status_text() const {
    switch (m_compiler.status()) {
        case BuildStatus::Idle:
            if (m_dll_manager.is_loaded()) {
                return "Scripts loaded (" + std::to_string(m_dll_manager.script_types().size()) + " types)";
            } else {
                return "No scripts loaded";
            }
        case BuildStatus::Configuring:
            return "Configuring...";
        case BuildStatus::Building:
            return "Building...";
        case BuildStatus::Success:
            return "Build successful";
        case BuildStatus::Failed:
            return "Build failed: " + m_compiler.last_error();
        default:
            return "";
    }
}

void ScriptManager::on_build_complete(bool success) {
    if (success) {
        // Unload old DLL
        m_dll_manager.unload();

        // Load new DLL
        std::string dll_path = m_compiler.dll_path();
        if (m_dll_manager.load(dll_path)) {
            engine::Logger::instance().info("ScriptManager", "Scripts reloaded successfully");
        } else {
            engine::Logger::instance().error("ScriptManager", "Failed to load scripts DLL: %s",
                m_dll_manager.last_error().c_str());
        }
    } else {
        engine::Logger::instance().error("ScriptManager", "Script build failed: %s",
            m_compiler.last_error().c_str());
    }
}

void ScriptManager::on_scripts_changed() {
    if (!m_auto_reload) {
        return;
    }

    engine::Logger::instance().info("ScriptManager", "Script files changed, queueing rebuild...");

    if (m_compiler.is_building()) {
        // Build in progress, queue another rebuild
        m_reload_pending = true;
    } else {
        rebuild();
    }
}

} // namespace editor

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <future>

namespace editor {

/// Build configuration for exporting a standalone game.
struct BuildConfig {
    std::string output_path;          // Output directory for the build
    std::string product_name;         // Name of the executable
    std::string default_scene;        // Scene to load on startup
    bool debug_build = false;         // Include debug symbols
    bool include_editor_scenes = false; // Include all scenes, not just default
};

/// Status of the game build process.
enum class GameBuildStatus {
    Idle,
    Preparing,
    CompilingScripts,
    CopyingAssets,
    CopyingRuntime,
    CreatingLauncher,
    Complete,
    Failed
};

/// Handles building and exporting standalone games.
class GameBuilder {
public:
    GameBuilder();
    ~GameBuilder();

    /// Set the project path.
    void set_project_path(const std::string& path) { m_project_path = path; }

    /// Set the engine source path.
    void set_engine_paths(const std::string& src_path, const std::string& build_path) {
        m_engine_src_path = src_path;
        m_engine_build_path = build_path;
    }

    /// Start an asynchronous build with the given configuration.
    void start_build(const BuildConfig& config);

    /// Reset the builder to idle state for a new build.
    void reset();

    /// Check if a build is currently in progress.
    bool is_building() const { return m_status != GameBuildStatus::Idle && m_status != GameBuildStatus::Complete && m_status != GameBuildStatus::Failed; }

    /// Get the current build status.
    GameBuildStatus status() const { return m_status; }

    /// Get the current build step description.
    const std::string& current_step() const { return m_current_step; }

    /// Get build progress (0.0 - 1.0).
    float progress() const { return m_progress; }

    /// Get any error message from a failed build.
    const std::string& error() const { return m_error; }

    /// Set callback for build completion.
    using BuildCompleteCallback = std::function<void(bool success)>;
    void set_build_complete_callback(BuildCompleteCallback callback) { m_complete_callback = std::move(callback); }

    /// Update the builder (call each frame to check for completion).
    void update();

private:
    bool build_sync(const BuildConfig& config);
    bool prepare_output_directory(const BuildConfig& config);
    bool compile_scripts(const BuildConfig& config);
    bool copy_assets(const BuildConfig& config);
    bool copy_runtime(const BuildConfig& config);
    bool create_launcher_config(const BuildConfig& config);

    std::string m_project_path;
    std::string m_engine_src_path;
    std::string m_engine_build_path;

    GameBuildStatus m_status = GameBuildStatus::Idle;
    std::string m_current_step;
    float m_progress = 0.0f;
    std::string m_error;

    std::future<bool> m_build_future;
    BuildCompleteCallback m_complete_callback;
};

} // namespace editor

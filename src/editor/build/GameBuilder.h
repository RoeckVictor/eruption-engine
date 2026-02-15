#pragma once

#include <string>
#include <vector>
#include <functional>
#include <future>
#include <atomic>

namespace editor {

/// Build configuration for exporting a standalone game.
struct BuildConfig {
    std::string output_path;
    std::string product_name;
    std::string default_scene;
    bool debug_build = false;
    bool include_editor_scenes = false;
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
    Cancelled,
    Failed
};

/// Handles building and exporting standalone games.
class GameBuilder {
public:
    GameBuilder();
    ~GameBuilder();

    void set_project_path(const std::string& path) { m_project_path = path; }

    void set_engine_paths(const std::string& src_path, const std::string& build_path) {
        m_engine_src_path = src_path;
        m_engine_build_path = build_path;
    }

    void start_build(const BuildConfig& config);

    void request_cancel();

    void reset();

    bool is_building() const {
        return m_status != GameBuildStatus::Idle &&
               m_status != GameBuildStatus::Complete &&
               m_status != GameBuildStatus::Cancelled &&
               m_status != GameBuildStatus::Failed;
    }

    GameBuildStatus status() const { return m_status; }

    const std::string& current_step() const { return m_current_step; }

    float progress() const { return m_progress; }

    const std::string& error() const { return m_error; }

    const std::string& actual_output_path() const { return m_actual_output_path; }

    using BuildCompleteCallback = std::function<void(bool success)>;
    void set_build_complete_callback(BuildCompleteCallback callback) { m_complete_callback = std::move(callback); }

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
    std::string m_actual_output_path;

    std::future<bool> m_build_future;
    BuildCompleteCallback m_complete_callback;
    std::atomic<bool> m_cancel_requested{false};
};

}

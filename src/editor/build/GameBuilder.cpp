#include "GameBuilder.h"
#include "engine/core/Logger.h"
#include "engine/platform/PlatformUtils.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace editor {

GameBuilder::GameBuilder() = default;

GameBuilder::~GameBuilder() {
    if (m_build_future.valid()) {
        m_build_future.wait();
    }
}

void GameBuilder::start_build(const BuildConfig& config) {
    if (is_building()) {
        engine::Logger::instance().warning("GameBuilder", "Build already in progress");
        return;
    }

    m_status = GameBuildStatus::Preparing;
    m_current_step = "Preparing build...";
    m_progress = 0.0f;
    m_error.clear();
    m_cancel_requested.store(false);

    m_build_future = std::async(std::launch::async, [this, config]() {
        return build_sync(config);
    });
}

void GameBuilder::request_cancel() {
    if (is_building()) {
        m_cancel_requested.store(true);
        engine::Logger::instance().info("GameBuilder", "Build cancellation requested");
    }
}

void GameBuilder::update() {
    if (!m_build_future.valid()) {
        return;
    }

    if (m_build_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        bool success = m_build_future.get();
        if (success) {
            m_status = GameBuildStatus::Complete;
        } else if (m_cancel_requested.load()) {
            m_status = GameBuildStatus::Cancelled;
        } else {
            m_status = GameBuildStatus::Failed;
        }

        if (m_complete_callback) {
            m_complete_callback(success);
        }
    }
}

void GameBuilder::reset() {
    // Wait for any pending build
    if (m_build_future.valid()) {
        m_build_future.wait();
    }

    m_status = GameBuildStatus::Idle;
    m_current_step.clear();
    m_progress = 0.0f;
    m_error.clear();
}

/// Sanitize a product name for safe use as a filename.
static std::string sanitize_product_name(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            result += '_';
        } else {
            result += c;
        }
    }
    // Trim leading/trailing spaces and dots (invalid on Windows)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) result.pop_back();
    while (!result.empty() && result.front() == ' ') result.erase(result.begin());
    if (result.empty()) result = "game";
    return result;
}

bool GameBuilder::build_sync(const BuildConfig& config) {
    // Place output in Debug/ or Release/ subfolder
    BuildConfig build_config = config;
    build_config.product_name = sanitize_product_name(build_config.product_name);
    std::string config_name = config.debug_build ? "Debug" : "Release";
    build_config.output_path = (fs::path(config.output_path) / config_name).string();
    m_actual_output_path = build_config.output_path;

    engine::Logger::instance().info("GameBuilder", "Starting %s build: %s → %s",
                                     config_name.c_str(), build_config.product_name.c_str(),
                                     build_config.output_path.c_str());

    auto check_cancel = [this]() -> bool {
        if (m_cancel_requested.load()) {
            m_error = "Build cancelled by user";
            m_current_step = "Cancelled";
            engine::Logger::instance().info("GameBuilder", "Build cancelled");
            return true;
        }
        return false;
    };

    // Step 1: Prepare output directory
    m_status = GameBuildStatus::Preparing;
    m_current_step = "Preparing output directory...";
    m_progress = 0.1f;
    if (!prepare_output_directory(build_config)) return false;
    if (check_cancel()) return false;

    // Step 2: Compile scripts
    m_status = GameBuildStatus::CompilingScripts;
    m_current_step = "Compiling scripts...";
    m_progress = 0.2f;
    if (!compile_scripts(build_config)) return false;
    if (check_cancel()) return false;

    // Step 3: Copy assets
    m_status = GameBuildStatus::CopyingAssets;
    m_current_step = "Copying assets...";
    m_progress = 0.4f;
    if (!copy_assets(build_config)) return false;
    if (check_cancel()) return false;

    // Step 4: Copy runtime
    m_status = GameBuildStatus::CopyingRuntime;
    m_current_step = "Copying runtime files...";
    m_progress = 0.7f;
    if (!copy_runtime(build_config)) return false;
    if (check_cancel()) return false;

    // Step 5: Create launcher config
    m_status = GameBuildStatus::CreatingLauncher;
    m_current_step = "Creating launcher configuration...";
    m_progress = 0.9f;
    if (!create_launcher_config(build_config)) return false;

    m_progress = 1.0f;
    m_current_step = "Build complete!";
    engine::Logger::instance().info("GameBuilder", "Build completed successfully: %s",
                                     build_config.output_path.c_str());
    return true;
}

bool GameBuilder::prepare_output_directory(const BuildConfig& config) {
    try {
        fs::path output_dir(config.output_path);

        // Create output directory if it doesn't exist
        if (!fs::exists(output_dir)) {
            fs::create_directories(output_dir);
        }

        // Create subdirectories
        fs::create_directories(output_dir / "Assets");
        fs::create_directories(output_dir / "shaders");

        return true;
    } catch (const std::exception& e) {
        m_error = std::string("Failed to prepare output directory: ") + e.what();
        engine::Logger::instance().error("GameBuilder", "%s", m_error.c_str());
        return false;
    }
}

bool GameBuilder::compile_scripts(const BuildConfig& config) {
    // For now, we just copy the pre-compiled DLL if it exists
    // Full script compilation would require calling ScriptCompiler

    fs::path project_dll = fs::path(m_project_path) / "Library" / "ScriptAssemblies" / "GameScripts.dll";
    fs::path output_dll = fs::path(config.output_path) / "GameScripts.dll";

    if (fs::exists(project_dll)) {
        try {
            fs::copy_file(project_dll, output_dll, fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied script DLL");
        } catch (const std::exception& e) {
            engine::Logger::instance().warning("GameBuilder", "Failed to copy scripts: %s", e.what());
            // Not a fatal error - game can run without scripts
        }
    }

    return true;
}

bool GameBuilder::copy_assets(const BuildConfig& config) {
    try {
        fs::path project_assets = fs::path(m_project_path) / "Assets";
        fs::path output_assets = fs::path(config.output_path) / "Assets";

        if (fs::exists(project_assets)) {
            // Copy all assets
            fs::copy(project_assets, output_assets,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);

            engine::Logger::instance().info("GameBuilder", "Copied assets");
        }

        return true;
    } catch (const std::exception& e) {
        m_error = std::string("Failed to copy assets: ") + e.what();
        engine::Logger::instance().error("GameBuilder", "%s", m_error.c_str());
        return false;
    }
}

bool GameBuilder::copy_runtime(const BuildConfig& config) {
    try {
        fs::path output_dir(config.output_path);

        // Copy shaders from engine (use exe directory, not working directory)
        fs::path exe_dir = engine::platform::executable_directory();
        fs::path shaders_src = exe_dir / "shaders";

        if (fs::exists(shaders_src)) {
            fs::copy(shaders_src, output_dir / "shaders",
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied shaders");
        }

        // Copy engine default assets (pixel grid defaults, materials, config)
        fs::path defaults_src = exe_dir / "assets" / "defaults";
        if (fs::exists(defaults_src)) {
            fs::copy(defaults_src, output_dir / "assets" / "defaults",
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied engine default assets");
        }

        fs::path materials_src = exe_dir / "assets" / "materials";
        if (fs::exists(materials_src)) {
            fs::copy(materials_src, output_dir / "assets" / "materials",
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied material definitions");
        }

        fs::path engine_config_src = exe_dir / "engine_config.json";
        if (fs::exists(engine_config_src)) {
            fs::copy_file(engine_config_src, output_dir / "engine_config.json",
                          fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied engine_config.json");
        }

        // Copy the standalone game runtime executable
        fs::path game_exe_src = exe_dir / "eruption_game.exe";
        fs::path game_exe_dst = output_dir / (config.product_name + ".exe");

        if (fs::exists(game_exe_src)) {
            fs::copy_file(game_exe_src, game_exe_dst, fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied game executable as %s",
                                             game_exe_dst.filename().string().c_str());
        } else {
            engine::Logger::instance().warning("GameBuilder", "Game executable not found: %s",
                                                game_exe_src.string().c_str());
        }

        return true;
    } catch (const std::exception& e) {
        m_error = std::string("Failed to copy runtime: ") + e.what();
        engine::Logger::instance().error("GameBuilder", "%s", m_error.c_str());
        return false;
    }
}

bool GameBuilder::create_launcher_config(const BuildConfig& config) {
    try {
        fs::path config_path = fs::path(config.output_path) / "game_config.json";
        std::ofstream file(config_path);

        if (!file.is_open()) {
            m_error = "Failed to create game configuration file";
            return false;
        }

        nlohmann::json j;
        j["productName"] = config.product_name;
        j["defaultScene"] = config.default_scene;
        j["debugBuild"] = config.debug_build;
        file << j.dump(2) << "\n";

        file.close();

        engine::Logger::instance().info("GameBuilder", "Created game configuration");
        return true;
    } catch (const std::exception& e) {
        m_error = std::string("Failed to create launcher config: ") + e.what();
        engine::Logger::instance().error("GameBuilder", "%s", m_error.c_str());
        return false;
    }
}

}

#include "GameBuilder.h"
#include "engine/core/Logger.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>

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

    m_build_future = std::async(std::launch::async, [this, config]() {
        return build_sync(config);
    });
}

void GameBuilder::update() {
    if (!m_build_future.valid()) {
        return;
    }

    if (m_build_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        bool success = m_build_future.get();
        m_status = success ? GameBuildStatus::Complete : GameBuildStatus::Failed;

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

bool GameBuilder::build_sync(const BuildConfig& config) {
    engine::Logger::instance().info("GameBuilder", "Starting build: %s", config.product_name.c_str());

    // Step 1: Prepare output directory
    m_status = GameBuildStatus::Preparing;
    m_current_step = "Preparing output directory...";
    m_progress = 0.1f;
    if (!prepare_output_directory(config)) {
        return false;
    }

    // Step 2: Compile scripts
    m_status = GameBuildStatus::CompilingScripts;
    m_current_step = "Compiling scripts...";
    m_progress = 0.2f;
    if (!compile_scripts(config)) {
        return false;
    }

    // Step 3: Copy assets
    m_status = GameBuildStatus::CopyingAssets;
    m_current_step = "Copying assets...";
    m_progress = 0.4f;
    if (!copy_assets(config)) {
        return false;
    }

    // Step 4: Copy runtime
    m_status = GameBuildStatus::CopyingRuntime;
    m_current_step = "Copying runtime files...";
    m_progress = 0.7f;
    if (!copy_runtime(config)) {
        return false;
    }

    // Step 5: Create launcher config
    m_status = GameBuildStatus::CreatingLauncher;
    m_current_step = "Creating launcher configuration...";
    m_progress = 0.9f;
    if (!create_launcher_config(config)) {
        return false;
    }

    m_progress = 1.0f;
    m_current_step = "Build complete!";
    engine::Logger::instance().info("GameBuilder", "Build completed successfully: %s",
                                     config.output_path.c_str());
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

        // Copy shaders from engine
        fs::path exe_dir = fs::current_path();
        fs::path shaders_src = exe_dir / "shaders";

        if (fs::exists(shaders_src)) {
            fs::copy(shaders_src, output_dir / "shaders",
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied shaders");
        }

        // Copy the game executable (eruption.exe)
        // In a real build, we'd compile the runtime for the target configuration
        // For now, we copy the existing eruption.exe if it exists
        fs::path game_exe_src = exe_dir / "eruption.exe";
        fs::path game_exe_dst = output_dir / (config.product_name + ".exe");

        if (fs::exists(game_exe_src)) {
            fs::copy_file(game_exe_src, game_exe_dst, fs::copy_options::overwrite_existing);
            engine::Logger::instance().info("GameBuilder", "Copied game executable as %s",
                                             game_exe_dst.filename().string().c_str());
        } else {
            engine::Logger::instance().warning("GameBuilder", "Game executable not found: %s",
                                                game_exe_src.string().c_str());
            // Not fatal - user might be building for the first time
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

        file << "{\n";
        file << "  \"productName\": \"" << config.product_name << "\",\n";
        file << "  \"defaultScene\": \"" << config.default_scene << "\",\n";
        file << "  \"debugBuild\": " << (config.debug_build ? "true" : "false") << "\n";
        file << "}\n";

        file.close();

        engine::Logger::instance().info("GameBuilder", "Created game configuration");
        return true;
    } catch (const std::exception& e) {
        m_error = std::string("Failed to create launcher config: ") + e.what();
        engine::Logger::instance().error("GameBuilder", "%s", m_error.c_str());
        return false;
    }
}

} // namespace editor

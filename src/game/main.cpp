#include "engine/core/Engine.h"
#include "game/GameApplication.h"
#include "engine/platform/PlatformUtils.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int /*argc*/, char* /*argv*/[]) {
#ifdef _WIN32
    // Allocate a console so printf/Logger output is visible for debugging
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    // Set working directory to executable directory for consistent path resolution
    std::string exe_dir = engine::platform::executable_directory();
    std::filesystem::current_path(exe_dir);

    printf("Working directory: %s\n", std::filesystem::current_path().string().c_str());

    std::string scene_path  = "Assets/Scenes/Main.json";
    std::string product_name = "Eruption Game";
    int width  = 1280;
    int height = 720;

    {
        std::ifstream config_file("game_config.json");
        if (config_file.is_open()) {
            try {
                auto j = nlohmann::json::parse(config_file);
                if (j.contains("defaultScene")) {
                    std::string scene_name = j["defaultScene"].get<std::string>();
                    // BuildSettingsPanel stores just the stem (e.g. "caca"), append .scene
                    std::filesystem::path sp(scene_name);
                    if (!sp.has_extension())
                        scene_name += ".scene";
                    scene_path = "Assets/" + scene_name;
                }
                if (j.contains("productName"))
                    product_name = j["productName"].get<std::string>();
                if (j.contains("windowWidth"))
                    width = j["windowWidth"].get<int>();
                if (j.contains("windowHeight"))
                    height = j["windowHeight"].get<int>();
            } catch (const nlohmann::json::exception& e) {
                fprintf(stderr, "Warning: Failed to parse game_config.json: %s\n", e.what());
            }
        } else {
            printf("Note: game_config.json not found, using defaults\n");
        }
    }

    printf("=== %s ===\n", product_name.c_str());
    printf("Scene: %s\n", scene_path.c_str());

    engine::Engine engine;

    if (!engine.init(product_name.c_str(), width, height, "engine_config.json")) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    GameApplication app(scene_path, product_name);
    engine.run(app);
    engine.shutdown();

    return 0;
}

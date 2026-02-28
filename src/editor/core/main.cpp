#include "engine/core/Engine.h"
#include "editor/core/EditorApplication.h"
#include "editor/core/ProjectManager.h"
#include "engine/platform/PlatformUtils.h"

#include <cstdio>
#include <filesystem>

int main(int argc, char* argv[]) {
    printf("=== Eruption Editor ===\n");

    // Set working directory to executable directory for consistent path resolution
    std::string exe_dir = engine::platform::executable_directory();
    std::filesystem::current_path(exe_dir);

    // Parse command line arguments
    std::string project_path;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) {
            project_path = argv[++i];
        }
    }

    engine::Engine engine;
    if (!engine.init("Eruption Editor", 1600, 900)) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    editor::EditorApplication app;

    // If a project path was provided, try to open it
    if (!project_path.empty()) {
        app.project_manager().open_project(project_path);
    }

    engine.run(app);

    engine.shutdown();
    printf("Eruption Editor shut down cleanly\n");
    return 0;
}

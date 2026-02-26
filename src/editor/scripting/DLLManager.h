#pragma once

#include "runtime/ComponentScript.h"
#include "runtime/SystemScript.h"
#include "engine/platform/IDynamicLibrary.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace editor {

struct ScriptTypeInfo {
    std::string name;
    runtime::ScriptFactory factory;
    bool is_system = false;
    runtime::SystemFactory system_factory = nullptr;
};

/// Manages loading and unloading of script DLLs.
/// Handles hot-reload by tracking script factories.
///
/// Script DLLs should export the following functions:
/// - GetScriptAPIVersion(int* major, int* minor) - Returns the API version
/// - GetScriptCount() -> int - Number of component scripts
/// - GetScriptName(int index) -> const char* - Name of script at index
/// - GetScriptFactory(int index) -> ScriptFactory - Factory function for script
/// - GetSystemCount() -> int - Number of system scripts (optional)
/// - GetSystemName(int index) -> const char* - Name of system at index
/// - GetSystemFactory(int index) -> SystemFactory - Factory function for system
/// - SetImGuiContext(ImGuiContext*) - Receives editor's ImGui context (optional)
class DLLManager {
public:
    DLLManager();
    ~DLLManager();

    bool load(const std::string& path);
    void unload();
    bool is_loaded() const { return m_library && m_library->is_loaded(); }

    const std::string& dll_path() const { return m_dll_path; }

    const std::vector<ScriptTypeInfo>& script_types() const { return m_script_types; }
    runtime::ComponentScript* create_script(const std::string& type_name);

    runtime::SystemScript* create_system(const std::string& type_name);

    bool has_script_type(const std::string& type_name) const;

    const std::string& last_error() const { return m_last_error; }

    /// Get the current API version that DLLs should target.
    static void api_version(int& major, int& minor);

    using LoadedCallback = std::function<void()>;
    void set_loaded_callback(LoadedCallback callback) { m_loaded_callback = callback; }

    using UnloadingCallback = std::function<void()>;
    void set_unloading_callback(UnloadingCallback callback) { m_unloading_callback = callback; }

private:
    void discover_scripts();

    std::unique_ptr<engine::platform::IDynamicLibrary> m_library;
    std::string m_dll_path;
    std::string m_last_error;

    std::vector<ScriptTypeInfo> m_script_types;
    std::unordered_map<std::string, size_t> m_type_index;

    LoadedCallback m_loaded_callback;
    UnloadingCallback m_unloading_callback;
};

}

#pragma once

#include "runtime/ComponentScript.h"
#include "runtime/SystemScript.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#ifdef _WIN32
#include <windows.h>
using DLLHandle = HMODULE;
#else
using DLLHandle = void*;
#endif

namespace editor {

/// Information about a registered script type.
struct ScriptTypeInfo {
    std::string name;
    runtime::ScriptFactory factory;
    bool is_system = false;
    runtime::SystemFactory system_factory = nullptr;
};

/// Manages loading and unloading of script DLLs.
/// Handles hot-reload by tracking script factories.
class DLLManager {
public:
    DLLManager();
    ~DLLManager();

    /// Load a script DLL from the given path.
    /// Returns true if successful.
    bool load(const std::string& path);

    /// Unload the currently loaded DLL.
    void unload();

    /// Check if a DLL is currently loaded.
    bool is_loaded() const { return m_handle != nullptr; }

    /// Get the path to the currently loaded DLL.
    const std::string& dll_path() const { return m_dll_path; }

    /// Get all registered script types.
    const std::vector<ScriptTypeInfo>& script_types() const { return m_script_types; }

    /// Create a script instance by type name.
    runtime::ComponentScript* create_script(const std::string& type_name);

    /// Create a system instance by type name.
    runtime::SystemScript* create_system(const std::string& type_name);

    /// Check if a script type is registered.
    bool has_script_type(const std::string& type_name) const;

    /// Get the last error message.
    const std::string& last_error() const { return m_last_error; }

    /// Callback for when DLL is loaded.
    using LoadedCallback = std::function<void()>;
    void set_loaded_callback(LoadedCallback callback) { m_loaded_callback = callback; }

    /// Callback for when DLL is about to be unloaded.
    using UnloadingCallback = std::function<void()>;
    void set_unloading_callback(UnloadingCallback callback) { m_unloading_callback = callback; }

private:
    void* get_symbol(const char* name);
    void discover_scripts();

    DLLHandle m_handle = nullptr;
    std::string m_dll_path;
    std::string m_last_error;

    std::vector<ScriptTypeInfo> m_script_types;
    std::unordered_map<std::string, size_t> m_type_index;

    LoadedCallback m_loaded_callback;
    UnloadingCallback m_unloading_callback;
};

} // namespace editor

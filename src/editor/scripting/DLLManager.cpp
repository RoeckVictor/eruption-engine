#include "DLLManager.h"
#include "engine/core/Logger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace editor {

DLLManager::DLLManager() = default;

DLLManager::~DLLManager() {
    unload();
}

bool DLLManager::load(const std::string& path) {
    // Unload any existing DLL first
    if (m_handle) {
        unload();
    }

    m_dll_path = path;

#ifdef _WIN32
    // On Windows, we need to copy the DLL to allow rebuilding
    // while the original is loaded. Use a temp copy.
    std::string temp_path = path + ".loaded";

    // Copy the file
    if (!CopyFileA(path.c_str(), temp_path.c_str(), FALSE)) {
        m_last_error = "Failed to copy DLL for loading";
        engine::Logger::instance().error("DLLManager", "Failed to copy DLL: %s", path.c_str());
        return false;
    }

    m_handle = LoadLibraryA(temp_path.c_str());
    if (!m_handle) {
        DWORD error = GetLastError();
        m_last_error = "LoadLibrary failed with error code: " + std::to_string(error);
        engine::Logger::instance().error("DLLManager", "Failed to load DLL: %s (error %lu)", path.c_str(), error);
        return false;
    }
#else
    m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!m_handle) {
        m_last_error = dlerror();
        engine::Logger::instance().error("DLLManager", "Failed to load DLL: %s (%s)", path.c_str(), m_last_error.c_str());
        return false;
    }
#endif

    // Discover registered scripts
    discover_scripts();

    engine::Logger::instance().info("DLLManager", "Loaded DLL: %s (%zu script types)", path.c_str(), m_script_types.size());

    // Notify callback
    if (m_loaded_callback) {
        m_loaded_callback();
    }

    return true;
}

void DLLManager::unload() {
    if (!m_handle) {
        return;
    }

    // Notify callback before unloading
    if (m_unloading_callback) {
        m_unloading_callback();
    }

    // Clear script types
    m_script_types.clear();
    m_type_index.clear();

#ifdef _WIN32
    FreeLibrary(m_handle);

    // Delete the temp copy
    std::string temp_path = m_dll_path + ".loaded";
    DeleteFileA(temp_path.c_str());
#else
    dlclose(m_handle);
#endif

    engine::Logger::instance().info("DLLManager", "Unloaded DLL: %s", m_dll_path.c_str());

    m_handle = nullptr;
    m_dll_path.clear();
}

void* DLLManager::get_symbol(const char* name) {
    if (!m_handle) {
        return nullptr;
    }

#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(m_handle, name));
#else
    return dlsym(m_handle, name);
#endif
}

void DLLManager::discover_scripts() {
    m_script_types.clear();
    m_type_index.clear();

    // Look for the registry function that lists all scripts
    // Convention: GetScriptRegistry returns array of script names
    using GetScriptCountFn = int(*)();
    using GetScriptNameFn = const char*(*)(int index);
    using GetScriptFactoryFn = runtime::ScriptFactory(*)(int index);

    auto get_count = reinterpret_cast<GetScriptCountFn>(get_symbol("GetScriptCount"));
    auto get_name = reinterpret_cast<GetScriptNameFn>(get_symbol("GetScriptName"));
    auto get_factory = reinterpret_cast<GetScriptFactoryFn>(get_symbol("GetScriptFactory"));

    if (get_count && get_name && get_factory) {
        int count = get_count();
        for (int i = 0; i < count; ++i) {
            ScriptTypeInfo info;
            info.name = get_name(i);
            info.factory = get_factory(i);
            info.is_system = false;

            m_type_index[info.name] = m_script_types.size();
            m_script_types.push_back(info);

            engine::Logger::instance().info("DLLManager", "  Registered script: %s", info.name.c_str());
        }
    }

    // Also look for system scripts
    using GetSystemCountFn = int(*)();
    using GetSystemNameFn = const char*(*)(int index);
    using GetSystemFactoryFn = runtime::SystemFactory(*)(int index);

    auto get_sys_count = reinterpret_cast<GetSystemCountFn>(get_symbol("GetSystemCount"));
    auto get_sys_name = reinterpret_cast<GetSystemNameFn>(get_symbol("GetSystemName"));
    auto get_sys_factory = reinterpret_cast<GetSystemFactoryFn>(get_symbol("GetSystemFactory"));

    if (get_sys_count && get_sys_name && get_sys_factory) {
        int count = get_sys_count();
        for (int i = 0; i < count; ++i) {
            ScriptTypeInfo info;
            info.name = get_sys_name(i);
            info.factory = nullptr;
            info.is_system = true;
            info.system_factory = get_sys_factory(i);

            m_type_index[info.name] = m_script_types.size();
            m_script_types.push_back(info);

            engine::Logger::instance().info("DLLManager", "  Registered system: %s", info.name.c_str());
        }
    }
}

runtime::ComponentScript* DLLManager::create_script(const std::string& type_name) {
    auto it = m_type_index.find(type_name);
    if (it == m_type_index.end()) {
        return nullptr;
    }

    const auto& info = m_script_types[it->second];
    if (info.is_system || !info.factory) {
        return nullptr;
    }

    return info.factory();
}

runtime::SystemScript* DLLManager::create_system(const std::string& type_name) {
    auto it = m_type_index.find(type_name);
    if (it == m_type_index.end()) {
        return nullptr;
    }

    const auto& info = m_script_types[it->second];
    if (!info.is_system || !info.system_factory) {
        return nullptr;
    }

    return info.system_factory();
}

bool DLLManager::has_script_type(const std::string& type_name) const {
    return m_type_index.find(type_name) != m_type_index.end();
}

} // namespace editor

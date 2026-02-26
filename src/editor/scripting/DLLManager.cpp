#include "DLLManager.h"
#include "ScriptAPIVersion.h"
#include "engine/core/Logger.h"
#include <imgui.h>
#include <sstream>

namespace editor {

DLLManager::DLLManager()
    : m_library(engine::platform::create_dynamic_library())
{
}

DLLManager::~DLLManager() {
    unload();
}

bool DLLManager::load(const std::string& path) {
    // Unload any existing DLL first
    if (m_library->is_loaded()) {
        unload();
    }

    m_dll_path = path;

    if (!m_library->load(path)) {
        m_last_error = m_library->last_error();
        engine::Logger::instance().error("DLLManager", "Failed to load DLL: %s (%s)",
            path.c_str(), m_last_error.c_str());
        return false;
    }

    // Check API version compatibility
    using GetAPIVersionFn = void(*)(int* major, int* minor);
    auto get_version = m_library->get_function<GetAPIVersionFn>("GetScriptAPIVersion");
    if (get_version) {
        int dll_major = 0, dll_minor = 0;
        get_version(&dll_major, &dll_minor);

        if (!ScriptAPIVersion::is_compatible(dll_major, dll_minor)) {
            std::ostringstream oss;
            oss << "Script API version mismatch: DLL has v" << dll_major << "." << dll_minor
                << ", editor expects v" << ScriptAPIVersion::MAJOR << "." << ScriptAPIVersion::MINOR;
            m_last_error = oss.str();
            engine::Logger::instance().error("DLLManager", "%s", m_last_error.c_str());
            m_library->unload();
            return false;
        }

        engine::Logger::instance().info("DLLManager", "Script API version: %d.%d (compatible)",
            dll_major, dll_minor);
    }

    // Share the editor's ImGui context with the DLL so scripts can use ImGui
    using SetImGuiContextFn = void(*)(ImGuiContext*);
    auto set_ctx = m_library->get_function<SetImGuiContextFn>("SetImGuiContext");
    if (set_ctx) {
        set_ctx(ImGui::GetCurrentContext());
    }

    // Discover registered scripts
    discover_scripts();

    engine::Logger::instance().info("DLLManager", "Loaded DLL: %s (%zu script types)",
        path.c_str(), m_script_types.size());

    // Notify callback
    if (m_loaded_callback) {
        m_loaded_callback();
    }

    return true;
}

void DLLManager::unload() {
    if (!m_library->is_loaded()) {
        return;
    }

    // Notify callback before unloading
    if (m_unloading_callback) {
        m_unloading_callback();
    }

    // Clear script types
    m_script_types.clear();
    m_type_index.clear();

    m_library->unload();

    engine::Logger::instance().info("DLLManager", "Unloaded DLL: %s", m_dll_path.c_str());

    m_dll_path.clear();
}

void DLLManager::discover_scripts() {
    m_script_types.clear();
    m_type_index.clear();

    // Look for the registry function that lists all scripts
    // Convention: GetScriptRegistry returns array of script names
    using GetScriptCountFn = int(*)();
    using GetScriptNameFn = const char*(*)(int index);
    using GetScriptFactoryFn = runtime::ScriptFactory(*)(int index);

    auto get_count = m_library->get_function<GetScriptCountFn>("GetScriptCount");
    auto get_name = m_library->get_function<GetScriptNameFn>("GetScriptName");
    auto get_factory = m_library->get_function<GetScriptFactoryFn>("GetScriptFactory");

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

    auto get_sys_count = m_library->get_function<GetSystemCountFn>("GetSystemCount");
    auto get_sys_name = m_library->get_function<GetSystemNameFn>("GetSystemName");
    auto get_sys_factory = m_library->get_function<GetSystemFactoryFn>("GetSystemFactory");

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
    // Safety check: ensure DLL is loaded before accessing factories
    if (!is_loaded()) {
        return nullptr;
    }

    auto it = m_type_index.find(type_name);
    if (it == m_type_index.end()) {
        return nullptr;
    }

    // Validate index is still valid (protects against stale references during hot-reload)
    size_t index = it->second;
    if (index >= m_script_types.size()) {
        return nullptr;
    }

    const auto& info = m_script_types[index];
    if (info.is_system || !info.factory) {
        return nullptr;
    }

    return info.factory();
}

runtime::SystemScript* DLLManager::create_system(const std::string& type_name) {
    // Safety check: ensure DLL is loaded before accessing factories
    if (!is_loaded()) {
        return nullptr;
    }

    auto it = m_type_index.find(type_name);
    if (it == m_type_index.end()) {
        return nullptr;
    }

    // Validate index is still valid (protects against stale references during hot-reload)
    size_t index = it->second;
    if (index >= m_script_types.size()) {
        return nullptr;
    }

    const auto& info = m_script_types[index];
    if (!info.is_system || !info.system_factory) {
        return nullptr;
    }

    return info.system_factory();
}

bool DLLManager::has_script_type(const std::string& type_name) const {
    return m_type_index.find(type_name) != m_type_index.end();
}

void DLLManager::api_version(int& major, int& minor) {
    major = ScriptAPIVersion::MAJOR;
    minor = ScriptAPIVersion::MINOR;
}

}
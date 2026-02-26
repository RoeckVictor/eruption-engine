#include "Win32DynamicLibrary.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace engine::platform {

Win32DynamicLibrary::~Win32DynamicLibrary() {
    unload();
}

bool Win32DynamicLibrary::load(const std::string& path) {
    // Unload any existing library first
    if (m_handle) {
        unload();
    }

    m_path = path;
    m_last_error.clear();

    if (m_hot_reload) {
        // Copy the DLL to allow rebuilding while loaded
        std::string temp_path = path + ".loaded";

        if (!CopyFileA(path.c_str(), temp_path.c_str(), FALSE)) {
            DWORD error = GetLastError();
            m_last_error = "Failed to copy DLL for loading (error " + std::to_string(error) + ")";
            return false;
        }

        m_handle = LoadLibraryA(temp_path.c_str());
        if (!m_handle) {
            DWORD error = GetLastError();
            m_last_error = "LoadLibrary failed (error " + std::to_string(error) + ")";
            DeleteFileA(temp_path.c_str());
            return false;
        }

        m_temp_path = temp_path;
    } else {
        m_handle = LoadLibraryA(path.c_str());
        if (!m_handle) {
            DWORD error = GetLastError();
            m_last_error = "LoadLibrary failed (error " + std::to_string(error) + ")";
            return false;
        }
    }

    return true;
}

void Win32DynamicLibrary::unload() {
    if (!m_handle) {
        return;
    }

    FreeLibrary(static_cast<HMODULE>(m_handle));
    m_handle = nullptr;

    // Delete the temp copy if hot-reload was used
    if (!m_temp_path.empty()) {
        DeleteFileA(m_temp_path.c_str());
        m_temp_path.clear();
    }

    m_path.clear();
}

void* Win32DynamicLibrary::get_symbol(const char* name) {
    if (!m_handle) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
}

}

#endif // _WIN32

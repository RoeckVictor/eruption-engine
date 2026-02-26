#include "UnixDynamicLibrary.h"

#ifndef _WIN32

#include <dlfcn.h>

namespace engine::platform {

UnixDynamicLibrary::~UnixDynamicLibrary() {
    unload();
}

bool UnixDynamicLibrary::load(const std::string& path) {
    // Unload any existing library first
    if (m_handle) {
        unload();
    }

    m_path = path;
    m_last_error.clear();

    m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!m_handle) {
        const char* err = dlerror();
        m_last_error = err ? err : "Unknown error";
        return false;
    }

    return true;
}

void UnixDynamicLibrary::unload() {
    if (!m_handle) {
        return;
    }

    dlclose(m_handle);
    m_handle = nullptr;
    m_path.clear();
}

void* UnixDynamicLibrary::get_symbol(const char* name) {
    if (!m_handle) {
        return nullptr;
    }
    return dlsym(m_handle, name);
}

}

#endif // !_WIN32

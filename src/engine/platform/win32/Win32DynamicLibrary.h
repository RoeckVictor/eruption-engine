#pragma once

#include "engine/platform/IDynamicLibrary.h"

namespace engine::platform {

//Windows implementation of IDynamicLibrary
/// Uses LoadLibrary/FreeLibrary/GetProcAddress
// Supports hot-reload by copying DLL to a temp file before loading
class Win32DynamicLibrary : public IDynamicLibrary {
public:
    Win32DynamicLibrary() = default;
    ~Win32DynamicLibrary() override;

    bool load(const std::string& path) override;
    void unload() override;
    bool is_loaded() const override { return m_handle != nullptr; }
    void* get_symbol(const char* name) override;
    const std::string& last_error() const override { return m_last_error; }
    const std::string& path() const override { return m_path; }

    void set_hot_reload_enabled(bool enabled) { m_hot_reload = enabled; }
    bool hot_reload_enabled() const { return m_hot_reload; }

private:
    void* m_handle = nullptr;
    std::string m_path;
    std::string m_temp_path;
    std::string m_last_error;
    bool m_hot_reload = true;
};

}

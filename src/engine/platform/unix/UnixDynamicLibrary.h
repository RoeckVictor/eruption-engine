#pragma once

#include "engine/platform/IDynamicLibrary.h"

namespace engine::platform {

// Unix/Linux implementation of IDynamicLibrary
// Uses dlopen/dlsym/dlclose
class UnixDynamicLibrary : public IDynamicLibrary {
public:
    UnixDynamicLibrary() = default;
    ~UnixDynamicLibrary() override;

    bool load(const std::string& path) override;
    void unload() override;
    bool is_loaded() const override { return m_handle != nullptr; }
    void* get_symbol(const char* name) override;
    const std::string& last_error() const override { return m_last_error; }
    const std::string& path() const override { return m_path; }

private:
    void* m_handle = nullptr;
    std::string m_path;
    std::string m_last_error;
};

}
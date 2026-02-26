#pragma once

#include <string>
#include <memory>

namespace engine::platform {

// Abstract interface for dynamic library loading
// Platform-specific implementations handle the actual library operations
class IDynamicLibrary {
public:
    virtual ~IDynamicLibrary() = default;

    virtual bool load(const std::string& path) = 0;
    virtual void unload() = 0;
    virtual bool is_loaded() const = 0;

    virtual void* get_symbol(const char* name) = 0;

    virtual const std::string& last_error() const = 0;

    virtual const std::string& path() const = 0;

    template<typename T>
    T get_function(const char* name) {
        return reinterpret_cast<T>(get_symbol(name));
    }

protected:
    IDynamicLibrary() = default;
};

std::unique_ptr<IDynamicLibrary> create_dynamic_library();

}
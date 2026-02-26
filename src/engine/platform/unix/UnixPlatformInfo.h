#pragma once

#include "engine/platform/IPlatformInfo.h"

namespace engine::platform {

class UnixPlatformInfo : public IPlatformInfo {
public:
    std::string executable_directory() const override;
    const char* shared_library_extension() const override;
    const char* shared_library_prefix() const override { return "lib"; }
    const char* executable_extension() const override { return ""; }
    char path_separator() const override { return '/'; }
    bool filesystem_case_insensitive() const override { return false; }
    std::string user_config_directory() const override;
    std::string user_documents_directory() const override;
    std::string find_cmake() const override;
};

} // namespace engine::platform

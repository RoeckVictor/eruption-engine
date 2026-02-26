#pragma once

#include "PlatformUtils.h"
#include <string>
#include <vector>
#include <memory>

namespace engine::platform {

// Abstract interface for native file dialogs
// Platform-specific implementations handle the actual dialog display
class IFileDialogService {
public:
    virtual ~IFileDialogService() = default;

    virtual std::string open_file(
        const std::string& title,
        const std::vector<FileFilter>& filters = {}) = 0;

    virtual std::string save_file(
        const std::string& title,
        const std::vector<FileFilter>& filters = {},
        const std::string& default_ext = "",
        const std::string& initial_dir = "") = 0;

    virtual std::string select_folder(const std::string& title = "Select Folder") = 0;

protected:
    IFileDialogService() = default;
};

std::unique_ptr<IFileDialogService> create_file_dialog_service();

}

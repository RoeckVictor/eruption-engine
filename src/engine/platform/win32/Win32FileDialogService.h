#pragma once

#include "engine/platform/IFileDialogService.h"

namespace engine::platform {

// Windows implementation of IFileDialogService
// Uses Win32 Common Dialogs (GetOpenFileName, GetSaveFileName) and COM IFileDialog
class Win32FileDialogService : public IFileDialogService {
public:
    Win32FileDialogService() = default;
    ~Win32FileDialogService() override = default;

    std::string open_file(
        const std::string& title,
        const std::vector<FileFilter>& filters) override;

    std::string save_file(
        const std::string& title,
        const std::vector<FileFilter>& filters,
        const std::string& default_ext,
        const std::string& initial_dir) override;

    std::string select_folder(const std::string& title) override;
};

}

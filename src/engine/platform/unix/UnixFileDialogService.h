#pragma once

#include "engine/platform/IFileDialogService.h"

namespace engine::platform {

// Unix/Linux implementation of IFileDialogService
// Uses zenity or kdialog as fallback for file dialogs
class UnixFileDialogService : public IFileDialogService {
public:
    UnixFileDialogService();
    ~UnixFileDialogService() override = default;

    std::string open_file(
        const std::string& title,
        const std::vector<FileFilter>& filters) override;

    std::string save_file(
        const std::string& title,
        const std::vector<FileFilter>& filters,
        const std::string& default_ext,
        const std::string& initial_dir) override;

    std::string select_folder(const std::string& title) override;

private:
    enum class DialogTool { None, Zenity, Kdialog };
    DialogTool m_tool = DialogTool::None;

    static bool has_command(const std::string& cmd);
    static std::string run_command_capture(const std::string& cmd);
    static std::string shell_escape(const std::string& s);
    static std::string build_zenity_filters(const std::vector<FileFilter>& filters);
};

}

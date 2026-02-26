#include "PlatformUtils.h"
#include "IFileDialogService.h"
#include "IProcessRunner.h"
#include "IPlatformInfo.h"

#include <filesystem>
#include <memory>
#include <algorithm>
#include <cctype>

namespace engine::platform {

namespace {

std::unique_ptr<IFileDialogService>& get_dialog_service() {
    static std::unique_ptr<IFileDialogService> instance;
    if (!instance) {
        instance = create_file_dialog_service();
    }
    return instance;
}

std::unique_ptr<IProcessRunner>& get_process_runner() {
    static std::unique_ptr<IProcessRunner> instance;
    if (!instance) {
        instance = create_process_runner();
    }
    return instance;
}

std::unique_ptr<IPlatformInfo>& get_platform_info() {
    static std::unique_ptr<IPlatformInfo> instance;
    if (!instance) {
        instance = create_platform_info();
    }
    return instance;
}

} // anonymous namespace

std::string open_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters)
{
    return get_dialog_service()->open_file(title, filters);
}

std::string save_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& default_ext,
    const std::string& initial_dir)
{
    return get_dialog_service()->save_file(title, filters, default_ext, initial_dir);
}

std::string folder_dialog(const std::string& title) {
    return get_dialog_service()->select_folder(title);
}

bool launch_detached(const std::string& exe_path, const std::vector<std::string>& args) {
    return get_process_runner()->launch_detached(exe_path, args);
}

void open_folder_in_file_manager(const std::string& folder_path) {
    get_process_runner()->open_folder_in_file_manager(folder_path);
}

void reveal_in_file_manager(const std::string& file_path) {
    get_process_runner()->reveal_in_file_manager(file_path);
}

std::string executable_directory() {
    return get_platform_info()->executable_directory();
}

const char* shared_library_extension() {
    return get_platform_info()->shared_library_extension();
}

const char* shared_library_prefix() {
    return get_platform_info()->shared_library_prefix();
}

std::string shared_library_name(const std::string& base_name) {
    return get_platform_info()->shared_library_name(base_name);
}

const char* executable_extension() {
    return get_platform_info()->executable_extension();
}

char path_separator() {
    return get_platform_info()->path_separator();
}

std::string find_cmake() {
    return get_platform_info()->find_cmake();
}

std::string user_config_directory() {
    return get_platform_info()->user_config_directory();
}

std::string user_documents_directory() {
    return get_platform_info()->user_documents_directory();
}

bool filesystem_case_insensitive() {
    return get_platform_info()->filesystem_case_insensitive();
}

std::string normalize_path_for_comparison(const std::string& path) {
    namespace fs = std::filesystem;

    try {
        fs::path normalized = fs::weakly_canonical(fs::path(path));
        std::string result = normalized.string();

        if (filesystem_case_insensitive()) {
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }
        return result;
    } catch (const std::exception&) {
        std::string result = path;
        std::replace(result.begin(), result.end(), '\\', '/');
        if (filesystem_case_insensitive()) {
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }
        return result;
    }
}

}

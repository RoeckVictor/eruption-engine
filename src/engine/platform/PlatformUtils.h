#pragma once

#include <string>
#include <vector>

namespace engine::platform {

struct FileFilter {
    std::string description;
    std::string pattern;
};

std::string open_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters = {});

std::string save_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters = {},
    const std::string& default_ext = "",
    const std::string& initial_dir = "");

std::string folder_dialog(const std::string& title = "Select Folder");

bool launch_detached(
    const std::string& exe_path,
    const std::vector<std::string>& args = {});

void open_folder_in_file_manager(const std::string& folder_path);
void reveal_in_file_manager(const std::string& file_path);

std::string executable_directory();

const char* shared_library_extension();
const char* shared_library_prefix();
std::string shared_library_name(const std::string& base_name);

const char* executable_extension();

char path_separator();

std::string find_cmake();

std::string user_config_directory();
std::string user_documents_directory();

bool filesystem_case_insensitive();
std::string normalize_path_for_comparison(const std::string& path);

}

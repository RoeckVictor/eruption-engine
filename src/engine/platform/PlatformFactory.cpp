#include "IFileDialogService.h"
#include "IDynamicLibrary.h"
#include "IProcessRunner.h"
#include "IPlatformInfo.h"

#ifdef _WIN32
#include "win32/Win32FileDialogService.h"
#include "win32/Win32DynamicLibrary.h"
#include "win32/Win32ProcessRunner.h"
#include "win32/Win32PlatformInfo.h"
#else
#include "unix/UnixFileDialogService.h"
#include "unix/UnixDynamicLibrary.h"
#include "unix/UnixProcessRunner.h"
#include "unix/UnixPlatformInfo.h"
#endif

namespace engine::platform {

std::unique_ptr<IFileDialogService> create_file_dialog_service() {
#ifdef _WIN32
    return std::make_unique<Win32FileDialogService>();
#else
    return std::make_unique<UnixFileDialogService>();
#endif
}

std::unique_ptr<IDynamicLibrary> create_dynamic_library() {
#ifdef _WIN32
    return std::make_unique<Win32DynamicLibrary>();
#else
    return std::make_unique<UnixDynamicLibrary>();
#endif
}

std::unique_ptr<IProcessRunner> create_process_runner() {
#ifdef _WIN32
    return std::make_unique<Win32ProcessRunner>();
#else
    return std::make_unique<UnixProcessRunner>();
#endif
}

std::unique_ptr<IPlatformInfo> create_platform_info() {
#ifdef _WIN32
    return std::make_unique<Win32PlatformInfo>();
#else
    return std::make_unique<UnixPlatformInfo>();
#endif
}

}

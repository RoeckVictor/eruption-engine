#pragma once

#ifdef _WIN32

#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace engine::platform::win32 {

/// Convert UTF-8 string to Windows wide string (UTF-16)
inline std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide.data(), len);
    wide.resize(len - 1);
    return wide;
}

/// Convert Windows wide string (UTF-16) to UTF-8 string
inline std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string str(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, str.data(), len, nullptr, nullptr);
    str.resize(len - 1);
    return str;
}

}

#endif // _WIN32

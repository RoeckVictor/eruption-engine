#include "Win32FileDialogService.h"
#include "Win32StringUtils.h"

#ifdef _WIN32

#include <commdlg.h>
#include <shobjidl.h>

namespace engine::platform {

using win32::utf8_to_wide;
using win32::wide_to_utf8;

static std::wstring build_win32_filter(const std::vector<FileFilter>& filters) {
    std::wstring result;
    for (const auto& f : filters) {
        result += utf8_to_wide(f.description);
        result += L'\0';
        result += utf8_to_wide(f.pattern);
        result += L'\0';
    }
    result += L'\0';
    return result;
}

std::string Win32FileDialogService::open_file(
    const std::string& title,
    const std::vector<FileFilter>& filters)
{
    std::wstring filter_str = build_win32_filter(filters);
    std::wstring wide_title = utf8_to_wide(title);

    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wide_title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return wide_to_utf8(filename);
    }
    return {};
}

std::string Win32FileDialogService::save_file(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& default_ext,
    const std::string& initial_dir)
{
    std::wstring filter_str = build_win32_filter(filters);
    std::wstring wide_title = utf8_to_wide(title);

    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wide_title.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // Windows lpstrDefExt must not include a leading dot
    std::string ext_no_dot = default_ext;
    if (!ext_no_dot.empty() && ext_no_dot[0] == '.') {
        ext_no_dot = ext_no_dot.substr(1);
    }
    std::wstring wide_ext = utf8_to_wide(ext_no_dot);
    std::wstring wide_initial_dir = utf8_to_wide(initial_dir);
    if (!wide_ext.empty()) {
        ofn.lpstrDefExt = wide_ext.c_str();
    }
    if (!wide_initial_dir.empty()) {
        ofn.lpstrInitialDir = wide_initial_dir.c_str();
    }

    if (GetSaveFileNameW(&ofn)) {
        return wide_to_utf8(filename);
    }
    return {};
}

std::string Win32FileDialogService::select_folder(const std::string& title) {
    std::string result;

    HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool we_initialized_com = SUCCEEDED(com_hr);
    bool com_available = we_initialized_com || com_hr == RPC_E_CHANGED_MODE;

    if (com_available) {
        IFileDialog* pDialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                              IID_IFileDialog, reinterpret_cast<void**>(&pDialog));

        if (SUCCEEDED(hr)) {
            DWORD options;
            pDialog->GetOptions(&options);
            pDialog->SetOptions(options | FOS_PICKFOLDERS);

            // Set title
            std::wstring wide_title = utf8_to_wide(title);
            if (!wide_title.empty()) {
                pDialog->SetTitle(wide_title.c_str());
            }

            hr = pDialog->Show(nullptr);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = nullptr;
                hr = pDialog->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszPath = nullptr;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr)) {
                        result = wide_to_utf8(pszPath);
                        CoTaskMemFree(pszPath);
                    }
                    pItem->Release();
                }
            }
            pDialog->Release();
        }

        // Only uninitialize if we were the ones who initialized COM
        if (we_initialized_com) {
            CoUninitialize();
        }
    }

    return result;
}

}

#endif // _WIN32

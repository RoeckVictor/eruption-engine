#pragma once

namespace editor {

/// Script API version for DLL compatibility checking.
/// Increment MAJOR when making breaking changes to the script interface.
/// Increment MINOR when adding new features that are backwards compatible.
struct ScriptAPIVersion {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;

    /// Checks if a DLL version is compatible with the current API.
    /// A DLL is compatible if:
    /// - MAJOR version matches exactly (breaking changes)
    /// - MINOR version is less than or equal to current (backwards compatible)
    static constexpr bool is_compatible(int dll_major, int dll_minor) {
        return dll_major == MAJOR && dll_minor <= MINOR;
    }
};

} // namespace editor

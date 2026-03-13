#pragma once
// =============================================================================
// OSVersionHelper.h -- Detects Windows version at runtime.
//
// Windows deprecated GetVersionEx() in Windows 8.1. The correct modern
// approach is RtlGetVersion() from ntdll.dll (always accurate, even in
// compatibility mode) combined with IsWindows10OrGreater() from VersionHelper.h.
//
// Call detect() once from main(). All other code queries the cached values.
// =============================================================================

#include <string>
#include <cstdint>

namespace aura {

struct WindowsVersion {
    uint32_t major{};       // e.g. 10
    uint32_t minor{};       // e.g. 0
    uint32_t build{};       // e.g. 22631 (Windows 11 23H2)
    std::string displayName; // e.g. "Windows 11 (22631)"
    bool isWindows10OrLater{};
    bool isWindows11OrLater{};
};

class OSVersionHelper {
public:
    // Call once from main() -- before UI is shown
    static void detect();

    // Returns cached WindowsVersion struct (valid after detect())
    static const WindowsVersion& version();

    // Returns true if running Windows 11 (build ≥ 22000)
    static bool isWindows11();

    // Returns true if build number meets minimum requirement
    static bool requiresBuild(uint32_t minBuild);

private:
    OSVersionHelper() = delete;
    static WindowsVersion s_version;
    static bool           s_detected;
};

} // namespace aura

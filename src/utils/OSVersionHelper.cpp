// =============================================================================
// OSVersionHelper.cpp — Runtime Windows version detection via RtlGetVersion
// =============================================================================

#include "../pch.h"  // PCH
#include "OSVersionHelper.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>   // OSVERSIONINFOEXW

#include <format>
#include <stdexcept>

// RtlGetVersion is in ntdll.dll. We load it dynamically to avoid a hard
// dependency and to bypass the GetVersionEx() compatibility shim.
using RtlGetVersionFn = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

namespace aura {

WindowsVersion OSVersionHelper::s_version{};
bool           OSVersionHelper::s_detected = false;

// -----------------------------------------------------------------------------
void OSVersionHelper::detect() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        throw std::runtime_error("OSVersionHelper: failed to get ntdll.dll handle");
    }

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(ntdll, "RtlGetVersion"));

    if (!rtlGetVersion) {
        throw std::runtime_error("OSVersionHelper: RtlGetVersion not found in ntdll");
    }

    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtlGetVersion(&info) != 0 /* STATUS_SUCCESS */) {
        throw std::runtime_error("OSVersionHelper: RtlGetVersion returned failure");
    }

    s_version.major = info.dwMajorVersion;
    s_version.minor = info.dwMinorVersion;
    s_version.build = info.dwBuildNumber;

    // Windows 11 is identified by build number ≥ 22000
    s_version.isWindows10OrLater = (info.dwMajorVersion >= 10);
    s_version.isWindows11OrLater = (info.dwMajorVersion >= 10 && info.dwBuildNumber >= 22000);

    // Build human-readable display name
    if (s_version.isWindows11OrLater) {
        s_version.displayName = std::format("Windows 11 (build {})", s_version.build);
    } else if (s_version.isWindows10OrLater) {
        s_version.displayName = std::format("Windows 10 (build {})", s_version.build);
    } else {
        s_version.displayName = std::format("Windows {}.{} (build {})",
            s_version.major, s_version.minor, s_version.build);
    }

    s_detected = true;

    AURA_LOG_INFO("OSVersionHelper", "Detected: {}", s_version.displayName);

    // Warn if running on an unsupported Windows version
    if (!s_version.isWindows10OrLater) {
        AURA_LOG_WARN("OSVersionHelper",
            "AuraCastPro requires Windows 10 or later. "
            "Some features may not work on {}.", s_version.displayName);
    }

    // DirectX 12 Ultimate features require Windows 10 20H1 (build 19041+)
    if (s_version.build < 19041) {
        AURA_LOG_WARN("OSVersionHelper",
            "Build {} detected. DirectX 12 Ultimate features require build 19041+.",
            s_version.build);
    }
}

// -----------------------------------------------------------------------------
const WindowsVersion& OSVersionHelper::version() {
    return s_version;
}

// -----------------------------------------------------------------------------
bool OSVersionHelper::isWindows11() {
    return s_version.isWindows11OrLater;
}

// -----------------------------------------------------------------------------
bool OSVersionHelper::requiresBuild(uint32_t minBuild) {
    return s_version.build >= minBuild;
}

} // namespace aura

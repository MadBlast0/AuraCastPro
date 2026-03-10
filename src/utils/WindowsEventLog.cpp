// =============================================================================
// WindowsEventLog.cpp — Task 159: Windows Event Log integration
// =============================================================================
#include "WindowsEventLog.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <format>

namespace aura {

static constexpr wchar_t kSourceName[] = L"AuraCastPro";

// ── Event IDs ─────────────────────────────────────────────────────────────────
static constexpr DWORD EVT_APP_START        = 1000;
static constexpr DWORD EVT_APP_STOP         = 1001;
static constexpr DWORD EVT_DEVICE_CONNECTED = 1001;
static constexpr DWORD EVT_DEVICE_DISC      = 1002;
static constexpr DWORD EVT_CONN_WARN        = 1002;
static constexpr DWORD EVT_SUBSYS_ERROR     = 1003;
static constexpr DWORD EVT_LICENCE_FAIL     = 1004;
static constexpr DWORD EVT_DISK_LOW         = 1005;

// =============================================================================
bool WindowsEventLog::registerSource() {
    // Create registry key:
    //   HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\AuraCastPro
    constexpr wchar_t kRegPath[] =
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\AuraCastPro";

    HKEY hKey = nullptr;
    DWORD disposition = 0;
    LONG rc = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        kRegPath,
        0, nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        nullptr,
        &hKey, &disposition);

    if (rc != ERROR_SUCCESS) {
        AURA_LOG_WARN("WindowsEventLog",
            "RegisterSource failed — insufficient permissions (rc={}). "
            "Run the installer as admin to enable Event Log integration.", rc);
        return false;
    }

    // TypesSupported: EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE
    DWORD typesSupported = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    RegSetValueExW(hKey, L"TypesSupported", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&typesSupported), sizeof(typesSupported));

    // EventMessageFile: path to the exe (which contains the message resource)
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    RegSetValueExW(hKey, L"EventMessageFile", 0, REG_EXPAND_SZ,
        reinterpret_cast<const BYTE*>(exePath),
        (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));

    RegCloseKey(hKey);

    if (disposition == REG_CREATED_NEW_KEY) {
        AURA_LOG_INFO("WindowsEventLog", "Event source registered successfully.");
    }
    return true;
}

// =============================================================================
void WindowsEventLog::open() {
    m_handle = RegisterEventSourceW(nullptr, kSourceName);
    if (!m_handle) {
        // Event source not registered — app will still work, just no Event Log
        AURA_LOG_DEBUG("WindowsEventLog",
            "RegisterEventSource failed (err={}) — Event Log writes disabled.",
            GetLastError());
    } else {
        AURA_LOG_DEBUG("WindowsEventLog", "Event Log source opened.");
    }
}

void WindowsEventLog::close() {
    if (m_handle) {
        DeregisterEventSource(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
}

// =============================================================================
static WORD severityToType(int sev) {
    if (sev == 0) return EVENTLOG_INFORMATION_TYPE;
    if (sev == 1) return EVENTLOG_WARNING_TYPE;
    return EVENTLOG_ERROR_TYPE;
}

void WindowsEventLog::info(uint32_t eventId, const std::string& message) {
    if (!m_handle) return;
    const std::wstring wmsg(message.begin(), message.end());
    const wchar_t* strings[] = { wmsg.c_str() };
    ReportEventW(static_cast<HANDLE>(m_handle),
        EVENTLOG_INFORMATION_TYPE, 0, eventId,
        nullptr, 1, 0, strings, nullptr);
}

void WindowsEventLog::warning(uint32_t eventId, const std::string& message) {
    if (!m_handle) return;
    const std::wstring wmsg(message.begin(), message.end());
    const wchar_t* strings[] = { wmsg.c_str() };
    ReportEventW(static_cast<HANDLE>(m_handle),
        EVENTLOG_WARNING_TYPE, 0, eventId,
        nullptr, 1, 0, strings, nullptr);
}

void WindowsEventLog::error(uint32_t eventId, const std::string& message) {
    if (!m_handle) return;
    const std::wstring wmsg(message.begin(), message.end());
    const wchar_t* strings[] = { wmsg.c_str() };
    ReportEventW(static_cast<HANDLE>(m_handle),
        EVENTLOG_ERROR_TYPE, 0, eventId,
        nullptr, 1, 0, strings, nullptr);
}

// =============================================================================
// Convenience wrappers
// =============================================================================
void WindowsEventLog::logAppStart(const std::string& version) {
    info(EVT_APP_START,
        std::format("AuraCastPro {} started.", version));
}

void WindowsEventLog::logAppStop() {
    info(EVT_APP_STOP, "AuraCastPro stopped.");
}

void WindowsEventLog::logDeviceConnected(const std::string& deviceName,
                                          const std::string& ip) {
    info(EVT_DEVICE_CONNECTED,
        std::format("Device connected: {} ({})", deviceName, ip));
}

void WindowsEventLog::logDeviceDisconnected(const std::string& deviceName) {
    info(EVT_DEVICE_DISC,
        std::format("Device disconnected: {}", deviceName));
}

void WindowsEventLog::logSubsystemError(const std::string& subsystem,
                                         const std::string& detail) {
    error(EVT_SUBSYS_ERROR,
        std::format("Subsystem error in {}: {}", subsystem, detail));
}

void WindowsEventLog::logLicenceFailure(const std::string& reason) {
    error(EVT_LICENCE_FAIL,
        std::format("Licence validation failed: {}", reason));
}

void WindowsEventLog::logDiskSpaceLow(uint64_t freeMb, uint64_t requiredMb) {
    warning(EVT_DISK_LOW,
        std::format("Disk space low: {} MB free, {} MB required for recording.",
                    freeMb, requiredMb));
}

// =============================================================================
WindowsEventLog& WindowsEventLog::instance() {
    static WindowsEventLog inst;
    return inst;
}

} // namespace aura

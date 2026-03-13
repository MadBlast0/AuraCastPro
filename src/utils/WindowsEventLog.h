#pragma once
// =============================================================================
// WindowsEventLog.h -- Task 159: Windows Event Log integration
//
// Writes structured entries to the Windows Application event log so that
// system administrators can monitor AuraCastPro in enterprise environments
// using Event Viewer, PowerShell Get-WinEvent, or SIEM tools.
//
// Log name:   "Application"
// Source:     "AuraCastPro"
//
// Event IDs:
//   1000  Informational -- app start / stop
//   1001  Informational -- device connected / disconnected
//   1002  Warning       -- connection drop / quality degraded
//   1003  Error         -- subsystem init failure
//   1004  Error         -- licence validation failure
//   1005  Warning       -- disk space low (recording)
//
// The event source must be registered once (requires admin rights):
//   scripts/register_dlls.bat calls:
//     reg add HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\AuraCastPro ...
// =============================================================================
#include <string>
#include <cstdint>

namespace aura {

class WindowsEventLog {
public:
    // Register "AuraCastPro" as an event source in the registry.
    // Called once by the installer (scripts/register_dlls.bat).
    // Returns false if registration fails (e.g. insufficient permissions).
    static bool registerSource();

    // Open the event source handle. Call once at app startup.
    // Safe to call if source is not registered -- logs to debugger only.
    void open();
    void close();

    bool isOpen() const { return m_handle != nullptr; }

    // Write an informational entry (Event ID 1000 family)
    void info(uint32_t eventId, const std::string& message);
    // Write a warning entry (Event ID 1002, 1005 family)
    void warning(uint32_t eventId, const std::string& message);
    // Write an error entry (Event ID 1003, 1004 family)
    void error(uint32_t eventId, const std::string& message);

    // Convenience wrappers for the most common events:
    void logAppStart(const std::string& version);
    void logAppStop();
    void logDeviceConnected(const std::string& deviceName, const std::string& ip);
    void logDeviceDisconnected(const std::string& deviceName);
    void logSubsystemError(const std::string& subsystem, const std::string& detail);
    void logLicenceFailure(const std::string& reason);
    void logDiskSpaceLow(uint64_t freeMb, uint64_t requiredMb);

    // Singleton
    static WindowsEventLog& instance();

    WindowsEventLog(const WindowsEventLog&) = delete;
    WindowsEventLog& operator=(const WindowsEventLog&) = delete;

private:
    WindowsEventLog() = default;
    ~WindowsEventLog() { close(); }

    void* m_handle{nullptr};   // HANDLE -- void* avoids including <windows.h> here
};

// Convenience macros (zero-overhead when disabled)
#define AURA_EVTLOG_INFO(id, msg)    ::aura::WindowsEventLog::instance().info(id, msg)
#define AURA_EVTLOG_WARN(id, msg)    ::aura::WindowsEventLog::instance().warning(id, msg)
#define AURA_EVTLOG_ERROR(id, msg)   ::aura::WindowsEventLog::instance().error(id, msg)

} // namespace aura

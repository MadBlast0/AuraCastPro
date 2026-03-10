#pragma once
// =============================================================================
// CrashReporter.h — Task 199: MiniDump crash reporter
// Writes a .dmp file to %APPDATA%\AuraCastPro\crashes\ on unhandled exception.
// =============================================================================
#include <string>
namespace aura {
class CrashReporter {
public:
    // Install the unhandled exception filter. Call from main() before anything else.
    static void install();
    // Write a minidump to the crashes folder.
    // ep may be nullptr (generates a dump without exception context).
    static void writeMiniDump(void* exceptionPointers);  // EXCEPTION_POINTERS*
    static void setAppVersion(const std::string& ver);
    static std::string lastCrashDumpPath();
private:
    static std::string s_appVersion;
    static std::string s_lastDumpPath;
};
} // namespace aura

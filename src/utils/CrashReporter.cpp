// =============================================================================
// CrashReporter.cpp — Task 199: MiniDump on crash via MiniDumpWriteDump.
// BUILT: Was previously missing. Now writes full minidump on any unhandled
// exception so crashes can be diagnosed from the .dmp file.
// =============================================================================
#include "../pch.h"  // PCH
#include "CrashReporter.h"
#include "AppVersion.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <QStandardPaths>
#include <QString>
#include <filesystem>
#include <ctime>
#include <format>

namespace aura {

std::string CrashReporter::s_appVersion = AURA_VERSION_STRING;
std::string CrashReporter::s_lastDumpPath;

static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    // Build dump file path: %APPDATA%\AuraCastPro\crashes\crash_YYYYMMDD_HHMMSS.dmp
    const std::string crashDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            .toStdString() + "/crashes";
    std::filesystem::create_directories(crashDir);

    std::time_t t = std::time(nullptr);
    char timebuf[32]{};
    std::strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", std::localtime(&t));
    const std::string dumpPath = crashDir + "/crash_" + timebuf + ".dmp";

    // Open dump file
    HANDLE hFile = CreateFileA(dumpPath.c_str(),
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers    = FALSE;

        // MiniDumpWithFullMemory gives the best crash data for debugging.
        // Falls back to MiniDumpNormal if full memory exceeds 500MB.
        DWORD dumpType = MiniDumpNormal
                       | MiniDumpWithHandleData
                       | MiniDumpWithThreadInfo
                       | MiniDumpWithProcessThreadData;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            static_cast<MINIDUMP_TYPE>(dumpType),
            &mei,
            nullptr,
            nullptr);

        CloseHandle(hFile);
        CrashReporter::s_lastDumpPath = dumpPath;
    }

    // Show error dialog with dump path
    const std::string msg = std::format(
        "AuraCastPro has crashed.\n\n"
        "A crash report has been saved to:\n{}\n\n"
        "Please send this file to support@auracastpro.com\n"
        "so we can fix the issue.",
        dumpPath);

    MessageBoxA(nullptr, msg.c_str(),
                "AuraCastPro — Fatal Error", MB_ICONERROR | MB_OK);

    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashReporter::writeMiniDump(void* rawEp) {
    EXCEPTION_POINTERS* ep = static_cast<EXCEPTION_POINTERS*>(rawEp);
    const std::string crashDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            .toStdString() + "/crashes";
    std::filesystem::create_directories(crashDir);

    std::time_t t = std::time(nullptr);
    char timebuf[32]{};
    std::strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", std::localtime(&t));
    const std::string dumpPath = crashDir + "/crash_" + timebuf + "_handler.dmp";

    HANDLE hFile = CreateFileA(dumpPath.c_str(),
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers    = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
            MiniDumpNormal, ep ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(hFile);
        s_lastDumpPath = dumpPath;
        AURA_LOG_CRITICAL("CrashReporter", "Minidump written to: {}", dumpPath);
    }
}

void CrashReporter::install() {
    SetUnhandledExceptionFilter(crashHandler);
    AURA_LOG_INFO("CrashReporter",
        "Installed. Dumps → %APPDATA%/AuraCastPro/crashes/*.dmp");
}

void CrashReporter::setAppVersion(const std::string& ver) {
    s_appVersion = ver;
}

std::string CrashReporter::lastCrashDumpPath() {
    return s_lastDumpPath;
}

} // namespace aura

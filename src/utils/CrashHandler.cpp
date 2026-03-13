// =============================================================================
// CrashHandler.cpp -- Windows structured exception + signal handler setup
// Task 208: Registers SEH and SIGABRT handlers so unhandled crashes write a
//           minidump before the process dies.
// =============================================================================
#include "../pch.h"  // PCH
#include "CrashHandler.h"
#include "CrashReporter.h"
#include "Logger.h"
#include <windows.h>
#include <dbghelp.h>
#include <csignal>
#pragma comment(lib, "dbghelp.lib")

namespace aura {

static LONG WINAPI sehHandler(EXCEPTION_POINTERS* ep) {
    AURA_LOG_CRITICAL("CrashHandler", "Unhandled SEH exception 0x{:08X} at 0x{:016X}",
        ep->ExceptionRecord->ExceptionCode,
        reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress));
    CrashReporter::writeMiniDump(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void signalHandler(int sig) {
    AURA_LOG_CRITICAL("CrashHandler", "Signal {} received -- writing minidump", sig);
    CrashReporter::writeMiniDump(nullptr);
    ::ExitProcess(static_cast<UINT>(sig));
}

void CrashHandler::install() {
    ::SetUnhandledExceptionFilter(sehHandler);
    ::signal(SIGABRT, signalHandler);
    ::signal(SIGTERM, signalHandler);
    AURA_LOG_INFO("CrashHandler", "Crash handler installed (SEH + signal).");
}

void CrashHandler::uninstall() {
    ::SetUnhandledExceptionFilter(nullptr);
    ::signal(SIGABRT, SIG_DFL);
    ::signal(SIGTERM, SIG_DFL);
}

} // namespace aura

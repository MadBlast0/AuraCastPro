#include "../pch.h"  // PCH
#include "ADBServerLifecycle.h"
#include "../utils/Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <sstream>

ADBServerLifecycle::ADBServerLifecycle() {
    m_adbPath = findAdbExe();
}

ADBServerLifecycle::~ADBServerLifecycle() {}

bool ADBServerLifecycle::ensureRunning() {
    if (isRunning()) return true;
    return start();
}

bool ADBServerLifecycle::start() {
    LOG_INFO("ADBServerLifecycle: Starting ADB server...");
    for (int attempt = 0; attempt < kMaxRetries; attempt++) {
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            LOG_WARN("ADBServerLifecycle: Retry {} of {}", attempt, kMaxRetries);
        }
        if (runAdb("start-server")) {
            // Verify it actually started
            std::string out;
            if (runAdb("get-state", &out)) {
                m_running   = true;
                m_failCount = 0;
                LOG_INFO("ADBServerLifecycle: ADB server started (state: {})", out);
                return true;
            }
        }
    }
    m_failCount++;
    LOG_ERROR("ADBServerLifecycle: Failed to start ADB server after {} attempts",
              kMaxRetries);
    if (m_onFatal)
        m_onFatal("Failed to start ADB server. "
                  "Please ensure adb.exe is present in the application folder.");
    return false;
}

bool ADBServerLifecycle::stop() {
    LOG_INFO("ADBServerLifecycle: Stopping ADB server");
    bool ok = runAdb("kill-server");
    m_running = false;
    return ok;
}

bool ADBServerLifecycle::runAdb(const std::string& args, std::string* output) {
    if (m_adbPath.empty()) {
        LOG_ERROR("ADBServerLifecycle: adb.exe not found");
        return false;
    }

    std::string cmd = "\"" + m_adbPath + "\" " + args;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;

    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.hStdOutput  = hWrite;
    si.hStdError   = hWrite;
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdCopy = cmd;
    if (!CreateProcessA(nullptr, cmdCopy.data(), nullptr, nullptr,
                        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        return false;
    }
    CloseHandle(hWrite);

    WaitForSingleObject(pi.hProcess, 5000);

    if (output) {
        char buf[256]; DWORD read;
        while (ReadFile(hRead, buf, sizeof(buf)-1, &read, nullptr) && read > 0) {
            buf[read] = 0;
            *output += buf;
        }
        // Trim trailing whitespace
        while (!output->empty() && (output->back() == '\n' || output->back() == '\r'
                                    || output->back() == ' '))
            output->pop_back();
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(hRead);
    return exitCode == 0;
}

std::string ADBServerLifecycle::findAdbExe() {
    // 1. Check next to our exe
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::filesystem::path p(exePath);
    auto candidate = p.parent_path() / "adb.exe";
    if (std::filesystem::exists(candidate)) return candidate.string();

    // 2. Check PATH via where command
    char buf[512] = {};
    FILE* f = _popen("where adb.exe 2>nul", "r");
    if (f) {
        fgets(buf, sizeof(buf), f);
        _pclose(f);
        std::string s(buf);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty() && std::filesystem::exists(s)) return s;
    }

    LOG_WARN("ADBServerLifecycle: adb.exe not found. ADB mirroring will be unavailable.");
    return "";
}

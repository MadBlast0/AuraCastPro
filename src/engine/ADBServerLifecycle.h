#pragma once
#include <string>
#include <functional>
#include <atomic>

// Manages the ADB server process lifecycle.
// ADB server must be running for any ADB operations.
class ADBServerLifecycle {
public:
    ADBServerLifecycle();
    ~ADBServerLifecycle();

    // Check if server is running; start it if not.
    // Returns true if server is ready after this call.
    bool ensureRunning();

    // Explicitly start the ADB server.
    bool start();

    // Explicitly kill the ADB server.
    bool stop();

    // True if adb server responded to get-state within last check
    bool isRunning() const { return m_running.load(); }

    // Path to adb.exe (default: looks in app dir, then PATH)
    void setAdbPath(const std::string& path) { m_adbPath = path; }
    const std::string& adbPath() const { return m_adbPath; }

    // Fires if server crashes and cannot be restarted after maxRetries
    void setOnFatalError(std::function<void(std::string)> cb) {
        m_onFatal = std::move(cb);
    }

private:
    bool runAdb(const std::string& args, std::string* output = nullptr);
    std::string findAdbExe();

    std::string m_adbPath;
    std::atomic<bool> m_running { false };
    int m_failCount = 0;
    static constexpr int kMaxRetries = 3;

    std::function<void(std::string)> m_onFatal;
};

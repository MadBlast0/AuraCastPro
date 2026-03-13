#pragma once
// =============================================================================
// DiskSpaceMonitor.h -- Warns and stops recording when disk is nearly full
// =============================================================================
#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace aura {

class DiskSpaceMonitor {
public:
    using LowDiskCallback = std::function<void(uint64_t freeBytes)>;
    using CriticalCallback = std::function<void()>; // stops recording

    DiskSpaceMonitor();
    ~DiskSpaceMonitor();

    void init();
    void start(const std::string& watchPath);
    void stop();
    void shutdown();

    // Thresholds (bytes)
    void setWarningThreshold(uint64_t bytes) { m_warningBytes = bytes; }
    void setCriticalThreshold(uint64_t bytes){ m_criticalBytes = bytes; }

    void setLowDiskCallback(LowDiskCallback cb)    { m_onLow = std::move(cb); }
    void setCriticalCallback(CriticalCallback cb)   { m_onCritical = std::move(cb); }

    uint64_t freeBytes() const { return m_freeBytes.load(); }
    uint64_t totalBytes() const { return m_totalBytes.load(); }
    double   freePercent() const;

private:
    std::atomic<bool>     m_running{false};
    std::atomic<uint64_t> m_freeBytes{0};
    std::atomic<uint64_t> m_totalBytes{0};
    std::thread           m_thread;
    std::string           m_watchPath;

    uint64_t m_warningBytes{2ULL * 1024 * 1024 * 1024};  // FIXED: 2 GB warning (was 1 GB)
    uint64_t m_criticalBytes{500ULL * 1024 * 1024};       // FIXED: 500 MB stop (was 256 MB)

    LowDiskCallback    m_onLow;
    CriticalCallback   m_onCritical;

    void monitorLoop();
    void updateDiskSpace();
};

} // namespace aura

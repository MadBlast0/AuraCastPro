// DiskSpaceMonitor.cpp
#include "../pch.h"  // PCH
#include "DiskSpaceMonitor.h"
#include "../utils/Logger.h"
#include <filesystem>
#include <chrono>

namespace aura {

DiskSpaceMonitor::DiskSpaceMonitor() {}
DiskSpaceMonitor::~DiskSpaceMonitor() { shutdown(); }

void DiskSpaceMonitor::init() {
    AURA_LOG_INFO("DiskSpaceMonitor",
        "Initialised. Warning at {}GB free, critical stop at {}MB free.",
        m_warningBytes / (1024*1024*1024),
        m_criticalBytes / (1024*1024));
}

void DiskSpaceMonitor::start(const std::string& watchPath) {
    m_watchPath = watchPath;
    try {
        std::filesystem::create_directories(std::filesystem::path(m_watchPath));
    } catch (const std::exception& ex) {
        AURA_LOG_WARN("DiskSpaceMonitor", "Failed to create watch path '{}': {}", m_watchPath, ex.what());
    }
    m_running.store(true);
    updateDiskSpace();
    m_thread = std::thread([this]() { monitorLoop(); });
    AURA_LOG_INFO("DiskSpaceMonitor", "Monitoring: {}", watchPath);
}

void DiskSpaceMonitor::monitorLoop() {
    while (m_running.load()) {
        updateDiskSpace();

        const uint64_t free = m_freeBytes.load();
        if (free < m_criticalBytes) {
            AURA_LOG_ERROR("DiskSpaceMonitor",
                "CRITICAL: Only {}MB free! Stopping recording.", free / (1024*1024));
            if (m_onCritical) m_onCritical();
        } else if (free < m_warningBytes) {
            AURA_LOG_WARN("DiskSpaceMonitor",
                "Low disk space: {}GB free.", free / (1024*1024*1024));
            if (m_onLow) m_onLow(free);
        }

        // Check every 30 seconds (FIXED: was 10s, task spec requires 30s)
        for (int i = 0; i < 300 && m_running.load(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void DiskSpaceMonitor::updateDiskSpace() {
    try {
        const auto space = std::filesystem::space(std::filesystem::path(m_watchPath));
        m_freeBytes.store(space.available);
        m_totalBytes.store(space.capacity);
    } catch (const std::exception& ex) {
        AURA_LOG_WARN("DiskSpaceMonitor", "filesystem::space() failed for '{}': {}", m_watchPath, ex.what());
    } catch (...) {
        AURA_LOG_WARN("DiskSpaceMonitor", "filesystem::space() threw unknown exception");
    }
}

double DiskSpaceMonitor::freePercent() const {
    const uint64_t total = m_totalBytes.load();
    if (total == 0) return 0;
    return (m_freeBytes.load() * 100.0) / total;
}

void DiskSpaceMonitor::stop()     { m_running.store(false); if (m_thread.joinable()) m_thread.join(); }
void DiskSpaceMonitor::shutdown() { stop(); }

} // namespace aura

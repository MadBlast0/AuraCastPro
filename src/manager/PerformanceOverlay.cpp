// =============================================================================
// PerformanceOverlay.cpp — Task 172: In-mirror-window performance stats overlay
// Displays semi-transparent stats panel over the DX12 mirror window.
// Toggled by Ctrl+Shift+O. Data pulled from NetworkStatsModel at 10 Hz.
// CPU% tracked via GetSystemTimes() delta. Memory via GetProcessMemoryInfo().
// =============================================================================
#include "../pch.h"  // PCH
#include "PerformanceOverlay.h"
#include "NetworkStatsModel.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include <QTimer>

namespace aura {

PerformanceOverlay::PerformanceOverlay(NetworkStatsModel* stats, QObject* parent)
    : QObject(parent), m_stats(stats)
{
    m_prevIdleTime = m_prevKernelTime = m_prevUserTime = 0;
}

PerformanceOverlay::~PerformanceOverlay() { shutdown(); }

void PerformanceOverlay::init() {
    AURA_LOG_INFO("PerformanceOverlay",
        "Initialised. Toggle: Ctrl+Shift+O. "
        "Metrics: FPS / Latency / Bitrate / GPU ms / CPU% / Packet loss.");
    snapshotCPUTimes();
}

void PerformanceOverlay::start() {
    if (m_refreshTimer) return;
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(100); // 10 Hz
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (!m_visible.load()) return;
        updateCPUUsage();
        updateMemoryUsage();
        emit overlayDataReady();
    });
    m_refreshTimer->start();
}

void PerformanceOverlay::stop() {
    if (m_refreshTimer) {
        m_refreshTimer->stop();
        m_refreshTimer->deleteLater();
        m_refreshTimer = nullptr;
    }
}

void PerformanceOverlay::shutdown() { stop(); }

void PerformanceOverlay::setVisible(bool v) {
    if (m_visible.exchange(v) == v) return;
    AURA_LOG_DEBUG("PerformanceOverlay", "Overlay {}.", v ? "ON" : "OFF");
    if (v && !m_refreshTimer) start();
    emit visibilityChanged(v);
}

// ── CPU% via GetSystemTimes() delta ──────────────────────────────────────────

void PerformanceOverlay::snapshotCPUTimes() {
    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        m_prevIdleTime   = ftToU64(idle);
        m_prevKernelTime = ftToU64(kernel);
        m_prevUserTime   = ftToU64(user);
    }
}

void PerformanceOverlay::updateCPUUsage() {
    FILETIME idle{}, kernel{}, user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) return;
    uint64_t ci = ftToU64(idle), ck = ftToU64(kernel), cu = ftToU64(user);
    uint64_t dIdle  = ci - m_prevIdleTime;
    uint64_t dBusy  = (ck + cu) - (m_prevKernelTime + m_prevUserTime) - dIdle;
    uint64_t dTotal = (ck + cu) - (m_prevKernelTime + m_prevUserTime);
    if (dTotal > 0)
        m_cpuUsagePct = 0.3 * (100.0 * dBusy / dTotal) + 0.7 * m_cpuUsagePct;
    m_prevIdleTime = ci; m_prevKernelTime = ck; m_prevUserTime = cu;
}

void PerformanceOverlay::updateMemoryUsage() {
    PROCESS_MEMORY_COUNTERS_EX pmc{ sizeof(pmc) };
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        m_workingSetMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
}

uint64_t PerformanceOverlay::ftToU64(const FILETIME& ft) const {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

} // namespace aura

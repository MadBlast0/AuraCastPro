// =============================================================================
// StreamHealthMonitor.cpp -- Real-time stream quality monitor
// Task 210: Tracks packet loss, jitter, decode errors and fires callbacks
//           when stream health degrades below thresholds.
// =============================================================================
#include "../pch.h"  // PCH
#include "StreamHealthMonitor.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <cmath>

namespace aura {

StreamHealthMonitor::StreamHealthMonitor() = default;
StreamHealthMonitor::~StreamHealthMonitor() { stop(); }

void StreamHealthMonitor::init() {
    m_running = false;
    AURA_LOG_INFO("StreamHealthMonitor", "Initialised.");
}

void StreamHealthMonitor::start() {
    m_running = true;
    m_thread = std::thread([this]{ monitorLoop(); });
    AURA_LOG_INFO("StreamHealthMonitor", "Monitoring started.");
}

void StreamHealthMonitor::stop() {
    m_running = false;
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void StreamHealthMonitor::recordPacket(bool lost, uint32_t jitterUs) {
    std::lock_guard lk(m_mtx);
    m_stats.totalPackets++;
    if (lost) m_stats.lostPackets++;
    m_stats.jitterAccum += jitterUs;
    m_stats.jitterSamples++;
}

void StreamHealthMonitor::recordDecodeError() {
    std::lock_guard lk(m_mtx);
    m_stats.decodeErrors++;
}

void StreamHealthMonitor::recordFrameTime(double frameMs) {
    std::lock_guard lk(m_mtx);
    m_stats.frameTimeAccum += frameMs;
    m_stats.frameTimeSamples++;
}

StreamHealthMonitor::Snapshot StreamHealthMonitor::snapshot() const {
    std::lock_guard lk(m_mtx);
    Snapshot s;
    s.lossRate   = m_stats.totalPackets > 0
                     ? static_cast<double>(m_stats.lostPackets) / m_stats.totalPackets
                     : 0.0;
    s.avgJitterUs = m_stats.jitterSamples > 0
                     ? m_stats.jitterAccum / m_stats.jitterSamples
                     : 0;
    s.avgFrameMs  = m_stats.frameTimeSamples > 0
                     ? m_stats.frameTimeAccum / m_stats.frameTimeSamples
                     : 0.0;
    s.decodeErrors = m_stats.decodeErrors;
    return s;
}

void StreamHealthMonitor::setDegradedCallback(std::function<void(Snapshot)> cb) {
    std::lock_guard lk(m_mtx);
    m_degradedCb = std::move(cb);
}

void StreamHealthMonitor::monitorLoop() {
    while (m_running) {
        std::unique_lock lk(m_mtx);
        m_cv.wait_for(lk, std::chrono::seconds(2), [this]{ return !m_running; });
        if (!m_running) break;

        Snapshot s;
        s.lossRate    = m_stats.totalPackets > 0
                          ? static_cast<double>(m_stats.lostPackets) / m_stats.totalPackets : 0.0;
        s.avgJitterUs = m_stats.jitterSamples > 0
                          ? m_stats.jitterAccum / m_stats.jitterSamples : 0;
        s.avgFrameMs  = m_stats.frameTimeSamples > 0
                          ? m_stats.frameTimeAccum / m_stats.frameTimeSamples : 0.0;
        s.decodeErrors = m_stats.decodeErrors;

        // Reset rolling counters
        m_stats = {};

        bool degraded = s.lossRate > 0.05       // > 5% packet loss
                     || s.avgJitterUs > 20'000   // > 20 ms jitter
                     || s.avgFrameMs  > 50.0     // > 50 ms avg frame time
                     || s.decodeErrors > 10;

        lk.unlock();

        if (degraded && m_degradedCb) {
            AURA_LOG_WARN("StreamHealthMonitor",
                "Stream degraded: loss={:.1f}% jitter={}µs frame={:.1f}ms errors={}",
                s.lossRate * 100, s.avgJitterUs, s.avgFrameMs, s.decodeErrors);
            m_degradedCb(s);
        }
    }
}

} // namespace aura

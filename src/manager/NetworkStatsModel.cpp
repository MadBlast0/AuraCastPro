// =============================================================================
// NetworkStatsModel.cpp — FIXED: Full implementation with 9 added properties.
// =============================================================================
#include "../pch.h"  // PCH
#include "NetworkStatsModel.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <cmath>

namespace aura {

NetworkStatsModel::NetworkStatsModel(QObject* parent) : QObject(parent) {}

void NetworkStatsModel::init() {
    m_uiRefreshTimer = new QTimer(this);
    connect(m_uiRefreshTimer, &QTimer::timeout,
            this, &NetworkStatsModel::onUiRefreshTimer);
    AURA_LOG_INFO("NetworkStatsModel",
        "Initialised. 14 Q_PROPERTYs. History: {} samples ({:.0f}s at 10Hz).",
        kHistoryLength, kHistoryLength / 10.0);
}

void NetworkStatsModel::start() {
    if (m_uiRefreshTimer) m_uiRefreshTimer->start(100); // 10 Hz UI refresh
}

void NetworkStatsModel::stop() {
    if (m_uiRefreshTimer) m_uiRefreshTimer->stop();
}

void NetworkStatsModel::shutdown() {
    stop();
}

// Called from network/decoder threads — all atomics, no locks needed
void NetworkStatsModel::updateStats(double latencyMs, double bitrateKbps,
                                     double fps, double packetLossPct,
                                     double gpuFrameMs) {
    m_latencyMs.store(latencyMs);
    m_bitrateKbps.store(bitrateKbps);
    m_fps.store(fps);
    m_packetLossPct.store(packetLossPct);
    m_gpuFrameMs.store(gpuFrameMs);

    // Peak bitrate
    double curPeak = m_peakBitrateKbps.load();
    while (bitrateKbps > curPeak &&
           !m_peakBitrateKbps.compare_exchange_weak(curPeak, bitrateKbps)) {}

    // Min/max latency
    double curMin = m_minLatencyMs.load();
    while (latencyMs < curMin &&
           !m_minLatencyMs.compare_exchange_weak(curMin, latencyMs)) {}
    double curMax = m_maxLatencyMs.load();
    while (latencyMs > curMax &&
           !m_maxLatencyMs.compare_exchange_weak(curMax, latencyMs)) {}

    // Jitter = |current latency - previous latency|
    const double jitter = std::abs(latencyMs - m_prevLatencyMs);
    m_jitterMs.store(jitter * 0.1 + m_jitterMs.load() * 0.9); // EMA smoothed
    m_prevLatencyMs = latencyMs;

    // EMA averages (alpha = 0.05 for slow, stable average)
    m_avgBitrateKbps.store(bitrateKbps * 0.05 + m_avgBitrateKbps.load() * 0.95);
    m_avgLatencyMs.store(latencyMs    * 0.05 + m_avgLatencyMs.load()    * 0.95);

    // Push to history ring buffers (mutex for QList thread safety)
    {
        std::lock_guard<std::mutex> lk(m_historyMutex);
        m_bitrateHistory.append(bitrateKbps);
        m_latencyHistory.append(latencyMs);
        // Keep at exactly kHistoryLength
        while (m_bitrateHistory.size() > kHistoryLength) m_bitrateHistory.removeFirst();
        while (m_latencyHistory.size() > kHistoryLength) m_latencyHistory.removeFirst();
    }
}

void NetworkStatsModel::incrementDroppedFrames() {
    m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
}

void NetworkStatsModel::incrementTotalFrames() {
    m_totalFrames.fetch_add(1, std::memory_order_relaxed);
}

void NetworkStatsModel::resetStats() {
    m_latencyMs.store(0);    m_bitrateKbps.store(0);
    m_fps.store(0);          m_packetLossPct.store(0);
    m_gpuFrameMs.store(0);
    m_avgBitrateKbps.store(0); m_peakBitrateKbps.store(0);
    m_avgLatencyMs.store(0);   m_minLatencyMs.store(9999);
    m_maxLatencyMs.store(0);   m_jitterMs.store(0);
    m_droppedFrames.store(0);  m_totalFrames.store(0);
    std::lock_guard<std::mutex> lk(m_historyMutex);
    m_bitrateHistory.clear();
    m_latencyHistory.clear();
}

QList<double> NetworkStatsModel::bitrateHistory() const {
    std::lock_guard<std::mutex> lk(m_historyMutex);
    return m_bitrateHistory;
}

QList<double> NetworkStatsModel::latencyHistory() const {
    std::lock_guard<std::mutex> lk(m_historyMutex);
    return m_latencyHistory;
}

// Called every 100ms on the UI thread — emit signal so QML updates graphs
void NetworkStatsModel::onUiRefreshTimer() {
    emit statsUpdated();
}

} // namespace aura

#include "NetworkStatsModel.moc"

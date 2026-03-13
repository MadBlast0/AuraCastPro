// =============================================================================
// NetworkPredictor.cpp -- EMA bandwidth predictor
// =============================================================================
#include "../pch.h"  // PCH
#include "NetworkPredictor.h"
#include "../utils/Logger.h"

namespace aura {

NetworkPredictor::NetworkPredictor() {}

void NetworkPredictor::init() {
    AURA_LOG_INFO("NetworkPredictor",
        "Initialised. EMA alpha={:.2f}. "
        "Smooths packet loss, jitter, RTT, bandwidth for BitratePID input.", kAlpha);
}

void NetworkPredictor::feedSample(double lossRate, double jitterMs,
                                   double rttMs, uint64_t bytesReceived,
                                   double intervalSec) {
    const double bwKbps = intervalSec > 0
        ? (bytesReceived * 8.0 / 1000.0) / intervalSec
        : 0.0;

    if (m_firstSample) {
        m_emaLoss.store(lossRate);
        m_emaJitter.store(jitterMs);
        m_emaRtt.store(rttMs);
        m_emaBandwidthKbps.store(bwKbps);
        m_firstSample = false;
        return;
    }

    m_emaLoss.store(ema(m_emaLoss.load(), lossRate, kAlpha));
    m_emaJitter.store(ema(m_emaJitter.load(), jitterMs, kAlpha));
    m_emaRtt.store(ema(m_emaRtt.load(), rttMs, kAlpha));
    m_emaBandwidthKbps.store(ema(m_emaBandwidthKbps.load(), bwKbps, kAlpha));
}

NetworkTelemetry NetworkPredictor::predict() const {
    NetworkTelemetry t;
    t.packetLossPct       = m_emaLoss.load() * 100.0;
    t.jitterMs            = m_emaJitter.load();
    t.rttMs               = m_emaRtt.load();
    t.bandwidthEstKbps    = m_emaBandwidthKbps.load();
    return t;
}

void NetworkPredictor::reset() {
    m_emaLoss.store(0);
    m_emaJitter.store(0);
    m_emaRtt.store(0);
    m_emaBandwidthKbps.store(0);
    m_firstSample = true;
    AURA_LOG_DEBUG("NetworkPredictor", "Reset.");
}

} // namespace aura

// =============================================================================
// RTT self-measurement  [ADDED -- Task 070 gap fix]
// =============================================================================
namespace aura {

uint32_t NetworkPredictor::sendProbe() {
    const uint32_t id = m_nextProbeId.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lk(m_probeMutex);
        m_pendingProbes[id] = std::chrono::steady_clock::now();
        // Discard probes older than 5 seconds to prevent table growth
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_pendingProbes.begin(); it != m_pendingProbes.end(); ) {
            auto age = std::chrono::duration<double, std::milli>(now - it->second).count();
            if (age > 5000.0) it = m_pendingProbes.erase(it);
            else ++it;
        }
    }
    if (m_probeCallback) m_probeCallback(id);
    return id;
}

void NetworkPredictor::onProbeEcho(uint32_t probeId) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(m_probeMutex);
    auto it = m_pendingProbes.find(probeId);
    if (it == m_pendingProbes.end()) return; // already expired

    const double rttMs = std::chrono::duration<double, std::milli>(now - it->second).count();
    m_pendingProbes.erase(it);

    // Feed measured RTT into EMA -- this is the self-measured value
    m_emaRtt.store(ema(m_emaRtt.load(), rttMs, kAlpha));
    AURA_LOG_TRACE("NetworkPredictor",
        "RTT probe {} echo: {:.1f}ms (ema {:.1f}ms)", probeId, rttMs, m_emaRtt.load());
}

} // namespace aura

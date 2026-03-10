// =============================================================================
// NetworkAdaptation.cpp — Dynamic quality adaptation based on network conditions
// Task 212: Adjusts encoder bitrate, resolution, and FEC strength in response
//           to real-time network measurements from NetworkPredictor + BitratePID.
// =============================================================================
#include "NetworkAdaptation.h"
#include "../utils/Logger.h"
#include <algorithm>

namespace aura {

NetworkAdaptation::NetworkAdaptation() = default;

void NetworkAdaptation::init(const Config& cfg) {
    m_cfg = cfg;
    m_currentBitrateKbps = cfg.maxBitrateKbps;
    AURA_LOG_INFO("NetworkAdaptation", "Init: target={}-{}kbps FEC={}",
        cfg.minBitrateKbps, cfg.maxBitrateKbps, cfg.enableFEC);
}

void NetworkAdaptation::update(double estimatedBandwidthKbps, double lossRate, double jitterMs) {
    std::lock_guard lk(m_mtx);

    // Calculate headroom: use 80% of estimated bandwidth
    double targetKbps = estimatedBandwidthKbps * 0.80;
    targetKbps = std::clamp(targetKbps,
        static_cast<double>(m_cfg.minBitrateKbps),
        static_cast<double>(m_cfg.maxBitrateKbps));

    // Loss-based adjustment: reduce more aggressively under loss
    if (lossRate > 0.10) {
        targetKbps *= 0.5;   // severe loss → halve bitrate
    } else if (lossRate > 0.03) {
        targetKbps *= 0.75;  // moderate loss → reduce 25%
    }

    // Smooth: exponential moving average (alpha=0.2 so changes aren't jerky)
    m_currentBitrateKbps = static_cast<uint32_t>(
        0.8 * m_currentBitrateKbps + 0.2 * targetKbps);

    // FEC strength: increase when loss is high
    uint8_t newFEC = 0;
    if (m_cfg.enableFEC) {
        if      (lossRate > 0.10) newFEC = 3;
        else if (lossRate > 0.05) newFEC = 2;
        else if (lossRate > 0.01) newFEC = 1;
    }

    // Resolution downgrade under extreme conditions
    bool shouldDowngrade = (lossRate > 0.15) || (jitterMs > 80.0);

    bool changed = (m_currentBitrateKbps != m_lastReportedKbps)
                || (newFEC != m_fecStrength)
                || (shouldDowngrade != m_downgraded);

    if (changed) {
        m_lastReportedKbps = m_currentBitrateKbps;
        m_fecStrength       = newFEC;
        m_downgraded        = shouldDowngrade;

        AURA_LOG_DEBUG("NetworkAdaptation",
            "Adapt → {}kbps FEC={} downgrade={}  (bw={:.0f} loss={:.1f}% jitter={:.0f}ms)",
            m_currentBitrateKbps, newFEC, shouldDowngrade,
            estimatedBandwidthKbps, lossRate * 100, jitterMs);

        if (m_adaptCallback)
            m_adaptCallback(m_currentBitrateKbps, newFEC, shouldDowngrade);
    }
}

NetworkAdaptation::AdaptState NetworkAdaptation::currentState() const {
    std::lock_guard lk(m_mtx);
    return { m_currentBitrateKbps, m_fecStrength, m_downgraded };
}

void NetworkAdaptation::setAdaptCallback(AdaptCallback cb) {
    std::lock_guard lk(m_mtx);
    m_adaptCallback = std::move(cb);
}

} // namespace aura

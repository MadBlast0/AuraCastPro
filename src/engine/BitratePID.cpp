// =============================================================================
// BitratePID.cpp — PID-based adaptive bitrate controller implementation
// =============================================================================

#include "../pch.h"  // PCH
#include "BitratePID.h"
#include "../utils/Logger.h"

#include <algorithm>
#include <cmath>

namespace aura {

// -----------------------------------------------------------------------------
BitratePID::BitratePID(const Config& config)
    : m_config(config) {
    reset(config.maxBitrateBps * 0.5f); // Start at 50% of max
    m_lastUpdate = std::chrono::steady_clock::now();
}

// -----------------------------------------------------------------------------
void BitratePID::reset(float initialBitrateBps) {
    std::lock_guard lock(m_mutex);
    const float clamped = clamp(initialBitrateBps,
                                 m_config.minBitrateBps,
                                 m_config.maxBitrateBps);
    m_currentBitrate.store(clamped);
    m_prevBitrate = clamped;
    m_integral    = 0.0f;
    m_prevError   = 0.0f;
    AURA_LOG_INFO("BitratePID", "Reset to {:.2f} Kbps", clamped / 1000.0f);
}

// -----------------------------------------------------------------------------
// Error signal: positive error = too much loss (cut bitrate)
//               negative error = less loss than target (can increase)
// -----------------------------------------------------------------------------
float BitratePID::computeError(const PIDTelemetry& t) const {
    return t.packetLossRate - m_config.targetLossRate;
}

// -----------------------------------------------------------------------------
float BitratePID::update(const PIDTelemetry& telemetry) {
    std::lock_guard lock(m_mutex);

    const auto now = std::chrono::steady_clock::now();
    const float dtMs = std::chrono::duration<float, std::milli>(now - m_lastUpdate).count();

    // Enforce minimum update interval to prevent high-frequency oscillation
    if (dtMs < m_config.updateIntervalMs) {
        return m_currentBitrate.load();
    }
    m_lastUpdate = now;

    const float dt = dtMs / 1000.0f; // Convert to seconds for standard PID formulation

    const float error = computeError(telemetry);

    // --- P term: immediate reaction to current error ---
    const float P = m_config.Kp * error;

    // --- I term: accumulated error (anti-windup: clamp integral) ---
    m_integral += error * dt;
    m_integral  = clamp(m_integral, -10.0f, 10.0f); // Anti-windup clamp
    const float I = m_config.Ki * m_integral;

    // --- D term: rate of change of error (dampens oscillation) ---
    const float dError = (error - m_prevError) / dt;
    const float D = m_config.Kd * dError;
    m_prevError = error;

    // Total PID output: positive = reduce bitrate, negative = increase
    const float pidOutput = P + I + D;

    // Emergency cut on critical packet loss (bypass normal PID response)
    float newBitrate;
    if (telemetry.packetLossRate >= m_config.criticalLossRate) {
        // Emergency: cut to 50% immediately
        newBitrate = m_prevBitrate * 0.5f;
        AURA_LOG_WARN("BitratePID", "Critical loss {:.1f}% — emergency cut to {:.0f} Kbps",
                      telemetry.packetLossRate * 100.0f, newBitrate / 1000.0f);
        // Reset integral to prevent windup after emergency cut
        m_integral = 0.0f;
    } else {
        // Normal PID adjustment
        // pidOutput > 0 means loss is above target: reduce bitrate
        // pidOutput < 0 means loss is below target: increase bitrate
        const float adjustment = -pidOutput * m_prevBitrate;

        // Limit recovery rate (increase slowly, cut quickly)
        const float maxIncrease = m_config.maxIncreaseRateBps * dt * 10.0f;
        const float limitedAdj  = clamp(adjustment, -m_prevBitrate, maxIncrease);

        newBitrate = m_prevBitrate + limitedAdj;
    }

    // Bandwidth ceiling: never exceed safety_factor * estimated_bandwidth
    if (telemetry.bandwidthEstBps > 0) {
        const float bwCeiling = telemetry.bandwidthEstBps * m_config.bandwidthSafetyFactor;
        newBitrate = std::min(newBitrate, bwCeiling);
    }

    // Jitter penalty: high jitter suggests the network is unstable;
    // apply additional 5% reduction per 10ms of jitter above threshold
    if (telemetry.jitterMs > 20.0f) {
        const float jitterPenalty = 1.0f - std::min(0.3f, (telemetry.jitterMs - 20.0f) * 0.005f);
        newBitrate *= jitterPenalty;
    }

    // Clamp to configured bounds
    newBitrate = clamp(newBitrate, m_config.minBitrateBps, m_config.maxBitrateBps);

    const float prevBitrate = m_prevBitrate;
    m_prevBitrate = newBitrate;
    m_currentBitrate.store(newBitrate);

    AURA_LOG_TRACE("BitratePID",
        "loss={:.2f}% jitter={:.1f}ms rtt={:.0f}ms "
        "P={:.3f} I={:.3f} D={:.3f} "
        "{:.0f} Kbps → {:.0f} Kbps",
        telemetry.packetLossRate * 100.0f,
        telemetry.jitterMs,
        telemetry.rttMs,
        P, I, D,
        prevBitrate / 1000.0f,
        newBitrate / 1000.0f);

    // Notify callback if bitrate changed by more than 5%
    if (m_onBitrateChanged && std::fabs(newBitrate - prevBitrate) / prevBitrate > 0.05f) {
        m_onBitrateChanged(newBitrate);
    }

    return newBitrate;
}

// -----------------------------------------------------------------------------
void BitratePID::setOnBitrateChanged(BitrateChangedCallback cb) {
    std::lock_guard lock(m_mutex);
    m_onBitrateChanged = std::move(cb);
}

// -----------------------------------------------------------------------------
float BitratePID::clamp(float value, float lo, float hi) const {
    return std::max(lo, std::min(hi, value));
}

} // namespace aura

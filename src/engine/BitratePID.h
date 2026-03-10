#pragma once
// =============================================================================
// BitratePID.h — Adaptive bitrate controller using a PID (Proportional-
//                Integral-Derivative) controller.
//
// The PID controller continuously adjusts the target encoding bitrate based
// on real-time network telemetry:
//   - Packet loss rate     (primary signal)
//   - Network jitter       (derivative term dampening)
//   - Round-trip time      (predictor input)
//   - Bandwidth estimate   (ceiling)
//
// Design goals:
//   - React quickly to sudden loss events (high Kp)
//   - Recover bitrate gradually after conditions improve (slow integral)
//   - Prevent oscillation on unstable links (derivative dampening)
//   - Never exceed estimated available bandwidth
//
// Thread-safety: All public methods are thread-safe.
// =============================================================================

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>

namespace aura {

// Input signal structure for BitratePID controller
struct PIDTelemetry {
    float packetLossRate{};   // 0.0 – 1.0 (fraction of lost packets)
    float jitterMs{};         // Packet arrival jitter in milliseconds
    float rttMs{};            // Round-trip time in milliseconds
    float bandwidthEstBps{};  // Estimated available bandwidth in bits/sec
};

class BitratePID {
public:
    // -----------------------------------------------------------------------
    // Configuration — tunable without recompiling
    // -----------------------------------------------------------------------
    struct Config {
        // PID gains
        float Kp = 0.8f;   // Proportional: how aggressively to cut on loss
        float Ki = 0.05f;  // Integral: corrects sustained under/over-estimate
        float Kd = 0.2f;   // Derivative: dampens oscillation on jittery links

        // Bitrate bounds (bits per second)
        float minBitrateBps  =  500'000.0f;  //  500 Kbps minimum
        float maxBitrateBps  = 50'000'000.0f; // 50 Mbps maximum

        // Loss thresholds
        float targetLossRate  = 0.01f; // 1% — acceptable loss target
        float criticalLossRate = 0.05f; // 5% — triggers emergency cut

        // Update interval
        float updateIntervalMs = 100.0f; // Run controller every 100 ms

        // Recovery speed: increase bitrate by at most this much per interval
        float maxIncreaseRateBps = 500'000.0f; // 500 Kbps per step

        // Safety margin: never exceed this fraction of estimated bandwidth
        float bandwidthSafetyFactor = 0.85f;
    };

    explicit BitratePID(const Config& config = Config{});

    // -----------------------------------------------------------------------
    // Update the controller with fresh telemetry.
    // Returns the new recommended bitrate in bits per second.
    // Call this on the network stats callback (every 100–500 ms).
    // -----------------------------------------------------------------------
    float update(const PIDTelemetry& telemetry);

    // Current recommended bitrate (thread-safe, read anytime)
    float currentBitrateBps() const { return m_currentBitrate.load(); }

    // Force an immediate bitrate value (e.g. on initial connect)
    void reset(float initialBitrateBps);

    // Register a callback invoked whenever the recommended bitrate changes
    // by more than 5%. Callback is called from the update() thread.
    using BitrateChangedCallback = std::function<void(float newBitrateBps)>;
    void setOnBitrateChanged(BitrateChangedCallback cb);

    const Config& config() const { return m_config; }

private:
    Config m_config;

    std::atomic<float> m_currentBitrate{};

    // PID state (protected by m_mutex)
    mutable std::mutex m_mutex;
    float m_integral{};
    float m_prevError{};
    float m_prevBitrate{};

    std::chrono::steady_clock::time_point m_lastUpdate{};

    BitrateChangedCallback m_onBitrateChanged;

    float clamp(float value, float lo, float hi) const;
    float computeError(const PIDTelemetry& t) const;
};

} // namespace aura

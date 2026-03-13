#pragma once
// =============================================================================
// NetworkPredictor.h -- Bandwidth, RTT & jitter estimator.
// FIXED (Task 070 gap): Added RTT self-measurement via timestamp embedding.
//   sendProbe() embeds current timestamp into outgoing control messages.
//   onProbeEcho() measures round-trip when the echo comes back.
//   The measured RTT feeds into feedSample() automatically.
// =============================================================================
#include <cstdint>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace aura {

struct NetworkTelemetry {
    double packetLossPct{0};
    double jitterMs{0};
    double rttMs{0};
    double bandwidthEstKbps{0};
};

class NetworkPredictor {
public:
    NetworkPredictor();

    void init();

    // Feed a new measurement. Call every 100-500ms from the stats thread.
    void feedSample(double lossRate, double jitterMs, double rttMs,
                    uint64_t bytesReceived, double intervalSec);

    // ── RTT self-measurement [ADDED -- Task 070 gap fix] ──────────────────────
    // Call sendProbe() when sending a control message to the device.
    // Returns a probe ID to embed in the message payload.
    uint32_t sendProbe();

    // Call onProbeEcho() when an echo/ACK message with probeId arrives back.
    // Internally measures RTT and feeds it into the EMA.
    void onProbeEcho(uint32_t probeId);

    // Callback for callers that need to send the probe bytes over the wire.
    // signature: void(uint32_t probeId)
    using ProbeCallback = std::function<void(uint32_t probeId)>;
    void setProbeCallback(ProbeCallback cb) { m_probeCallback = cb; }

    NetworkTelemetry predict() const;
    void reset();

private:
    static constexpr double kAlpha = 0.1;

    std::atomic<double> m_emaLoss{0};
    std::atomic<double> m_emaJitter{0};
    std::atomic<double> m_emaRtt{0};
    std::atomic<double> m_emaBandwidthKbps{0};
    bool   m_firstSample{true};

    // RTT probe table: probeId -> send timestamp
    std::mutex m_probeMutex;
    std::unordered_map<uint32_t,
        std::chrono::steady_clock::time_point> m_pendingProbes;
    std::atomic<uint32_t> m_nextProbeId{1};
    ProbeCallback m_probeCallback;

    static double ema(double prev, double newVal, double alpha) {
        return alpha * newVal + (1.0 - alpha) * prev;
    }
};

} // namespace aura

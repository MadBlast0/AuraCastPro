#pragma once
#include <functional>
#include <mutex>
#include <cstdint>

namespace aura {
class NetworkAdaptation {
public:
    struct Config {
        uint32_t minBitrateKbps{500};
        uint32_t maxBitrateKbps{20'000};
        bool     enableFEC{true};
    };
    struct AdaptState {
        uint32_t bitrateKbps;
        uint8_t  fecStrength;  // 0=off, 1-3 = repair packets per group
        bool     downgraded;   // resolution downgrade active
    };
    using AdaptCallback = std::function<void(uint32_t bitrateKbps, uint8_t fec, bool downgrade)>;

    NetworkAdaptation();
    void init(const Config& cfg = {});
    void update(double estimatedBandwidthKbps, double lossRate, double jitterMs);
    AdaptState currentState() const;
    void setAdaptCallback(AdaptCallback cb);

private:
    mutable std::mutex m_mtx;
    Config   m_cfg;
    uint32_t m_currentBitrateKbps{10'000};
    uint32_t m_lastReportedKbps{0};
    uint8_t  m_fecStrength{0};
    bool     m_downgraded{false};
    AdaptCallback m_adaptCallback;
};
} // namespace aura

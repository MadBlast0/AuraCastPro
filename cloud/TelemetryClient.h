#pragma once
// =============================================================================
// TelemetryClient.h -- Anonymous opt-in crash/usage telemetry
// =============================================================================
#include <string>
#include <memory>
#include <atomic>

namespace aura {

class TelemetryClient {
public:
    TelemetryClient();
    ~TelemetryClient();

    void init();
    void shutdown();

    void setEnabled(bool enabled) { m_enabled.store(enabled); }
    bool isEnabled() const { return m_enabled.load(); }

    // Report app startup (OS version, GPU, app version)
    void reportStartup(const std::string& gpuName, const std::string& osVersion);

    // Report session statistics (duration, codec, resolution -- NO screen content)
    void reportSession(double durationSec, const std::string& codec,
                       uint32_t width, uint32_t height, double avgBitrateKbps);

    // Report a crash (stack trace from CrashHandler)
    void reportCrash(const std::string& stackTrace);

private:
    std::atomic<bool> m_enabled{false}; // disabled by default
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    void sendAsync(const std::string& endpoint, const std::string& payload);
};

} // namespace aura

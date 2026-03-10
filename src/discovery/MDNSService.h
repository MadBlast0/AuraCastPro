#pragma once
#include <vector>
#include <mutex>
// =============================================================================
// MDNSService.h — Zeroconf mDNS service registration via Apple Bonjour SDK
//
// Registers AuraCastPro on the local network so iPhones and Android phones
// can see it as a screen mirroring target — same way Apple TV appears on
// an iPhone's AirPlay list.
//
// Two registrations:
//   _airplay._tcp   port 7236  → iPhones see "AuraCastPro" in Screen Mirroring
//   _googlecast._tcp port 8009 → Android sees it in Cast targets
// =============================================================================
#pragma once
#include <vector>
#include <mutex>
#include <string>
#include <functional>
#include <memory>
#include <atomic>

namespace aura {

struct MDNSRecord {
    std::string serviceType;   // e.g. "_airplay._tcp"
    std::string serviceName;   // e.g. "AuraCastPro"
    uint16_t    port;
    std::string txtRecord;     // Key=Value pairs separated by \n
};

class MDNSService {
public:
    explicit MDNSService();
    ~MDNSService();

    void init();
    void start();
    void stop();
    void shutdown();

    // Update the device name shown on phones (e.g. after user renames in settings)
    void setDisplayName(const std::string& name);

    bool isRunning() const { return m_running.load(); }

private:
    std::string m_displayName{"AuraCastPro"};
    std::atomic<bool> m_running{false};
    std::vector<MDNSRecord> m_extraRecords;
    mutable std::mutex m_mutex;

    struct BonjourState;
    std::unique_ptr<BonjourState> m_bonjour;

    void registerService(const MDNSRecord& record);
    void buildAirPlayTxtRecord(std::string& out) const;
    void buildCastTxtRecord(std::string& out) const;
};

} // namespace aura

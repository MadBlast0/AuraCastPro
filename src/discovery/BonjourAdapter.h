#pragma once
// =============================================================================
// BonjourAdapter.h — Apple Bonjour SDK wrapper for mDNS registration/browsing
// =============================================================================
#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace aura {

struct BonjourService {
    std::string type;       // "_airplay._tcp"
    std::string name;       // "AuraCastPro"
    std::string host;       // hostname
    std::string ip;         // resolved IP
    uint16_t    port{0};
    std::string txtRecord;
};

class BonjourAdapter {
public:
    using ServiceFoundCallback = std::function<void(const BonjourService&)>;
    using ServiceLostCallback  = std::function<void(const std::string& name,
                                                     const std::string& type)>;

    BonjourAdapter();
    ~BonjourAdapter();

    void init();
    void start();
    void stop();
    void shutdown();

    // Register this machine as a service
    bool registerService(const std::string& type, const std::string& name,
                         uint16_t port, const std::string& txtRecord);
    void unregisterAll();

    // Browse for services on the network
    void browseForService(const std::string& type,
                          ServiceFoundCallback onFound,
                          ServiceLostCallback  onLost);

    bool isAvailable() const { return m_available; }

private:
    bool m_available{false};
    bool m_running{false};

    struct BonjourState;
    std::unique_ptr<BonjourState> m_state;
};

} // namespace aura

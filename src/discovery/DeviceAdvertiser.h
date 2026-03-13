#pragma once
// =============================================================================
// DeviceAdvertiser.h -- Broadcasts AuraCastPro on local network
// =============================================================================
#include <string>
#include <memory>

namespace aura {
class BonjourAdapter;

class DeviceAdvertiser {
public:
    explicit DeviceAdvertiser(BonjourAdapter* bonjour);
    ~DeviceAdvertiser();

    void init();
    void start();
    void stop();
    void shutdown();

    void setDisplayName(const std::string& name);

private:
    BonjourAdapter* m_bonjour;
    std::string     m_displayName{"AuraCastPro"};
    bool            m_running{false};
};
} // namespace aura

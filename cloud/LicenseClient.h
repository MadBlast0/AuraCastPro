#pragma once
// =============================================================================
// LicenseClient.h — Online license activation and subscription heartbeat
// =============================================================================
#include <string>
#include <functional>
#include <memory>

namespace aura {

enum class ActivationResult { Success, InvalidKey, AlreadyActivated, NetworkError, ServerError };

struct ActivationResponse {
    ActivationResult result{ActivationResult::NetworkError};
    std::string      tier;
    std::string      email;
    std::string      expiryDate;
    std::string      message; // human-readable result
};

class LicenseClient {
public:
    LicenseClient();
    ~LicenseClient();

    void init();
    void shutdown();

    // Activate a license key online (async)
    void activate(const std::string& key, const std::string& email,
                  std::function<void(ActivationResponse)> callback);

    // Deactivate (for machine transfer) — synchronous
    bool deactivate(const std::string& key);

    // Send heartbeat for subscription licenses (async, silent)
    void sendHeartbeat(const std::string& key);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string httpsPost(const std::string& endpoint, const std::string& body) const;
    static std::string machineId();
};

} // namespace aura

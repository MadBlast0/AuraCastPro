// =============================================================================
// BonjourAdapter.cpp -- Bonjour SDK API wrapper
// =============================================================================
#include "../pch.h"  // PCH
#include "BonjourAdapter.h"
#include "../utils/Logger.h"
#include "BonjourApi.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

namespace aura {

struct BonjourAdapter::BonjourState {
    std::vector<bonjour::DNSServiceRef> registrations;
};

BonjourAdapter::BonjourAdapter() : m_state(std::make_unique<BonjourState>()) {}
BonjourAdapter::~BonjourAdapter() { shutdown(); }

void BonjourAdapter::init() {
    m_available = bonjour::isAvailable();
    if (m_available) {
    AURA_LOG_INFO("BonjourAdapter",
        "Bonjour runtime available via {}. Service registration ready.",
        bonjour::libraryPath());
    } else {
    AURA_LOG_WARN("BonjourAdapter",
        "Bonjour runtime not found. "
        "Install Bonjour for Windows to enable network service discovery.");
    }
}

void BonjourAdapter::start()    { m_running = true; }
void BonjourAdapter::stop()     { m_running = false; unregisterAll(); }
void BonjourAdapter::shutdown() { stop(); }

bool BonjourAdapter::registerService(const std::string& type,
                                      const std::string& name,
                                      uint16_t port,
                                      const std::string& txtRecord) {
    if (!m_available) return false;

    // Build TXT record binary blob
    std::vector<uint8_t> txt;
    std::istringstream ss(txtRecord);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) {
            txt.push_back(static_cast<uint8_t>(line.size()));
            txt.insert(txt.end(), line.begin(), line.end());
        }
    }

    bonjour::DNSServiceRef ref = nullptr;
    const auto err = bonjour::registerService(
        &ref, 0, bonjour::kDNSServiceInterfaceIndexAny,
        name.c_str(), type.c_str(),
        nullptr, nullptr, htons(port),
        static_cast<uint16_t>(txt.size()),
        txt.empty() ? nullptr : txt.data());

    if (err == bonjour::kDNSServiceErr_NoError) {
        m_state->registrations.push_back(ref);
        AURA_LOG_INFO("BonjourAdapter",
            "Registered: '{}' as {} on port {}", name, type, port);
        return true;
    }
    AURA_LOG_ERROR("BonjourAdapter", "DNSServiceRegister failed: {}", err);
    return false;
}

void BonjourAdapter::unregisterAll() {
    for (auto ref : m_state->registrations) {
        bonjour::deallocate(ref);
    }
    m_state->registrations.clear();
}

void BonjourAdapter::browseForService(const std::string& type,
                                       ServiceFoundCallback onFound,
                                       ServiceLostCallback  onLost) {
    if (!m_available) return;

    (void)onFound; (void)onLost;
    AURA_LOG_WARN("BonjourAdapter",
        "Browsing for {} is not implemented in the runtime Bonjour path yet.",
        type);
}

} // namespace aura

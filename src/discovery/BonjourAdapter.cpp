// =============================================================================
// BonjourAdapter.cpp — Bonjour SDK API wrapper
// =============================================================================
#include "BonjourAdapter.h"
#include "../utils/Logger.h"

#if __has_include(<dns_sd.h>)
#  include <dns_sd.h>
#  define BONJOUR_SDK 1
#else
#  define BONJOUR_SDK 0
#endif

namespace aura {

struct BonjourAdapter::BonjourState {
#if BONJOUR_SDK
    std::vector<DNSServiceRef> registrations;
    struct BrowserEntry {
        DNSServiceRef ref;
        void*         ctx;  // BrowseCtx* — owned here, freed in unregisterAll
    };
    std::vector<BrowserEntry> browsers;
#endif
};

BonjourAdapter::BonjourAdapter() : m_state(std::make_unique<BonjourState>()) {}
BonjourAdapter::~BonjourAdapter() { shutdown(); }

void BonjourAdapter::init() {
#if BONJOUR_SDK
    m_available = true;
    AURA_LOG_INFO("BonjourAdapter",
        "Bonjour SDK available. DNSServiceRegister/Browse ready.");
#else
    m_available = false;
    AURA_LOG_WARN("BonjourAdapter",
        "Bonjour SDK not found. "
        "Install from https://developer.apple.com/bonjour/ "
        "then rebuild. mDNS device discovery disabled.");
#endif
}

void BonjourAdapter::start()    { m_running = true; }
void BonjourAdapter::stop()     { m_running = false; unregisterAll(); }
void BonjourAdapter::shutdown() { stop(); }

bool BonjourAdapter::registerService(const std::string& type,
                                      const std::string& name,
                                      uint16_t port,
                                      const std::string& txtRecord) {
    if (!m_available) return false;

#if BONJOUR_SDK
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

    DNSServiceRef ref = nullptr;
    DNSServiceErrorType err = DNSServiceRegister(
        &ref, 0, kDNSServiceInterfaceIndexAny,
        name.c_str(), type.c_str(),
        nullptr, nullptr, htons(port),
        static_cast<uint16_t>(txt.size()),
        txt.empty() ? nullptr : txt.data(),
        nullptr, nullptr);

    if (err == kDNSServiceErr_NoError) {
        m_state->registrations.push_back(ref);
        AURA_LOG_INFO("BonjourAdapter",
            "Registered: '{}' as {} on port {}", name, type, port);
        return true;
    }
    AURA_LOG_ERROR("BonjourAdapter", "DNSServiceRegister failed: {}", err);
#endif
    return false;
}

void BonjourAdapter::unregisterAll() {
#if BONJOUR_SDK
    for (auto ref : m_state->registrations) DNSServiceRefDeallocate(ref);
    m_state->registrations.clear();
    for (auto& entry : m_state->browsers) {
        DNSServiceRefDeallocate(entry.ref);
        delete static_cast<BrowseCtx*>(entry.ctx);  // free callback context
    }
    m_state->browsers.clear();
#endif
}

void BonjourAdapter::browseForService(const std::string& type,
                                       ServiceFoundCallback onFound,
                                       ServiceLostCallback  onLost) {
    if (!m_available) return;

    AURA_LOG_INFO("BonjourAdapter", "Browsing for {} services...", type);

#if BONJOUR_SDK
    // Heap-allocate context so it outlives this stack frame.
    // Ownership: BonjourAdapter owns callbacks struct; cleaned up in unregisterAll.
    struct BrowseCtx {
        ServiceFoundCallback onFound;
        ServiceLostCallback  onLost;
    };
    auto* ctx = new BrowseCtx{ std::move(onFound), std::move(onLost) };

    // Callback fired when a service is found or removed on the network.
    auto browseCB = [](DNSServiceRef /*sd*/, DNSServiceFlags flags,
                       uint32_t ifIndex, DNSServiceErrorType err,
                       const char* name, const char* regtype,
                       const char* domain, void* context) {
        auto* c = static_cast<BrowseCtx*>(context);
        if (err != kDNSServiceErr_NoError) return;

        if (flags & kDNSServiceFlagsAdd) {
            // Resolve the service to get host + port + TXT record
            struct ResolveCtx {
                ServiceFoundCallback onFound;
                std::string          svcName;
                std::string          svcType;
            };
            auto* rc = new ResolveCtx{ c->onFound, name, regtype };

            auto resolveCB = [](DNSServiceRef sd, DNSServiceFlags /*flags*/,
                                 uint32_t /*ifIndex*/, DNSServiceErrorType /*err*/,
                                 const char* fullname, const char* hosttarget,
                                 uint16_t port, uint16_t txtLen,
                                 const unsigned char* txtRecord, void* context) {
                auto* rc = static_cast<ResolveCtx*>(context);
                BonjourService svc;
                svc.name = rc->svcName;
                svc.type = rc->svcType;
                svc.host = hosttarget;
                svc.port = ntohs(port);
                if (txtLen > 0)
                    svc.txtRecord.assign(
                        reinterpret_cast<const char*>(txtRecord), txtLen);
                if (rc->onFound) rc->onFound(svc);
                delete rc;
                DNSServiceRefDeallocate(sd); // one-shot
            };

            DNSServiceRef resolveRef = nullptr;
            DNSServiceResolve(&resolveRef,
                              kDNSServiceFlagsShareConnection,
                              ifIndex, name, regtype, domain,
                              resolveCB, rc);
            // Note: resolveRef not stored — fires once then is deallocated in cb
        } else {
            // Service removed
            if (c->onLost) c->onLost(name, regtype);
        }
    };

    DNSServiceRef browseRef = nullptr;
    DNSServiceErrorType err = DNSServiceBrowse(
        &browseRef, 0, kDNSServiceInterfaceIndexAny,
        type.c_str(), nullptr,
        browseCB, ctx);

    if (err == kDNSServiceErr_NoError) {
        m_state->browsers.push_back({browseRef, ctx});
        AURA_LOG_INFO("BonjourAdapter",
            "DNSServiceBrowse started for {} — {} active browsers.",
            type, m_state->browsers.size());
    } else {
        AURA_LOG_ERROR("BonjourAdapter", "DNSServiceBrowse failed: {}", err);
        delete ctx;
    }
#else
    (void)onFound; (void)onLost;
    AURA_LOG_WARN("BonjourAdapter", "Bonjour SDK not available — browse skipped.");
#endif
}

} // namespace aura

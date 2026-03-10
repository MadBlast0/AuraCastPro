// =============================================================================
// MDNSService.cpp — mDNS/Bonjour service registration
//
// Uses Apple's dns_sd.h API (Bonjour for Windows) to register services.
// The Bonjour SDK must be installed from:
//   developer.apple.com/bonjour/index.html
//
// TXT record values for AirPlay (required by iOS devices):
//   deviceid  = MAC address of this PC (used as unique identifier)
//   features  = 0x5A7FFFF7,0x1E  (bitmask enabling H.265, 4K, audio features)
//   srcvers   = 220.68            (AirPlay source version)
//   pk        = <ed25519 public key hex> (pairing public key)
//   pi        = <UUID>            (persistent identifier)
//
// TXT record for Google Cast:
//   id        = <device UUID>
//   md        = AuraCastPro
//   ve        = 05
//   ic        = /setup/icon.png
//   fn        = AuraCastPro
//   ca        = 4101
//   st        = 0
//   bs        = <MAC>
//   nf        = 1
//   rs        = (empty)
// =============================================================================

#include "MDNSService.h"
#include "BonjourAdapter.h"
#include "../utils/Logger.h"
#include "../utils/NetworkTools.h"

#include <format>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>

// OpenSSL for Ed25519 keypair generation + stable UUID
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Conditionally include Bonjour SDK
#if __has_include(<dns_sd.h>)
#  include <dns_sd.h>
#  define BONJOUR_AVAILABLE 1
#else
#  define BONJOUR_AVAILABLE 0
#endif

namespace aura {

struct MDNSService::BonjourState {
#if BONJOUR_AVAILABLE
    DNSServiceRef airplayRef{nullptr};
    DNSServiceRef castRef{nullptr};
#endif
    bool bonjourAvailable{false};
};

// -----------------------------------------------------------------------------
MDNSService::MDNSService()
    : m_bonjour(std::make_unique<BonjourState>()) {}

MDNSService::~MDNSService() { shutdown(); }

// -----------------------------------------------------------------------------
// Generate or load a stable Ed25519 keypair from AppData.
// The public key goes into the AirPlay TXT record (pk= field).
// The persistent identifier (pi=) is a random UUID stored alongside it.
// -----------------------------------------------------------------------------
static std::pair<std::string, std::string> getOrCreateAirPlayIdentity() {
    // Paths
    char appdata[MAX_PATH]{};
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    const std::filesystem::path dir = std::filesystem::path(appdata) / "AuraCastPro";
    std::filesystem::create_directories(dir);

    const auto pkFile  = dir / "airplay_pubkey.hex";
    const auto piFile  = dir / "airplay_uuid.txt";

    std::string pkHex, piUUID;

    // Try to load existing values
    if (std::filesystem::exists(pkFile) && std::filesystem::exists(piFile)) {
        std::ifstream pk(pkFile); pk >> pkHex;
        std::ifstream pi(piFile); pi >> piUUID;
        if (pkHex.size() == 64 && piUUID.size() == 36) {
            return { pkHex, piUUID };
        }
    }

    // Generate Ed25519 keypair
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (ctx) {
        EVP_PKEY_keygen_init(ctx);
        EVP_PKEY_keygen(ctx, &pkey);
        EVP_PKEY_CTX_free(ctx);
    }

    if (pkey) {
        // Extract 32-byte raw public key
        size_t rawLen = 32;
        uint8_t rawPub[32]{};
        EVP_PKEY_get_raw_public_key(pkey, rawPub, &rawLen);
        EVP_PKEY_free(pkey);

        // Encode as 64-char hex string
        std::ostringstream oss;
        for (uint8_t b : rawPub)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        pkHex = oss.str();
    } else {
        // Fallback: random 32-byte key
        uint8_t rnd[32]{};
        RAND_bytes(rnd, 32);
        std::ostringstream oss;
        for (uint8_t b : rnd)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        pkHex = oss.str();
    }

    // Generate random UUID v4 for pi=
    uint8_t uuid[16]{};
    RAND_bytes(uuid, 16);
    uuid[6] = (uuid[6] & 0x0F) | 0x40; // version 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // variant bits
    piUUID = std::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9],
        uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    // Persist
    { std::ofstream pk(pkFile);  pk << pkHex; }
    { std::ofstream pi(piFile);  pi << piUUID; }

    AURA_LOG_INFO("MDNSService", "Generated AirPlay Ed25519 identity. pk={:.8s}... pi={}",
        pkHex, piUUID);

    return { pkHex, piUUID };
}

// -----------------------------------------------------------------------------
void MDNSService::init() {
#if BONJOUR_AVAILABLE
    m_bonjour->bonjourAvailable = true;
    AURA_LOG_INFO("MDNSService", "Bonjour SDK found. mDNS registration available.");
#else
    m_bonjour->bonjourAvailable = false;
    AURA_LOG_WARN("MDNSService",
        "Bonjour SDK not found (dns_sd.h missing). "
        "mDNS registration disabled — devices will not discover AuraCastPro automatically. "
        "Install Bonjour SDK from developer.apple.com/bonjour");
#endif
}

// -----------------------------------------------------------------------------
void MDNSService::buildAirPlayTxtRecord(std::string& out) const {
    // Features bitmask: 0x5A7FFFF7,0x1E enables:
    //   - H.265 video (bit 11)
    //   - 4K resolution (bit 30)
    //   - Audio: AAC-ELD, ALAC
    //   - Screen mirroring
    //   - Audio buffering
    std::string mac = NetworkTools::bestLocalIPv4(); // Will use MAC in production

    // Load or generate stable Ed25519 identity
    auto [pkHex, piUUID] = getOrCreateAirPlayIdentity();

    std::ostringstream txt;
    txt << "deviceid=" << mac << "\n";
    txt << "features=0x5A7FFFF7,0x1E\n";
    txt << "srcvers=220.68\n";
    txt << "flags=0x4\n";
    txt << "model=AppleTV3,2\n";    // Pretend to be Apple TV for best compatibility
    txt << "pi=" << piUUID << "\n"; // Stable persistent UUID (generated once, saved)
    txt << "pk=" << pkHex  << "\n"; // Ed25519 public key (64 hex chars = 32 bytes)
    out = txt.str();
}

// -----------------------------------------------------------------------------
void MDNSService::buildCastTxtRecord(std::string& out) const {
    std::ostringstream txt;
    txt << "id=auracastpro0001\n";
    txt << "md=AuraCastPro\n";
    txt << "ve=05\n";
    txt << "ic=/setup/icon.png\n";
    txt << "fn=" << m_displayName << "\n";
    txt << "ca=4101\n";   // Capabilities: 4096 (video) + 4 (audio) + 1 (multizone)
    txt << "st=0\n";      // Status: idle

    // Use the primary network interface MAC address for the 'bs' field.
    // AirPlay receivers are identified by MAC — using the real MAC prevents
    // device-pairing mismatches when multiple AuraCastPro installs are on LAN.
    std::string mac = "FA8FCA000000"; // fallback: locally-administered unicast
    {
        auto ifaces = NetworkTools::enumerateInterfaces(false);
        if (!ifaces.empty() && !ifaces.front().macAddress.empty()) {
            // Strip colons: "AA:BB:CC:DD:EE:FF" → "AABBCCDDEEFF"
            mac.clear();
            for (char c : ifaces.front().macAddress)
                if (c != ':') mac += c;
        }
    }
    txt << "bs=" << mac << "\n";

    txt << "nf=1\n";
    txt << "rs=\n";
    out = txt.str();
}

// -----------------------------------------------------------------------------
void MDNSService::start() {
    if (m_running.exchange(true)) return;

    if (!m_bonjour->bonjourAvailable) {
        AURA_LOG_WARN("MDNSService", "Cannot start — Bonjour not available.");
        return;
    }

#if BONJOUR_AVAILABLE
    // ── AirPlay registration ────────────────────────────────────────────────
    {
        std::string txt;
        buildAirPlayTxtRecord(txt);

        // Convert newline-separated K=V string to DNS TXT record format
        // (each entry prefixed by its length byte)
        std::vector<uint8_t> txtData;
        std::istringstream ss(txt);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) {
                txtData.push_back(static_cast<uint8_t>(line.size()));
                txtData.insert(txtData.end(), line.begin(), line.end());
            }
        }

        DNSServiceErrorType err = DNSServiceRegister(
            &m_bonjour->airplayRef,
            0,                              // flags
            kDNSServiceInterfaceIndexAny,   // all network interfaces
            m_displayName.c_str(),          // service instance name
            "_airplay._tcp",               // service type
            nullptr,                        // domain (default)
            nullptr,                        // host (default = this machine)
            htons(7236),                    // port
            static_cast<uint16_t>(txtData.size()),
            txtData.data(),
            nullptr,                        // callback (async notification)
            nullptr                         // context
        );

        if (err == kDNSServiceErr_NoError) {
            AURA_LOG_INFO("MDNSService",
                "AirPlay service registered: '{}' on _airplay._tcp port 7236.",
                m_displayName);
        } else {
            AURA_LOG_ERROR("MDNSService",
                "AirPlay registration failed: DNSServiceRegister error {}", err);
        }
    }

    // ── Google Cast registration ────────────────────────────────────────────
    {
        std::string txt;
        buildCastTxtRecord(txt);

        std::vector<uint8_t> txtData;
        std::istringstream ss(txt);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) {
                txtData.push_back(static_cast<uint8_t>(line.size()));
                txtData.insert(txtData.end(), line.begin(), line.end());
            }
        }

        DNSServiceErrorType err = DNSServiceRegister(
            &m_bonjour->castRef,
            0,
            kDNSServiceInterfaceIndexAny,
            m_displayName.c_str(),
            "_googlecast._tcp",
            nullptr,
            nullptr,
            htons(8009),
            static_cast<uint16_t>(txtData.size()),
            txtData.data(),
            nullptr,
            nullptr
        );

        if (err == kDNSServiceErr_NoError) {
            AURA_LOG_INFO("MDNSService",
                "Cast service registered: '{}' on _googlecast._tcp port 8009.",
                m_displayName);
        } else {
            AURA_LOG_ERROR("MDNSService",
                "Cast registration failed: DNSServiceRegister error {}", err);
        }
    }

    AURA_LOG_INFO("MDNSService",
        "Broadcasting. iPhones should now see '{}' in Screen Mirroring. "
        "Android devices should see it in Cast targets.", m_displayName);
#endif
}

// -----------------------------------------------------------------------------
void MDNSService::stop() {
    if (!m_running.exchange(false)) return;
#if BONJOUR_AVAILABLE
    if (m_bonjour->airplayRef) {
        DNSServiceRefDeallocate(m_bonjour->airplayRef);
        m_bonjour->airplayRef = nullptr;
    }
    if (m_bonjour->castRef) {
        DNSServiceRefDeallocate(m_bonjour->castRef);
        m_bonjour->castRef = nullptr;
    }
#endif
    AURA_LOG_INFO("MDNSService", "Services unregistered.");
}

// -----------------------------------------------------------------------------
void MDNSService::shutdown() { stop(); }

// -----------------------------------------------------------------------------
void MDNSService::setDisplayName(const std::string& name) {
    m_displayName = name;
    if (m_running.load()) {
        // Re-register with new name
        stop();
        start();
    }
}


void MDNSService::registerService(const MDNSRecord& record) {
    // Add to the local registry — will be announced on next mDNS send cycle
    std::lock_guard<std::mutex> lk(m_mutex);
    // Replace existing record with same name+type, or append
    for (auto& r : m_extraRecords) {
        if (r.name == record.name && r.type == record.type) {
            r = record;
            AURA_LOG_DEBUG("MDNSService", "Updated mDNS record: {} type={}", 
                           record.name, (int)record.type);
            return;
        }
    }
    m_extraRecords.push_back(record);
    AURA_LOG_INFO("MDNSService", "Registered mDNS record: {} type={}",
                  record.name, (int)record.type);
}

} // namespace aura

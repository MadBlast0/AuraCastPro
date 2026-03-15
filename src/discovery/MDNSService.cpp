// =============================================================================
// MDNSService.cpp -- In-process mDNS responder for AirPlay / Cast discovery
// =============================================================================

#include "../pch.h"  // PCH
#include "MDNSService.h"
#include "../utils/Logger.h"
#include "../utils/NetworkTools.h"

#include <format>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <array>
#include <cctype>

// OpenSSL for Ed25519 keypair generation + stable UUID
#include <openssl/evp.h>
#include <openssl/rand.h>
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

namespace aura {

namespace {
constexpr const char* kAirPlayFeatures = "0x5A7FFFF7,0x1E";
constexpr const char* kAirPlayFlags    = "0x84";
constexpr const char* kAirPlayVV       = "2";
constexpr const char* kRaopTxtVers     = "1";
constexpr const char* kRaopChannels    = "2";
constexpr const char* kRaopCodecs      = "0,1,2,3";
constexpr const char* kRaopEncryption  = "0";
constexpr const char* kRaopRhd         = "5.6.0.0";
constexpr const char* kRaopSf          = "0x4";
constexpr const char* kRaopSv          = "false";
constexpr const char* kRaopDa          = "true";
constexpr const char* kRaopSr          = "44100";
constexpr const char* kRaopSs          = "16";
constexpr const char* kRaopTp          = "UDP";
constexpr const char* kRaopMd          = "0,1,2";
constexpr const char* kRaopVn          = "65537";
constexpr uint16_t kMDNSPort           = 5353;
constexpr uint32_t kServiceTTL         = 120;
constexpr uint32_t kTextTTL            = 4500;
constexpr uint32_t kHostTTL            = 120;
constexpr uint16_t kTypeA              = 0x0001;
constexpr uint16_t kTypePTR            = 0x000c;
constexpr uint16_t kTypeTXT            = 0x0010;
constexpr uint16_t kTypeSRV            = 0x0021;
constexpr uint16_t kTypeANY            = 0x00ff;
constexpr uint16_t kClassIN            = 0x0001;
constexpr uint16_t kResponseFlags      = 0x8400; // QR + AA
constexpr std::chrono::seconds kPeriodicAnnounce{30};

struct ServiceDefinition {
    std::string instanceName;
    std::string serviceType;
    std::string hostName;
    uint16_t port{0};
    std::vector<uint8_t> txtRecord;
};

static std::pair<std::string, std::string> getOrCreateAirPlayIdentity() {
    char appdata[MAX_PATH]{};
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    const std::filesystem::path dir = std::filesystem::path(appdata) / "AuraCastPro";
    std::filesystem::create_directories(dir);

    const auto pkFile = dir / "airplay_pubkey.hex";
    const auto piFile = dir / "airplay_uuid.txt";

    std::string pkHex;
    std::string piUUID;

    if (std::filesystem::exists(pkFile) && std::filesystem::exists(piFile)) {
        std::ifstream pk(pkFile);
        std::ifstream pi(piFile);
        pk >> pkHex;
        pi >> piUUID;
        if (pkHex.size() == 64 && piUUID.size() == 36) {
            return {pkHex, piUUID};
        }
    }

    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (ctx) {
        EVP_PKEY_keygen_init(ctx);
        EVP_PKEY_keygen(ctx, &pkey);
        EVP_PKEY_CTX_free(ctx);
    }

    if (pkey) {
        size_t rawLen = 32;
        uint8_t rawPub[32]{};
        EVP_PKEY_get_raw_public_key(pkey, rawPub, &rawLen);
        EVP_PKEY_free(pkey);

        std::ostringstream oss;
        for (uint8_t b : rawPub) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
        pkHex = oss.str();
    } else {
        uint8_t rnd[32]{};
        RAND_bytes(rnd, 32);
        std::ostringstream oss;
        for (uint8_t b : rnd) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
        pkHex = oss.str();
    }

    uint8_t uuid[16]{};
    RAND_bytes(uuid, 16);
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    piUUID = std::format(
        "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    { std::ofstream pk(pkFile); pk << pkHex; }
    { std::ofstream pi(piFile); pi << piUUID; }

    AURA_LOG_INFO("MDNSService", "Generated AirPlay Ed25519 identity. pk={:.8s}... pi={}",
        pkHex, piUUID);
    return {pkHex, piUUID};
}

static std::string sanitizeHostLabel(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) out.push_back(static_cast<char>(std::tolower(uc)));
        else if (c == ' ' || c == '-' || c == '_') out.push_back('-');
    }
    if (out.empty()) out = "auracastpro";
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.empty()) out = "auracastpro";
    return out;
}

static std::string bestAirPlayDeviceId() {
    const auto ifaces = NetworkTools::enumerateInterfaces(false);
    for (const auto& iface : ifaces) {
        if (iface.ipv4.empty() || iface.ipv4.starts_with("169.254.")) continue;
        if (iface.macAddress.empty()) continue;

        std::string mac;
        mac.reserve(iface.macAddress.size());
        for (char c : iface.macAddress) {
            if (c != ':') {
                mac.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        }
        if (mac.size() == 12) return mac;
    }

    std::string fallback = NetworkTools::bestLocalIPv4();
    fallback.erase(std::remove(fallback.begin(), fallback.end(), '.'), fallback.end());
    return fallback;
}

static std::vector<uint8_t> encodeTxtRecord(const std::string& txtRecord) {
    std::vector<uint8_t> out;
    std::istringstream ss(txtRecord);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.size() > 255) line.resize(255);
        out.push_back(static_cast<uint8_t>(line.size()));
        out.insert(out.end(), line.begin(), line.end());
    }
    return out;
}

static void appendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

static void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

static void appendName(std::vector<uint8_t>& out, const std::string& fqdn) {
    std::stringstream ss(fqdn);
    std::string label;
    while (std::getline(ss, label, '.')) {
        if (label.empty()) continue;
        if (label.size() > 63) label.resize(63);
        out.push_back(static_cast<uint8_t>(label.size()));
        out.insert(out.end(), label.begin(), label.end());
    }
    out.push_back(0);
}

static bool readName(const std::vector<uint8_t>& packet, size_t& offset,
    std::string& out, int depth = 0) {
    if (depth > 8 || offset >= packet.size()) return false;

    std::string name;
    size_t cursor = offset;
    bool jumped = false;
    size_t jumpReturn = offset;

    while (cursor < packet.size()) {
        const uint8_t len = packet[cursor];
        if (len == 0) {
            ++cursor;
            if (!jumped) offset = cursor;
            out = name.empty() ? "." : name;
            return true;
        }
        if ((len & 0xC0) == 0xC0) {
            if (cursor + 1 >= packet.size()) return false;
            const uint16_t ptr = static_cast<uint16_t>(((len & 0x3F) << 8) | packet[cursor + 1]);
            if (!jumped) {
                jumpReturn = cursor + 2;
                offset = jumpReturn;
            }
            cursor = ptr;
            jumped = true;
            if (++depth > 8) return false;
            continue;
        }
        ++cursor;
        if (cursor + len > packet.size()) return false;
        if (!name.empty()) name.push_back('.');
        name.append(reinterpret_cast<const char*>(packet.data() + cursor), len);
        cursor += len;
    }
    return false;
}

static std::string normalizeFqdn(std::string name) {
    if (name == ".") return {};
    while (!name.empty() && name.back() == '.') name.pop_back();
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

static std::string fqdn(const std::string& name) {
    return normalizeFqdn(name) + ".local";
}

static void appendResourceRecord(std::vector<uint8_t>& out,
    const std::string& name,
    uint16_t type,
    uint16_t rrClass,
    uint32_t ttl,
    const std::vector<uint8_t>& rdata) {
    appendName(out, name);
    appendU16(out, type);
    appendU16(out, rrClass);
    appendU32(out, ttl);
    appendU16(out, static_cast<uint16_t>(rdata.size()));
    out.insert(out.end(), rdata.begin(), rdata.end());
}

static std::vector<uint8_t> makePtrData(const std::string& target) {
    std::vector<uint8_t> out;
    appendName(out, target);
    return out;
}

static std::vector<uint8_t> makeSrvData(uint16_t port, const std::string& hostName) {
    std::vector<uint8_t> out;
    appendU16(out, 0);
    appendU16(out, 0);
    appendU16(out, port);
    appendName(out, hostName);
    return out;
}

static std::array<uint8_t, 4> ipv4Bytes(const std::string& ip) {
    std::array<uint8_t, 4> bytes{};
    in_addr addr{};
    if (InetPtonA(AF_INET, ip.c_str(), &addr) == 1) {
        std::memcpy(bytes.data(), &addr.S_un.S_addr, bytes.size());
    }
    return bytes;
}

static std::vector<uint8_t> makeAData(const std::string& ip) {
    const auto bytes = ipv4Bytes(ip);
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

static std::vector<ServiceDefinition> buildServices(const std::string& displayName,
    const std::vector<MDNSRecord>& extraRecords) {
    std::vector<ServiceDefinition> services;
    const std::string mac = bestAirPlayDeviceId();
    auto [pkHex, piUUID] = getOrCreateAirPlayIdentity();

    std::string airplayTxt;
    {
        std::ostringstream txt;
        txt << "deviceid=" << mac << "\n";
        txt << "features=" << kAirPlayFeatures << "\n";
        txt << "srcvers=220.68\n";
        txt << "flags=" << kAirPlayFlags << "\n";
        txt << "vv=" << kAirPlayVV << "\n";
        txt << "pw=false\n";
        txt << "model=AppleTV5,3\n";
        txt << "pi=" << piUUID << "\n";
        // txt << "pk=" << pkHex << "\n";  // REMOVED: pk makes iOS think password is required
        txt << "statusFlags=0x4\n";
        airplayTxt = txt.str();
    }

    std::string raopTxt;
    {
        std::ostringstream txt;
        txt << "txtvers=" << kRaopTxtVers << "\n";
        txt << "ch=" << kRaopChannels << "\n";
        txt << "cn=" << kRaopCodecs << "\n";
        txt << "da=" << kRaopDa << "\n";
        txt << "et=" << kRaopEncryption << "\n";
        txt << "vv=" << kAirPlayVV << "\n";
        txt << "ft=" << kAirPlayFeatures << "\n";
        txt << "am=AppleTV5,3\n";
        txt << "md=" << kRaopMd << "\n";
        txt << "rhd=" << kRaopRhd << "\n";
        txt << "pw=false\n";
        txt << "sf=" << kRaopSf << "\n";
        txt << "sr=" << kRaopSr << "\n";
        txt << "ss=" << kRaopSs << "\n";
        txt << "sv=" << kRaopSv << "\n";
        txt << "tp=" << kRaopTp << "\n";
        txt << "vs=220.68\n";
        txt << "vn=" << kRaopVn << "\n";
        // txt << "pk=" << pkHex << "\n";  // REMOVED: pk makes iOS think password is required
        raopTxt = txt.str();
    }

    std::string castTxt;
    {
        std::ostringstream txt;
        txt << "id=auracastpro0001\n";
        txt << "md=AuraCastPro\n";
        txt << "ve=05\n";
        txt << "ic=/setup/icon.png\n";
        txt << "fn=" << displayName << "\n";
        txt << "ca=4101\n";
        txt << "st=0\n";
        std::string mac = "FA8FCA000000";
        auto ifaces = NetworkTools::enumerateInterfaces(false);
        if (!ifaces.empty() && !ifaces.front().macAddress.empty()) {
            mac.clear();
            for (char c : ifaces.front().macAddress) if (c != ':') mac += c;
        }
        txt << "bs=" << mac << "\n";
        txt << "nf=1\n";
        txt << "rs=\n";
        castTxt = txt.str();
    }

    services.push_back({displayName, "_airplay._tcp", sanitizeHostLabel(displayName), 7100, encodeTxtRecord(airplayTxt)});
    services.push_back({mac + "@" + displayName, "_raop._tcp", sanitizeHostLabel(displayName), 7100, encodeTxtRecord(raopTxt)});
    services.push_back({displayName, "_googlecast._tcp", sanitizeHostLabel(displayName), 8009, encodeTxtRecord(castTxt)});

    for (const auto& extra : extraRecords) {
        if (extra.name.empty() || extra.type.empty() || extra.port == 0) continue;
        services.push_back({extra.name, extra.type, sanitizeHostLabel(extra.name), extra.port, encodeTxtRecord(extra.txtRecord)});
    }
    return services;
}

} // namespace

struct MDNSService::RuntimeState {
    SOCKET socket{INVALID_SOCKET};
    std::thread worker;
    bool winsockStarted{false};
    std::string localIp;
    std::chrono::steady_clock::time_point lastAnnounce{};
};

MDNSService::MDNSService()
    : m_runtime(std::make_unique<RuntimeState>()) {}

MDNSService::~MDNSService() { shutdown(); }

void MDNSService::init() {
    AURA_LOG_INFO("MDNSService",
        "Using built-in mDNS responder. No external Bonjour service required.");
}

void MDNSService::buildAirPlayTxtRecord(std::string& out) const {
    std::vector<MDNSRecord> extra;
    const auto services = buildServices(m_displayName, extra);
    out.clear();
    for (const auto& s : services) {
        if (s.serviceType == "_airplay._tcp") {
            std::string txt(reinterpret_cast<const char*>(s.txtRecord.data()), s.txtRecord.size());
            out = txt;
            return;
        }
    }
}

void MDNSService::buildCastTxtRecord(std::string& out) const {
    std::vector<MDNSRecord> extra;
    const auto services = buildServices(m_displayName, extra);
    out.clear();
    for (const auto& s : services) {
        if (s.serviceType == "_googlecast._tcp") {
            std::string txt(reinterpret_cast<const char*>(s.txtRecord.data()), s.txtRecord.size());
            out = txt;
            return;
        }
    }
}

void MDNSService::start() {
    AURA_LOG_INFO("MDNSService", "=== START CALLED ===");
    
    if (m_running.exchange(true)) {
        AURA_LOG_WARN("MDNSService", "Already running, ignoring start() call");
        return;
    }

    AURA_LOG_INFO("MDNSService", "Initializing Winsock for mDNS...");
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        m_running = false;
        AURA_LOG_ERROR("MDNSService", "WSAStartup failed for built-in mDNS.");
        return;
    }
    m_runtime->winsockStarted = true;
    AURA_LOG_INFO("MDNSService", "Winsock initialized");

    AURA_LOG_INFO("MDNSService", "Detecting local IP address...");
    m_runtime->localIp = NetworkTools::bestLocalIPv4();
    if (m_runtime->localIp.empty() || m_runtime->localIp == "127.0.0.1") {
        m_running = false;
        WSACleanup();
        m_runtime->winsockStarted = false;
        AURA_LOG_ERROR("MDNSService", 
            "No usable LAN IPv4 address found for mDNS. Please check your network connection.\n"
            "Make sure you're connected to Wi-Fi or Ethernet.");
        return;
    }
    AURA_LOG_INFO("MDNSService", "=== LOCAL IP: {} ===", m_runtime->localIp);

    AURA_LOG_INFO("MDNSService", "Creating UDP socket...");
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        m_running = false;
        WSACleanup();
        m_runtime->winsockStarted = false;
        AURA_LOG_ERROR("MDNSService", "socket() failed for built-in mDNS: {}", WSAGetLastError());
        return;
    }
    AURA_LOG_INFO("MDNSService", "Socket created: {}", sock);

    AURA_LOG_INFO("MDNSService", "Setting SO_REUSEADDR...");
    BOOL reuse = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    AURA_LOG_INFO("MDNSService", "Binding to 0.0.0.0:5353...");
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(kMDNSPort);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
        const int err = WSAGetLastError();
        AURA_LOG_ERROR("MDNSService", 
            "bind(5353) failed for built-in mDNS: {} (0x{:X})\n"
            "Port 5353 may be in use by another service (Bonjour, iTunes, etc.)\n"
            "Try closing other apps or restarting your computer.", err, err);
        closesocket(sock);
        m_running = false;
        WSACleanup();
        m_runtime->winsockStarted = false;
        return;
    }
    AURA_LOG_INFO("MDNSService", "Bind successful");

    AURA_LOG_INFO("MDNSService", "Joining multicast group 224.0.0.251...");
    ip_mreq membership{};
    membership.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    membership.imr_interface.s_addr = inet_addr(m_runtime->localIp.c_str());
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            reinterpret_cast<const char*>(&membership), sizeof(membership)) != 0) {
        AURA_LOG_WARN("MDNSService", "IP_ADD_MEMBERSHIP failed: {}", WSAGetLastError());
    } else {
        AURA_LOG_INFO("MDNSService", "Joined multicast group successfully");
    }

    AURA_LOG_INFO("MDNSService", "Setting socket options...");
    const DWORD timeoutMs = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    in_addr ifaceAddr{};
    ifaceAddr.s_addr = inet_addr(m_runtime->localIp.c_str());
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&ifaceAddr), sizeof(ifaceAddr));

    u_char ttl = 255;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    m_runtime->socket = sock;
    m_runtime->lastAnnounce = std::chrono::steady_clock::now() - kPeriodicAnnounce;

    AURA_LOG_INFO("MDNSService", "=== MDNS FULLY CONFIGURED - SPAWNING WORKER THREAD ===");

    m_runtime->worker = std::thread([this]() {
        AURA_LOG_INFO("MDNSService", "=== mDNS Worker Thread Started ===");
        
        const auto multicastIp = std::string("224.0.0.251");

        auto collectServices = [&]() {
            std::lock_guard<std::mutex> lock(m_mutex);
            AURA_LOG_DEBUG("MDNSService", "Collecting services for advertisement");
            return buildServices(m_displayName, m_extraRecords);
        };

        auto sendPacket = [&](const std::vector<uint8_t>& packet, const std::string& destIp) {
            sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_port = htons(kMDNSPort);
            dest.sin_addr.s_addr = inet_addr(destIp.c_str());
            int sent = sendto(m_runtime->socket, reinterpret_cast<const char*>(packet.data()),
                static_cast<int>(packet.size()), 0,
                reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
            AURA_LOG_DEBUG("MDNSService", "Sent {} bytes to {} (result: {})", packet.size(), destIp, sent);
        };

        auto buildResponse = [&](uint16_t queryId, bool goodbye) {
            const auto services = collectServices();
            const std::string hostName = fqdn(sanitizeHostLabel(m_displayName));
            const std::string localIp = m_runtime->localIp;

            struct Record {
                std::string name;
                uint16_t type;
                uint16_t rrClass;
                uint32_t ttl;
                std::vector<uint8_t> rdata;
            };
            std::vector<Record> answers;

            answers.push_back({"_services._dns-sd._udp.local", kTypePTR, kClassIN,
                goodbye ? 0U : kServiceTTL, makePtrData("_airplay._tcp.local")});
            answers.push_back({"_services._dns-sd._udp.local", kTypePTR, kClassIN,
                goodbye ? 0U : kServiceTTL, makePtrData("_raop._tcp.local")});
            answers.push_back({"_services._dns-sd._udp.local", kTypePTR, kClassIN,
                goodbye ? 0U : kServiceTTL, makePtrData("_googlecast._tcp.local")});

            for (const auto& service : services) {
                const std::string serviceFqdn = fqdn(service.serviceType);
                const std::string instanceFqdn = fqdn(service.instanceName + "." + service.serviceType);
                const std::string hostFqdn = fqdn(service.hostName);
                answers.push_back({serviceFqdn, kTypePTR, kClassIN,
                    goodbye ? 0U : kServiceTTL, makePtrData(instanceFqdn)});
                answers.push_back({instanceFqdn, kTypeSRV, kClassIN | 0x8000,
                    goodbye ? 0U : kServiceTTL, makeSrvData(service.port, hostFqdn)});
                answers.push_back({instanceFqdn, kTypeTXT, kClassIN,
                    goodbye ? 0U : kTextTTL, service.txtRecord});
            }

            answers.push_back({hostName, kTypeA, kClassIN | 0x8000,
                goodbye ? 0U : kHostTTL, makeAData(localIp)});

            std::vector<uint8_t> packet;
            appendU16(packet, queryId);
            appendU16(packet, kResponseFlags);
            appendU16(packet, 0);
            appendU16(packet, static_cast<uint16_t>(answers.size()));
            appendU16(packet, 0);
            appendU16(packet, 0);
            for (const auto& rr : answers) {
                appendResourceRecord(packet, rr.name, rr.type, rr.rrClass, rr.ttl, rr.rdata);
            }
            return packet;
        };

        auto shouldAnswer = [&](const std::string& qname, uint16_t qtype) {
            const auto services = collectServices();
            const std::string hostName = fqdn(sanitizeHostLabel(m_displayName));
            const std::string normName = normalizeFqdn(qname);

            if (normName == "_services._dns-sd._udp.local") return true;
            if ((qtype == kTypeA || qtype == kTypeANY) && normName == hostName) return true;

            for (const auto& service : services) {
                const std::string serviceFqdn = fqdn(service.serviceType);
                const std::string instanceFqdn = fqdn(service.instanceName + "." + service.serviceType);
                if ((qtype == kTypePTR || qtype == kTypeANY) && normName == serviceFqdn) return true;
                if ((qtype == kTypeSRV || qtype == kTypeTXT || qtype == kTypeANY) && normName == instanceFqdn) return true;
            }
            return false;
        };

        auto announce = [&](bool goodbye) {
            try {
                AURA_LOG_INFO("MDNSService", "Building {} packet...", goodbye ? "goodbye" : "announcement");
                const auto packet = buildResponse(0, goodbye);
                AURA_LOG_INFO("MDNSService", "Packet built: {} bytes", packet.size());
                sendPacket(packet, multicastIp);
                AURA_LOG_INFO("MDNSService", "{} AirPlay/Cast via built-in mDNS on {}",
                    goodbye ? "Goodbye announced for" : "Broadcasting",
                    m_runtime->localIp);
            } catch (const std::exception& e) {
                AURA_LOG_ERROR("MDNSService", "Exception in announce: {}", e.what());
            } catch (...) {
                AURA_LOG_ERROR("MDNSService", "Unknown exception in announce");
            }
        };

        AURA_LOG_INFO("MDNSService", "Sending initial announcement...");
        announce(false);
        AURA_LOG_INFO("MDNSService", "Initial announcement sent, entering receive loop...");

        std::vector<uint8_t> buf(2048);
        while (m_running.load()) {
            sockaddr_in from{};
            int fromLen = sizeof(from);
            const int n = recvfrom(m_runtime->socket, reinterpret_cast<char*>(buf.data()),
                static_cast<int>(buf.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);

            if (!m_running.load()) break;

            if (n > 0) {
                std::vector<uint8_t> packet(buf.begin(), buf.begin() + n);
                if (packet.size() >= 12) {
                    const uint16_t id = static_cast<uint16_t>((packet[0] << 8) | packet[1]);
                    const uint16_t flags = static_cast<uint16_t>((packet[2] << 8) | packet[3]);
                    const uint16_t qdCount = static_cast<uint16_t>((packet[4] << 8) | packet[5]);
                    if ((flags & 0x8000) == 0 && qdCount > 0) {
                        size_t offset = 12;
                        bool relevant = false;
                        for (uint16_t i = 0; i < qdCount && offset < packet.size(); ++i) {
                            std::string qname;
                            if (!readName(packet, offset, qname)) break;
                            if (offset + 4 > packet.size()) break;
                            const uint16_t qtype = static_cast<uint16_t>((packet[offset] << 8) | packet[offset + 1]);
                            offset += 2;
                            offset += 2; // class
                            if (shouldAnswer(qname, qtype)) relevant = true;
                        }
                        if (relevant) {
                            sendPacket(buildResponse(id, false), multicastIp);
                        }
                    }
                }
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - m_runtime->lastAnnounce >= kPeriodicAnnounce) {
                announce(false);
                m_runtime->lastAnnounce = now;
            }
        }

        announce(true);
    });

    AURA_LOG_INFO("MDNSService",
        "Built-in mDNS responder started on UDP 5353 ({}). \n"
        "Your PC '{}' should now appear in:\n"
        "  • iPhone/iPad: Control Center → Screen Mirroring\n"
        "  • Android: Quick Settings → Cast\n"
        "Make sure your phone and PC are on the SAME Wi-Fi network!",
        m_runtime->localIp, m_displayName);
}

void MDNSService::stop() {
    if (!m_running.exchange(false)) return;

    // Send goodbye packets immediately before closing socket
    // This ensures devices are notified even if the worker thread is busy
    if (m_runtime->socket != INVALID_SOCKET && !m_runtime->localIp.empty()) {
        try {
            const auto services = buildServices(m_displayName, m_extraRecords);
            const std::string hostName = fqdn(sanitizeHostLabel(m_displayName));
            
            // Build goodbye packet (TTL=0)
            std::vector<uint8_t> goodbyePacket;
            appendU16(goodbyePacket, 0);  // Transaction ID
            appendU16(goodbyePacket, kResponseFlags);  // Flags
            appendU16(goodbyePacket, 0);  // Questions
            
            // Count answers
            uint16_t answerCount = 3;  // _services PTR records
            for (const auto& service : services) {
                answerCount += 3;  // PTR + SRV + TXT per service
            }
            answerCount += 1;  // A record
            appendU16(goodbyePacket, answerCount);
            appendU16(goodbyePacket, 0);  // Authority
            appendU16(goodbyePacket, 0);  // Additional
            
            // Add all records with TTL=0
            appendResourceRecord(goodbyePacket, "_services._dns-sd._udp.local", kTypePTR, kClassIN, 0, makePtrData("_airplay._tcp.local"));
            appendResourceRecord(goodbyePacket, "_services._dns-sd._udp.local", kTypePTR, kClassIN, 0, makePtrData("_raop._tcp.local"));
            appendResourceRecord(goodbyePacket, "_services._dns-sd._udp.local", kTypePTR, kClassIN, 0, makePtrData("_googlecast._tcp.local"));
            
            for (const auto& service : services) {
                const std::string serviceFqdn = fqdn(service.serviceType);
                const std::string instanceFqdn = fqdn(service.instanceName + "." + service.serviceType);
                const std::string hostFqdn = fqdn(service.hostName);
                appendResourceRecord(goodbyePacket, serviceFqdn, kTypePTR, kClassIN, 0, makePtrData(instanceFqdn));
                appendResourceRecord(goodbyePacket, instanceFqdn, kTypeSRV, kClassIN | 0x8000, 0, makeSrvData(service.port, hostFqdn));
                appendResourceRecord(goodbyePacket, instanceFqdn, kTypeTXT, kClassIN, 0, service.txtRecord);
            }
            
            appendResourceRecord(goodbyePacket, hostName, kTypeA, kClassIN | 0x8000, 0, makeAData(m_runtime->localIp));
            
            // Send goodbye packet to multicast address
            sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_port = htons(kMDNSPort);
            dest.sin_addr.s_addr = inet_addr("224.0.0.251");
            
            // Send multiple times to ensure delivery
            for (int i = 0; i < 3; ++i) {
                sendto(m_runtime->socket, reinterpret_cast<const char*>(goodbyePacket.data()),
                       static_cast<int>(goodbyePacket.size()), 0,
                       reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
                if (i < 2) Sleep(50);  // Small delay between sends
            }
            
            AURA_LOG_INFO("MDNSService", "Sent goodbye packets (TTL=0) to notify devices of shutdown");
        } catch (...) {
            // Ignore errors during shutdown
        }
    }

    if (m_runtime->socket != INVALID_SOCKET) {
        closesocket(m_runtime->socket);
        m_runtime->socket = INVALID_SOCKET;
    }

    if (m_runtime->worker.joinable()) {
        m_runtime->worker.join();
    }

    if (m_runtime->winsockStarted) {
        WSACleanup();
        m_runtime->winsockStarted = false;
    }

    AURA_LOG_INFO("MDNSService", "Built-in mDNS responder stopped.");
}

void MDNSService::shutdown() { stop(); }

void MDNSService::setDisplayName(const std::string& name) {
    m_displayName = name;
    if (m_running.load()) {
        stop();
        start();
    }
}

void MDNSService::registerService(const MDNSRecord& record) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& r : m_extraRecords) {
        if (r.name == record.name && r.type == record.type) {
            r = record;
            AURA_LOG_DEBUG("MDNSService", "Updated in-process mDNS record: {} type={}",
                record.name, record.type);
            return;
        }
    }
    m_extraRecords.push_back(record);
    AURA_LOG_INFO("MDNSService", "Registered in-process mDNS record: {} type={}",
        record.name, record.type);
}

} // namespace aura

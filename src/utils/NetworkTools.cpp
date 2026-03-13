// =============================================================================
// NetworkTools.cpp -- Network enumeration and port utilities (Windows)
// =============================================================================

#include "../pch.h"  // PCH
#include "NetworkTools.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <format>
#include <algorithm>
#include <stdexcept>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace aura {

// -----------------------------------------------------------------------------
std::vector<NetworkInterface> NetworkTools::enumerateInterfaces(bool includeLoopback) {
    std::vector<NetworkInterface> result;

    // Allocate buffer for adapter info (IP_ADAPTER_ADDRESSES)
    ULONG bufSize = 15000;
    std::vector<uint8_t> buf(bufSize);

    DWORD rc = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr,
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data()),
        &bufSize);

    if (rc == ERROR_BUFFER_OVERFLOW) {
        buf.resize(bufSize);
        rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_DNS_SERVER,
                                  nullptr,
                                  reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data()),
                                  &bufSize);
    }

    if (rc != NO_ERROR) {
        AURA_LOG_ERROR("NetworkTools", "GetAdaptersAddresses failed: {}", rc);
        return result;
    }

    for (auto* adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
         adapter != nullptr;
         adapter = adapter->Next) {

        // Skip adapters that are not operational
        if (adapter->OperStatus != IfOperStatusUp) continue;

        // Skip loopback if not requested
        if (!includeLoopback && adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        NetworkInterface iface;
        iface.isLoopback  = (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK);
        iface.isWireless  = (adapter->IfType == IF_TYPE_IEEE80211);
        iface.speed       = adapter->TransmitLinkSpeed / 8; // bits -> bytes per sec

        // Adapter name (UTF-8)
        char nameBuf[256]{};
        WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1,
                            nameBuf, sizeof(nameBuf), nullptr, nullptr);
        iface.name = nameBuf;

        // MAC address
        if (adapter->PhysicalAddressLength == 6) {
            iface.macAddress = std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                adapter->PhysicalAddress[0], adapter->PhysicalAddress[1],
                adapter->PhysicalAddress[2], adapter->PhysicalAddress[3],
                adapter->PhysicalAddress[4], adapter->PhysicalAddress[5]);
        }

        // IP addresses
        for (auto* addr = adapter->FirstUnicastAddress; addr != nullptr; addr = addr->Next) {
            char ipBuf[INET6_ADDRSTRLEN]{};
            if (addr->Address.lpSockaddr->sa_family == AF_INET) {
                inet_ntop(AF_INET,
                    &reinterpret_cast<sockaddr_in*>(addr->Address.lpSockaddr)->sin_addr,
                    ipBuf, sizeof(ipBuf));
                iface.ipv4 = ipBuf;
            } else if (addr->Address.lpSockaddr->sa_family == AF_INET6) {
                inet_ntop(AF_INET6,
                    &reinterpret_cast<sockaddr_in6*>(addr->Address.lpSockaddr)->sin6_addr,
                    ipBuf, sizeof(ipBuf));
                iface.ipv6 = ipBuf;
            }
        }

        if (!iface.ipv4.empty()) {
            result.push_back(std::move(iface));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
std::string NetworkTools::bestLocalIPv4() {
    auto ifaces = enumerateInterfaces(false);

    // Sort: wired before wireless, then by speed descending
    std::sort(ifaces.begin(), ifaces.end(), [](const NetworkInterface& a, const NetworkInterface& b) {
        if (a.isWireless != b.isWireless) return !a.isWireless; // wired first
        return a.speed > b.speed;
    });

    for (const auto& iface : ifaces) {
        // Skip APIPA addresses (169.254.x.x)
        if (iface.ipv4.starts_with("169.254.")) continue;
        // Skip Microsoft Teredo/ISATAP virtual adapters
        if (iface.name.find("Teredo") != std::string::npos) continue;
        if (iface.name.find("ISATAP") != std::string::npos) continue;

        AURA_LOG_DEBUG("NetworkTools", "Best local IP: {} on {}", iface.ipv4, iface.name);
        return iface.ipv4;
    }

    AURA_LOG_WARN("NetworkTools", "No suitable local IP found, falling back to 127.0.0.1");
    return "127.0.0.1";
}

// -----------------------------------------------------------------------------
bool NetworkTools::isPortAvailable(uint16_t port, bool tcp) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int type = tcp ? SOCK_STREAM : SOCK_DGRAM;
    int proto = tcp ? IPPROTO_TCP : IPPROTO_UDP;

    SOCKET s = socket(AF_INET, type, proto);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    const bool available = (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    closesocket(s);
    return available;
}

// -----------------------------------------------------------------------------
uint16_t NetworkTools::findFreePort(uint16_t start, uint16_t end, bool tcp) {
    for (uint16_t port = start; port <= end; ++port) {
        if (isPortAvailable(port, tcp)) return port;
    }
    AURA_LOG_WARN("NetworkTools", "No free {} port found in range [{}, {}]",
                  tcp ? "TCP" : "UDP", start, end);
    return 0;
}

// -----------------------------------------------------------------------------
std::string NetworkTools::formatBandwidth(uint64_t bytesPerSec) {
    const double bitsPerSec = static_cast<double>(bytesPerSec) * 8.0;
    if (bitsPerSec >= 1e9) return std::format("{:.1f} Gbps", bitsPerSec / 1e9);
    if (bitsPerSec >= 1e6) return std::format("{:.1f} Mbps", bitsPerSec / 1e6);
    if (bitsPerSec >= 1e3) return std::format("{:.1f} Kbps", bitsPerSec / 1e3);
    return std::format("{:.0f} bps", bitsPerSec);
}

// -----------------------------------------------------------------------------
std::string NetworkTools::hrToString(long hr) {
    return std::format("0x{:08X}", static_cast<uint32_t>(hr));
}

} // namespace aura

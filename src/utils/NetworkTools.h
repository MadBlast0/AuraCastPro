#pragma once
// =============================================================================
// NetworkTools.h — Network utility functions used across multiple subsystems.
//
// Includes:
//   - Local IP address enumeration
//   - Port availability checking
//   - IPv4 / IPv6 formatting helpers
//   - Network interface selection heuristics
// =============================================================================

#include <string>
#include <vector>
#include <cstdint>

namespace aura {

struct NetworkInterface {
    std::string name;        // Adapter friendly name
    std::string ipv4;        // e.g. "192.168.1.10"
    std::string ipv6;        // Link-local or global, may be empty
    std::string macAddress;  // e.g. "AA:BB:CC:DD:EE:FF"
    bool        isLoopback{};
    bool        isWireless{};
    uint64_t    speed{};     // Bytes per second (0 = unknown)
};

class NetworkTools {
public:
    // Enumerate all active network interfaces (excludes loopback by default)
    static std::vector<NetworkInterface> enumerateInterfaces(bool includeLoopback = false);

    // Return the "best" local IPv4 address for mDNS / socket binding.
    // Prefers non-loopback, non-APIPA (169.254.x.x) wired or wireless adapters.
    static std::string bestLocalIPv4();
    // Alias used by Settings/ReceiverSocket modules
    static std::string getBestInterface() { return bestLocalIPv4(); }
    static std::vector<NetworkInterface> getNetworkInterfaces() { return enumerateInterfaces(false); }

    // Return true if the given TCP or UDP port is available on 0.0.0.0
    static bool isPortAvailable(uint16_t port, bool tcp = true);

    // Find a free port in the given range [start, end].
    // Returns 0 if no free port is found.
    static uint16_t findFreePort(uint16_t start = 7000, uint16_t end = 8100, bool tcp = true);

    // Format bytes/sec as human-readable string (e.g. "54.3 Mbps")
    static std::string formatBandwidth(uint64_t bytesPerSec);

    // Convert HRESULT to hex string for logging
    static std::string hrToString(long hr);

private:
    NetworkTools() = delete;
};

} // namespace aura

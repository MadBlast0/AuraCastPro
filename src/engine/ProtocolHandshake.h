#pragma once
// =============================================================================
// ProtocolHandshake.h -- Shared protocol negotiation and auth utilities
// =============================================================================
#include <string>
#include <vector>
#include <cstdint>

namespace aura {

class ProtocolHandshake {
public:
    void init();
    void start();
    void stop();
    void shutdown();

    // Generate a random 4-digit PIN for AirPlay pairing display
    static std::string generateAirPlayPIN();

    // Derive AES-128 session key from SRP shared secret
    static std::vector<uint8_t> deriveAirPlaySessionKey(
        const std::vector<uint8_t>& srpSecret,
        const std::string& context);

    // Build RTSP response with common headers
    static std::string buildRTSPOK(int cseq, const std::string& extraHeaders = "");

    // Validate a device's HMAC-SHA1 pairing verification
    static bool verifyHMAC(const std::vector<uint8_t>& data,
                           const std::vector<uint8_t>& key,
                           const std::vector<uint8_t>& expected);
};

} // namespace aura

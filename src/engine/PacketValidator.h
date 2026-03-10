#pragma once
// =============================================================================
// PacketValidator.h — Task 072: 5-check security validation on every UDP packet
// =============================================================================
#include <cstdint>
#include <string>
#include <span>

namespace aura {

// Known protocol magic bytes (first 4 bytes of every valid AuraCastPro packet)
static constexpr uint32_t kAirPlayMagic = 0x52545350; // "RTSP"
static constexpr uint32_t kCastMagic    = 0x43415354; // "CAST"
static constexpr uint32_t kScrcpyMagic  = 0x53435243; // "SCRC"

enum class ValidateResult {
    Ok,
    TooSmall,     // < 12 bytes
    TooLarge,     // > 65507 bytes
    BadMagic,     // magic bytes don't match protocol
    SeqJump,      // sequence number > 1000 ahead of expected
    UnknownSource // source IP not in connected device list
};

class PacketValidator {
public:
    PacketValidator() = default;

    // Set the IP address of the currently connected device.
    // Only packets from this IP are accepted.
    void setAllowedSource(const std::string& ip) { m_allowedSourceIp = ip; }
    void clearAllowedSource()                    { m_allowedSourceIp.clear(); }

    // Reset expected sequence number (call on new stream)
    void resetSequence() { m_nextExpectedSeq = 0; m_seqInitialised = false; }

    // ── Main validation entry point ───────────────────────────────────────────
    // Runs all 5 checks in order. Returns Ok or the first failing check.
    ValidateResult validate(std::span<const uint8_t> data,
                            const std::string& senderIp,
                            uint16_t seqNum);

    // Human-readable reason string for logging
    static const char* resultString(ValidateResult r);

    // Stats
    uint64_t accepted()  const { return m_accepted; }
    uint64_t rejected()  const { return m_rejected; }

private:
    std::string m_allowedSourceIp;
    uint16_t    m_nextExpectedSeq{0};
    bool        m_seqInitialised{false};

    uint64_t m_accepted{0};
    uint64_t m_rejected{0};

    static constexpr size_t   kMinSize  = 12;
    static constexpr size_t   kMaxSize  = 65507;
    static constexpr uint16_t kMaxSeqJump = 1000;
};

} // namespace aura

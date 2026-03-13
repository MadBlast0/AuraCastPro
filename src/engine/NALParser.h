#pragma once
// =============================================================================
// NALParser.h -- NAL (Network Abstraction Layer) unit extractor.
//
// H.265/HEVC and AV1 video streams are divided into NAL units.
// AirPlay sends video as fragmented RTP packets -- a single NAL unit may be
// split across multiple UDP packets (STAP-A, FU-A fragmentation).
//
// This parser:
//   1. Receives raw packet payloads from PacketReorderCache
//   2. Detects packet fragmentation type (single, STAP-A, FU-A)
//   3. Reassembles fragmented NAL units
//   4. Emits complete, ready-to-decode NAL units downstream
//
// Output format: Annex B (start code 0x00 0x00 0x00 0x01 prefix)
// This is the format expected by Windows Media Foundation decoders.
// =============================================================================

#include <cstdint>
#include <vector>
#include <functional>
#include <span>

namespace aura {

enum class NalUnitType : uint8_t {
    // H.265 NAL types (from ITU-T H.265 spec, Table 7-1)
    VPS            = 32, // Video Parameter Set
    SPS            = 33, // Sequence Parameter Set
    PPS            = 34, // Picture Parameter Set
    IDR_W_RADL     = 19, // IDR keyframe
    IDR_N_LP       = 20, // IDR keyframe (no leading pictures)
    TRAIL_R        = 1,  // Regular P/B frame (reference)
    TRAIL_N        = 0,  // Regular P/B frame (non-reference)
    // RTP packetisation types (RFC 7798)
    RTP_STAP_A     = 48, // Single-Time Aggregation Packet
    RTP_FU         = 49, // Fragmentation Unit
    Unknown        = 255
};

struct NalUnit {
    NalUnitType          type{NalUnitType::Unknown};
    int                  nalType{0};       // int alias (tests use this)
    bool                 isKeyframe{};
    bool                 isParameterSet{}; // VPS, SPS, or PPS
    std::vector<uint8_t> data;             // Complete NAL, Annex B format
};

class NALParser {
public:
    // Called for each complete, reassembled NAL unit
    using NalCallback = std::function<void(NalUnit nal)>;

    explicit NALParser(NalCallback callback = nullptr);

    // Compatibility API used by tests
    void init() {}
    void setNALCallback(std::function<void(std::vector<uint8_t>&&, bool, bool)> cb) {
        m_compatCb = std::move(cb);
    }
    void feedRTPPayload(const uint8_t* data, size_t len, uint16_t /*seq*/, bool /*marker*/) {
        feedPacket(std::span<const uint8_t>(data, len));
    }


    // Feed a raw packet payload (after reorder and before demux)
    // May emit zero, one, or multiple NAL units via the callback.
    void feedPacket(std::span<const uint8_t> payload);

    // Reset reassembly state (call on stream restart or keyframe request)
    void reset();

    // Statistics
    uint64_t nalUnitsEmitted()  const { return m_emitted; }
    uint64_t fragmentsDropped() const { return m_fragsDropped; }

private:
    NalCallback m_callback;
    std::function<void(std::vector<uint8_t>&&, bool, bool)> m_compatCb;

    // FU-A reassembly state
    bool                 m_fuInProgress{false};
    NalUnitType          m_fuType{NalUnitType::Unknown};
    std::vector<uint8_t> m_fuBuffer; // Accumulates FU-A fragments

    uint64_t m_emitted{0};
    uint64_t m_fragsDropped{0};

    // Annex B start code: 0x00 0x00 0x00 0x01
    static constexpr uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

    void processSingleNAL(std::span<const uint8_t> payload);
    void processSTAP_A(std::span<const uint8_t> payload);
    void processFU(std::span<const uint8_t> payload);
    void emitNal(NalUnitType type, std::span<const uint8_t> data);
    static NalUnitType parseH265Type(uint8_t headerByte0);
    static bool isKeyframeType(NalUnitType t);
    static bool isParameterSetType(NalUnitType t);
};

} // namespace aura

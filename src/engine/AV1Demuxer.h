#pragma once
// =============================================================================
// AV1Demuxer.h -- AV1 OBU (Open Bitstream Unit) assembler
//
// AV1 uses OBUs instead of NAL units. The structure is:
//   Sequence Header OBU  (like SPS -- describes codec config)
//   Frame Header OBU     (metadata for one frame)
//   Tile Group OBU       (actual compressed frame data)
//
// RTP packetisation of AV1: RFC draft-ietf-payload-av1
//   - Aggregation packets: multiple small OBUs in one RTP packet
//   - Fragmentation: large OBUs split across multiple RTP packets
// =============================================================================
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>

namespace aura {

struct AV1AccessUnit {
    std::vector<uint8_t> data;         // Complete AV1 bitstream (IVF or annexb)
    bool     isKeyframe{false};
    uint64_t presentationTimeUs{0};
};

class AV1Demuxer {
public:
    using AccessUnitCallback = std::function<void(AV1AccessUnit au)>;

    AV1Demuxer();
    ~AV1Demuxer();

    void init();
    void reset();

    // Feed one RTP payload (after sequence number reordering)
    void feedRTPPayload(const uint8_t* data, size_t len, uint64_t pts);

    void setCallback(AccessUnitCallback cb);
    bool hasSequenceHeader() const { return m_hasSeqHeader; }

private:
    bool               m_hasSeqHeader{false};
    std::vector<uint8_t> m_seqHeader;
    std::vector<uint8_t> m_fragBuffer;  // Reassembly buffer for fragmented OBUs
    bool               m_inFragment{false};
    AccessUnitCallback m_callback;

    void parseOBUs(const uint8_t* data, size_t len, uint64_t pts);
    static uint8_t obuType(uint8_t headerByte) { return (headerByte >> 3) & 0x0F; }
};

} // namespace aura

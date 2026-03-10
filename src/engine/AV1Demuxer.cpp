// =============================================================================
// AV1Demuxer.cpp — AV1 OBU reassembly from RTP
// =============================================================================
#include "AV1Demuxer.h"
#include "../utils/Logger.h"
#include <cstring>

namespace aura {

// AV1 OBU types
enum AV1OBUType : uint8_t {
    OBU_SEQUENCE_HEADER        = 1,
    OBU_TEMPORAL_DELIMITER     = 2,
    OBU_FRAME_HEADER           = 3,
    OBU_TILE_GROUP             = 4,
    OBU_METADATA               = 5,
    OBU_FRAME                  = 6,   // Combined frame header + tile group
    OBU_REDUNDANT_FRAME_HEADER = 7,
    OBU_TILE_LIST              = 8,
    OBU_PADDING                = 15,
};

AV1Demuxer::AV1Demuxer() {}
AV1Demuxer::~AV1Demuxer() {}

void AV1Demuxer::init() {
    AURA_LOG_INFO("AV1Demuxer",
        "Initialised. AV1 OBU reassembly from RTP (draft-ietf-payload-av1). "
        "Handles: aggregation, fragmentation, sequence header caching.");
}

void AV1Demuxer::reset() {
    m_hasSeqHeader = false;
    m_seqHeader.clear();
    m_fragBuffer.clear();
    m_inFragment = false;
    AURA_LOG_DEBUG("AV1Demuxer", "Reset.");
}

void AV1Demuxer::feedRTPPayload(const uint8_t* data, size_t len, uint64_t pts) {
    if (!data || len == 0) return;

    // AV1 RTP payload format (draft-ietf-payload-av1):
    // Byte 0: Aggregation header
    //   Z bit (bit 7): continuation of previous fragment
    //   Y bit (bit 6): will be continued in next packet
    //   W bits (5:4):  number of OBU elements (0 = one OBU, no size prefix)
    //   N bit (bit 3):  new coded video sequence starts here
    //   Reserved (2:0)

    if (len < 1) return;

    const uint8_t aggHeader = data[0];
    const bool isFragment   = (aggHeader >> 7) & 1;   // Z: continuation
    const bool hasMore      = (aggHeader >> 6) & 1;   // Y: more fragments follow
    const bool isNewSeq     = (aggHeader >> 3) & 1;   // N: new sequence

    if (isNewSeq) {
        m_fragBuffer.clear();
        m_inFragment = false;
    }

    // Skip aggregation header byte
    const uint8_t* payload = data + 1;
    size_t         payloadLen = len - 1;

    if (isFragment || m_inFragment) {
        // Accumulate fragment
        m_fragBuffer.insert(m_fragBuffer.end(), payload, payload + payloadLen);
        m_inFragment = hasMore;

        if (!hasMore && !m_fragBuffer.empty()) {
            // Fragment complete — parse the reassembled OBUs
            parseOBUs(m_fragBuffer.data(), m_fragBuffer.size(), pts);
            m_fragBuffer.clear();
        }
    } else {
        // Non-fragmented — parse directly
        parseOBUs(payload, payloadLen, pts);
    }
}

void AV1Demuxer::parseOBUs(const uint8_t* data, size_t len, uint64_t pts) {
    AV1AccessUnit au;
    au.presentationTimeUs = pts;

    size_t offset = 0;
    while (offset < len) {
        if (offset >= len) break;

        const uint8_t obuHeader = data[offset++];
        const bool    hasExtension = (obuHeader >> 2) & 1;
        const bool    hasSizeField = (obuHeader >> 1) & 1;
        const uint8_t type = obuType(obuHeader);

        if (hasExtension) offset++; // skip extension byte

        // Read LEB128 size if present
        size_t obuSize = 0;
        if (hasSizeField) {
            int shift = 0;
            while (offset < len) {
                uint8_t b = data[offset++];
                obuSize |= static_cast<size_t>(b & 0x7F) << shift;
                if (!(b & 0x80)) break;
                shift += 7;
            }
        } else {
            obuSize = len - offset;
        }

        if (offset + obuSize > len) break;

        const uint8_t* obuData = data + offset - 1; // include header byte
        const size_t   obuTotalLen = 1 + (hasExtension ? 1 : 0) + obuSize;

        if (type == OBU_SEQUENCE_HEADER) {
            m_seqHeader.assign(obuData, obuData + obuTotalLen);
            m_hasSeqHeader = true;
            AURA_LOG_DEBUG("AV1Demuxer", "Sequence header updated ({} bytes).", obuTotalLen);
        }

        if (type == OBU_FRAME || type == OBU_FRAME_HEADER) {
            // Check show_existing_frame flag to detect keyframes
            // Bit 0 of frame_header = show_existing_frame (simplified check)
            if (offset < len && !(data[offset] & 0x80)) {
                au.isKeyframe = true;
            }
        }

        // Append this OBU to the access unit
        if (type != OBU_TEMPORAL_DELIMITER) {
            au.data.insert(au.data.end(), obuData, obuData + obuTotalLen);
        }

        offset += obuSize;
    }

    if (au.data.empty() || !m_hasSeqHeader) return;

    // Prepend sequence header to keyframes
    if (au.isKeyframe && !m_seqHeader.empty()) {
        std::vector<uint8_t> withSeqHdr;
        withSeqHdr.insert(withSeqHdr.end(), m_seqHeader.begin(), m_seqHeader.end());
        withSeqHdr.insert(withSeqHdr.end(), au.data.begin(), au.data.end());
        au.data = std::move(withSeqHdr);
    }

    AURA_LOG_TRACE("AV1Demuxer", "AU emitted: {} bytes, keyframe={}", 
                   au.data.size(), au.isKeyframe);

    if (m_callback) m_callback(std::move(au));
}

void AV1Demuxer::setCallback(AccessUnitCallback cb) {
    m_callback = std::move(cb);
}

} // namespace aura

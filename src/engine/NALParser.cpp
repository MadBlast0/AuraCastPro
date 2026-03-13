// =============================================================================
// NALParser.cpp -- RTP NAL unit reassembly (H.265 / RFC 7798)
// =============================================================================

#include "../pch.h"  // PCH
#include "NALParser.h"
#include "../utils/Logger.h"

#include <cstring>
#include <stdexcept>

namespace aura {

constexpr uint8_t NALParser::kStartCode[];

// -----------------------------------------------------------------------------
NALParser::NALParser(NalCallback callback)
    : m_callback(std::move(callback)) {
    m_fuBuffer.reserve(1024 * 1024); // Reserve 1 MB for large NAL assembly
}

// -----------------------------------------------------------------------------
void NALParser::reset() {
    m_fuInProgress = false;
    m_fuBuffer.clear();
    AURA_LOG_DEBUG("NALParser", "Reset. emitted={} fragsDrop={}",
                   m_emitted, m_fragsDropped);
}

// -----------------------------------------------------------------------------
// RTP H.265 payload header (RFC 7798 Section 4.4):
//   Byte 0:  forbidden_zero_bit(1) | nal_unit_type(6) | nuh_layer_id_msb(1)
//   Byte 1:  nuh_layer_id_lsb(5)  | nuh_temporal_id_plus1(3)
// nal_unit_type is bits [9:5] of the 16-bit RTP payload header.
// -----------------------------------------------------------------------------
NalUnitType NALParser::parseH265Type(uint8_t headerByte0) {
    const uint8_t t = (headerByte0 >> 1) & 0x3F; // bits [6:1]
    switch (t) {
        case 0:  return NalUnitType::TRAIL_N;
        case 1:  return NalUnitType::TRAIL_R;
        case 19: return NalUnitType::IDR_W_RADL;
        case 20: return NalUnitType::IDR_N_LP;
        case 32: return NalUnitType::VPS;
        case 33: return NalUnitType::SPS;
        case 34: return NalUnitType::PPS;
        case 48: return NalUnitType::RTP_STAP_A;
        case 49: return NalUnitType::RTP_FU;
        default: return NalUnitType::Unknown;
    }
}

// -----------------------------------------------------------------------------
bool NALParser::isKeyframeType(NalUnitType t) {
    return t == NalUnitType::IDR_W_RADL || t == NalUnitType::IDR_N_LP;
}

bool NALParser::isParameterSetType(NalUnitType t) {
    return t == NalUnitType::VPS || t == NalUnitType::SPS || t == NalUnitType::PPS;
}

// -----------------------------------------------------------------------------
void NALParser::feedPacket(std::span<const uint8_t> payload) {
    if (payload.size() < 2) return; // Minimum RTP H.265 header = 2 bytes

    const NalUnitType rtpType = parseH265Type(payload[0]);

    switch (rtpType) {
        case NalUnitType::RTP_STAP_A:
            processSTAP_A(payload);
            break;
        case NalUnitType::RTP_FU:
            processFU(payload);
            break;
        default:
            // Single NAL unit packet (most common case for small frames)
            processSingleNAL(payload);
            break;
    }
}

// -----------------------------------------------------------------------------
// Single NAL Unit Packet: the entire payload IS one NAL unit.
// Prepend Annex B start code and emit.
// -----------------------------------------------------------------------------
void NALParser::processSingleNAL(std::span<const uint8_t> payload) {
    const NalUnitType type = parseH265Type(payload[0]);
    emitNal(type, payload);
}

// -----------------------------------------------------------------------------
// STAP-A: Single-Time Aggregation Packet containing multiple NAL units.
// Format after 2-byte RTP header:
//   [2-byte NAL size][NAL data][2-byte NAL size][NAL data]...
// -----------------------------------------------------------------------------
void NALParser::processSTAP_A(std::span<const uint8_t> payload) {
    // Skip 2-byte STAP-A header
    std::size_t offset = 2;

    while (offset + 2 <= payload.size()) {
        const uint16_t nalSize = (static_cast<uint16_t>(payload[offset]) << 8)
                                | static_cast<uint16_t>(payload[offset + 1]);
        offset += 2;

        if (offset + nalSize > payload.size()) {
            AURA_LOG_WARN("NALParser", "STAP-A: NAL size {} overflows packet", nalSize);
            break;
        }

        const auto nalData = payload.subspan(offset, nalSize);
        if (!nalData.empty()) {
            const NalUnitType type = parseH265Type(nalData[0]);
            emitNal(type, nalData);
        }
        offset += nalSize;
    }
}

// -----------------------------------------------------------------------------
// FU (Fragmentation Unit): large NAL unit split across multiple RTP packets.
// Format:
//   Byte 0-1: RTP H.265 header (type = 49 = FU)
//   Byte 2:   FU header: S(1) E(1) nal_unit_type(6)
//   Byte 3+:  Fragment data
// S=1 -> start of NAL unit
// E=1 -> end of NAL unit (emit reassembled buffer)
// -----------------------------------------------------------------------------
void NALParser::processFU(std::span<const uint8_t> payload) {
    if (payload.size() < 3) return;

    const uint8_t fuHeader  = payload[2];
    const bool    isStart   = (fuHeader & 0x80) != 0;
    const bool    isEnd     = (fuHeader & 0x40) != 0;
    const uint8_t nalType6b = fuHeader & 0x3F;

    // Reconstruct H.265 NAL header from RTP FU header
    // Original NAL header: forbidden(1) | nal_type(6) | layer_id_msb(1)
    const uint8_t nalHdr0 = (payload[0] & 0x81) | (nalType6b << 1);
    const uint8_t nalHdr1 = payload[1];

    const auto fragment = payload.subspan(3); // Fragment data (no FU header)

    if (isStart) {
        if (m_fuInProgress) {
            // Previous FU sequence was incomplete -- discard
            AURA_LOG_WARN("NALParser", "FU: New start before previous end -- discarding {} bytes",
                          m_fuBuffer.size());
            m_fragsDropped++;
        }
        m_fuBuffer.clear();
        m_fuBuffer.push_back(nalHdr0);
        m_fuBuffer.push_back(nalHdr1);
        m_fuBuffer.insert(m_fuBuffer.end(), fragment.begin(), fragment.end());
        m_fuInProgress = true;
        m_fuType = parseH265Type(nalHdr0);
    } else if (m_fuInProgress) {
        m_fuBuffer.insert(m_fuBuffer.end(), fragment.begin(), fragment.end());

        if (isEnd) {
            // All fragments received -- emit the reassembled NAL unit
            emitNal(m_fuType, m_fuBuffer);
            m_fuBuffer.clear();
            m_fuInProgress = false;
        }
    } else {
        // Fragment received without a start -- packet loss
        AURA_LOG_TRACE("NALParser", "FU: Fragment without start (packet loss?)");
        m_fragsDropped++;
    }
}

// -----------------------------------------------------------------------------
void NALParser::emitNal(NalUnitType type, std::span<const uint8_t> data) {
    NalUnit nal;
    nal.type           = type;
    nal.isKeyframe     = isKeyframeType(type);
    nal.isParameterSet = isParameterSetType(type);

    // Prepend Annex B start code: 0x00 0x00 0x00 0x01
    nal.data.reserve(4 + data.size());
    nal.data.insert(nal.data.end(), std::begin(kStartCode), std::end(kStartCode));
    nal.data.insert(nal.data.end(), data.begin(), data.end());

    ++m_emitted;

    if (m_callback) m_callback(std::move(nal));
}

} // namespace aura

// =============================================================================
// H265Demuxer.cpp -- H.265 access unit assembler
// =============================================================================
#include "../pch.h"  // PCH
#include "H265Demuxer.h"
#include "../utils/Logger.h"

namespace aura {

// H.265 NAL unit types relevant to demuxing
enum H265NalType : uint8_t {
    // Video Coding Layer
    IDR_W_RADL   = 19,
    IDR_N_LP     = 20,
    CRA_NUT      = 21,

    // Parameter sets
    VPS_NUT      = 32,
    SPS_NUT      = 33,
    PPS_NUT      = 34,

    // Access unit delimiter
    AUD_NUT      = 35,

    // SEI
    PREFIX_SEI   = 39,
    SUFFIX_SEI   = 40,
};

// Annex B start code prefix
static const uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

// -----------------------------------------------------------------------------
H265Demuxer::H265Demuxer() {}
H265Demuxer::~H265Demuxer() {}

// -----------------------------------------------------------------------------
void H265Demuxer::init() {
    AURA_LOG_INFO("H265Demuxer",
        "Initialised. Assembles NAL units -> complete access units. "
        "Caches VPS/SPS/PPS. Prepends param sets to every IDR.");
}

// -----------------------------------------------------------------------------
void H265Demuxer::reset() {
    m_hasVPS = m_hasSPS = m_hasPPS = false;
    m_vps.clear(); m_sps.clear(); m_pps.clear();
    m_pending.clear();
    AURA_LOG_DEBUG("H265Demuxer", "Reset.");
}

// -----------------------------------------------------------------------------
void H265Demuxer::feedNAL(NalUnit nal, uint64_t presentationTimeUs) {
    const uint8_t type = static_cast<uint8_t>(nal.type);

    // ── Cache parameter sets ─────────────────────────────────────────────────
    if (type == VPS_NUT) {
        m_vps = nal.data;
        m_hasVPS = true;
        AURA_LOG_DEBUG("H265Demuxer", "VPS updated ({} bytes)", nal.data.size());
        return;
    }
    if (type == SPS_NUT) {
        m_sps = nal.data;
        m_hasSPS = true;
        // Parse SPS to extract width/height (abbreviated -- full parse in Phase 6)
        AURA_LOG_DEBUG("H265Demuxer", "SPS updated ({} bytes)", nal.data.size());
        return;
    }
    if (type == PPS_NUT) {
        m_pps = nal.data;
        m_hasPPS = true;
        AURA_LOG_DEBUG("H265Demuxer", "PPS updated ({} bytes)", nal.data.size());
        return;
    }

    // ── Access Unit Delimiter signals end of previous AU ─────────────────────
    if (type == AUD_NUT) {
        if (!m_pending.empty()) {
            emitAccessUnit();
        }
        m_currentPts = presentationTimeUs;
        return;
    }

    // ── Accumulate VCL NALs into current access unit ─────────────────────────
    if (m_pending.empty()) {
        m_currentPts = presentationTimeUs;
    }
    m_pending.push_back(std::move(nal));
}

// -----------------------------------------------------------------------------
void H265Demuxer::emitAccessUnit() {
    if (m_pending.empty()) return;
    if (!hasParameterSets()) {
        AURA_LOG_WARN("H265Demuxer", "Dropping AU -- no parameter sets yet.");
        m_pending.clear();
        return;
    }

    AccessUnit au;
    au.presentationTimeUs = m_currentPts;

    // Check if this is a keyframe (IDR)
    for (const auto& nal : m_pending) {
        if (nal.type == NalUnitType::IDR_W_RADL || nal.type == NalUnitType::IDR_N_LP) {
            au.isKeyframe = true;
            break;
        }
    }

    // For IDR frames: prepend VPS + SPS + PPS so the decoder can re-initialise
    if (au.isKeyframe) {
        auto append = [&](const std::vector<uint8_t>& paramSet) {
            au.annexBData.insert(au.annexBData.end(), kStartCode, kStartCode + 4);
            au.annexBData.insert(au.annexBData.end(),
                                 paramSet.begin(), paramSet.end());
        };
        append(m_vps);
        append(m_sps);
        append(m_pps);
    }

    // Append all VCL NAL units
    for (const auto& nal : m_pending) {
        // Each NAL unit's data already starts with the Annex B start code
        au.annexBData.insert(au.annexBData.end(),
                             nal.data.begin(), nal.data.end());
    }

    m_pending.clear();

    AURA_LOG_TRACE("H265Demuxer", "AU emitted: {} bytes, keyframe={}, pts={}us",
                   au.annexBData.size(), au.isKeyframe, au.presentationTimeUs);

    if (m_callback) m_callback(std::move(au));
}

// -----------------------------------------------------------------------------
void H265Demuxer::setCallback(AccessUnitCallback cb) {
    m_callback = std::move(cb);
}


// H.265 NAL unit type 35 = ACCESS_UNIT_DELIMITER_NUT
// Used by the downstream pipeline to detect frame boundaries
bool H265Demuxer::isAccessUnitDelimiter(const NalUnit& nal) const {
    // HEVC NAL unit header (2 bytes):
    //   [0]: forbidden_zero_bit(1) | nal_unit_type(6) | nuh_layer_id(6 upper bits)
    //   [1]: nuh_layer_id(2 lower bits) | nuh_temporal_id_plus1(3)
    // nal_unit_type = bits [9:14] of the 16-bit header = (header >> 9) & 0x3F
    if (nal.data.size() < 2) return false;
    const uint16_t header = (static_cast<uint16_t>(nal.data[0]) << 8) | nal.data[1];
    const uint8_t nalType = (header >> 9) & 0x3F;
    return nalType == 35; // ACCESS_UNIT_DELIMITER_NUT
}

} // namespace aura

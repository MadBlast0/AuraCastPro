#pragma once
// =============================================================================
// H265Demuxer.h — H.265/HEVC access unit assembler
//
// Receives NAL units from NALParser and assembles them into complete
// access units (one complete video frame) ready for the VideoDecoder.
//
// Also extracts and caches parameter sets (VPS/SPS/PPS) and prepends
// them to the first frame and every IDR frame (required by the decoder).
// =============================================================================
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>

namespace aura {

// NalUnit is defined in NALParser.h (included below)

struct AccessUnit {
    std::vector<uint8_t> annexBData;  // Complete access unit in Annex B format
    bool     isKeyframe{false};
    uint64_t presentationTimeUs{0};
    uint32_t width{0};
    uint32_t height{0};
};

class H265Demuxer {
public:
    using AccessUnitCallback = std::function<void(AccessUnit au)>;

    H265Demuxer();
    ~H265Demuxer();

    void init();
    void reset();

    // Feed a parsed NAL unit (from NALParser)
    void feedNAL(NalUnit nal, uint64_t presentationTimeUs);

    void setCallback(AccessUnitCallback cb);

    bool hasParameterSets() const { return m_hasVPS && m_hasSPS && m_hasPPS; }

private:
    bool m_hasVPS{false};
    bool m_hasSPS{false};
    bool m_hasPPS{false};

    std::vector<uint8_t> m_vps;
    std::vector<uint8_t> m_sps;
    std::vector<uint8_t> m_pps;

    std::vector<NalUnit>  m_pending;    // NALs for current access unit
    uint64_t              m_currentPts{0};

    AccessUnitCallback m_callback;

    void emitAccessUnit();
    bool isAccessUnitDelimiter(const NalUnit& nal) const;
};

} // namespace aura

#include "../pch.h"  // PCH
#include "H264Demuxer.h"
#include "../utils/Logger.h"
#include <cstring>

H264NALType H264Demuxer::nalType(const uint8_t* data) {
    if (!data) return H264NALType::Unknown;
    uint8_t t = data[0] & 0x1F;
    switch (t) {
        case 1:  return H264NALType::NonIDR;
        case 5:  return H264NALType::IDR;
        case 6:  return H264NALType::SEI;
        case 7:  return H264NALType::SPS;
        case 8:  return H264NALType::PPS;
        case 9:  return H264NALType::AUD;
        case 12: return H264NALType::Filler;
        default: return H264NALType::Unknown;
    }
}

void H264Demuxer::feedNAL(const uint8_t* data, size_t size, int64_t tsUs) {
    if (!data || size == 0) return;

    H264NALType type = nalType(data);

    switch (type) {
        case H264NALType::SPS:
            m_sps.assign(data, data + size);
            m_hasSPS = true;
            LOG_DEBUG("H264Demuxer: Got SPS ({} bytes)", size);
            break;

        case H264NALType::PPS:
            m_pps.assign(data, data + size);
            m_hasPPS = true;
            LOG_DEBUG("H264Demuxer: Got PPS ({} bytes)", size);
            break;

        case H264NALType::IDR:
            // Flush any pending non-IDR before this IDR
            if (!m_pendingVCL.empty()) flush();
            m_pendingVCL.assign(data, data + size);
            m_pendingIsIDR = true;
            m_pendingTs    = tsUs;
            flush();
            break;

        case H264NALType::NonIDR:
            // New slice starts a new access unit -- flush previous
            if (!m_pendingVCL.empty()) flush();
            m_pendingVCL.assign(data, data + size);
            m_pendingIsIDR = false;
            m_pendingTs    = tsUs;
            break;

        case H264NALType::AUD:
            // Access Unit Delimiter -- boundary marker, flush pending
            if (!m_pendingVCL.empty()) flush();
            break;

        case H264NALType::SEI:
        case H264NALType::Filler:
        case H264NALType::Unknown:
            // Ignore
            break;
    }
}

void H264Demuxer::flush() {
    if (m_pendingVCL.empty()) return;
    if (!m_hasSPS || !m_hasPPS) {
        LOG_WARN("H264Demuxer: Dropping frame -- waiting for SPS/PPS");
        m_pendingVCL.clear();
        return;
    }

    H264AccessUnit au;
    au.sps         = m_sps;
    au.pps         = m_pps;
    au.vcl         = std::move(m_pendingVCL);
    au.isIDR       = m_pendingIsIDR;
    au.timestampUs = m_pendingTs;

    m_pendingVCL.clear();
    m_pendingIsIDR = false;

    if (m_callback) m_callback(std::move(au));
}

void H264Demuxer::reset() {
    m_sps.clear();
    m_pps.clear();
    m_pendingVCL.clear();
    m_hasSPS       = false;
    m_hasPPS       = false;
    m_pendingIsIDR = false;
    m_pendingTs    = 0;
}

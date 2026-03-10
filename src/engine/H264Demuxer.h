#pragma once
#include <vector>
#include <functional>
#include <cstdint>

// H.264/AVC NAL unit types
enum class H264NALType : uint8_t {
    NonIDR  = 1,
    IDR     = 5,
    SEI     = 6,
    SPS     = 7,
    PPS     = 8,
    AUD     = 9,
    Filler  = 12,
    Unknown = 0xFF
};

struct H264AccessUnit {
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    std::vector<uint8_t> vcl;  // IDR or non-IDR slice data
    bool isIDR = false;
    int64_t timestampUs = 0;
};

// Groups H.264 NAL units into complete access units for the decoder.
// Callback fires once per complete frame.
class H264Demuxer {
public:
    using FrameCallback = std::function<void(H264AccessUnit)>;

    void setFrameCallback(FrameCallback cb) { m_callback = std::move(cb); }

    // Feed a single NAL unit (without start code)
    void feedNAL(const uint8_t* data, size_t size, int64_t timestampUs = 0);

    void reset();

private:
    FrameCallback m_callback;

    std::vector<uint8_t> m_sps;
    std::vector<uint8_t> m_pps;
    std::vector<uint8_t> m_pendingVCL;
    bool   m_hasSPS  = false;
    bool   m_hasPPS  = false;
    bool   m_pendingIsIDR = false;
    int64_t m_pendingTs  = 0;

    void flush();
    static H264NALType nalType(const uint8_t* data);
};

#pragma once
// =============================================================================
// HardwareEncoder.h -- GPU hardware encoder (NVENC/AMF/QuickSync)
// Used for RTMP streaming when re-encoding is needed (e.g. bitrate shaping)
// =============================================================================
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>

struct ID3D12Resource;

namespace aura {

enum class EncoderType { Auto, NVENC, AMF, QuickSync, Software };

struct EncoderConfig {
    uint32_t    width{1920};
    uint32_t    height{1080};
    uint32_t    fps{60};
    int         bitrateKbps{8000};
    std::string codec{"H264"};  // H264 or H265
    EncoderType preferred{EncoderType::Auto};
};

struct EncodedPacket {
    std::vector<uint8_t> data;
    int64_t pts{0};
    bool    isKeyframe{false};
};

class HardwareEncoder {
public:
    using PacketCallback = std::function<void(const EncodedPacket&)>;

    HardwareEncoder();
    ~HardwareEncoder();

    void init();
    void shutdown();

    bool open(const EncoderConfig& cfg);
    void close();

    // Feed a decoded NV12 frame (from VideoDecoder)
    void feedFrame(ID3D12Resource* nv12Texture, int64_t pts);

    void setCallback(PacketCallback cb);

    EncoderType activeEncoder() const { return m_activeEncoder; }
    bool isOpen() const { return m_open.load(); }

private:
    std::atomic<bool> m_open{false};
    EncoderType       m_activeEncoder{EncoderType::Software};
    PacketCallback    m_callback;

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    EncoderType detectBestEncoder() const;
};

} // namespace aura

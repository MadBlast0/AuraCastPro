#pragma once
// =============================================================================
// SoftwareVideoDecoder.h -- FFmpeg software H.265/H.264 decoder (fallback)
// =============================================================================

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>

struct ID3D12Device;
struct ID3D12Resource;

namespace aura {

enum class VideoCodec;
struct DecodedFrame;

class SoftwareVideoDecoder {
public:
    using FrameCallback = std::function<void(const DecodedFrame&)>;

    SoftwareVideoDecoder(ID3D12Device* device, VideoCodec codec);
    ~SoftwareVideoDecoder();

    void init();
    void shutdown();

    bool submitNAL(const std::vector<uint8_t>& annexBData, bool isKeyframe,
                   uint64_t presentationTimeUs);
    void processOutput();
    void setFrameCallback(FrameCallback cb);
    void resetState();

    bool isInitialised() const { return m_initialised.load(); }
    VideoCodec codec() const { return m_codec; }
    uint64_t framesDecoded() const { return m_decoded.load(); }
    uint64_t decodeErrors() const { return m_errors.load(); }

private:
    ID3D12Device* m_device;
    VideoCodec    m_codec;
    std::atomic<bool>     m_initialised{false};
    FrameCallback         m_frameCallback;
    std::atomic<uint64_t> m_decoded{0};
    std::atomic<uint64_t> m_errors{0};
    uint32_t m_frameIndex{0};

    struct FFmpegState;
    std::unique_ptr<FFmpegState> m_ffmpeg;
};

} // namespace aura

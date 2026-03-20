#pragma once
// =============================================================================
// VideoDecoder.h -- Hardware video decoder (Windows Media Foundation / DXVA2)
//
// Decodes H.265/HEVC and AV1 using GPU hardware decoders.
// Output: NV12 texture in GPU memory -- zero CPU copies.
// =============================================================================

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>

struct ID3D12Device;
struct ID3D12Resource;

namespace aura {

enum class VideoCodec { H264, H265, AV1 };

struct DecodedFrame {
    ID3D12Resource* texture{nullptr};
    uint32_t        width{};
    uint32_t        height{};
    uint64_t        presentationTimeUs{};
    bool            isKeyframe{};
    uint32_t        frameIndex{};
    double          gpuDecodeTimeMs{0.0}; // GPU decode time for stats overlay
    bool            wasDropped{false};    // true if frame was dropped (late/corrupt)
};

class VideoDecoder {
public:
    using FrameCallback = std::function<void(const DecodedFrame&)>;

    VideoDecoder(ID3D12Device* device, VideoCodec codec);
    ~VideoDecoder();

    void init();
    void shutdown();

    bool submitNAL(const std::vector<uint8_t>& annexBData, bool isKeyframe,
                   uint64_t presentationTimeUs);
    void processOutput();
    void setFrameCallback(FrameCallback cb);

    // Reset decoder state on stream restart (new session, seek)
    void resetState();

    bool isInitialised() const { return m_initialised.load(); }
    VideoCodec codec() const { return m_codec; }
    uint64_t framesDecoded() const { return m_decoded.load(); }
    uint64_t decodeErrors() const { return m_errors.load(); }

    // Static method to detect which codecs are available on the system
    static bool isCodecAvailable(VideoCodec codec);

private:
    ID3D12Device* m_device;
    VideoCodec    m_codec;
    std::atomic<bool>     m_initialised{false};
    FrameCallback         m_frameCallback;
    std::atomic<uint64_t> m_decoded{0};
    std::atomic<uint64_t> m_errors{0};
    uint32_t m_frameIndex{0};
    bool m_hasParameterSets{false};

    struct MFState;
    std::unique_ptr<MFState> m_mf;
};

} // namespace aura

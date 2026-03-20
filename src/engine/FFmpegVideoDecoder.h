#pragma once
// Simple FFmpeg H.264 decoder - handles AVCC format natively

#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <cstdint>

struct AVCodecContext;
struct AVCodec;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace aura {

struct DecodedVideoFrame {
    uint8_t* yPlane{nullptr};
    uint8_t* uPlane{nullptr};
    uint8_t* vPlane{nullptr};
    int width{0};
    int height{0};
    int yStride{0};
    int uStride{0};
    int vStride{0};
    uint64_t pts{0};
};

class FFmpegVideoDecoder {
public:
    using FrameCallback = std::function<void(const DecodedVideoFrame&)>;

    FFmpegVideoDecoder();
    ~FFmpegVideoDecoder();

    void init();
    void shutdown();
    
    // Feed raw data (AVCC format, RTP, or Annex-B - FFmpeg handles all)
    void feedData(const uint8_t* data, size_t size);
    
    // Process any available decoded frames
    void processFrames();
    
    void setFrameCallback(FrameCallback cb);
    
    int decodedFrameCount() const { return m_decodedFrames.load(); }

private:
    AVCodecContext* m_codecCtx{nullptr};
    const AVCodec* m_codec{nullptr};
    AVFrame* m_frame{nullptr};
    AVPacket* m_packet{nullptr};
    
    FrameCallback m_frameCallback;
    std::atomic<int> m_decodedFrames{0};
    std::atomic<bool> m_initialized{false};
};

} // namespace aura

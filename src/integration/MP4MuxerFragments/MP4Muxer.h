#pragma once
// =============================================================================
// MP4Muxer.h — Fragmented MP4 muxer (frag_keyframe + empty_moov)
//
// Wraps FFmpeg libavformat for fragmented MP4 output.
// Used by StreamRecorder for crash-safe recordings.
// Each keyframe starts a new fragment — playable even if app crashes.
// =============================================================================
#include <string>
#include <cstdint>
#include <memory>
#include <vector>

struct AVFormatContext;
struct AVStream;

namespace aura {

class MP4Muxer {
public:
    MP4Muxer();
    ~MP4Muxer();

    bool open(const std::string& outputPath, uint32_t width, uint32_t height,
              uint32_t fps, const std::string& videoCodec = "H265");
    void close();

    void writeVideoPacket(const uint8_t* data, size_t size,
                          int64_t pts, bool isKeyframe);
    void writeAudioPacket(const uint8_t* data, size_t size, int64_t pts);

    bool isOpen() const { return m_open; }
    uint64_t bytesWritten() const { return m_bytesWritten; }

private:
    bool           m_open{false};
    uint64_t       m_bytesWritten{0};

    AVFormatContext* m_fmtCtx{nullptr};
    AVStream*        m_videoStream{nullptr};
    AVStream*        m_audioStream{nullptr};
};

} // namespace aura

#pragma once
// =============================================================================
// StreamRecorder.h -- Fragmented MP4 recording via FFmpeg
// =============================================================================
#include <string>
#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>

namespace aura {

class StreamRecorder {
public:
    StreamRecorder();
    ~StreamRecorder();

    void init();
    void shutdown();

    // Start recording to the given output path (.mp4)
    bool startRecording(const std::string& outputPath,
                        uint32_t videoWidth, uint32_t videoHeight,
                        uint32_t fps, const std::string& videoCodec);
    void stopRecording();
    void pauseRecording();
    void resumeRecording();

    // Feed compressed video packets (stream copy -- no re-encode)
    void onVideoPacket(const uint8_t* data, size_t size, int64_t pts, bool isKeyframe);
    void feedVideoPacket(const uint8_t* data, size_t size, int64_t pts, bool isKeyframe) {
        onVideoPacket(data, size, pts, isKeyframe);
    }

    // Feed raw PCM audio (encoded to AAC internally)
    void onAudioSamples(const float* samples, uint32_t numFrames,
                        uint32_t sampleRate, uint32_t channels);

    bool isRecording() const { return m_recording.load(); }
    bool isPaused()    const { return m_paused.load(); }
    uint64_t bytesWritten() const { return m_bytesWritten.load(); }
    double   durationSec()  const;

    // Generate timestamped filename: "AuraCastPro_2025-01-15_14-32-07.mp4"
    static std::string generateOutputPath(const std::string& folder);

private:
    std::atomic<bool>     m_recording{false};
    std::atomic<bool>     m_paused{false};
    std::atomic<uint64_t> m_bytesWritten{0};
    int64_t               m_startPts{0};

    struct FFmpegState;
    std::unique_ptr<FFmpegState> m_ffmpeg;

    bool initVideoStream(uint32_t w, uint32_t h, uint32_t fps,
                         const std::string& codec);
    bool initAudioStream(uint32_t sampleRate, uint32_t channels);
    void flushAudioEncoder();
};

} // namespace aura

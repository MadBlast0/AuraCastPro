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

struct RecordingConfig {
    uint32_t videoWidth{1920};
    uint32_t videoHeight{1080};
    uint32_t fps{60};
    std::string videoCodec{"hevc"};
    std::string containerFormat{"mp4"};  // mp4, mkv, flv, mov, avi
    std::string videoEncoder{"copy"};    // copy, h264_nvenc, h265_nvenc, libx264, libx265
    std::string audioEncoder{"aac"};     // aac, opus, mp3, flac
    uint32_t videoBitrateKbps{50000};
    uint32_t audioBitrateKbps{192};
    std::string rateControl{"cbr"};      // cbr, vbr, cqp, lossless
    std::string encoderPreset{"medium"}; // ultrafast, fast, medium, slow, veryslow
    std::string encoderProfile{"high"};  // baseline, main, high
    std::string encoderTuning{"hq"};     // hq, ll, ull, lossless
    std::string multipassMode{"single"}; // single, two-pass
    int keyframeInterval{0};             // 0 = auto
    bool lookAhead{true};
    bool adaptiveQuantization{true};
    int bFrames{2};
    std::string customEncoderOptions{};
    std::string customMuxerSettings{};
    int audioTrackMask{1};               // Bitmask for audio tracks (1 = track 1 enabled)
    std::string rescaleFilter{"lanczos"};
    std::string rescaleResolution{};     // Empty = no rescale
    bool autoFileSplitting{false};
    std::string fileSplitMode{"time"};   // time, size
    uint64_t fileSplitSizeMB{2000};      // 2GB default
    uint32_t fileSplitTimeMinutes{60};   // 60 minutes default
    bool generateFileNameWithoutSpace{false};
};

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
    
    // Enhanced start recording with full configuration
    bool startRecording(const std::string& outputPath, const RecordingConfig& config);
    
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
    static std::string generateOutputPath(const std::string& folder, 
                                          const std::string& extension = ".mp4",
                                          bool noSpaces = false);

private:
    std::atomic<bool>     m_recording{false};
    std::atomic<bool>     m_paused{false};
    std::atomic<uint64_t> m_bytesWritten{0};
    int64_t               m_startPts{0};
    RecordingConfig       m_config;

    struct FFmpegState;
    std::unique_ptr<FFmpegState> m_ffmpeg;

    bool initVideoStream();
    bool initAudioStream(uint32_t sampleRate, uint32_t channels);
    void flushAudioEncoder();
    std::string mapEncoderName(const std::string& encoderSetting);
    AVCodecID mapAudioCodecId(const std::string& audioEncoder);
    void applyEncoderSettings(AVCodecContext* ctx);
    bool initRescaler();
    bool shouldSplitFile();
    bool splitToNewFile();
    std::string generateSplitFilePath();
};

} // namespace aura

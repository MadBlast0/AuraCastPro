#pragma once
// =============================================================================
// RTMPOutput.h — RTMP/RTMPS live streaming output
// =============================================================================
#include <string>
#include <memory>
#include <atomic>
#include <cstdint>

namespace aura {

struct RTMPConfig {
    std::string url;            // rtmp://live.twitch.tv/app/<key>
    std::string videoCodec{"H264"}; // H264 (max compat) or H265 (RTMPS only)
    int         videoBitrateKbps{4000};
    int         audioBitrateKbps{128};
    uint32_t    width{1920};
    uint32_t    height{1080};
    uint32_t    fps{60};
};

class RTMPOutput {
public:
    RTMPOutput();
    ~RTMPOutput();

    void init();
    void shutdown();

    bool connect(const RTMPConfig& config);
    void disconnect();

    void sendVideoPacket(const uint8_t* data, size_t size, int64_t pts, bool keyframe);
    void sendAudioSamples(const float* samples, uint32_t numFrames,
                          uint32_t sampleRate, uint32_t channels);

    bool isConnected() const { return m_connected.load(); }
    uint64_t bytesStreamed() const { return m_bytesStreamed.load(); }
    double   streamDurationSec() const;

private:
    std::atomic<bool>     m_connected{false};
    std::atomic<uint64_t> m_bytesStreamed{0};
    int64_t               m_startTime{0};

    struct FFmpegRTMP;
    std::unique_ptr<FFmpegRTMP> m_rtmp;

    bool initVideoStream(const RTMPConfig& cfg);
    bool initAudioStream(const RTMPConfig& cfg);
    void flushAudioEncoder();
};

} // namespace aura

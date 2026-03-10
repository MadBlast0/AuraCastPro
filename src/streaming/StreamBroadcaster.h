#pragma once
// =============================================================================
// StreamBroadcaster.h — Fan-out: sends frames to multiple sinks simultaneously
// (VCamBridge, StreamRecorder, RTMPOutput) without extra encoding copies
// =============================================================================
#include <functional>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

namespace aura {

class VCamBridge;
class StreamRecorder;
class RTMPOutput;

struct BroadcastPacket {
    const uint8_t* videoData{nullptr};
    size_t         videoSize{0};
    int64_t        pts{0};
    bool           isKeyframe{false};
    const float*   audioSamples{nullptr};
    uint32_t       audioNumFrames{0};
    uint32_t       audioSampleRate{48000};
    uint32_t       audioChannels{2};
};

class StreamBroadcaster {
public:
    StreamBroadcaster(VCamBridge*    vcam,
                      StreamRecorder* recorder,
                      RTMPOutput*    rtmp);
    ~StreamBroadcaster();

    void init();
    void start();
    void stop();
    void shutdown();

    // Single call distributes to all active sinks
    void broadcast(const BroadcastPacket& pkt);

    void setRecordingEnabled(bool v) { m_recordingEnabled = v; }
    void setRTMPEnabled(bool v)      { m_rtmpEnabled = v; }
    void setVCamEnabled(bool v)      { m_vcamEnabled = v; }

private:
    VCamBridge*     m_vcam;
    StreamRecorder* m_recorder;
    RTMPOutput*     m_rtmp;

    bool m_recordingEnabled{false};
    bool m_rtmpEnabled{false};
    bool m_vcamEnabled{true};
    bool m_running{false};
};

} // namespace aura

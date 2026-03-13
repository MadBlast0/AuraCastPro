// =============================================================================
// StreamBroadcaster.cpp -- Multi-sink frame distribution
// =============================================================================
#include "../pch.h"  // PCH
#include "StreamBroadcaster.h"
#include "RTMPOutput.h"
#include "../integration/VCamBridge.h"
#include "../integration/StreamRecorder.h"
#include "../utils/Logger.h"

namespace aura {

StreamBroadcaster::StreamBroadcaster(VCamBridge* vcam,
                                     StreamRecorder* recorder,
                                     RTMPOutput* rtmp)
    : m_vcam(vcam), m_recorder(recorder), m_rtmp(rtmp) {}

StreamBroadcaster::~StreamBroadcaster() {}

void StreamBroadcaster::init() {
    AURA_LOG_INFO("StreamBroadcaster",
        "Initialised. Sinks: VirtualCamera + Recorder + RTMP. "
        "Zero-copy fan-out -- same compressed packet sent to all active sinks.");
}

void StreamBroadcaster::start() {
    m_running = true;
    AURA_LOG_INFO("StreamBroadcaster", "Broadcasting started.");
}

void StreamBroadcaster::stop() {
    m_running = false;
}

void StreamBroadcaster::shutdown() { stop(); }

void StreamBroadcaster::broadcast(const BroadcastPacket& pkt) {
    if (!m_running) return;

    // Send to recording (video + audio)
    if (m_recordingEnabled && m_recorder && m_recorder->isRecording()) {
        m_recorder->onVideoPacket(pkt.videoData, pkt.videoSize,
                                  pkt.pts, pkt.isKeyframe);
        if (pkt.audioSamples && pkt.audioNumFrames > 0) {
            m_recorder->onAudioSamples(pkt.audioSamples, pkt.audioNumFrames,
                                       pkt.audioSampleRate, pkt.audioChannels);
        }
    }

    // Send to RTMP (video + audio)
    if (m_rtmpEnabled && m_rtmp && m_rtmp->isConnected()) {
        m_rtmp->sendVideoPacket(pkt.videoData, pkt.videoSize,
                                pkt.pts, pkt.isKeyframe);
        if (pkt.audioSamples && pkt.audioNumFrames > 0) {
            m_rtmp->sendAudioSamples(pkt.audioSamples, pkt.audioNumFrames,
                                     pkt.audioSampleRate, pkt.audioChannels);
        }
    }
    // VCam receives the rendered GPU frame directly (not compressed packets)
    // so it's driven separately from the render loop, not here.
}

} // namespace aura

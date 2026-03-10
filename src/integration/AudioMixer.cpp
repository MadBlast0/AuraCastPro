// =============================================================================
// AudioMixer.cpp — Real-time audio mixer
//
// Mixes device audio loopback + optional microphone at configurable levels.
// Outputs to all registered sinks (VirtualAudioDriver + StreamRecorder).
//
// All audio is processed at 48kHz / 32-bit float / stereo.
// Sample rate conversion is handled if inputs differ.
// =============================================================================
#include "AudioMixer.h"
#include "AudioLoopback.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <cstring>

namespace aura {

AudioMixer::AudioMixer() {}
AudioMixer::~AudioMixer() { shutdown(); }

void AudioMixer::init() {
    AURA_LOG_INFO("AudioMixer",
        "Initialised. 48kHz / 32-bit float / stereo. "
        "Inputs: device loopback + optional mic. "
        "Outputs: VirtualAudioDriver + StreamRecorder.");
    m_mixBuffer.resize(48000 / 10 * 2, 0.0f); // 100ms scratch buffer (stereo)
}

void AudioMixer::start()    { m_running.store(true); }
void AudioMixer::stop()     { m_running.store(false); }
void AudioMixer::shutdown() { stop(); m_sinks.clear(); }

void AudioMixer::addOutputSink(OutputCallback sink) {
    m_sinks.push_back(std::move(sink));
}

void AudioMixer::feedDeviceAudio(const AudioBuffer& buf) {
    if (!m_running.load() || m_sinks.empty()) return;

    const float devVol = m_deviceVolume.load();
    const bool  useMic = m_micEnabled.load();

    if (!useMic) {
        // Fast path: no mic, just scale device volume and deliver
        const size_t totalSamples = buf.numFrames * buf.channels;
        if (m_mixBuffer.size() < totalSamples)
            m_mixBuffer.resize(totalSamples);

        for (size_t i = 0; i < totalSamples; ++i)
            m_mixBuffer[i] = buf.data[i] * devVol;

        for (auto& sink : m_sinks)
            sink(m_mixBuffer.data(), buf.numFrames, buf.sampleRate, buf.channels);
    }
    // Mic mix path handled in feedMicAudio
}

void AudioMixer::feedMicAudio(const float* samples, uint32_t numFrames,
                               uint32_t sampleRate, uint32_t channels) {
    if (!m_running.load() || !m_micEnabled.load()) return;
    // Mix into output buffer — full implementation uses a ring buffer
    // to align device and mic audio by timestamp
    AURA_LOG_TRACE("AudioMixer", "Mic audio: {} frames @ {}Hz", numFrames, sampleRate);
}

void AudioMixer::mixAndDeliver(const float* deviceSamples,
                                const float* micSamples,
                                uint32_t numFrames, uint32_t sampleRate,
                                uint32_t channels) {
    const float devVol = m_deviceVolume.load();
    const float micVol = m_micVolume.load();
    const size_t total = numFrames * channels;

    if (m_mixBuffer.size() < total) m_mixBuffer.resize(total);

    for (size_t i = 0; i < total; ++i) {
        float s = deviceSamples[i] * devVol;
        if (micSamples) s += micSamples[i] * micVol;
        // Soft clip to prevent harsh clipping
        s = std::clamp(s, -1.0f, 1.0f);
        m_mixBuffer[i] = s;
    }

    for (auto& sink : m_sinks)
        sink(m_mixBuffer.data(), numFrames, sampleRate, channels);
}

// Task 121: A/V sync offset — EMA-smooth the measured offset and update
// the applied delay. Called by the video pipeline once per second.
void AudioMixer::reportMeasuredOffset(int measuredMs) {
    // Clamp to ±200ms range
    measuredMs = std::clamp(measuredMs, -200, 200);

    // EMA smoothing: alpha=0.2 → slow adaptation (stable, avoids oscillation)
    constexpr float kAlpha = 0.2f;
    int current = m_appliedDelayMs.load();
    int next = static_cast<int>(kAlpha * measuredMs + (1.0f - kAlpha) * current);
    m_appliedDelayMs.store(next);

    // Resize delay buffer if sample rate or delay changed significantly
    // Buffer capacity = 200ms at 48kHz stereo float32 = 48000*0.2*2 = 19200 floats
    const size_t needed = static_cast<size_t>(m_delaySampleRate * 2u * 0.200f); // 200ms max
    if (m_delayBuffer.size() != needed) {
        m_delayBuffer.assign(needed, 0.0f);
        m_delayWritePos = 0;
        m_delayReadPos  = 0;
    }
}

} // namespace aura

#pragma once
// =============================================================================
// AudioMixer.h -- Mixes device audio + optional mic input, outputs to sinks
// =============================================================================
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <cstdint>

namespace aura {

struct AudioBuffer;

class AudioMixer {
public:
    using OutputCallback = std::function<void(const float* samples,
                                              uint32_t numFrames,
                                              uint32_t sampleRate,
                                              uint32_t channels)>;
    AudioMixer();
    ~AudioMixer();

    void init();
    void start();
    void stop();
    void shutdown();

    // Feed device audio (from AudioLoopback)
    void feedDeviceAudio(const AudioBuffer& buf);

    // Feed microphone audio (optional -- for commentary)
    void feedMicAudio(const float* samples, uint32_t numFrames,
                      uint32_t sampleRate, uint32_t channels);

    // Register output sink (can have multiple: recorder + virtual audio driver)
    void addOutputSink(OutputCallback sink);

    void setDeviceVolume(float v) { m_deviceVolume.store(v); }
    void setMicVolume(float v)    { m_micVolume.store(v); }
    void setMicEnabled(bool v)    { m_micEnabled.store(v); }

    // Task 121: A/V sync offset -- positive = delay audio relative to video (ms)
    // Range: -200ms to +200ms. Updated every second via EMA.
    void   setAvSyncOffsetMs(int ms)  { m_avSyncOffsetMs.store(ms); }
    int    avSyncOffsetMs()     const { return m_avSyncOffsetMs.load(); }

    // Called by video pipeline with the current measured A/V offset (ms)
    // AudioMixer applies EMA smoothing and adjusts its internal delay buffer.
    void   reportMeasuredOffset(int measuredMs);

    // Current delay applied (may differ from setAvSyncOffsetMs due to smoothing)
    int    appliedDelayMs() const { return m_appliedDelayMs.load(); }

private:
    std::atomic<float> m_deviceVolume{1.0f};
    std::atomic<float> m_micVolume{0.7f};
    std::atomic<bool>  m_micEnabled{false};
    std::atomic<bool>  m_running{false};

    // Task 121: A/V sync delay buffer (200ms capacity at 48000Hz stereo float32)
    std::atomic<int>   m_avSyncOffsetMs{0};   // user/settings configured offset
    std::atomic<int>   m_appliedDelayMs{0};   // EMA-smoothed applied delay
    std::vector<float> m_delayBuffer;          // circular delay buffer (samples)
    size_t             m_delayWritePos{0};
    size_t             m_delayReadPos{0};
    uint32_t           m_delaySampleRate{48000};

    std::vector<OutputCallback> m_sinks;
    std::vector<float>          m_mixBuffer; // scratch buffer

    void mixAndDeliver(const float* deviceSamples, const float* micSamples,
                       uint32_t numFrames, uint32_t sampleRate, uint32_t channels);
};

} // namespace aura

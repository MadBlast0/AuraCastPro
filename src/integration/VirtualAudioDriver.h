#pragma once
// =============================================================================
// VirtualAudioDriver.h -- Virtual audio output device driver interface
//
// Exposes the mixed device audio as a virtual audio input device so
// recording software (OBS, Audacity) can capture it separately from
// the system audio mix.
//
// Uses a Windows kernel-mode audio driver (Audio_Installer.inf).
// The driver creates a virtual loopback: AudioMixer output -> virtual input.
// =============================================================================
#include <cstdint>
#include <atomic>
#include <memory>

namespace aura {

class VirtualAudioDriver {
public:
    VirtualAudioDriver();
    ~VirtualAudioDriver();

    void init();
    void start();
    void stop();
    void shutdown();

    // Write PCM samples to the virtual device (from AudioMixer output)
    void writeSamples(const float* samples, uint32_t numFrames,
                      uint32_t sampleRate, uint32_t channels);

    bool isDriverInstalled() const { return m_driverInstalled; }
    bool isRunning()         const { return m_running.load(); }

private:
    bool               m_driverInstalled{false};
    std::atomic<bool>  m_running{false};

    struct DriverState;
    std::unique_ptr<DriverState> m_state;

    bool checkDriverInstalled() const;
};

} // namespace aura

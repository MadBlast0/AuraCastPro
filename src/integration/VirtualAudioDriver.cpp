// =============================================================================
// VirtualAudioDriver.cpp — Virtual audio device output
//
// The virtual audio driver (AudioVirtualCable.sys) is a kernel-mode
// WDM audio driver that creates two endpoints:
//   "AuraCastPro Audio Output"  — receives mixed PCM from AudioMixer
//   "AuraCastPro Audio Input"   — exposes same audio as a capture source
//
// This user-mode component writes PCM to a shared memory region that
// the kernel driver reads via a kernel event + circular buffer.
//
// Shared memory: "Global\\AuraCastPro_AudioBridge_v1"
// Event:         "Global\\AuraCastPro_AudioReady_v1"
// =============================================================================
#include "VirtualAudioDriver.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>

namespace aura {

// Shared memory layout
struct AudioSharedMemory {
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t writePos;           // write cursor (frames)
    uint32_t readPos;            // read cursor (set by driver)
    uint32_t bufferFrames;       // ring buffer size in frames
    float    samples[48000 * 2]; // 1 second stereo @ 48kHz
};

struct VirtualAudioDriver::DriverState {
    HANDLE            hMapping{nullptr};
    AudioSharedMemory* sharedMem{nullptr};
    HANDLE            hReadyEvent{nullptr};
};

VirtualAudioDriver::VirtualAudioDriver() : m_state(std::make_unique<DriverState>()) {}
VirtualAudioDriver::~VirtualAudioDriver() { shutdown(); }

bool VirtualAudioDriver::checkDriverInstalled() const {
    // Check if our virtual device appears in the Device Manager
    // by looking for the service registry key
    HKEY hKey;
    const LONG result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\AuraCastAudio",
        0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

void VirtualAudioDriver::init() {
    m_driverInstalled = checkDriverInstalled();

    if (!m_driverInstalled) {
        AURA_LOG_WARN("VirtualAudioDriver",
            "Virtual audio driver not installed. "
            "Run the installer to install AudioVirtualCable.sys, "
            "or run: pnputil /add-driver drivers/Audio_Installer.inf /install");
        return;
    }

    // Create shared memory region
    m_state->hMapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        0, sizeof(AudioSharedMemory),
        L"Global\\AuraCastPro_AudioBridge_v1");

    if (m_state->hMapping) {
        m_state->sharedMem = static_cast<AudioSharedMemory*>(
            MapViewOfFile(m_state->hMapping, FILE_MAP_ALL_ACCESS,
                          0, 0, sizeof(AudioSharedMemory)));
    }

    if (m_state->sharedMem) {
        m_state->sharedMem->sampleRate   = 48000;
        m_state->sharedMem->channels     = 2;
        m_state->sharedMem->writePos     = 0;
        m_state->sharedMem->readPos      = 0;
        m_state->sharedMem->bufferFrames = 48000; // 1s ring buffer
    }

    m_state->hReadyEvent = CreateEventW(
        nullptr, FALSE, FALSE, L"Global\\AuraCastPro_AudioReady_v1");

    AURA_LOG_INFO("VirtualAudioDriver",
        "Virtual audio driver initialised. "
        "Shared memory: 1s stereo ring buffer @ 48kHz.");
}

void VirtualAudioDriver::start() {
    if (!m_driverInstalled) return;
    m_running.store(true);
    AURA_LOG_INFO("VirtualAudioDriver", "Virtual audio output active.");
}

void VirtualAudioDriver::stop() {
    m_running.store(false);
}

void VirtualAudioDriver::writeSamples(const float* samples, uint32_t numFrames,
                                       uint32_t /*sampleRate*/, uint32_t /*channels*/) {
    if (!m_running.load() || !m_state->sharedMem) return;

    AudioSharedMemory* shm = m_state->sharedMem;
    const uint32_t bufSize = shm->bufferFrames * 2; // stereo

    for (uint32_t i = 0; i < numFrames * 2; ++i) {
        shm->samples[shm->writePos % bufSize] = samples[i];
        shm->writePos++;
    }

    // Signal the driver that new audio is available
    if (m_state->hReadyEvent) SetEvent(m_state->hReadyEvent);
}

void VirtualAudioDriver::shutdown() {
    stop();
    if (m_state->sharedMem) {
        UnmapViewOfFile(m_state->sharedMem);
        m_state->sharedMem = nullptr;
    }
    if (m_state->hMapping) {
        CloseHandle(m_state->hMapping);
        m_state->hMapping = nullptr;
    }
    if (m_state->hReadyEvent) {
        CloseHandle(m_state->hReadyEvent);
        m_state->hReadyEvent = nullptr;
    }
}

} // namespace aura

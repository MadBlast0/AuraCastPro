// =============================================================================
// AudioLoopback.cpp -- WASAPI loopback audio capture
//
// Uses IAudioClient in shared loopback mode to capture the system audio mix.
// The captured PCM is delivered via callback to the AudioMixer.
//
// WASAPI initialisation sequence:
//   1. CoCreateInstance(CLSID_MMDeviceEnumerator) -> IMMDeviceEnumerator
//   2. GetDefaultAudioEndpoint(eRender, eConsole) -> IMMDevice
//      (eRender = output device; loopback captures what's going to speakers)
//   3. device->Activate(IID_IAudioClient) -> IAudioClient
//   4. GetMixFormat() -> WAVEFORMATEX (native format: 48kHz / 32-bit float / 2ch)
//   5. Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
//                 bufferDuration=200ms, ...)
//   6. GetService(IID_IAudioCaptureClient) -> IAudioCaptureClient
//   7. Start() -> begin capture
//   8. Loop: GetBuffer() -> process -> ReleaseBuffer()
// =============================================================================

#include "../pch.h"  // PCH
#include "AudioLoopback.h"
#include <mutex>
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <thread>
#include <stdexcept>
#include <format>

#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

// WASAPI CLSID / IID (not in all SDK versions as constexpr)
static const CLSID CLSID_MMDeviceEnumerator_  = __uuidof(MMDeviceEnumerator);
static const IID   IID_IMMDeviceEnumerator_   = __uuidof(IMMDeviceEnumerator);
static const IID   IID_IAudioClient_          = __uuidof(IAudioClient);
static const IID   IID_IAudioCaptureClient_   = __uuidof(IAudioCaptureClient);

namespace aura {

struct AudioLoopback::WASAPIState {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice>           device;
    ComPtr<IAudioClient>        client;
    ComPtr<IAudioCaptureClient> captureClient;
    WAVEFORMATEX*               waveFormat{nullptr};
    UINT32                      bufferFrameCount{0};
    HANDLE                      readyEvent{nullptr};
    std::thread                 captureThread;
    bool                        initialised{false};
};

// -----------------------------------------------------------------------------
AudioLoopback::AudioLoopback()
    : m_wasapi(std::make_unique<WASAPIState>()) {}

AudioLoopback::~AudioLoopback() { shutdown(); }

// -----------------------------------------------------------------------------
void AudioLoopback::init() {
    AURA_LOG_INFO("AudioLoopback", "Initialising WASAPI loopback capture...");

    // COM must be initialised on this thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        throw std::runtime_error(
            std::format("AudioLoopback: CoInitializeEx failed: {:08X}", (uint32_t)hr));
    }

    // 1. Create device enumerator
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator_, nullptr, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator_,
                          reinterpret_cast<void**>(m_wasapi->enumerator.GetAddressOf()));
    if (FAILED(hr)) {
        throw std::runtime_error(
            std::format("AudioLoopback: CoCreateInstance MMDeviceEnumerator failed: {:08X}",
                        (uint32_t)hr));
    }

    // 2. Get render endpoint -- use selected device if set, else Windows default
    if (!m_deviceId.empty()) {
        // Activate a specific device by endpoint ID
        const std::wstring wid(m_deviceId.begin(), m_deviceId.end());
        hr = m_wasapi->enumerator->GetDevice(wid.c_str(),
            m_wasapi->device.GetAddressOf());
        if (FAILED(hr)) {
            AURA_LOG_WARN("AudioLoopback",
                "GetDevice({}) failed: {:08X} -- falling back to default",
                m_deviceId, (uint32_t)hr);
            m_deviceId.clear();
        } else {
            AURA_LOG_INFO("AudioLoopback",
                "Using selected audio endpoint: {}", m_deviceId);
        }
    }
    if (m_deviceId.empty()) {
        hr = m_wasapi->enumerator->GetDefaultAudioEndpoint(
            eRender, eConsole, m_wasapi->device.GetAddressOf());
        if (FAILED(hr)) {
            throw std::runtime_error(
                std::format("AudioLoopback: GetDefaultAudioEndpoint failed: {:08X}",
                            (uint32_t)hr));
        }
        AURA_LOG_INFO("AudioLoopback", "Using Windows default audio endpoint.");
    }

    // 3. Activate IAudioClient
    hr = m_wasapi->device->Activate(IID_IAudioClient_, CLSCTX_ALL,
                                    nullptr,
                                    reinterpret_cast<void**>(m_wasapi->client.GetAddressOf()));
    if (FAILED(hr)) {
        throw std::runtime_error(
            std::format("AudioLoopback: Activate IAudioClient failed: {:08X}", (uint32_t)hr));
    }

    // 4. Get native mix format (do NOT change this -- shared mode requires native format)
    hr = m_wasapi->client->GetMixFormat(&m_wasapi->waveFormat);
    if (FAILED(hr)) {
        throw std::runtime_error("AudioLoopback: GetMixFormat failed");
    }

    const WAVEFORMATEX& wf = *m_wasapi->waveFormat;
    AURA_LOG_INFO("AudioLoopback",
        "Native audio format: {}Hz / {} channels / {} bits",
        wf.nSamplesPerSec, wf.nChannels, wf.wBitsPerSample);

    // 5. Create event handle for notification
    m_wasapi->readyEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    // 6. Initialise with loopback flag and 200ms buffer
    constexpr REFERENCE_TIME bufferDuration = 2000000; // 200ms in 100ns units
    hr = m_wasapi->client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration, 0,
        m_wasapi->waveFormat,
        nullptr);
    if (FAILED(hr)) {
        throw std::runtime_error(
            std::format("AudioLoopback: Initialize failed: {:08X}", (uint32_t)hr));
    }

    hr = m_wasapi->client->SetEventHandle(m_wasapi->readyEvent);
    if (FAILED(hr)) {
        throw std::runtime_error("AudioLoopback: SetEventHandle failed");
    }

    m_wasapi->client->GetBufferSize(&m_wasapi->bufferFrameCount);

    // 7. Get capture client
    hr = m_wasapi->client->GetService(IID_IAudioCaptureClient_,
        reinterpret_cast<void**>(m_wasapi->captureClient.GetAddressOf()));
    if (FAILED(hr)) {
        throw std::runtime_error("AudioLoopback: GetService IAudioCaptureClient failed");
    }

    m_wasapi->initialised = true;
    AURA_LOG_INFO("AudioLoopback", "WASAPI loopback initialised. Buffer: {} frames.",
                  m_wasapi->bufferFrameCount);

    // Register for default-device-change notifications now that enumerator is live
    registerNotifier();
}

// -----------------------------------------------------------------------------
void AudioLoopback::start() {
    if (!m_wasapi->initialised) {
        AURA_LOG_ERROR("AudioLoopback", "start() called before init().");
        return;
    }
    if (m_running.exchange(true)) return;

    m_wasapi->client->Start();
    AURA_LOG_INFO("AudioLoopback", "Loopback capture started.");

    m_wasapi->captureThread = std::thread([this]() { captureLoop(); });
}

// -----------------------------------------------------------------------------
void AudioLoopback::captureLoop() {
    while (m_running.load()) {
        // Wait up to 200ms for audio data to be available
        WaitForSingleObject(m_wasapi->readyEvent, 200);
        if (!m_running.load()) break;

        BYTE*  data      = nullptr;
        UINT32 numFrames = 0;
        DWORD  flags     = 0;

        HRESULT hr = m_wasapi->captureClient->GetBuffer(
            &data, &numFrames, &flags, nullptr, nullptr);

        if (FAILED(hr)) continue;
        if (numFrames == 0) {
            m_wasapi->captureClient->ReleaseBuffer(0);
            continue;
        }

        AudioCallback localCb;
        {
            std::lock_guard<std::mutex> lk(m_callbackMu);
            localCb = m_callback;  // copy under lock -- avoids holding mutex during callback
        }
        if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && localCb && data) {
            const WAVEFORMATEX& wf = *m_wasapi->waveFormat;
            AudioBuffer buf;
            buf.data       = reinterpret_cast<const float*>(data);
            buf.numFrames  = numFrames;
            buf.sampleRate = wf.nSamplesPerSec;
            buf.channels   = wf.nChannels;
            localCb(buf);
        }

        m_wasapi->captureClient->ReleaseBuffer(numFrames);
        m_framesCapture.fetch_add(numFrames, std::memory_order_relaxed);
    }
}

// -----------------------------------------------------------------------------
void AudioLoopback::stop() {
    if (!m_running.exchange(false)) return;
    SetEvent(m_wasapi->readyEvent); // wake up the capture thread
    if (m_wasapi->captureThread.joinable()) m_wasapi->captureThread.join();
    if (m_wasapi->initialised) m_wasapi->client->Stop();
    AURA_LOG_INFO("AudioLoopback", "Stopped. Total frames captured: {}",
                  m_framesCapture.load());
}

void AudioLoopback::shutdown() {
    unregisterNotifier();
    stop();
}

void AudioLoopback::setCallback(AudioCallback cb) {
    std::lock_guard<std::mutex> lk(m_callbackMu);
    m_callback = std::move(cb);
}

// =============================================================================
// Task 119 -- Output device selection
// =============================================================================
void AudioLoopback::setOutputDevice(const std::string& deviceId) {
    if (m_deviceId == deviceId) return;
    m_deviceId = deviceId;

    AURA_LOG_INFO("AudioLoopback",
        "Output device selected: {}",
        deviceId.empty() ? "(Windows default)" : deviceId);

    // If already running, restart capture on the new device
    if (m_running.load()) {
        AURA_LOG_INFO("AudioLoopback",
            "Restarting WASAPI capture on new device...");
        stop();
        // Re-init with new device -- init() reads m_deviceId
        init();
        start();
    }
}


// -----------------------------------------------------------------------------
// initWASAPI() -- Re-opens the WASAPI capture session (called on device change)
// The actual setup logic lives in init(). This private helper resets COM state
// then delegates back so init() can re-enumerate from scratch.
bool AudioLoopback::initWASAPI() {
    try {
        // Release existing COM objects before re-initialising
        if (m_wasapi) {
            if (m_wasapi->captureClient) m_wasapi->captureClient.Reset();
            if (m_wasapi->client)        m_wasapi->client.Reset();
            if (m_wasapi->device)        m_wasapi->device.Reset();
            if (m_wasapi->enumerator)    m_wasapi->enumerator.Reset();
            m_wasapi->initialised = false;
        }
        init();
        return true;
    } catch (const std::exception& e) {
        AURA_LOG_ERROR("AudioLoopback",
            "initWASAPI failed: {}", e.what());
        return false;
    }
}

// -----------------------------------------------------------------------------
// registerNotifier() -- Subscribe to Windows default-device-change events.
// The DeviceNotifier COM object calls onDefaultDeviceChanged() when the user
// changes the default playback device in Windows Settings / Sound Control Panel.
// We grab the IMMDeviceEnumerator (already created in init()) and register there.
void AudioLoopback::registerNotifier() {
    if (m_notifier) return;  // already registered

    if (!m_wasapi || !m_wasapi->enumerator) {
        AURA_LOG_WARN("AudioLoopback",
            "Cannot register device notifier -- enumerator not ready.");
        return;
    }

    m_notifier = new DeviceNotifier(this);
    HRESULT hr = m_wasapi->enumerator->RegisterEndpointNotificationCallback(m_notifier);
    if (FAILED(hr)) {
        AURA_LOG_WARN("AudioLoopback",
            "RegisterEndpointNotificationCallback failed: 0x{:08X}. "
            "Default-device changes will require manual restart.", (uint32_t)hr);
        m_notifier->Release();
        m_notifier = nullptr;
    } else {
        AURA_LOG_DEBUG("AudioLoopback",
            "Default-device change notifier registered.");
    }
}

// -----------------------------------------------------------------------------
// unregisterNotifier() -- Unsubscribe before shutdown to prevent stale callbacks.
void AudioLoopback::unregisterNotifier() {
    if (!m_notifier) return;

    if (m_wasapi && m_wasapi->enumerator)
        m_wasapi->enumerator->UnregisterEndpointNotificationCallback(m_notifier);

    m_notifier->Release();
    m_notifier = nullptr;
    AURA_LOG_DEBUG("AudioLoopback", "Device notifier unregistered.");
}

// -----------------------------------------------------------------------------
// onDefaultDeviceChanged() -- Called on the MMDevice notification thread when
// the Windows default render endpoint changes.
// We restart capture only if we were tracking the default device (m_deviceId=="").
void AudioLoopback::onDefaultDeviceChanged() {
    if (!m_deviceId.empty()) {
        // User pinned a specific device -- ignore default-device changes
        return;
    }

    if (!m_running.load()) return;

    AURA_LOG_INFO("AudioLoopback",
        "Windows default render device changed. Restarting WASAPI loopback...");

    // stop() + init() + start() on a fresh thread to avoid deadlocking
    // the MMDevice notification thread (which must not block).
    std::thread([this]() {
        stop();
        if (initWASAPI()) {
            start();
            AURA_LOG_INFO("AudioLoopback",
                "WASAPI loopback restarted on new default device.");
        } else {
            AURA_LOG_ERROR("AudioLoopback",
                "WASAPI restart failed after device change.");
        }
    }).detach();
}

} // namespace aura

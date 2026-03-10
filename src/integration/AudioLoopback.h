#pragma once
// =============================================================================
// AudioLoopback.h — WASAPI loopback audio capture
//
// Captures whatever the Windows audio engine is playing (device audio output)
// using WASAPI shared-mode loopback. This is different from microphone input —
// it captures the decoded audio from the device as it would sound through speakers.
//
// Task 119: setOutputDevice(id) selects which render endpoint to capture.
//           Call AudioDeviceEnumerator::instance().listOutputDevices() to get ids.
//
// Device-change detection: registers an IMMNotificationClient so that when
// the Windows default output device changes, capture automatically restarts
// on the new device — no manual restart required.
//
// Output: 48kHz / 32-bit float / stereo PCM ring buffer
// =============================================================================
#include <functional>
#include <mutex>
#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>

namespace aura {

struct AudioBuffer {
    const float* data;
    uint32_t     numFrames;
    uint32_t     sampleRate;
    uint32_t     channels;
};

class AudioLoopback {
public:
    using AudioCallback = std::function<void(const AudioBuffer&)>;

    AudioLoopback();
    ~AudioLoopback();

    void init();
    void start();
    void stop();
    void shutdown();

    void setCallback(AudioCallback cb);

    // Task 119: Select which output device to capture from.
    // deviceId = AudioDeviceInfo::id, or "" for Windows default output device.
    // Must be called before start(). If called while running, restarts capture.
    void setOutputDevice(const std::string& deviceId);
    std::string outputDeviceId() const { return m_deviceId; }

    bool isRunning() const { return m_running.load(); }
    uint64_t framesCapture() const { return m_framesCapture.load(); }

    // Called by DeviceNotifier when Windows default render endpoint changes.
    // Public so the inner COM class can reach it; do not call directly.
    void onDefaultDeviceChanged();

private:
    std::atomic<bool>     m_running{false};
    std::atomic<uint64_t> m_framesCapture{0};
    AudioCallback         m_callback;
    mutable std::mutex    m_callbackMu;   // guards m_callback across WASAPI + main threads
    std::string           m_deviceId;     // "" = use Windows default

    struct WASAPIState;
    std::unique_ptr<WASAPIState> m_wasapi;

    // IMMNotificationClient inner class — receives default-device-change callbacks
    // from the Windows multimedia device API and forwards to onDefaultDeviceChanged().
    class DeviceNotifier : public IMMNotificationClient {
    public:
        explicit DeviceNotifier(AudioLoopback* owner) : m_owner(owner) {}

        // IUnknown
        ULONG STDMETHODCALLTYPE AddRef()  override { return InterlockedIncrement(&m_ref); }
        ULONG STDMETHODCALLTYPE Release() override {
            ULONG r = InterlockedDecrement(&m_ref);
            if (r == 0) delete this;
            return r;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
                *ppv = static_cast<IMMNotificationClient*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        // IMMNotificationClient — only OnDefaultDeviceChanged matters for loopback
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
            EDataFlow flow, ERole role, LPCWSTR /*pwstrDeviceId*/) override
        {
            // Only react to render-endpoint changes (loopback taps the render device)
            if (flow == eRender && role == eConsole && m_owner)
                m_owner->onDefaultDeviceChanged();
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR)               override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR)             override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
            LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    private:
        AudioLoopback* m_owner;
        LONG           m_ref{1};
    };

    DeviceNotifier* m_notifier{nullptr};  // registered with IMMDeviceEnumerator

    void captureLoop();
    bool initWASAPI();          // (re)open WASAPI capture on m_deviceId
    void registerNotifier();    // subscribe to default-device-change events
    void unregisterNotifier();  // unsubscribe before shutdown
};

} // namespace aura

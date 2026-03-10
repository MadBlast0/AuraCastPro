// =============================================================================
// AudioDeviceEnumerator.cpp — Task 119: WASAPI audio output device selection
//
// Uses IMMDeviceEnumerator (Windows Core Audio) to list all active render
// endpoints and register for endpoint change notifications.
// =============================================================================
#include "AudioDeviceEnumerator.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <combaseapi.h>

#include <mutex>
#include <atomic>

using Microsoft::WRL::ComPtr;

namespace aura {

// =============================================================================
// IMMNotificationClient implementation — fires m_changeCallback on device change
// =============================================================================
class DeviceNotificationClient : public IMMNotificationClient {
public:
    std::function<void(const std::string&)> onDefaultChanged;

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef()  override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (--m_ref == 0) { delete this; return 0; }
        return m_ref;
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

    // IMMNotificationClient — only DefaultDeviceChanged matters
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
        EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) override
    {
        if (flow == eRender && role == eConsole && pwstrDeviceId && onDefaultChanged) {
            const std::wstring ws(pwstrDeviceId);
            onDefaultChanged(std::string(ws.begin(), ws.end()));
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR)          override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR)        override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    std::atomic<ULONG> m_ref{1};
};

// =============================================================================
// Impl
// =============================================================================
struct AudioDeviceEnumerator::Impl {
    ComPtr<IMMDeviceEnumerator> enumerator;
    DeviceNotificationClient*   notifClient{nullptr};
    std::string                 activeDeviceId;   // "" = Windows default
    DeviceChangeCallback        changeCallback;
    std::mutex                  mutex;
    bool                        comInitialised{false};

    bool init() {
        // CoInitializeEx — safe to call multiple times in same apartment
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInitialised = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        if (FAILED(hr)) {
            AURA_LOG_ERROR("AudioDeviceEnumerator",
                "CoCreateInstance(MMDeviceEnumerator) failed: {:08X}", (uint32_t)hr);
            return false;
        }

        // Register for device-change notifications
        notifClient = new DeviceNotificationClient();
        notifClient->onDefaultChanged = [this](const std::string& newId) {
            std::lock_guard lock(mutex);
            AURA_LOG_INFO("AudioDeviceEnumerator",
                "Default audio device changed → {}", newId);
            if (changeCallback) changeCallback(newId);
        };
        enumerator->RegisterEndpointNotificationCallback(notifClient);

        AURA_LOG_INFO("AudioDeviceEnumerator",
            "WASAPI device enumerator ready.");
        return true;
    }

    ~Impl() {
        if (enumerator && notifClient) {
            enumerator->UnregisterEndpointNotificationCallback(notifClient);
            notifClient->Release();
        }
        if (comInitialised) CoUninitialize();
    }
};

// =============================================================================
// AudioDeviceEnumerator
// =============================================================================
AudioDeviceEnumerator::AudioDeviceEnumerator()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->init();
}

AudioDeviceEnumerator::~AudioDeviceEnumerator() = default;

// -----------------------------------------------------------------------------
std::vector<AudioDeviceInfo> AudioDeviceEnumerator::listOutputDevices() {
    std::vector<AudioDeviceInfo> result;
    if (!m_impl->enumerator) return result;

    // Get default device id first so we can mark it
    std::string defaultId;
    {
        ComPtr<IMMDevice> defaultDev;
        if (SUCCEEDED(m_impl->enumerator->GetDefaultAudioEndpoint(
                eRender, eConsole, &defaultDev)))
        {
            LPWSTR wid = nullptr;
            if (SUCCEEDED(defaultDev->GetId(&wid)) && wid) {
                const std::wstring ws(wid);
                defaultId = std::string(ws.begin(), ws.end());
                CoTaskMemFree(wid);
            }
        }
    }

    // Enumerate all active render endpoints
    ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = m_impl->enumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) {
        AURA_LOG_ERROR("AudioDeviceEnumerator",
            "EnumAudioEndpoints failed: {:08X}", (uint32_t)hr);
        return result;
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device))) continue;

        // Device ID
        LPWSTR wid = nullptr;
        if (FAILED(device->GetId(&wid)) || !wid) continue;
        const std::wstring wsId(wid);
        CoTaskMemFree(wid);

        // Friendly name from property store
        ComPtr<IPropertyStore> props;
        std::string friendlyName = "(Unknown Device)";
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) &&
                pv.vt == VT_LPWSTR && pv.pwszVal)
            {
                const std::wstring wsName(pv.pwszVal);
                friendlyName = std::string(wsName.begin(), wsName.end());
            }
            PropVariantClear(&pv);
        }

        AudioDeviceInfo info;
        info.id        = std::string(wsId.begin(), wsId.end());
        info.name      = friendlyName;
        info.isDefault = (info.id == defaultId);

        result.push_back(std::move(info));
    }

    AURA_LOG_INFO("AudioDeviceEnumerator",
        "Found {} active render endpoint(s).", result.size());
    return result;
}

// -----------------------------------------------------------------------------
void AudioDeviceEnumerator::setActiveDevice(const std::string& deviceId) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->activeDeviceId = deviceId;
    AURA_LOG_INFO("AudioDeviceEnumerator",
        "Active output device set to: {}",
        deviceId.empty() ? "(Windows default)" : deviceId);
}

std::string AudioDeviceEnumerator::activeDeviceId() const {
    std::lock_guard lock(m_impl->mutex);
    return m_impl->activeDeviceId;
}

void AudioDeviceEnumerator::setDeviceChangeCallback(DeviceChangeCallback cb) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->changeCallback = std::move(cb);
}

// -----------------------------------------------------------------------------
AudioDeviceEnumerator& AudioDeviceEnumerator::instance() {
    static AudioDeviceEnumerator inst;
    return inst;
}

} // namespace aura

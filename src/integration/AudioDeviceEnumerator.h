#pragma once
// =============================================================================
// AudioDeviceEnumerator.h — Task 119: WASAPI audio output device selection
//
// Enumerates all active Windows audio render endpoints (speakers, HDMI, etc.)
// and allows selecting which device AudioLoopback should capture from.
//
// Usage:
//   auto& enumerator = AudioDeviceEnumerator::instance();
//   auto devices = enumerator.listOutputDevices();
//   // devices is vector of {id, name} — populate a combobox in SettingsPage
//
//   enumerator.setActiveDevice(devices[0].id);
//   // Now AudioLoopback.init() will capture from that device
// =============================================================================
#include <string>
#include <vector>
#include <functional>

namespace aura {

struct AudioDeviceInfo {
    std::string id;       // Windows endpoint ID (opaque WSTR, stable across reboots)
    std::string name;     // Human-readable: "Speakers (Realtek High Definition Audio)"
    bool        isDefault{false};
};

class AudioDeviceEnumerator {
public:
    AudioDeviceEnumerator();
    ~AudioDeviceEnumerator();

    // Returns all active render (output) endpoints.
    // Call this to populate the UI device picker in SettingsPage.
    std::vector<AudioDeviceInfo> listOutputDevices();

    // Set the device AudioLoopback and AudioMixer should use.
    // Pass AudioDeviceInfo::id ("" = use Windows default output device).
    // Persists the choice to SettingsModel; takes effect at next start().
    void setActiveDevice(const std::string& deviceId);

    // Returns the currently selected device id ("" = default).
    std::string activeDeviceId() const;

    // Subscribe to device-change events (e.g. user plugs in headphones).
    // Callback receives the new default device id.
    using DeviceChangeCallback = std::function<void(const std::string& newDefaultId)>;
    void setDeviceChangeCallback(DeviceChangeCallback cb);

    // Singleton — one enumerator per process
    static AudioDeviceEnumerator& instance();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace aura

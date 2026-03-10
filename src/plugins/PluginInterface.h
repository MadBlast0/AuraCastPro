#pragma once
// =============================================================================
// PluginInterface.h — Abstract base class for all AuraCastPro plugins
//
// Plugins are DLLs that export:
//   extern "C" __declspec(dllexport) aura::IPlugin* CreatePlugin();
//   extern "C" __declspec(dllexport) void DestroyPlugin(aura::IPlugin*);
//
// Plugins are discovered from: %APPDATA%\AuraCastPro\plugins\*.dll
// They are version-checked before loading.
//
// Plugin hooks available:
//   - onVideoFrame:  called for every decoded frame (post-decode, pre-render)
//   - onAudioBuffer: called for every audio buffer (post-mix, pre-output)
//   - onDeviceEvent: called on connect/disconnect/state change
//   - getSettingsPage: optionally return a QML component for the settings UI
// =============================================================================
#pragma once
#include <cstdint>
#include <string>

namespace aura {

struct VideoFrameInfo {
    uint32_t    width;
    uint32_t    height;
    const void* nv12Data;     // CPU pointer (if readback available) or nullptr
    uint64_t    ptsUs;
    bool        isKeyframe;
};

struct AudioBufferInfo {
    const float* samples;
    uint32_t     numFrames;
    uint32_t     sampleRate;
    uint32_t     channels;
};

enum class DeviceEventType { Connected, Disconnected, StreamingStarted, StreamingStopped };

struct DeviceEvent {
    DeviceEventType type;
    std::string     deviceId;
    std::string     deviceName;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Plugin metadata
    virtual const char* name()        const = 0;
    virtual const char* version()     const = 0;
    virtual const char* description() const = 0;
    virtual const char* author()      const = 0;

    // AuraCastPro plugin API version this plugin was compiled against
    static constexpr uint32_t kAPIVersion = 1;
    virtual uint32_t apiVersion() const { return kAPIVersion; }

    // Lifecycle
    virtual bool init()     = 0;  // return false to abort loading
    virtual void shutdown() = 0;

    // ── Hooks (all optional — return false to pass through unchanged) ────────
    // Called on decoder thread for each decoded frame
    virtual bool onVideoFrame(VideoFrameInfo& /*frame*/) { return false; }

    // Called on audio thread for each audio buffer
    virtual bool onAudioBuffer(AudioBufferInfo& /*buf*/) { return false; }

    // Called on main thread for device connection events
    virtual void onDeviceEvent(const DeviceEvent& /*event*/) {}

    // Optionally return a QML file path for a plugin settings page
    virtual const char* settingsQMLPath() const { return nullptr; }
};

// ─── IPluginHost — Services AuraCastPro exposes back to plugins ──────────────
// Task 160: Plugins receive a pointer to IPluginHost during init().
// This lets plugins log messages, show notifications, and read settings
// without depending on AuraCastPro internals.

class IPluginHost {
public:
    virtual ~IPluginHost() = default;

    // Write a log line to the AuraCastPro log (prefixed with plugin name)
    virtual void logMessage(const char* level, const char* message) = 0;

    // Show a Windows toast notification (non-blocking)
    virtual void showNotification(const char* title, const char* body) = 0;

    // Read a persisted setting value (returns "" if key not found)
    virtual const char* getSettingValue(const char* key) = 0;

    // Set a persisted setting value
    virtual void setSettingValue(const char* key, const char* value) = 0;

    // Returns the AuraCastPro version string, e.g. "0.1.0"
    virtual const char* appVersion() const = 0;

    // Returns the path to the plugin data directory (writable, plugin-private)
    // e.g. "%APPDATA%\AuraCastPro\plugins\MyPlugin\"
    virtual const char* pluginDataDirectory(const char* pluginName) = 0;
};

} // namespace aura

// DLL export macros for plugin authors
#define AURA_PLUGIN_API_VERSION "1.0"   // string compared by PluginLoader

#define AURA_PLUGIN_EXPORT(PluginClass)                                      \
    extern "C" __declspec(dllexport)                                          \
    aura::IPlugin* CreatePlugin() { return new PluginClass(); }              \
    extern "C" __declspec(dllexport)                                          \
    void DestroyPlugin(aura::IPlugin* p) { delete p; }                       \
    extern "C" __declspec(dllexport)                                          \
    const char* GetPluginAPIVersion() { return AURA_PLUGIN_API_VERSION; }

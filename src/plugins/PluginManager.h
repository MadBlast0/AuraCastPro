#pragma once
// =============================================================================
// PluginManager.h — Task 163: Plugin discovery, lifecycle, and dispatch
// =============================================================================
#include "PluginInterface.h"
#include <vector>
#include <string>
#include <memory>

namespace aura {

class PluginLoader;

struct LoadedPlugin {
    std::string path;
    IPlugin*    instance{nullptr};
    void*       dllHandle{nullptr};   // HMODULE, owned by PluginLoader
    bool        active{false};
};

class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    void init();
    void start();
    void stop();
    void shutdown();

    // Scan directory and load all valid .dll plugins
    void discoverAndLoad(const std::string& pluginDirectory);
    bool loadPlugin(const std::string& dllPath);
    void unloadPlugin(const std::string& name);
    void unloadAll();

    // Hot-path dispatch (called on video/audio threads)
    void dispatchVideoFrame(VideoFrameInfo& frame);
    void dispatchAudioBuffer(AudioBufferInfo& buf);
    void dispatchDeviceEvent(const DeviceEvent& event);

    // Runtime control
    void setPluginEnabled(const std::string& name, bool enabled);

    // Accessors
    const std::vector<LoadedPlugin>& plugins() const { return m_plugins; }
    int loadedCount() const;
    std::vector<std::string> loadedNames() const;

private:
    std::vector<LoadedPlugin>       m_plugins;
    std::unique_ptr<PluginLoader>   m_loader;
};

} // namespace aura

#pragma once
// PluginLoader.h -- DLL load/unload utilities (used by PluginManager)
#include <string>
#include <memory>

namespace aura {
class IPlugin;

class PluginLoader {
public:
    void init();
    void start();
    void stop();
    void shutdown();

    // Attempt to load a DLL and return its IPlugin instance
    static IPlugin* load(const std::string& dllPath, void** outHandle);
    static void     unload(IPlugin* plugin, void* handle);
};
} // namespace aura

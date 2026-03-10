// =============================================================================
// PluginManager.cpp — Task 163: Plugin lifecycle and frame-hook dispatch engine
//
// Delegates all DLL loading to PluginLoader (which handles SEM_FAILCRITICALERRORS,
// API version checks, SEH-safe FreeLibrary, etc.). PluginManager's job is:
//   • discoverAndLoad() — scan plugins directory, call PluginLoader::load()
//   • per-frame dispatch — onVideoFrame / onAudioBuffer / onDeviceEvent
//   • enable/disable individual plugins at runtime
// =============================================================================
#include "PluginManager.h"
#include "PluginLoader.h"
#include "../utils/Logger.h"

#include <filesystem>

namespace aura {

// ── Constructor / Destructor ──────────────────────────────────────────────────

PluginManager::PluginManager()
    : m_loader(std::make_unique<PluginLoader>()) {}

PluginManager::~PluginManager() { shutdown(); }

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void PluginManager::init() {
    m_loader->init();
    AURA_LOG_INFO("PluginManager",
        "Initialised. Plugin API version: {}. "
        "Discovery path: %%APPDATA%%\\AuraCastPro\\plugins\\",
        IPlugin::kAPIVersion);
}

void PluginManager::start() {
    m_loader->start();
    AURA_LOG_INFO("PluginManager",
        "Plugin system active. {} plugin(s) loaded.", loadedCount());
}

void PluginManager::stop()     { m_loader->stop(); }

void PluginManager::shutdown() {
    unloadAll();
    m_loader->shutdown();
}

// ── Discovery & Loading ───────────────────────────────────────────────────────

void PluginManager::discoverAndLoad(const std::string& pluginDir) {
    if (!std::filesystem::exists(pluginDir)) {
        AURA_LOG_DEBUG("PluginManager",
            "Plugin directory does not exist: {}. No plugins loaded.", pluginDir);
        return;
    }

    int loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(pluginDir)) {
        if (entry.path().extension() == ".dll") {
            if (loadPlugin(entry.path().string()))
                loaded++;
        }
    }
    AURA_LOG_INFO("PluginManager",
        "Discovered {} plugin(s) from '{}'.", loaded, pluginDir);
}

bool PluginManager::loadPlugin(const std::string& dllPath) {
    // Delegate all DLL loading complexity to PluginLoader
    void*    handle  = nullptr;
    IPlugin* plugin  = m_loader->load(dllPath, &handle);

    if (!plugin) {
        // PluginLoader already logged the specific error
        return false;
    }

    // API version check (PluginLoader logs a warning if mismatched; we enforce here)
    if (plugin->apiVersion() != IPlugin::kAPIVersion) {
        AURA_LOG_ERROR("PluginManager",
            "Plugin '{}' API version {} rejected (expected {}). Unloading.",
            plugin->name(), plugin->apiVersion(), IPlugin::kAPIVersion);
        m_loader->unload(plugin, handle);
        return false;
    }

    // Initialise — plugin performs its own setup
    if (!plugin->init()) {
        AURA_LOG_WARN("PluginManager",
            "Plugin '{}' init() returned false. Unloading.", plugin->name());
        m_loader->unload(plugin, handle);
        return false;
    }

    m_plugins.push_back({ dllPath, plugin, handle, true });

    AURA_LOG_INFO("PluginManager",
        "Plugin active: '{}' v{} by {} — {}",
        plugin->name(), plugin->version(),
        plugin->author(), plugin->description());
    return true;
}

// ── Unloading ─────────────────────────────────────────────────────────────────

void PluginManager::unloadAll() {
    for (auto& p : m_plugins) {
        if (p.instance) {
            m_loader->unload(p.instance, p.dllHandle);
            p.instance  = nullptr;
            p.dllHandle = nullptr;
        }
    }
    m_plugins.clear();
    AURA_LOG_INFO("PluginManager", "All plugins unloaded.");
}

void PluginManager::unloadPlugin(const std::string& name) {
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it->instance && std::string(it->instance->name()) == name) {
            m_loader->unload(it->instance, it->dllHandle);
            AURA_LOG_INFO("PluginManager", "Plugin '{}' unloaded.", name);
            m_plugins.erase(it);
            return;
        }
    }
    AURA_LOG_WARN("PluginManager", "unloadPlugin: '{}' not found.", name);
}

// ── Per-frame dispatch (hot path — called on video/audio threads) ─────────────

void PluginManager::dispatchVideoFrame(VideoFrameInfo& frame) {
    for (auto& p : m_plugins) {
        if (p.active && p.instance)
            p.instance->onVideoFrame(frame);
    }
}

void PluginManager::dispatchAudioBuffer(AudioBufferInfo& buf) {
    for (auto& p : m_plugins) {
        if (p.active && p.instance)
            p.instance->onAudioBuffer(buf);
    }
}

void PluginManager::dispatchDeviceEvent(const DeviceEvent& event) {
    for (auto& p : m_plugins) {
        if (p.active && p.instance)
            p.instance->onDeviceEvent(event);
    }
}

// ── Runtime enable/disable ────────────────────────────────────────────────────

void PluginManager::setPluginEnabled(const std::string& name, bool enabled) {
    for (auto& p : m_plugins) {
        if (p.instance && std::string(p.instance->name()) == name) {
            p.active = enabled;
            AURA_LOG_INFO("PluginManager",
                "Plugin '{}' {}.", name, enabled ? "enabled" : "disabled");
            return;
        }
    }
}

// ── Utility ───────────────────────────────────────────────────────────────────

int PluginManager::loadedCount() const {
    int n = 0;
    for (const auto& p : m_plugins) if (p.active) n++;
    return n;
}

std::vector<std::string> PluginManager::loadedNames() const {
    std::vector<std::string> names;
    for (const auto& p : m_plugins)
        if (p.instance) names.emplace_back(p.instance->name());
    return names;
}

} // namespace aura

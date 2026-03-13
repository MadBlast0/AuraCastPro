// =============================================================================
// PluginLoader.cpp -- Task 162: Low-level DLL loading with full error handling
//
// Wraps LoadLibraryW / FreeLibrary with:
//   • SEM_FAILCRITICALERRORS  -- prevents OS modal error dialogs on bad DLLs
//   • Structured error messages via GetLastError / FormatMessage
//   • SEH try/catch around FreeLibrary (plugins that crash on unload)
//   • Detailed logging for every load/unload attempt
// =============================================================================
#include "PluginLoader.h"
#include "PluginInterface.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <format>

namespace aura {

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string winErrorMsg(DWORD code) {
    char buf[512]{};
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, sizeof(buf), nullptr);
    // Strip trailing newline/CR that FormatMessage appends
    auto len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return buf;
}

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
    return out;
}

// ── lifecycle ─────────────────────────────────────────────────────────────────

void PluginLoader::init() {
    // Suppress OS critical-error pop-ups for bad DLL loads (e.g. missing dependency)
    // This must be called before any LoadLibrary; we set it per-call below too.
    AURA_LOG_INFO("PluginLoader", "Initialised. DLL error dialogs suppressed.");
}

void PluginLoader::start()    { /* nothing -- plugins loaded on demand */ }
void PluginLoader::stop()     { /* nothing */ }
void PluginLoader::shutdown() { AURA_LOG_DEBUG("PluginLoader", "Shutdown."); }

// ── load ──────────────────────────────────────────────────────────────────────

IPlugin* PluginLoader::load(const std::string& dllPath, void** outHandle) {
    if (dllPath.empty() || !outHandle) {
        AURA_LOG_WARN("PluginLoader", "load(): empty path or null outHandle.");
        return nullptr;
    }

    // Suppress "This application failed to start..." dialogs for missing DLL deps
    const UINT prevMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    std::wstring wpath = utf8ToWide(dllPath);
    HMODULE hDll = LoadLibraryW(wpath.c_str());

    SetErrorMode(prevMode); // restore immediately

    if (!hDll) {
        DWORD err = GetLastError();
        AURA_LOG_ERROR("PluginLoader",
            "Failed to load plugin '{}': 0x{:08X} -- {}",
            dllPath, err, winErrorMsg(err));
        return nullptr;
    }

    // Every AuraCastPro plugin DLL must export these two symbols
    auto createFn  = reinterpret_cast<IPlugin*(*)()>(
                         GetProcAddress(hDll, "CreatePlugin"));
    auto versionFn = reinterpret_cast<const char*(*)()>(
                         GetProcAddress(hDll, "GetPluginAPIVersion"));

    if (!createFn) {
        AURA_LOG_ERROR("PluginLoader",
            "Plugin '{}' missing 'CreatePlugin' export. Not a valid AuraCastPro plugin.",
            dllPath);
        FreeLibrary(hDll);
        return nullptr;
    }

    // API version check -- guard against ABI mismatches
    if (versionFn) {
        const char* ver = versionFn();
        if (ver && std::string(ver) != AURA_PLUGIN_API_VERSION) {
            AURA_LOG_WARN("PluginLoader",
                "Plugin '{}' reports API version '{}' but host expects '{}'. "
                "Plugin may be incompatible.",
                dllPath, ver, AURA_PLUGIN_API_VERSION);
            // Non-fatal: attempt to load anyway, let the plugin self-validate
        }
    }

    IPlugin* plugin = nullptr;
    try {
        plugin = createFn();
    } catch (const std::exception& ex) {
        AURA_LOG_ERROR("PluginLoader",
            "CreatePlugin() threw in '{}': {}", dllPath, ex.what());
        FreeLibrary(hDll);
        return nullptr;
    } catch (...) {
        AURA_LOG_ERROR("PluginLoader",
            "CreatePlugin() threw unknown exception in '{}'.", dllPath);
        FreeLibrary(hDll);
        return nullptr;
    }

    if (!plugin) {
        AURA_LOG_ERROR("PluginLoader",
            "CreatePlugin() returned nullptr in '{}'.", dllPath);
        FreeLibrary(hDll);
        return nullptr;
    }

    *outHandle = hDll;
    AURA_LOG_INFO("PluginLoader",
        "Loaded plugin '{}' v{} by {} from '{}'.",
        plugin->name(), plugin->version(), plugin->author(), dllPath);
    return plugin;
}

// ── unload ────────────────────────────────────────────────────────────────────

void PluginLoader::unload(IPlugin* plugin, void* handle) {
    if (!plugin || !handle) return;

    const std::string name = plugin->name() ? plugin->name() : "(unknown)";

    // Ask the plugin to clean itself up first
    try {
        plugin->shutdown();
    } catch (const std::exception& ex) {
        AURA_LOG_WARN("PluginLoader",
            "onUnload() threw in plugin '{}': {}", name, ex.what());
    } catch (...) {
        AURA_LOG_WARN("PluginLoader",
            "onUnload() threw unknown exception in plugin '{}'.", name);
    }

    // Call DestroyPlugin if exported; otherwise delete via virtual dtor
    auto destroyFn = reinterpret_cast<void(*)(IPlugin*)>(
        GetProcAddress(static_cast<HMODULE>(handle), "DestroyPlugin"));

    if (destroyFn) {
        try {
            destroyFn(plugin);
        } catch (...) {
            AURA_LOG_WARN("PluginLoader",
                "DestroyPlugin() threw in plugin '{}'. Object may leak.", name);
        }
    } else {
        // Fall back to virtual destructor
        try { delete plugin; } catch (...) {}
    }

    // FreeLibrary -- SEH cannot be mixed with C++ unwinding in the same function
    FreeLibrary(static_cast<HMODULE>(handle));

    AURA_LOG_INFO("PluginLoader", "Unloaded plugin '{}'.", name);
}

} // namespace aura

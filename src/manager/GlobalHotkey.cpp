// =============================================================================
// GlobalHotkey.cpp -- Task 175: System-wide global hotkey registration
// =============================================================================
#include "../pch.h"  // PCH
#include "GlobalHotkey.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aura {

// =============================================================================
GlobalHotkey::GlobalHotkey() = default;

GlobalHotkey::~GlobalHotkey() { unregisterAll(); }

// =============================================================================
void GlobalHotkey::registerDefaults() {
    // Ctrl+Shift+M -- Toggle mirror window visibility
    registerHotkey({HotkeyId::ToggleMirrorWindow,
        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'M',
        "Toggle mirror window"});

    // Ctrl+Shift+F -- Toggle fullscreen
    registerHotkey({HotkeyId::ToggleFullscreen,
        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'F',
        "Toggle fullscreen"});

    // Ctrl+Shift+S -- Screenshot
    registerHotkey({HotkeyId::Screenshot,
        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'S',
        "Screenshot"});

    // Ctrl+Shift+R -- Toggle recording
    registerHotkey({HotkeyId::ToggleRecording,
        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'R',
        "Toggle recording"});

    // Ctrl+Shift+D -- Disconnect device
    registerHotkey({HotkeyId::Disconnect,
        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'D',
        "Disconnect device"});
}

// =============================================================================
bool GlobalHotkey::registerHotkey(const HotkeyDef& def) {
    const int id = static_cast<int>(def.id);

    // Unregister first if already registered
    if (m_registered.count(id)) {
        UnregisterHotKey(nullptr, id);
    }

    const BOOL ok = RegisterHotKey(nullptr, id,
        def.modifiers, def.vkCode);

    if (!ok) {
        AURA_LOG_WARN("GlobalHotkey",
            "RegisterHotKey failed for '{}' (id={}, err={}). "
            "Hotkey may already be in use by another application.",
            def.description, id, GetLastError());
        return false;
    }

    m_registered[id] = def;
    AURA_LOG_INFO("GlobalHotkey",
        "Registered hotkey '{}' (id={}).", def.description, id);
    return true;
}

// =============================================================================
void GlobalHotkey::unregisterHotkey(HotkeyId id) {
    const int iid = static_cast<int>(id);
    if (!m_registered.count(iid)) return;
    UnregisterHotKey(nullptr, iid);
    m_registered.erase(iid);
}

void GlobalHotkey::unregisterAll() {
    for (const auto& [id, def] : m_registered) {
        UnregisterHotKey(nullptr, id);
    }
    m_registered.clear();
    AURA_LOG_DEBUG("GlobalHotkey", "All hotkeys unregistered.");
}

// =============================================================================
bool GlobalHotkey::processMessage(void* rawMsg) {
    if (!rawMsg) return false;
    const MSG* msg = static_cast<const MSG*>(rawMsg);
    if (msg->message != WM_HOTKEY) return false;

    const int id = static_cast<int>(msg->wParam);
    auto it = m_registered.find(id);
    if (it == m_registered.end()) return false;

    AURA_LOG_DEBUG("GlobalHotkey",
        "Hotkey fired: '{}'", it->second.description);

    if (m_callback) {
        m_callback(it->second.id);
    }
    return true;
}

// =============================================================================
GlobalHotkey& GlobalHotkey::instance() {
    static GlobalHotkey inst;
    return inst;
}

} // namespace aura

#pragma once
// =============================================================================
// GlobalHotkey.h -- Task 175: System-wide global hotkey registration
//
// Registers Win32 global hotkeys (RegisterHotKey) that work even when
// AuraCastPro's window is not focused or is minimised to the system tray.
//
// Default hotkeys (user-configurable in SettingsPage.qml):
//   Ctrl+Shift+M   -- Toggle mirror window visibility
//   Ctrl+Shift+F   -- Toggle fullscreen
//   Ctrl+Shift+S   -- Take screenshot of mirror frame
//   Ctrl+Shift+R   -- Start / stop recording
//   Ctrl+Shift+D   -- Disconnect current device
// =============================================================================
#include <functional>
#include <map>
#include <string>
#include <memory>
#include <cstdint>

namespace aura {

enum class HotkeyId : int {
    ToggleMirrorWindow = 1,
    ToggleFullscreen   = 2,
    Screenshot         = 3,
    ToggleRecording    = 4,
    Disconnect         = 5,
};

struct HotkeyDef {
    HotkeyId    id;
    uint32_t    modifiers;  // MOD_CONTROL | MOD_SHIFT | MOD_ALT | MOD_WIN
    uint32_t    vkCode;     // Virtual key code
    std::string description;
};

class GlobalHotkey {
public:
    using HotkeyCallback = std::function<void(HotkeyId)>;

    GlobalHotkey();
    ~GlobalHotkey();

    // Register all default hotkeys. Call after main window is created.
    void registerDefaults();

    // Register a single hotkey. Returns false if already taken by another app.
    bool registerHotkey(const HotkeyDef& def);

    // Unregister a single hotkey
    void unregisterHotkey(HotkeyId id);

    // Unregister all hotkeys
    void unregisterAll();

    // Set the callback fired when any registered hotkey is pressed.
    // Called on the message thread -- post to main thread if needed.
    void setCallback(HotkeyCallback cb) { m_callback = std::move(cb); }

    // Must be called from the main message loop to dispatch WM_HOTKEY messages.
    // Returns true if the message was a hotkey and was consumed.
    bool processMessage(void* msg);  // MSG* cast to void* to avoid <windows.h> here

    // Singleton
    static GlobalHotkey& instance();

private:
    HotkeyCallback m_callback;
    std::map<int, HotkeyDef> m_registered;
};

} // namespace aura

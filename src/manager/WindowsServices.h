#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>
#include <vector>

// ─── System Tray ─────────────────────────────────────────────────────────────

class SystemTray {
public:
    enum class IconState { Idle, Mirroring, Recording };

    SystemTray();
    ~SystemTray();

    bool init(HWND ownerHwnd, HICON icon);
    void setTooltip(const std::string& text);
    void setState(IconState state);
    void showBalloon(const std::string& title, const std::string& msg, DWORD timeoutMs = 3000);
    void remove();

    void setOnOpen(std::function<void()> cb)             { m_onOpen    = std::move(cb); }
    void setOnQuit(std::function<void()> cb)             { m_onQuit    = std::move(cb); }
    void setOnStartStop(std::function<void()> cb)        { m_onStartStop = std::move(cb); }
    void setOnRecordToggle(std::function<void()> cb)     { m_onRecord  = std::move(cb); }

    // Call from WndProc when msg == WM_TRAY_NOTIFY
    bool handleMessage(WPARAM wp, LPARAM lp);

    static constexpr UINT WM_TRAY_NOTIFY = WM_USER + 1;
    static constexpr UINT ID_TRAY        = 1001;
    static constexpr UINT ID_OPEN        = 2001;
    static constexpr UINT ID_STARTMIRROR = 2002;
    static constexpr UINT ID_RECORD      = 2003;
    static constexpr UINT ID_QUIT        = 2099;

private:
    void showContextMenu();

    NOTIFYICONDATAW m_nid = {};
    HWND m_hwnd = nullptr;
    bool m_added = false;
    IconState m_state = IconState::Idle;

    std::function<void()> m_onOpen, m_onQuit, m_onStartStop, m_onRecord;
};

// ─── Global Hotkey Manager ────────────────────────────────────────────────────

class HotkeyManager {
public:
    enum class Action {
        ToggleMirroring,
        ToggleRecording,
        ToggleFullscreen,
        ToggleOverlay,
        Screenshot,
        Count
    };

    HotkeyManager() = default;
    ~HotkeyManager();

    bool registerAll(HWND hwnd);
    void unregisterAll(HWND hwnd);

    void setCallback(Action action, std::function<void()> cb);

    // Call from WndProc on WM_HOTKEY
    bool handleMessage(WPARAM wp);

private:
    struct HotkeyDef {
        Action action;
        UINT   modifiers;
        UINT   vk;
        int    id;
        std::function<void()> callback;
    };

    static const HotkeyDef kDefaults[];
    std::vector<HotkeyDef> m_hotkeys;
    HWND m_hwnd = nullptr;
};

// ─── Toast Notifier ───────────────────────────────────────────────────────────

class ToastNotifier {
public:
    enum class Type { Info, Success, Warning, Error };

    static void show(const std::wstring& title,
                     const std::wstring& message,
                     Type type = Type::Info);

    static void showWithAction(const std::wstring& title,
                               const std::wstring& message,
                               const std::wstring& actionLabel,
                               std::function<void()> callback);

    // Fallback using system tray balloon (when WinRT unavailable)
    static void showFallback(HWND trayHwnd,
                             const std::string& title,
                             const std::string& msg);
};

// ─── Error Dialog / Windows Event Log / Crash Handler ────────────────────────
// Full definitions are in their own headers; include them here for convenience.
#include "ErrorDialog.h"
#include "../utils/WindowsEventLog.h"
#include "../utils/CrashHandler.h"

// ─── Startup Registry ─────────────────────────────────────────────────────────

class StartupRegistry {
public:
    static bool enable();   // Write HKCU Run key
    static bool disable();  // Delete HKCU Run key
    static bool isEnabled();
};

// Task 178: Alias used in main.cpp for clarity
using WindowsAutoStart = StartupRegistry;

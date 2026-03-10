#pragma once
// =============================================================================
// USBHotplug.h — Task 062: WM_DEVICECHANGE listener for Android USB detection
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbt.h>
#include <functional>
#include <string>
#include <thread>
#include <atomic>

namespace aura {

// Listens for Windows WM_DEVICECHANGE messages on a hidden message-only window.
// Fires onDeviceArrived / onDeviceRemoved callbacks on the listener thread.
// Thread-safe: callbacks may be invoked from the listener thread — callers
// must marshal to their own thread if needed.
class USBHotplug {
public:
    using Callback = std::function<void()>;

    USBHotplug() = default;
    ~USBHotplug() { stop(); }

    // Register callbacks BEFORE calling start().
    void setOnDeviceArrived(Callback cb)  { m_onArrived  = std::move(cb); }
    void setOnDeviceRemoved(Callback cb)  { m_onRemoved  = std::move(cb); }

    // Creates a hidden HWND_MESSAGE window and registers for USB device notifications.
    // Spins a dedicated message-pump thread.
    void start();

    // Unregisters notifications and destroys the window. Blocks until thread exits.
    void stop();

    bool isRunning() const { return m_running.load(); }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    Callback          m_onArrived;
    Callback          m_onRemoved;
    std::thread       m_thread;
    std::atomic<bool> m_running{false};
    HWND              m_hwnd{nullptr};
    HDEVNOTIFY        m_hNotify{nullptr};

    // The hidden window runs its own message loop on m_thread
    void messageLoop();
    void registerDeviceNotification();
};

} // namespace aura

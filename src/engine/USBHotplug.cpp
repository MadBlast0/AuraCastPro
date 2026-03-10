// =============================================================================
// USBHotplug.cpp — Task 062: WM_DEVICECHANGE for Android USB auto-detect
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include "USBHotplug.h"
#include "../utils/Logger.h"
#include <initguid.h>
#include <usbiodef.h>   // GUID_DEVINTERFACE_USB_DEVICE
#include <stdexcept>

// Window class name for the hidden message window
static constexpr const wchar_t* kWndClass = L"AuraCastPro_USBHotplug";

namespace aura {

// ─── Static WndProc ───────────────────────────────────────────────────────────

LRESULT CALLBACK USBHotplug::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Retrieve the USBHotplug* stashed in GWLP_USERDATA at window creation
    auto* self = reinterpret_cast<USBHotplug*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_DEVICECHANGE && self) {
        if (wp == DBT_DEVICEARRIVAL) {
            auto* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lp);
            if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                AURA_LOG_INFO("USBHotplug", "USB device arrived — checking for Android.");
                if (self->m_onArrived) self->m_onArrived();
            }
        } else if (wp == DBT_DEVICEREMOVECOMPLETE) {
            auto* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lp);
            if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                AURA_LOG_INFO("USBHotplug", "USB device removed.");
                if (self->m_onRemoved) self->m_onRemoved();
            }
        }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void USBHotplug::start() {
    if (m_running.exchange(true)) return;

    m_thread = std::thread([this]{ messageLoop(); });
    AURA_LOG_INFO("USBHotplug", "USB hotplug listener started.");
}

void USBHotplug::stop() {
    if (!m_running.exchange(false)) return;

    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_QUIT, 0, 0);
    }
    if (m_thread.joinable()) m_thread.join();
    AURA_LOG_INFO("USBHotplug", "USB hotplug listener stopped.");
}

// ─── Private ─────────────────────────────────────────────────────────────────

void USBHotplug::messageLoop() {
    // Register a window class for our hidden window
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWndClass;
    RegisterClassExW(&wc); // may already be registered — ignore ERROR_CLASS_ALREADY_EXISTS

    // Create a message-only window (no UI, no taskbar entry)
    m_hwnd = CreateWindowExW(0, kWndClass, L"AuraCastPro USBHotplug",
                              0, 0, 0, 0, 0,
                              HWND_MESSAGE,    // <— message-only parent
                              nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!m_hwnd) {
        AURA_LOG_ERROR("USBHotplug", "CreateWindowEx failed: 0x{:X}", GetLastError());
        m_running = false;
        return;
    }

    // Stash 'this' so wndProc can find us
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    registerDeviceNotification();

    // Run message loop until WM_QUIT
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    if (m_hNotify) {
        UnregisterDeviceNotification(m_hNotify);
        m_hNotify = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void USBHotplug::registerDeviceNotification() {
    DEV_BROADCAST_DEVICEINTERFACE_W filter = {};
    filter.dbcc_size       = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    // Listen for all USB device interfaces
    filter.dbcc_classguid  = GUID_DEVINTERFACE_USB_DEVICE;

    m_hNotify = RegisterDeviceNotificationW(
        m_hwnd,
        &filter,
        DEVICE_NOTIFY_WINDOW_HANDLE);

    if (!m_hNotify) {
        AURA_LOG_WARN("USBHotplug",
            "RegisterDeviceNotification failed: 0x{:X} — hotplug disabled.",
            GetLastError());
    } else {
        AURA_LOG_INFO("USBHotplug",
            "Device notification registered for GUID_DEVINTERFACE_USB_DEVICE.");
    }
}

} // namespace aura

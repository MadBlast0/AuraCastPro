// =============================================================================
// ClipboardBridge.cpp -- Task 150: Bidirectional clipboard sharing
// =============================================================================
#include "../pch.h"  // PCH
#include "ClipboardBridge.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <thread>
#include <atomic>
#include <string>
#include <mutex>

namespace aura {

// =============================================================================
// Hidden clipboard listener window -- receives WM_CLIPBOARDUPDATE
// =============================================================================
struct ClipboardBridge::Impl {
    HWND      listenerWnd{nullptr};
    std::thread monitorThread;
    std::atomic<bool> running{false};
    std::string lastPushedText;    // prevents echo loops
    std::mutex  mutex;

    // Pointer to the active ClipboardBridge -- set in startMonitoring()
    // so the WndProc uses the same instance that main.cpp created (not the singleton).
    static ClipboardBridge* s_activeInstance;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                     WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_CLIPBOARDUPDATE) {
            auto* self = reinterpret_cast<Impl*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            ClipboardBridge* bridge = s_activeInstance;
            if (self && bridge && bridge->autoSync()) {
                const std::string text = ClipboardBridge::readWindowsClipboard();
                if (!text.empty()) {
                    std::lock_guard lock(self->mutex);
                    if (text != self->lastPushedText) {
                        self->lastPushedText = text;
                        AURA_LOG_DEBUG("ClipboardBridge",
                            "Clipboard changed ({} chars) -- auto-syncing to device.",
                            text.size());
                        bridge->pushToDevice();
                    }
                }
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

// =============================================================================
ClipboardBridge::ClipboardBridge()
    : m_impl(std::make_unique<Impl>())
{}

ClipboardBridge::~ClipboardBridge() {
    stopMonitoring();
}

// =============================================================================
void ClipboardBridge::startMonitoring() {
    if (m_impl->running.load()) return;
    Impl::s_activeInstance = this;  // point WndProc at this instance
    m_impl->running.store(true);

    // Clipboard listener windows must run on a dedicated thread with a
    // message pump -- the WM_CLIPBOARDUPDATE message is thread-affine.
    m_impl->monitorThread = std::thread([this]() {
        // Register a minimal message-only window class
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc   = Impl::WndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"AuraCastProClipboard";
        RegisterClassExW(&wc);

        m_impl->listenerWnd = CreateWindowExW(
            0, L"AuraCastProClipboard", nullptr, 0,
            0, 0, 0, 0,
            HWND_MESSAGE,  // message-only window -- invisible
            nullptr, GetModuleHandleW(nullptr), nullptr);

        if (!m_impl->listenerWnd) {
            AURA_LOG_ERROR("ClipboardBridge",
                "Failed to create clipboard listener window: {}",
                GetLastError());
            return;
        }

        SetWindowLongPtrW(m_impl->listenerWnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(m_impl.get()));

        // Register for WM_CLIPBOARDUPDATE notifications
        if (!AddClipboardFormatListener(m_impl->listenerWnd)) {
            AURA_LOG_WARN("ClipboardBridge",
                "AddClipboardFormatListener failed: {}", GetLastError());
        }

        AURA_LOG_INFO("ClipboardBridge",
            "Clipboard monitor started (auto-sync={}).", m_autoSync.load());

        // Pump messages
        MSG msg;
        while (m_impl->running.load() &&
               GetMessageW(&msg, m_impl->listenerWnd, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        RemoveClipboardFormatListener(m_impl->listenerWnd);
        DestroyWindow(m_impl->listenerWnd);
        m_impl->listenerWnd = nullptr;
    });
}

// =============================================================================
void ClipboardBridge::stopMonitoring() {
    Impl::s_activeInstance = nullptr;  // detach WndProc before destroying thread
    if (!m_impl->running.exchange(false)) return;
    if (m_impl->listenerWnd) {
        PostMessageW(m_impl->listenerWnd, WM_QUIT, 0, 0);
    }
    if (m_impl->monitorThread.joinable()) {
        m_impl->monitorThread.join();
    }
    AURA_LOG_INFO("ClipboardBridge", "Clipboard monitor stopped.");
}

// =============================================================================
void ClipboardBridge::pushToDevice() {
    const std::string text = readWindowsClipboard();
    if (text.empty()) return;

    // The actual send mechanism is injected by the protocol layer.
    // AirPlay2Host calls clipboardBridge.setReceivedCallback and also
    // wires the send path via:
    //   aura::ClipboardBridge::instance().pushToDevice()
    //   -> m_airplay2Host->sendClipboardToDevice(text)
    // This is done in main.cpp via a lambda capture.
    // Here we just fire the callback to notify the protocol layer.
    if (m_callback) {
        m_callback(text);
    }
    AURA_LOG_DEBUG("ClipboardBridge",
        "Pushed {} chars to device.", text.size());
}

// =============================================================================
void ClipboardBridge::receiveFromDevice(const std::string& text) {
    if (text.empty()) return;

    // Prevent echo: don't re-sync what we just received
    {
        std::lock_guard lock(m_impl->mutex);
        m_impl->lastPushedText = text;
    }

    if (writeWindowsClipboard(text)) {
        AURA_LOG_INFO("ClipboardBridge",
            "Received {} chars from device -> written to Windows clipboard.",
            text.size());
    }
}

// =============================================================================
void ClipboardBridge::setReceivedCallback(ClipboardReceivedCallback cb) {
    m_callback = std::move(cb);
}

// =============================================================================
std::string ClipboardBridge::readWindowsClipboard() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return {};

    if (!OpenClipboard(nullptr)) return {};

    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        const wchar_t* wtext = static_cast<const wchar_t*>(GlobalLock(hData));
        if (wtext) {
            const std::wstring ws(wtext);
            result = std::string(ws.begin(), ws.end());
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

// =============================================================================
bool ClipboardBridge::writeWindowsClipboard(const std::string& text) {
    if (!OpenClipboard(nullptr)) {
        AURA_LOG_WARN("ClipboardBridge",
            "OpenClipboard failed: {}", GetLastError());
        return false;
    }

    EmptyClipboard();

    const std::wstring ws(text.begin(), text.end());
    const size_t bytes = (ws.size() + 1) * sizeof(wchar_t);

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem));
    if (!dst) {
        GlobalFree(hMem);   // GlobalLock failed -- we still own the memory, must free it
        CloseClipboard();
        return false;
    }
    wcscpy_s(dst, ws.size() + 1, ws.c_str());
    GlobalUnlock(hMem);

    // After SetClipboardData succeeds Windows owns hMem -- do NOT call GlobalFree
    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);   // SetClipboardData failed -- we still own it, free it
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

ClipboardBridge* ClipboardBridge::Impl::s_activeInstance = nullptr;

// =============================================================================
ClipboardBridge& ClipboardBridge::instance() {
    static ClipboardBridge inst;
    return inst;
}

} // namespace aura

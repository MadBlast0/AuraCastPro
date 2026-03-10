#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>
#include <map>

class AndroidControlBridge;

// ─── GesturePassthrough ───────────────────────────────────────────────────────
// Translates Windows Pointer API (multi-touch) events into Android gestures.

class GesturePassthrough {
public:
    struct TouchPoint {
        UINT32 id;
        float  x, y;     // screen-space in mirror window
        bool   down;
    };

    void setDeviceDimensions(uint32_t w, uint32_t h) {
        m_devW = w; m_devH = h;
    }
    void setWindowDimensions(uint32_t w, uint32_t h) {
        m_winW = w; m_winH = h;
    }
    void setControlBridge(AndroidControlBridge* bridge) { m_bridge = bridge; }

    // Call from WndProc for WM_POINTER* messages
    bool handlePointerMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    void processGesture();
    float mapX(float wx) const { return wx / m_winW * m_devW; }
    float mapY(float wy) const { return wy / m_winH * m_devH; }

    AndroidControlBridge* m_bridge = nullptr;
    std::map<UINT32, TouchPoint> m_points;
    uint32_t m_devW = 1080, m_devH = 1920;
    uint32_t m_winW = 1280, m_winH = 720;

    // Two-finger gesture state
    float m_prevTwoFingerDist = 0.0f;
    float m_prevTwoFingerCX   = 0.0f;
    float m_prevTwoFingerCY   = 0.0f;

    // Three-finger gesture state
    float m_prevThreeFingerCY = 0.0f;
};

// ─── ScreenshotCapture ────────────────────────────────────────────────────────
// Reads the current DX12 swapchain frame and saves as PNG.

class ScreenshotCapture {
public:
    struct Result {
        bool        success;
        std::string filePath;
        std::string errorMessage;
    };

    // outputFolder: e.g. AppData\AuraCastPro\screenshots
    void setOutputFolder(const std::string& folder) { m_folder = folder; }

    // Capture from CPU-accessible BGRA buffer (caller provides after GPU readback)
    Result capture(const uint8_t* bgra, uint32_t width, uint32_t height);

private:
    std::string generateFilename() const;
    bool savePNG(const std::string& path,
                 const uint8_t* bgra,
                 uint32_t width,
                 uint32_t height);

    std::string m_folder;
};

// ─── ClipboardBridge ─────────────────────────────────────────────────────────
// Syncs clipboard between PC and Android device.

class ClipboardBridge {
public:
    void setAdbPath(const std::string& adbPath)    { m_adb    = adbPath; }
    void setDeviceSerial(const std::string& serial) { m_serial = serial; }

    // Start/stop background polling
    void start(HWND hwnd);
    void stop();

    // Call from WndProc when WM_CLIPBOARDUPDATE fires
    void onWindowsClipboardChanged();

    // Last known Android clipboard text
    const std::string& lastAndroidClipboard() const { return m_lastAndroid; }

    // Notification callback (fires on main thread)
    void setOnAndroidClipboardChanged(std::function<void(std::string)> cb) {
        m_onChanged = std::move(cb);
    }

private:
    void pollAndroidClipboard();
    bool sendToAndroid(const std::string& text);
    std::string getWindowsClipboard();
    std::string runAdb(const std::string& args);

    std::string m_adb, m_serial;
    std::string m_lastWindows, m_lastAndroid;
    std::atomic<bool> m_running { false };
    std::thread m_thread;
    HWND m_hwnd = nullptr;

    std::function<void(std::string)> m_onChanged;
};

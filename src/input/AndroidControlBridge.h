#pragma once
// =============================================================================
// AndroidControlBridge.h — Sends touch/key events to Android via ADB
// =============================================================================
#include <functional>
#include <string>
#include <atomic>
#include <memory>

namespace aura {
struct TouchEvent;
struct KeyEvent;

class AndroidControlBridge {
public:
    AndroidControlBridge();
    ~AndroidControlBridge();

    void init();
    void start();
    void stop();
    void shutdown();

    // Connect to a device by serial ID (from ADBBridge)
    bool connect(const std::string& deviceSerial,
                 uint32_t deviceWidth, uint32_t deviceHeight);
    void disconnect();

    // Send events to the connected Android device
    void sendTouch(const TouchEvent& e);
    void sendKey(const KeyEvent& e);
    void sendText(const std::string& text); // Clipboard paste simulation

    // ── Task 147: Gesture convenience wrappers ───────────────────────────────
    void sendTap(int x, int y);
    void sendSwipe(int x0, int y0, int x1, int y1, int durationMs = 100);
    void sendKeyEvent(int androidKeycode); // e.g. 3=Home, 4=Back, 187=Recents

    bool isConnected() const { return m_connected.load(); }

private:
    std::atomic<bool> m_connected{false};
    std::string       m_deviceSerial;
    uint32_t          m_deviceWidth{1080};
    uint32_t          m_deviceHeight{1920};

    struct ScrcpyControl;
    std::unique_ptr<ScrcpyControl> m_ctrl;

    bool openControlSocket();
    void sendControlMessage(const uint8_t* msg, size_t len);
};

} // namespace aura

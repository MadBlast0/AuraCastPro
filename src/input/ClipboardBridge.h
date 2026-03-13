#pragma once
// =============================================================================
// ClipboardBridge.h -- Task 150: Bidirectional clipboard sharing
//
// Synchronises the Windows clipboard with the connected mobile device.
//
// Flow (iOS / AirPlay):
//   PC->Device: User copies text on PC -> AirPlay DACP "setProperty" sends it
//   Device->PC: AirPlay "getProperty" clipboard callback -> Windows SetClipboardData
//
// Flow (Android / ADB):
//   PC->Device:  adb shell am broadcast -a clipper.set --es text "..."
//   Device->PC:  scrcpy clipboard synchronisation channel (MSG_TYPE_CLIPBOARD)
//
// Only plain text is supported in v1.0 -- images and rich text in a future release.
// =============================================================================
#include <string>
#include <functional>
#include <memory>
#include <atomic>

namespace aura {

class ClipboardBridge {
public:
    // Called when device sends clipboard content to us
    using ClipboardReceivedCallback = std::function<void(const std::string& text)>;

    ClipboardBridge();
    ~ClipboardBridge();

    // Start monitoring Windows clipboard for changes.
    // When the user copies something, sends it to the device automatically
    // if autoSync is enabled.
    void startMonitoring();
    void stopMonitoring();

    // Enable/disable automatic sync of Windows clipboard to device
    void setAutoSync(bool enabled) { m_autoSync.store(enabled); }
    bool autoSync() const { return m_autoSync.load(); }

    // Manually push current Windows clipboard text to the connected device
    void pushToDevice();

    // Receive text from device -- writes it to Windows clipboard
    void receiveFromDevice(const std::string& text);

    // Called by the Android/AirPlay protocol layer when device clipboard changes
    void setReceivedCallback(ClipboardReceivedCallback cb);

    // Read the current Windows clipboard text (empty if not text or clipboard empty)
    static std::string readWindowsClipboard();

    // Write text to Windows clipboard
    static bool writeWindowsClipboard(const std::string& text);

    // Singleton
    static ClipboardBridge& instance();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<bool> m_autoSync{true};
    ClipboardReceivedCallback m_callback;
};

} // namespace aura

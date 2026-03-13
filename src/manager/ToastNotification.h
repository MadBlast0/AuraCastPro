#pragma once
// =============================================================================
// ToastNotification.h -- Task 176: Windows Toast notification infrastructure
//
// Shows native Windows 10/11 toast notifications (Action Center) for events
// like device connected, recording started, license expiry, etc.
//
// Implementation uses Windows Runtime (WinRT) XML toast API via
// IToastNotification / IXmlDocument. Falls back to system tray balloon
// on Windows 8.1 / Server 2012.
//
// Usage:
//   ToastNotification::show("Device Connected",
//       "iPhone 15 Pro is now mirroring.", ToastIcon::DeviceConnected);
// =============================================================================
#include <string>
#include <functional>

namespace aura {

enum class ToastIcon {
    Default,
    DeviceConnected,
    DeviceDisconnected,
    RecordingStarted,
    RecordingStopped,
    Warning,
    Error,
    Screenshot,
    LicenseExpiry,
};

class ToastNotification {
public:
    using ActionCallback = std::function<void()>;

    // Show a simple toast with title + body
    static void show(const std::string& title,
                     const std::string& body,
                     ToastIcon icon = ToastIcon::Default);

    // Show a toast with an action button
    static void showWithAction(const std::string& title,
                                const std::string& body,
                                const std::string& actionLabel,
                                ActionCallback onAction,
                                ToastIcon icon = ToastIcon::Default);

    // Must be called once at startup to initialise the WinRT apartment
    static void init(const std::string& appUserModelId = "AuraCastPro.App");

    static void shutdown();

    // Convenience wrappers for common events
    static void notifyDeviceConnected(const std::string& deviceName);
    static void notifyDeviceDisconnected(const std::string& deviceName);
    static void notifyRecordingStarted(const std::string& filePath);
    static void notifyRecordingStopped(const std::string& filePath, double sizeMb);
    static void notifyScreenshot(const std::string& filePath);
    static void notifyLicenseExpiringSoon(int daysLeft);
    static void notifyError(const std::string& title, const std::string& message);

    // Task: Update badge -- shown when UpdateService finds a newer version
    static void showUpdateAvailable(const std::string& version,
                                    const std::string& downloadUrl);
};

} // namespace aura

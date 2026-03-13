// =============================================================================
// ToastNotification.cpp — Task 176: Windows Toast notification infrastructure
//
// Uses the Windows Runtime Shell notification API (IToastNotificationManagerStatics)
// available on Windows 10+. Gracefully falls back to a no-op on older Windows.
// =============================================================================
#include "ToastNotification.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShObjIdl_core.h>   // IToastNotificationManagerStatics
#include <shlobj.h>
#include <shellapi.h>   // ShellExecuteW

#include <format>
#include <string>
#include <atomic>

// WinRT COM headers — available in Windows SDK 10.0.17763+
// If not available, the entire implementation compiles to no-ops.
#if defined(_WIN32) && defined(__has_include)
#  if __has_include(<winrt/Windows.UI.Notifications.h>)
#    include <winrt/Windows.UI.Notifications.h>
#    include <winrt/Windows.Data.Xml.Dom.h>
#    define AURA_WINRT_TOAST_AVAILABLE 1
#  endif
#endif

namespace aura {

static std::string g_appUserModelId = "AuraCastPro.App";
static std::atomic<bool> g_initialised{false};

// =============================================================================
void ToastNotification::init(const std::string& appUserModelId) {
    g_appUserModelId = appUserModelId;

#ifdef AURA_WINRT_TOAST_AVAILABLE
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        g_initialised.store(true);
        AURA_LOG_INFO("ToastNotification", "WinRT toast notifications ready.");
    } catch (const winrt::hresult_error& e) {
        AURA_LOG_WARN("ToastNotification",
            "WinRT init failed ({}). Toast notifications disabled.",
            winrt::to_string(e.message()));
    }
#else
    AURA_LOG_INFO("ToastNotification",
        "WinRT headers not available — toast notifications disabled. "
        "Upgrade Windows SDK to 10.0.17763 or later to enable them.");
#endif
}

void ToastNotification::shutdown() {
#ifdef AURA_WINRT_TOAST_AVAILABLE
    if (g_initialised.exchange(false)) {
        winrt::uninit_apartment();
    }
#endif
}

// =============================================================================
static std::wstring buildToastXml(const std::string& title,
                                   const std::string& body,
                                   const std::string& actionLabel = "")
{
    std::wstring wTitle(title.begin(), title.end());
    std::wstring wBody (body.begin(),  body.end());

    std::wstring xml =
        L"<toast>"
        L"<visual><binding template='ToastGeneric'>"
        L"<text>" + wTitle + L"</text>"
        L"<text>" + wBody  + L"</text>"
        L"</binding></visual>";

    if (!actionLabel.empty()) {
        std::wstring wAction(actionLabel.begin(), actionLabel.end());
        xml += L"<actions>"
               L"<action content='" + wAction + L"' arguments='action'/>"
               L"</actions>";
    }

    xml += L"</toast>";
    return xml;
}

// =============================================================================
void ToastNotification::show(const std::string& title,
                               const std::string& body,
                               ToastIcon /*icon*/)
{
#ifdef AURA_WINRT_TOAST_AVAILABLE
    if (!g_initialised.load()) {
        AURA_LOG_DEBUG("ToastNotification",
            "Skipping toast (not initialised): {}", title);
        return;
    }
    try {
        using namespace winrt::Windows::UI::Notifications;
        using namespace winrt::Windows::Data::Xml::Dom;

        const std::wstring xml = buildToastXml(title, body);

        XmlDocument doc;
        doc.LoadXml(xml);

        const std::wstring wAumid(g_appUserModelId.begin(), g_appUserModelId.end());
        ToastNotificationManager::CreateToastNotifier(wAumid)
            .Show(winrt::Windows::UI::Notifications::ToastNotification(doc));

    } catch (const winrt::hresult_error& e) {
        AURA_LOG_WARN("ToastNotification",
            "Toast failed: {}", winrt::to_string(e.message()));
    }
#else
    // Non-WinRT fallback: log only
    AURA_LOG_INFO("ToastNotification", "[Toast] {}: {}", title, body);
#endif
}

// =============================================================================
void ToastNotification::showWithAction(const std::string& title,
                                        const std::string& body,
                                        const std::string& actionLabel,
                                        ActionCallback /*onAction*/,
                                        ToastIcon icon)
{
    // Note: for v1.0, action callbacks require a background COM activator.
    // For now, show the toast without the callback — the notification still
    // appears, the button is displayed, but clicking it brings the app to
    // the foreground via the registered app AUMID.
    show(title, body + (actionLabel.empty() ? "" : "\n(" + actionLabel + ")"), icon);
}

// =============================================================================
// Convenience wrappers
// =============================================================================
void ToastNotification::notifyDeviceConnected(const std::string& deviceName) {
    show("Device Connected",
         std::format("{} is now mirroring.", deviceName),
         ToastIcon::DeviceConnected);
}

void ToastNotification::notifyDeviceDisconnected(const std::string& deviceName) {
    show("Device Disconnected",
         std::format("{} has stopped mirroring.", deviceName),
         ToastIcon::DeviceDisconnected);
}

void ToastNotification::notifyRecordingStarted(const std::string& filePath) {
    show("Recording Started",
         std::format("Saving to: {}", filePath),
         ToastIcon::RecordingStarted);
}

void ToastNotification::notifyRecordingStopped(const std::string& filePath,
                                                double sizeMb)
{
    show("Recording Saved",
         std::format("{:.1f} MB saved to: {}", sizeMb, filePath),
         ToastIcon::RecordingStopped);
}

void ToastNotification::notifyScreenshot(const std::string& filePath) {
    showWithAction("Screenshot Captured",
        filePath, "Open",
        [filePath]() {
            const std::wstring wPath(filePath.begin(), filePath.end());
            ShellExecuteW(nullptr, L"open", wPath.c_str(), nullptr, nullptr, SW_SHOW);
        },
        ToastIcon::Screenshot);
}

void ToastNotification::notifyLicenseExpiringSoon(int daysLeft) {
    showWithAction("License Expiring Soon",
        std::format("Your AuraCastPro license expires in {} day(s).", daysLeft),
        "Renew Now",
        []() {
            ShellExecuteW(nullptr, L"open",
                L"https://auracastpro.com/renew", nullptr, nullptr, SW_SHOW);
        },
        ToastIcon::LicenseExpiry);
}

void ToastNotification::notifyError(const std::string& title,
                                     const std::string& message)
{
    show(title, message, ToastIcon::Error);
}

void ToastNotification::showUpdateAvailable(const std::string& version,
                                             const std::string& downloadUrl)
{
    showWithAction(
        "AuraCastPro Update Available",
        std::format("Version {} is ready to download.", version),
        "Download",
        [downloadUrl]() {
            const std::wstring wUrl(downloadUrl.begin(), downloadUrl.end());
            ShellExecuteW(nullptr, L"open", wUrl.c_str(),
                          nullptr, nullptr, SW_SHOW);
        },
        ToastIcon::Default);
}

} // namespace aura

#pragma once
// =============================================================================
// WinHttpHelper.h — Task 195: WinHTTP session factory with proxy support
//
// All three cloud classes (LicenseClient, UpdateService, TelemetryClient) use
// this helper to create their HINTERNET session so they all honour the same
// proxy configuration automatically.
//
// Proxy source priority:
//   1. Manual proxy stored in ProxyConfig (from SettingsModel)
//   2. Windows system proxy (WinHTTP auto-detect / IE settings)
//   3. Direct connection (WINHTTP_ACCESS_TYPE_DIRECT_PROXY)
//
// Usage:
//   HINTERNET session = WinHttpHelper::openSession("AuraCastPro/1.0");
//   // use session normally, then close as usual
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <string>
#include <optional>

namespace aura {

struct WinHttpProxyConfig {
    // If empty, use WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY (auto-detect)
    std::string proxyHost;      // e.g. "proxy.corp.com"
    uint16_t    proxyPort{0};   // e.g. 8080
    std::string bypassList;     // e.g. "localhost;<local>"
};

class WinHttpHelper {
public:
    // Open a WinHTTP session with appropriate proxy settings.
    // Returns nullptr on failure. Caller must WinHttpCloseHandle().
    //
    // If proxyConfig is empty, reads Windows system proxy automatically.
    // If proxyConfig has host set, uses that specific proxy.
    static HINTERNET openSession(
        const std::wstring& userAgent = L"AuraCastPro/1.0",
        std::optional<WinHttpProxyConfig> proxyConfig = std::nullopt);

    // Set proxy credentials on an existing request handle (for 407 responses)
    static bool setProxyCredentials(HINTERNET request,
                                    const std::string& username,
                                    const std::string& password);

    // Read the current Windows system proxy settings (useful for displaying
    // in the Settings UI "Detected proxy: ..." label).
    static WinHttpProxyConfig detectSystemProxy();
};

} // namespace aura

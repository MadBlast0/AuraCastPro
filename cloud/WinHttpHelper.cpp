// =============================================================================
// WinHttpHelper.cpp — Task 195: WinHTTP session factory with proxy support
// =============================================================================
#include "WinHttpHelper.h"
#include "../src/utils/Logger.h"

static constexpr uint16_t kDefaultProxyPort = 8080;

namespace aura {

// =============================================================================
HINTERNET WinHttpHelper::openSession(
    const std::wstring& userAgent,
    std::optional<WinHttpProxyConfig> proxyConfig)
{
    HINTERNET session = nullptr;

    if (proxyConfig.has_value() && !proxyConfig->proxyHost.empty()) {
        // ── Manual proxy ──────────────────────────────────────────────────────
        const std::string proxyStr =
            proxyConfig->proxyHost + ":" + std::to_string(proxyConfig->proxyPort);
        const std::wstring proxyW(proxyStr.begin(), proxyStr.end());

        const std::wstring bypassW = proxyConfig->bypassList.empty()
            ? L"<local>"
            : std::wstring(proxyConfig->bypassList.begin(),
                           proxyConfig->bypassList.end());

        session = WinHttpOpen(
            userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_NAMED_PROXY,
            proxyW.c_str(),
            bypassW.c_str(),
            0);

        if (session) {
            AURA_LOG_DEBUG("WinHttpHelper",
                "Session opened with manual proxy: {}", proxyStr);
        } else {
            AURA_LOG_WARN("WinHttpHelper",
                "Manual proxy session failed (err={}), falling back to auto.",
                GetLastError());
        }
    }

    if (!session) {
        // ── Auto-detect / system proxy ────────────────────────────────────────
        // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY reads WinHTTP system proxy,
        // WPAD auto-detect, and PAC files automatically.
        session = WinHttpOpen(
            userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (session) {
            AURA_LOG_DEBUG("WinHttpHelper",
                "Session opened with automatic proxy.");
        }
    }

    if (!session) {
        // ── Direct (last resort) ──────────────────────────────────────────────
        session = WinHttpOpen(
            userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (session) {
            AURA_LOG_WARN("WinHttpHelper",
                "Session opened direct (no proxy). Last error was {}.",
                GetLastError());
        } else {
            AURA_LOG_ERROR("WinHttpHelper",
                "WinHttpOpen failed completely. Error: {}", GetLastError());
        }
    }

    return session;
}

// =============================================================================
bool WinHttpHelper::setProxyCredentials(HINTERNET request,
                                         const std::string& username,
                                         const std::string& password)
{
    if (username.empty()) return false;

    const std::wstring wUser(username.begin(), username.end());
    const std::wstring wPass(password.begin(), password.end());

    const BOOL ok = WinHttpSetCredentials(
        request,
        WINHTTP_AUTH_TARGET_PROXY,
        WINHTTP_AUTH_SCHEME_BASIC,
        wUser.c_str(),
        wPass.c_str(),
        nullptr);

    if (!ok) {
        AURA_LOG_WARN("WinHttpHelper",
            "SetProxyCredentials failed: {}", GetLastError());
    }
    return ok == TRUE;
}

// =============================================================================
WinHttpProxyConfig WinHttpHelper::detectSystemProxy() {
    WinHttpProxyConfig result;

    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxy{};
    if (!WinHttpGetIEProxyConfigForCurrentUser(&ieProxy)) return result;

    if (ieProxy.lpszProxy) {
        const std::wstring ws(ieProxy.lpszProxy);
        const std::string  s(ws.begin(), ws.end());

        // Extract HTTPS proxy if present
        std::string entry = s;
        const size_t httpsPos = s.find("https=");
        if (httpsPos != std::string::npos) {
            entry = s.substr(httpsPos + 6);
            const size_t semi = entry.find(';');
            if (semi != std::string::npos) entry = entry.substr(0, semi);
        }
        if (entry.starts_with("http://"))  entry = entry.substr(7);
        if (entry.starts_with("https://")) entry = entry.substr(8);
        if (entry.starts_with("http="))    entry = entry.substr(5);

        const size_t colon = entry.rfind(':');
        if (colon != std::string::npos) {
            result.proxyHost = entry.substr(0, colon);
            try { result.proxyPort = (uint16_t)std::stoi(entry.substr(colon + 1)); }
            catch (...) { result.proxyPort = kDefaultProxyPort; }
        }

        GlobalFree(ieProxy.lpszProxy);
    }
    if (ieProxy.lpszProxyBypass) {
        const std::wstring ws(ieProxy.lpszProxyBypass);
        result.bypassList = std::string(ws.begin(), ws.end());
        GlobalFree(ieProxy.lpszProxyBypass);
    }
    if (ieProxy.lpszAutoConfigUrl) GlobalFree(ieProxy.lpszAutoConfigUrl);

    return result;
}

} // namespace aura

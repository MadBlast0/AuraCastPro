// =============================================================================
// ProxySettings.cpp — Task 195: HTTPS proxy support for all cloud API calls
// =============================================================================
#include "ProxySettings.h"
#include "../src/utils/Logger.h"

#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QString>
#include <QProcessEnvironment>

static constexpr uint16_t kDefaultProxyPort = 8080;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <string>
#include <algorithm>

namespace aura {

// =============================================================================
ProxySettings::ProxySettings() {
    // Default: use system proxy
    m_config.mode = ProxyMode::SystemDefault;
    loadFromSystem();
}

ProxySettings::~ProxySettings() = default;

// =============================================================================
void ProxySettings::loadFromSettings(const ProxyConfig& cfg) {
    m_config = cfg;
    AURA_LOG_INFO("ProxySettings",
        "Proxy loaded from settings: mode={} host={}:{}",
        static_cast<int>(cfg.mode), cfg.host, cfg.port);
}

// =============================================================================
void ProxySettings::loadFromEnvironment() {
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Check HTTPS_PROXY first, then HTTP_PROXY
    QString proxyStr = env.value("HTTPS_PROXY");
    if (proxyStr.isEmpty()) proxyStr = env.value("https_proxy");
    if (proxyStr.isEmpty()) proxyStr = env.value("HTTP_PROXY");
    if (proxyStr.isEmpty()) proxyStr = env.value("http_proxy");

    if (proxyStr.isEmpty()) {
        AURA_LOG_DEBUG("ProxySettings", "No proxy env vars set.");
        return;
    }

    // Parse "http://user:pass@host:port" or "host:port"
    QUrl url = QUrl::fromUserInput(proxyStr);
    if (!url.isValid() || url.host().isEmpty()) {
        AURA_LOG_WARN("ProxySettings",
            "Invalid HTTPS_PROXY env var: {}", proxyStr.toStdString());
        return;
    }

    m_config.mode     = ProxyMode::EnvVar;
    m_config.host     = url.host().toStdString();
    m_config.port     = static_cast<uint16_t>(url.port(kDefaultProxyPort));
    m_config.username = url.userName().toStdString();
    m_config.password = url.password().toStdString();

    // NO_PROXY bypass list
    const QString noProxy = env.value("NO_PROXY", env.value("no_proxy"));
    m_config.noProxyList  = noProxy.toStdString();

    AURA_LOG_INFO("ProxySettings",
        "Proxy from env: {}:{}", m_config.host, m_config.port);
}

// =============================================================================
void ProxySettings::loadFromSystem() {
    // Use WinHTTP to read the system proxy (set via netsh winhttp set proxy
    // or inherited from IE/Edge proxy settings).
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxy{};
    if (!WinHttpGetIEProxyConfigForCurrentUser(&ieProxy)) {
        AURA_LOG_DEBUG("ProxySettings",
            "WinHttpGetIEProxyConfigForCurrentUser returned no proxy.");
        m_config.mode = ProxyMode::NoProxy;
        return;
    }

    if (ieProxy.fAutoDetect || ieProxy.lpszAutoConfigUrl) {
        // PAC file / WPAD auto-detect — let Qt handle via ApplicationProxy
        m_config.mode = ProxyMode::SystemDefault;
        AURA_LOG_INFO("ProxySettings", "System proxy: auto-detect / PAC");
    } else if (ieProxy.lpszProxy) {
        // Manual proxy string, e.g. "http=proxy.corp.com:8080;https=proxy.corp.com:8080"
        const std::wstring proxyWStr(ieProxy.lpszProxy);
        std::string proxyStr(proxyWStr.begin(), proxyWStr.end());

        // Find HTTPS entry if present
        std::string entry = proxyStr;
        const size_t httpsPos = proxyStr.find("https=");
        if (httpsPos != std::string::npos) {
            entry = proxyStr.substr(httpsPos + 6);
            const size_t semi = entry.find(';');
            if (semi != std::string::npos) entry = entry.substr(0, semi);
        } else {
            // Strip "http=" prefix if present
            if (entry.starts_with("http=")) entry = entry.substr(5);
        }

        // Strip protocol prefix
        if (entry.starts_with("http://"))  entry = entry.substr(7);
        if (entry.starts_with("https://")) entry = entry.substr(8);

        // Split host:port
        const size_t colon = entry.rfind(':');
        if (colon != std::string::npos) {
            m_config.host = entry.substr(0, colon);
            try { m_config.port = static_cast<uint16_t>(std::stoi(entry.substr(colon + 1))); }
            catch (...) { m_config.port = kDefaultProxyPort; }
        } else {
            m_config.host = entry;
            m_config.port = kDefaultProxyPort;
        }

        if (!m_config.host.empty()) {
            m_config.mode = ProxyMode::Manual;
            AURA_LOG_INFO("ProxySettings",
                "System proxy: {}:{}", m_config.host, m_config.port);
        }
    } else {
        m_config.mode = ProxyMode::NoProxy;
        AURA_LOG_INFO("ProxySettings", "No system proxy configured.");
    }

    // Bypass list
    if (ieProxy.lpszProxyBypass) {
        const std::wstring bypassWStr(ieProxy.lpszProxyBypass);
        m_config.noProxyList = std::string(bypassWStr.begin(), bypassWStr.end());
    }

    // Free WinHTTP allocated strings
    if (ieProxy.lpszAutoConfigUrl) GlobalFree(ieProxy.lpszAutoConfigUrl);
    if (ieProxy.lpszProxy)         GlobalFree(ieProxy.lpszProxy);
    if (ieProxy.lpszProxyBypass)   GlobalFree(ieProxy.lpszProxyBypass);
}

// =============================================================================
void ProxySettings::apply(QNetworkAccessManager* nam) const {
    if (!nam) return;
    nam->setProxy(activeProxy());
    AURA_LOG_DEBUG("ProxySettings", "Proxy applied to QNetworkAccessManager.");
}

// =============================================================================
QNetworkProxy ProxySettings::activeProxy() const {
    switch (m_config.mode) {
    case ProxyMode::NoProxy:
        return QNetworkProxy(QNetworkProxy::NoProxy);

    case ProxyMode::SystemDefault:
        // Qt reads system proxy via QNetworkProxyFactory::systemProxyForQuery
        // which is already enabled globally in main.cpp via
        // QNetworkProxyFactory::setUseSystemConfiguration(true).
        return QNetworkProxy(QNetworkProxy::DefaultProxy);

    case ProxyMode::Manual:
    case ProxyMode::EnvVar: {
        QNetworkProxy proxy(QNetworkProxy::HttpProxy);
        proxy.setHostName(QString::fromStdString(m_config.host));
        proxy.setPort(m_config.port);
        if (!m_config.username.empty())
            proxy.setUser(QString::fromStdString(m_config.username));
        if (!m_config.password.empty())
            proxy.setPassword(QString::fromStdString(m_config.password));
        return proxy;
    }
    }
    return QNetworkProxy(QNetworkProxy::DefaultProxy);
}

// =============================================================================
// Static factories
// =============================================================================
ProxySettings ProxySettings::fromConfig(const ProxyConfig& cfg) {
    ProxySettings ps;
    ps.loadFromSettings(cfg);
    return ps;
}

ProxySettings ProxySettings::systemProxy() {
    ProxySettings ps;
    ps.loadFromSystem();
    return ps;
}

ProxySettings ProxySettings::noProxy() {
    ProxySettings ps;
    ps.m_config.mode = ProxyMode::NoProxy;
    return ps;
}

} // namespace aura

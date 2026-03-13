#pragma once
// =============================================================================
// ProxySettings.h -- Task 195: HTTPS proxy support for all cloud API calls
//
// Reads proxy configuration from:
//   1. SettingsModel (user-configured manual proxy)
//   2. Windows system proxy (WinHTTP / Internet Explorer settings)
//   3. Environment variables: HTTPS_PROXY, HTTP_PROXY, NO_PROXY
//
// All three cloud classes (LicenseClient, UpdateService, TelemetryClient)
// call ProxySettings::apply(QNetworkAccessManager*) on construction.
// =============================================================================
#include <string>
#include <memory>

class QNetworkAccessManager;
class QNetworkProxy;

namespace aura {

enum class ProxyMode {
    SystemDefault,   // Use Windows system proxy (WinHTTP)
    Manual,          // User-configured host:port + optional credentials
    NoProxy,         // Bypass all proxies (direct connection)
    EnvVar,          // From HTTPS_PROXY / HTTP_PROXY environment variable
};

struct ProxyConfig {
    ProxyMode   mode{ProxyMode::SystemDefault};
    std::string host;           // e.g. "proxy.corp.com"
    uint16_t    port{8080};
    std::string username;       // optional
    std::string password;       // optional -- stored encrypted in SecurityVault
    std::string noProxyList;    // comma-separated bypass list, e.g. "localhost,192.168.*"
};

class ProxySettings {
public:
    ProxySettings();
    ~ProxySettings();

    // Load proxy config from SettingsModel (persisted settings)
    void loadFromSettings(const ProxyConfig& cfg);

    // Load proxy from environment variable (HTTPS_PROXY, HTTP_PROXY)
    void loadFromEnvironment();

    // Load proxy from Windows system settings (WinHTTP)
    void loadFromSystem();

    // Apply the current proxy config to a QNetworkAccessManager.
    // Call this immediately after constructing the QNAM.
    void apply(QNetworkAccessManager* nam) const;

    // Return the active QNetworkProxy object
    QNetworkProxy activeProxy() const;

    const ProxyConfig& config() const { return m_config; }

    // Convenience: build from a ProxyConfig and return ready-to-use object
    static ProxySettings fromConfig(const ProxyConfig& cfg);
    static ProxySettings systemProxy();
    static ProxySettings noProxy();

private:
    ProxyConfig m_config;
};

} // namespace aura

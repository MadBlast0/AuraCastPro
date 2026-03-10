#pragma once
#include <QString>
#include <QNetworkProxy>

// Reads Windows system proxy settings and applies them to Qt network requests.
// Critical for enterprise users behind corporate firewalls.
class ProxySettings {
public:
    // Call once at startup before any QNetworkAccessManager is created.
    // Reads WINHTTP / IE proxy settings and configures QNetworkProxy globally.
    static void applySystemProxy();

    // Returns true if a proxy was detected and applied
    static bool isProxyActive() { return s_proxyActive; }

    // Current proxy description (for diagnostics)
    static QString description() { return s_description; }

private:
    static bool s_proxyActive;
    static QString s_description;
};

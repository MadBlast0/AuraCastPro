#include "../pch.h"  // PCH
#include "ProxySettings.h"
#include "../utils/Logger.h"
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QList>

bool    ProxySettings::s_proxyActive  = false;
QString ProxySettings::s_description  = "No proxy";

void ProxySettings::applySystemProxy() {
    // Use Qt's built-in system proxy detection (reads WinHTTP/IE settings)
    QNetworkProxyQuery query(QUrl("https://license.auracastpro.com"));
    QList<QNetworkProxy> proxies =
        QNetworkProxyFactory::systemProxyForQuery(query);

    if (!proxies.isEmpty()) {
        const QNetworkProxy& proxy = proxies.first();
        if (proxy.type() != QNetworkProxy::NoProxy &&
            proxy.type() != QNetworkProxy::DefaultProxy) {

            QNetworkProxy::setApplicationProxy(proxy);
            s_proxyActive = true;

            QString typeStr;
            switch (proxy.type()) {
                case QNetworkProxy::HttpProxy:   typeStr = "HTTP";   break;
                case QNetworkProxy::Socks5Proxy: typeStr = "SOCKS5"; break;
                case QNetworkProxy::HttpCachingProxy: typeStr = "HTTP (caching)"; break;
                default: typeStr = "Unknown";
            }

            s_description = QString("%1 proxy: %2:%3")
                .arg(typeStr)
                .arg(proxy.hostName())
                .arg(proxy.port());

            LOG_INFO("ProxySettings: Applied {} proxy {}:{}",
                     typeStr.toStdString(),
                     proxy.hostName().toStdString(),
                     proxy.port());
            return;
        }
    }

    // No proxy -- use direct connection
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    s_proxyActive = false;
    s_description = "Direct connection (no proxy)";
    LOG_INFO("ProxySettings: No proxy detected -- direct connection");
}

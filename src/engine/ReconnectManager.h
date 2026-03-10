#pragma once
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>

// Monitors packet flow and attempts automatic reconnection when it stops.
// Uses exponential backoff with a maximum attempt count.
class ReconnectManager {
public:
    struct Config {
        int    silenceTimeoutSec  = 3;   // no packets → trigger disconnect
        int    retryIntervalSec   = 5;   // seconds between reconnect attempts
        int    maxRetries         = 12;  // give up after this many attempts
        int    checkIntervalMs    = 1000;
    };

    using Callback = std::function<void()>;

    explicit ReconnectManager(Config cfg = {});
    ~ReconnectManager();

    // Call these from the packet receive callback
    void onPacketReceived();

    // Called when connection established
    void onConnected(const std::string& deviceId);
    void onDisconnected();

    // Wired to reconnect logic
    void setReconnectAttempt(Callback cb)  { m_reconnectCb  = std::move(cb); }
    void setOnTimeout(Callback cb)         { m_timeoutCb    = std::move(cb); }
    void setOnGiveUp(Callback cb)          { m_giveUpCb     = std::move(cb); }
    void setOnReconnected(Callback cb)     { m_reconnectedCb = std::move(cb); }

    void start();
    void stop();

    bool isConnected() const { return m_connected.load(); }
    int  retryCount()  const { return m_retryCount.load(); }

private:
    void monitorLoop();

    Config                    m_cfg;
    std::atomic<bool>         m_running    { false };
    std::atomic<bool>         m_connected  { false };
    std::atomic<int>          m_retryCount { 0 };
    std::atomic<int64_t>      m_lastPacketMs { 0 };
    std::string               m_deviceId;
    std::thread               m_thread;

    Callback m_reconnectCb;
    Callback m_timeoutCb;
    Callback m_giveUpCb;
    Callback m_reconnectedCb;

    static int64_t nowMs();
};

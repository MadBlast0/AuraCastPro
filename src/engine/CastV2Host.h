#pragma once
// =============================================================================
// CastV2Host.h -- Google Cast V2 receiver: TLS + Protobuf
// =============================================================================
#include <functional>
#include <string>
#include <memory>
#include <thread>
#include <atomic>

namespace aura {

struct CastSessionInfo {
    std::string sessionId;
    std::string ipAddress;
    std::string appId;
};

class CastV2Host {
public:
    using SessionStartedCallback = std::function<void(const CastSessionInfo&)>;
    using SessionEndedCallback   = std::function<void(const std::string& id)>;

    CastV2Host();
    ~CastV2Host();

    void init();
    void start();
    void stop();
    void shutdown();

    // Enable/disable new incoming mirror sessions
    void setMirroringActive(bool active) { m_mirroringActive.store(active, std::memory_order_relaxed); }

    void setSessionStartedCallback(SessionStartedCallback cb);
    void setSessionEndedCallback(SessionEndedCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_mirroringActive{true};
    std::atomic<int>  m_activeSessions{0};  // live handleSession thread count
    std::thread       m_acceptThread;

    SessionStartedCallback m_onStarted;
    SessionEndedCallback   m_onEnded;

    void acceptLoop();
    void handleSession(int clientSocket, const std::string& clientIp);
};

} // namespace aura

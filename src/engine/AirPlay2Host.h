#pragma once
// =============================================================================
// AirPlay2Host.h — AirPlay 2 receiver: SRP-6a pairing + AES-128-CTR decrypt
// =============================================================================
#include <functional>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <array>
#include <cstdint>
#include <vector>

namespace aura {

struct AirPlaySessionInfo {
    std::string deviceId;
    std::string deviceName;
    std::string ipAddress;
    std::string videoCodec;   // "H265" | "H264"
    uint16_t    videoPort{0};
    uint16_t    audioPort{0};
    bool        isPaired{false};
};

class AirPlay2Host {
public:
    using SessionStartedCallback  = std::function<void(const AirPlaySessionInfo&)>;
    using SessionEndedCallback    = std::function<void(const std::string& deviceId)>;
    using PinRequestCallback      = std::function<void(const std::string& pin)>;
    // Fired after SRP-6a verify completes: true = paired, false = wrong PIN
    using SessionPausedCallback   = std::function<void(const std::string& deviceId)>;
    using PairingResultCallback   = std::function<void(bool success)>;

    AirPlay2Host();
    ~AirPlay2Host();

    void init();
    void start();
    void stop();
    void shutdown();

    // Enable/disable new incoming mirror sessions
    // (existing sessions continue until their client disconnects)
    void setMirroringActive(bool active) { m_mirroringActive.store(active, std::memory_order_relaxed); }

    void setSessionStartedCallback(SessionStartedCallback cb);
    void setSessionEndedCallback(SessionEndedCallback cb);
    void setSessionPausedCallback(SessionPausedCallback cb);
    void setPinRequestCallback(PinRequestCallback cb);
    void setPairingResultCallback(PairingResultCallback cb);  // NEW

    // Decrypt an AES-128-CTR video packet in-place (called by receiver pipeline)
    static void decryptVideoPacket(uint8_t* data, int len,
                                   const std::array<uint8_t, 16>& key,
                                   const std::array<uint8_t, 16>& iv);

    std::string generatePin() const;
    int         activeSessionCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::atomic<bool>  m_running{false};
    std::atomic<bool>  m_mirroringActive{true};
    std::atomic<int>   m_activeSessions{0};  // counts live handleSession threads
    std::thread        m_acceptThread;

    SessionStartedCallback m_onSessionStarted;
    SessionEndedCallback   m_onSessionEnded;
    SessionPausedCallback  m_onSessionPaused;
    PinRequestCallback     m_onPinRequest;
    PairingResultCallback  m_onPairingResult;  // NEW

    void acceptLoop();
    void handleSession(int clientSocket, std::string clientIp);
};

} // namespace aura

#pragma once
// =============================================================================
// TimingServer.h -- UDP server for AirPlay timing/NTP sync on port 7000
//
// The iPad sends NTP timing requests to establish clock synchronization
// before sending video data. We need to respond to these requests.
// =============================================================================

#include <atomic>
#include <cstdint>
#include <thread>
#include <string>

namespace aura {

class TimingServer {
public:
    explicit TimingServer(uint16_t port = 7000);
    ~TimingServer();

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    void runLoop();
    void handleTimingRequest(const uint8_t* data, size_t len, 
                            const std::string& clientIp, uint16_t clientPort);

    uint16_t m_port;
    std::atomic<bool> m_running{false};
    std::atomic<uintptr_t> m_socket{0};
    std::thread m_thread;
};

} // namespace aura

#pragma once
// =============================================================================
// ReceiverSocket.h -- UDP socket optimised for real-time video reception.
//
// Design:
//   - Single non-blocking UDP socket bound to a configurable port
//   - Oversized kernel receive buffer (8 MB) to absorb burst packets
//   - Delivers raw packet bytes to a registered callback on the IO thread
//   - Integrated packet timestamping for jitter measurement
//   - Thread-safe start / stop lifecycle
//
// The socket is driven by UDPServerThreadPool which calls receiveOnce()
// in a tight loop from dedicated IO threads. This file owns the socket
// lifetime; UDPServerThreadPool owns the threading.
// =============================================================================

#include <functional>
#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>
#include <string>
#include <chrono>

#include "PacketValidator.h"

// Forward-declare Boost.Asio types to keep this header light
namespace boost::asio {
class io_context;
namespace ip { class udp; template<class> class basic_endpoint; }
} // namespace boost::asio

namespace aura {

struct RawPacket {
    std::vector<uint8_t> data;       // Payload bytes
    std::string          senderIp;   // Source IP string
    uint16_t             senderPort; // Source UDP port
    std::chrono::steady_clock::time_point receivedAt; // Arrival timestamp
};

class ReceiverSocket {
public:
    // Called from the IO thread for every received packet.
    // Must be fast and non-blocking -- do not perform heavy work here.
    // Post work to a queue and process on a separate thread.
    using PacketCallback = std::function<void(RawPacket packet)>;

    explicit ReceiverSocket(uint16_t port, PacketCallback callback);
    ~ReceiverSocket();

    // Bind the socket and start listening.
    // Throws std::runtime_error on bind failure.
    void start();

    // Gracefully stop receiving and close the socket.
    void stop();

    // Receive a single packet (blocking up to timeoutMs).
    // Called by UDPServerThreadPool in a loop.
    // Returns false if the socket was stopped.
    bool receiveOnce(int timeoutMs = 5);

    bool isRunning() const { return m_running.load(); }
    uint16_t port() const { return m_port; }

    // Allow packets only from this IP (wires into PacketValidator -- Task 072)
    void setAllowedSource(const std::string& ip) { m_validator.setAllowedSource(ip); }
    void clearAllowedSource()                    { m_validator.clearAllowedSource(); }
    void resetPacketSequence()                   { m_validator.resetSequence(); }

    // Task 074: Bind to a specific local network interface IP.
    // Must be called before start(). "" = bind to all interfaces (INADDR_ANY).
    // Example: "192.168.1.5"
    void setBindInterface(const std::string& ip) { m_bindIp = ip; }
    const std::string& bindInterface() const { return m_bindIp; }

    // Statistics
    uint64_t packetsReceived() const { return m_packetsReceived.load(); }
    uint64_t bytesReceived()   const { return m_bytesReceived.load(); }

private:
    uint16_t       m_port{7236};  // default AirPlay UDP port
    PacketCallback m_callback;
    std::atomic<bool> m_running{false};
    std::string    m_bindIp;   // Task 074: "" = INADDR_ANY, else specific interface IP

    // Platform socket handle (SOCKET on Windows = uintptr_t)
    std::atomic<uintptr_t> m_socket{0};

    // Receive buffer -- reused across calls to avoid allocation per packet
    static constexpr std::size_t kMaxPacketSize = 65536;
    std::vector<uint8_t> m_recvBuf;

    // Packet security validator -- Task 072
    aura::PacketValidator m_validator;

    // Statistics counters
    std::atomic<uint64_t> m_packetsReceived{0};
    std::atomic<uint64_t> m_bytesReceived{0};

    void createAndBind();
    void setSocketBuffers();
    void closeSocket();
};

} // namespace aura

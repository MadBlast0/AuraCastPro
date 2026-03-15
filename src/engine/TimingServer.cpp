#include "TimingServer.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include <chrono>
#include <cstring>

namespace aura {

TimingServer::TimingServer(uint16_t port) : m_port(port) {}

TimingServer::~TimingServer() {
    stop();
}

void TimingServer::start() {
    if (m_running.load()) return;

    // Create UDP socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        AURA_LOG_ERROR("TimingServer", "Failed to create socket: {}", WSAGetLastError());
        return;
    }

    // Allow address reuse (multiple processes can bind to same port)
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR) {
        AURA_LOG_WARN("TimingServer", "Failed to set SO_REUSEADDR: {}", WSAGetLastError());
    }

    // Bind to port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        AURA_LOG_ERROR("TimingServer", "Failed to bind to port {}: {}", m_port, WSAGetLastError());
        closesocket(sock);
        return;
    }

    // Set socket to non-blocking
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    m_socket.store(static_cast<uintptr_t>(sock));
    m_running.store(true);

    AURA_LOG_INFO("TimingServer", "Started on UDP port {}", m_port);

    // Start receive thread
    m_thread = std::thread([this] { runLoop(); });
}

void TimingServer::stop() {
    if (!m_running.load()) return;

    m_running.store(false);

    if (m_thread.joinable()) {
        m_thread.join();
    }

    SOCKET sock = static_cast<SOCKET>(m_socket.load());
    if (sock != 0) {
        closesocket(sock);
        m_socket.store(0);
    }

    AURA_LOG_INFO("TimingServer", "Stopped");
}

void TimingServer::runLoop() {
    uint8_t buffer[2048];
    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    SOCKET sock = static_cast<SOCKET>(m_socket.load());

    while (m_running.load()) {
        int received = recvfrom(sock, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                               reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

        if (received > 0) {
            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
            uint16_t clientPort = ntohs(clientAddr.sin_port);

            handleTimingRequest(buffer, received, clientIp, clientPort);
        } else if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                AURA_LOG_ERROR("TimingServer", "recvfrom error: {}", err);
            }
        }

        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void TimingServer::handleTimingRequest(const uint8_t* data, size_t len,
                                       const std::string& clientIp, uint16_t clientPort) {
    // AirPlay timing protocol uses NTP-style packets
    // Minimum NTP packet is 48 bytes
    if (len < 8) {
        AURA_LOG_WARN("TimingServer", "Received short packet ({} bytes) from {}:{}", 
                     len, clientIp, clientPort);
        return;
    }

    AURA_LOG_DEBUG("TimingServer", "Received timing request ({} bytes) from {}:{}", 
                  len, clientIp, clientPort);

    // Simple NTP response: echo back the request with current timestamp
    // For AirPlay, we can use a simplified response
    uint8_t response[32];
    std::memset(response, 0, sizeof(response));

    // Copy first 8 bytes from request (typically contains sequence/type info)
    if (len >= 8) {
        std::memcpy(response, data, 8);
    }

    // Add current timestamp (NTP format: seconds since 1900 + fractional seconds)
    // For simplicity, we'll use a basic timestamp
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000;

    // NTP epoch is 1900, Unix epoch is 1970 (70 years = 2208988800 seconds difference)
    uint64_t ntpSeconds = seconds + 2208988800ULL;
    uint32_t ntpFraction = static_cast<uint32_t>((nanos * 4294967296ULL) / 1000000000ULL);

    // Write timestamp at offset 8 (big-endian)
    response[8]  = (ntpSeconds >> 56) & 0xFF;
    response[9]  = (ntpSeconds >> 48) & 0xFF;
    response[10] = (ntpSeconds >> 40) & 0xFF;
    response[11] = (ntpSeconds >> 32) & 0xFF;
    response[12] = (ntpSeconds >> 24) & 0xFF;
    response[13] = (ntpSeconds >> 16) & 0xFF;
    response[14] = (ntpSeconds >> 8) & 0xFF;
    response[15] = ntpSeconds & 0xFF;

    response[16] = (ntpFraction >> 24) & 0xFF;
    response[17] = (ntpFraction >> 16) & 0xFF;
    response[18] = (ntpFraction >> 8) & 0xFF;
    response[19] = ntpFraction & 0xFF;

    // Send response back to client
    SOCKET sock = static_cast<SOCKET>(m_socket.load());
    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    inet_pton(AF_INET, clientIp.c_str(), &destAddr.sin_addr);
    destAddr.sin_port = htons(clientPort);

    int sent = sendto(sock, reinterpret_cast<const char*>(response), sizeof(response), 0,
                     reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));

    if (sent > 0) {
        AURA_LOG_DEBUG("TimingServer", "Sent timing response ({} bytes) to {}:{}", 
                      sent, clientIp, clientPort);
    } else {
        AURA_LOG_ERROR("TimingServer", "Failed to send timing response: {}", WSAGetLastError());
    }
}

} // namespace aura

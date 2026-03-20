// =============================================================================
// ReceiverSocket.cpp -- High-performance UDP receive socket
// =============================================================================

#include "../pch.h"  // PCH
#include "ReceiverSocket.h"
#include "PacketValidator.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdexcept>
#include <format>

#pragma comment(lib, "ws2_32.lib")

namespace aura {

// Kernel socket buffer size: 8 MB.
// This large buffer prevents packet drops when the network layer bursts
// faster than our processing thread can drain the socket.
static constexpr int kSocketRecvBufSize = 8 * 1024 * 1024; // 8 MB

// -----------------------------------------------------------------------------
ReceiverSocket::ReceiverSocket(uint16_t port, PacketCallback callback)
    : m_port(port)
    , m_callback(std::move(callback)) {
    m_recvBuf.resize(kMaxPacketSize);

    // Initialise Winsock (safe to call multiple times)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("ReceiverSocket: WSAStartup failed");
    }
}

// -----------------------------------------------------------------------------
ReceiverSocket::~ReceiverSocket() {
    stop();
    WSACleanup();
}

// -----------------------------------------------------------------------------
void ReceiverSocket::start() {
    if (m_running.load()) return;
    createAndBind();
    m_running.store(true);
    AURA_LOG_INFO("ReceiverSocket", "Listening on UDP port {}", m_port);
}

// -----------------------------------------------------------------------------
void ReceiverSocket::stop() {
    if (!m_running.exchange(false)) return;
    closeSocket();
    AURA_LOG_INFO("ReceiverSocket", "Stopped. Packets: {} Bytes: {}",
                  m_packetsReceived.load(), m_bytesReceived.load());
}

// -----------------------------------------------------------------------------
bool ReceiverSocket::receiveOnce(int timeoutMs) {
    if (!m_running.load()) return false;

    SOCKET s = static_cast<SOCKET>(m_socket);

    // Poll the socket using select() with timeout
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(s, &readSet);

    timeval tv;
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int ready = select(0, &readSet, nullptr, nullptr, &tv);
    if (ready <= 0) return false; // timeout or select error

    sockaddr_in senderAddr{};
    int addrLen = sizeof(senderAddr);

    const int received = recvfrom(s,
        reinterpret_cast<char*>(m_recvBuf.data()),
        static_cast<int>(m_recvBuf.size()),
        0,
        reinterpret_cast<sockaddr*>(&senderAddr),
        &addrLen);

    if (received == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINTR && m_running.load()) {
            AURA_LOG_ERROR("ReceiverSocket", "recvfrom error: {}", err);
        }
        return false;
    }

    if (received == 0) return false;

    // Build RawPacket -- copy payload bytes
    RawPacket pkt;
    pkt.data.assign(m_recvBuf.begin(), m_recvBuf.begin() + received);
    pkt.receivedAt = std::chrono::steady_clock::now();
    pkt.senderPort = ntohs(senderAddr.sin_port);

    // Convert sender IP to string
    char ipStr[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &senderAddr.sin_addr, ipStr, sizeof(ipStr));
    pkt.senderIp = ipStr;

    m_packetsReceived.fetch_add(1, std::memory_order_relaxed);
    m_bytesReceived.fetch_add(static_cast<uint64_t>(received), std::memory_order_relaxed);

    // ── Task 072: packet validation [WIRED IN] ───────────────────────────────
    // RTP sequence number is bytes 2-3 (big-endian uint16).
    uint16_t seqNum = 0;
    if (pkt.data.size() >= 4) {
        seqNum = static_cast<uint16_t>(
            (static_cast<uint16_t>(pkt.data[2]) << 8) | pkt.data[3]);
    }
    const auto vResult = m_validator.validate(
        std::span<const uint8_t>(pkt.data.data(), pkt.data.size()),
        pkt.senderIp, seqNum);

    if (vResult != aura::ValidateResult::Ok) {
        AURA_LOG_TRACE("ReceiverSocket",
            "Packet dropped: {} (from {}:{})",
            aura::PacketValidator::resultString(vResult),
            pkt.senderIp, pkt.senderPort);
        return false; // DROP -- do not deliver to callback
    }

    // Deliver to registered callback
    if (m_callback) {
        m_callback(std::move(pkt));
    }

    return true;
}

// -----------------------------------------------------------------------------
void ReceiverSocket::createAndBind() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        throw std::runtime_error(std::format(
            "ReceiverSocket: socket() failed: {}", WSAGetLastError()));
    }
    m_socket = static_cast<uintptr_t>(s);

    // Allow address reuse so we can restart quickly after a crash
    int optVal = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optVal), sizeof(optVal));

    setSocketBuffers();

    // Task 074: Bind to specific interface if set, else all interfaces (INADDR_ANY)
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(m_port);

    if (!m_bindIp.empty()) {
        if (inet_pton(AF_INET, m_bindIp.c_str(), &addr.sin_addr) != 1) {
            AURA_LOG_WARN("ReceiverSocket",
                "Invalid bind IP '{}' -- falling back to INADDR_ANY", m_bindIp);
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            AURA_LOG_INFO("ReceiverSocket",
                "Binding to interface {} port {}", m_bindIp, m_port);
        }
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        closesocket(s);
        throw std::runtime_error(std::format(
            "ReceiverSocket: bind() on port {} failed: {}", m_port, err));
    }
}

// -----------------------------------------------------------------------------
void ReceiverSocket::setSocketBuffers() {
    SOCKET s = static_cast<SOCKET>(m_socket);

    // Set a large kernel receive buffer to handle network bursts
    int rcvBufSize = kSocketRecvBufSize;
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&rcvBufSize), sizeof(rcvBufSize)) == SOCKET_ERROR) {
        AURA_LOG_WARN("ReceiverSocket", "Could not set SO_RCVBUF to {} bytes: {}",
                      kSocketRecvBufSize, WSAGetLastError());
    } else {
        // Verify the kernel actually honoured our request
        int actual = 0;
        int len = sizeof(actual);
        getsockopt(s, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<char*>(&actual), &len);
        AURA_LOG_DEBUG("ReceiverSocket", "SO_RCVBUF: requested {} bytes, got {} bytes",
                       kSocketRecvBufSize, actual);
    }
}

// -----------------------------------------------------------------------------
void ReceiverSocket::closeSocket() {
    if (m_socket != 0) {
        closesocket(static_cast<SOCKET>(m_socket));
        m_socket = 0;
    }
}

} // namespace aura

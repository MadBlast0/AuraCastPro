#include "MirroringListener.h"
#include "../utils/Logger.h"
#include <vector>

MirroringListener::MirroringListener(int port)
    : m_port(port)
    , m_listenSocket(INVALID_SOCKET)
    , m_running(false)
{
}

MirroringListener::~MirroringListener() {
    stop();
}

bool MirroringListener::start() {
    if (m_running.load()) {
        AURA_LOG_WARN("MirroringListener", "Already running on port {}", m_port);
        return true;
    }

    // Create TCP socket
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        AURA_LOG_ERROR("MirroringListener", "Failed to create socket: {}", WSAGetLastError());
        return false;
    }

    // Set SO_REUSEADDR
    int reuse = 1;
    if (setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<char*>(&reuse), sizeof(reuse)) < 0) {
        AURA_LOG_WARN("MirroringListener", "Failed to set SO_REUSEADDR: {}", WSAGetLastError());
    }

    // Bind to port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_short>(m_port));

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        AURA_LOG_ERROR("MirroringListener", "Failed to bind to port {}: {}", m_port, WSAGetLastError());
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    // Listen
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        AURA_LOG_ERROR("MirroringListener", "Failed to listen on port {}: {}", m_port, WSAGetLastError());
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    m_running.store(true);
    m_acceptThread = std::thread(&MirroringListener::acceptLoop, this);

    AURA_LOG_INFO("MirroringListener", "Started on port {} - waiting for video stream connection", m_port);
    return true;
}

void MirroringListener::stop() {
    if (!m_running.load()) {
        return;
    }

    AURA_LOG_INFO("MirroringListener", "Stopping...");
    m_running.store(false);

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    AURA_LOG_INFO("MirroringListener", "Stopped");
}

void MirroringListener::acceptLoop() {
    AURA_LOG_INFO("MirroringListener", "Accept loop started on port {}", m_port);

    while (m_running.load()) {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(m_listenSocket, 
                                     reinterpret_cast<sockaddr*>(&clientAddr), 
                                     &clientAddrLen);

        if (clientSocket == INVALID_SOCKET) {
            if (m_running.load()) {
                AURA_LOG_ERROR("MirroringListener", "Accept failed: {}", WSAGetLastError());
            }
            break;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));

        AURA_LOG_INFO("MirroringListener", "=== VIDEO STREAM CONNECTION from {} ===", clientIp);

        // Handle this client in a new thread
        std::thread clientThread(&MirroringListener::handleClient, this, clientSocket);
        clientThread.detach();
    }

    AURA_LOG_INFO("MirroringListener", "Accept loop ended");
}

void MirroringListener::handleClient(SOCKET clientSocket) {
    AURA_LOG_INFO("MirroringListener", "Handling video stream client");

    std::vector<uint8_t> headerBuffer(128);
    int frameCount = 0;

    while (m_running.load()) {
        // Read 128-byte header
        int totalRead = 0;
        while (totalRead < 128) {
            int n = recv(clientSocket, reinterpret_cast<char*>(headerBuffer.data() + totalRead), 
                        128 - totalRead, 0);
            if (n <= 0) {
                if (n == 0) {
                    AURA_LOG_INFO("MirroringListener", "Client closed video stream connection");
                } else {
                    AURA_LOG_ERROR("MirroringListener", "Recv error: {}", WSAGetLastError());
                }
                goto cleanup;
            }
            totalRead += n;
        }

        // Check if this is actually RTSP text (POST/GET) instead of binary video data
        if (headerBuffer[0] == 'P' || headerBuffer[0] == 'G') {
            AURA_LOG_DEBUG("MirroringListener", "Received RTSP request on video port (ignoring)");
            // Skip this - it's not video data
            continue;
        }

        // Log first frame header for debugging
        if (frameCount == 0) {
            std::string hexDump;
            for (int i = 0; i < 32; i++) {
                hexDump += std::format("{:02x} ", headerBuffer[i]);
            }
            AURA_LOG_INFO("MirroringListener", "First video frame header (32 bytes): {}", hexDump);
        }

        // Parse payload size from header (bytes 4-7, big endian)
        uint32_t payloadSize = (static_cast<uint32_t>(headerBuffer[4]) << 24) |
                               (static_cast<uint32_t>(headerBuffer[5]) << 16) |
                               (static_cast<uint32_t>(headerBuffer[6]) << 8) |
                               static_cast<uint32_t>(headerBuffer[7]);

        if (payloadSize == 0 || payloadSize > 10 * 1024 * 1024) {
            AURA_LOG_WARN("MirroringListener", "Invalid payload size: {}", payloadSize);
            continue;
        }

        // Read payload
        std::vector<uint8_t> payload(payloadSize);
        totalRead = 0;
        while (totalRead < static_cast<int>(payloadSize)) {
            int n = recv(clientSocket, reinterpret_cast<char*>(payload.data() + totalRead), 
                        payloadSize - totalRead, 0);
            if (n <= 0) {
                if (n == 0) {
                    AURA_LOG_INFO("MirroringListener", "Client closed during payload read");
                } else {
                    AURA_LOG_ERROR("MirroringListener", "Recv error during payload: {}", WSAGetLastError());
                }
                goto cleanup;
            }
            totalRead += n;
        }

        frameCount++;
        if (frameCount % 30 == 0) {
            AURA_LOG_INFO("MirroringListener", "Received {} video frames (latest: {} bytes payload)", 
                         frameCount, payloadSize);
        }

        // Call callback if set
        if (m_videoDataCallback) {
            m_videoDataCallback(headerBuffer.data(), headerBuffer.size(), 
                               payload.data(), payload.size());
        }
    }

cleanup:
    closesocket(clientSocket);
    AURA_LOG_INFO("MirroringListener", "Video stream client disconnected (received {} frames)", frameCount);
}

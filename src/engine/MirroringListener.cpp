#include "MirroringListener.h"
#include "../utils/Logger.h"
#include <vector>
#include <algorithm>

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
    constexpr size_t kReadChunk = 16 * 1024;
    constexpr size_t kMaxBuffered = 4 * 1024 * 1024;
    constexpr uint32_t kMaxPayload = 2 * 1024 * 1024;
    std::vector<uint8_t> readBuffer(kReadChunk);
    std::vector<uint8_t> streamBuffer;
    streamBuffer.reserve(256 * 1024);
    int frameCount = 0;
    int invalidFrames = 0;

    auto readU32BE = [](const uint8_t* p) -> uint32_t {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) |
               static_cast<uint32_t>(p[3]);
    };
    auto readU32LE = [](const uint8_t* p) -> uint32_t {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    };

    while (m_running.load()) {
        const int n = recv(clientSocket, reinterpret_cast<char*>(readBuffer.data()),
                           static_cast<int>(readBuffer.size()), 0);
        if (n <= 0) {
            if (n == 0) {
                AURA_LOG_INFO("MirroringListener", "Client closed video stream connection");
            } else {
                AURA_LOG_ERROR("MirroringListener", "Recv error: {}", WSAGetLastError());
            }
            goto cleanup;
        }
        streamBuffer.insert(streamBuffer.end(), readBuffer.begin(), readBuffer.begin() + n);

        bool madeProgress = true;
        while (madeProgress && !streamBuffer.empty()) {
            madeProgress = false;

            // RTSP interleaved framing: '$' channel length_hi length_lo payload...
            if (streamBuffer.size() >= 4 && streamBuffer[0] == 0x24) {
                const uint16_t packetLen = static_cast<uint16_t>(
                    (static_cast<uint16_t>(streamBuffer[2]) << 8) | streamBuffer[3]);
                if (packetLen == 0 || packetLen > 65535) {
                    streamBuffer.erase(streamBuffer.begin());
                    madeProgress = true;
                    continue;
                }
                if (streamBuffer.size() < (4 + packetLen)) {
                    break;
                }

                const uint8_t* payload = streamBuffer.data() + 4;
                const size_t payloadSize = packetLen;
                frameCount++;
                if (frameCount % 120 == 0) {
                    AURA_LOG_INFO("MirroringListener",
                                  "Interleaved packets processed: {} (latest {} bytes)",
                                  frameCount, payloadSize);
                }
                if (m_videoDataCallback) {
                    m_videoDataCallback(nullptr, 0, payload, payloadSize);
                }
                streamBuffer.erase(streamBuffer.begin(), streamBuffer.begin() + 4 + packetLen);
                madeProgress = true;
                continue;
            }

            // Legacy mirroring framing: 128-byte header + payload length.
            // Different senders encode payload length in different places/endian.
            if (streamBuffer.size() >= 128) {
                const uint8_t* b = streamBuffer.data();
                const uint32_t candidateSizes[] = {
                    readU32BE(b + 4),  // historical default
                    readU32LE(b + 4),
                    readU32BE(b + 0),
                    readU32LE(b + 0)
                };

                uint32_t payloadSize = 0;
                for (uint32_t s : candidateSizes) {
                    if (s > 0 && s <= kMaxPayload && streamBuffer.size() >= (128 + s)) {
                        payloadSize = s;
                        break;
                    }
                }

                if (payloadSize > 0) {
                    const uint8_t* header = streamBuffer.data();
                    const uint8_t* payload = streamBuffer.data() + 128;
                    frameCount++;
                    if (frameCount % 120 == 0) {
                        AURA_LOG_INFO("MirroringListener",
                                      "Legacy frames processed: {} (latest {} bytes)",
                                      frameCount, payloadSize);
                    }
                    if (m_videoDataCallback) {
                        m_videoDataCallback(header, 128, payload, payloadSize);
                    }
                    streamBuffer.erase(streamBuffer.begin(), streamBuffer.begin() + 128 + payloadSize);
                    madeProgress = true;
                    continue;
                }
            }
            
            // Raw stream mode: If we have enough data and it doesn't match known formats,
            // treat it as raw AVCC stream and send chunks to callback
            if (streamBuffer.size() >= 1024) {
                // Send 1KB chunks as raw video data
                const size_t chunkSize = std::min(size_t(16384), streamBuffer.size());
                if (m_videoDataCallback) {
                    m_videoDataCallback(nullptr, 0, streamBuffer.data(), chunkSize);
                }
                streamBuffer.erase(streamBuffer.begin(), streamBuffer.begin() + chunkSize);
                frameCount++;
                if (frameCount % 120 == 0) {
                    AURA_LOG_INFO("MirroringListener",
                                  "Raw stream chunks processed: {} (latest {} bytes)",
                                  frameCount, chunkSize);
                }
                madeProgress = true;
                continue;
            }

            // Not parseable yet. Shift one byte and resync only if we have a lot of data.
            if (streamBuffer.size() > 256) {
                ++invalidFrames;
                if (invalidFrames % 2000 == 0) {
                    AURA_LOG_WARN("MirroringListener",
                                  "Resyncing stream (invalid frames: {}, buffered: {} bytes)",
                                  invalidFrames, streamBuffer.size());
                }
                streamBuffer.erase(streamBuffer.begin());
                madeProgress = true;
                continue;
            }
        }

        // Prevent unbounded growth if we can't parse incoming bytes.
        if (streamBuffer.size() > kMaxBuffered) {
            const size_t drop = streamBuffer.size() - (kMaxBuffered / 2);
            AURA_LOG_WARN("MirroringListener",
                          "Dropping {} bytes from buffered stream (resync)", drop);
            streamBuffer.erase(streamBuffer.begin(), streamBuffer.begin() + drop);
        }
    }

cleanup:
    closesocket(clientSocket);
    AURA_LOG_INFO("MirroringListener",
                  "Video stream client disconnected (received {} frames, {} invalid)",
                  frameCount, invalidFrames);
}

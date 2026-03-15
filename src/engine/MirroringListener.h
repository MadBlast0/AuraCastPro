#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>

// MirroringListener: Separate TCP listener for receiving video stream data
// This is NOT the RTSP control connection - it's a dedicated connection for video frames
// iPad opens a new TCP connection to this port after Stream SETUP completes
class MirroringListener {
public:
    MirroringListener(int port);
    ~MirroringListener();

    bool start();
    void stop();
    bool isRunning() const { return m_running.load(); }

    // Callback when video data is received
    using VideoDataCallback = std::function<void(const uint8_t* header, size_t headerSize, 
                                                   const uint8_t* payload, size_t payloadSize)>;
    void setVideoDataCallback(VideoDataCallback callback) { m_videoDataCallback = callback; }

private:
    void acceptLoop();
    void handleClient(SOCKET clientSocket);

    int m_port;
    SOCKET m_listenSocket;
    std::atomic<bool> m_running;
    std::thread m_acceptThread;
    VideoDataCallback m_videoDataCallback;
};

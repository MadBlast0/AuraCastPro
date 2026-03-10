#pragma once
// =============================================================================
// UDPServerThreadPool.h — Thread pool driving the UDP receive socket.
//
// Creates N worker threads that each call ReceiverSocket::receiveOnce()
// in a tight loop. Having multiple threads prevents a single slow packet
// callback from blocking socket reads.
//
// Default: 2 IO threads.
// =============================================================================
#include "ReceiverSocket.h"
#include <atomic>
#include <thread>
#include <vector>
#include <memory>

namespace aura {
class UDPServerThreadPool {
public:
    explicit UDPServerThreadPool(std::shared_ptr<ReceiverSocket> socket, int numThreads = 2);
    ~UDPServerThreadPool();
    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }
private:
    std::shared_ptr<ReceiverSocket> m_socket;
    int m_numThreads{4};  // worker threads for packet I/O
    std::atomic<bool> m_running{false};
    std::vector<std::thread> m_threads;
    void workerLoop(int threadId);
    int  detectOptimalThreadCount();
};
} // namespace aura

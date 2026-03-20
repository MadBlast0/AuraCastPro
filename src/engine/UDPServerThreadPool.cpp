// =============================================================================
// UDPServerThreadPool.cpp -- Multi-threaded UDP socket drain pool
// =============================================================================

#include "../pch.h"  // PCH
#include "UDPServerThreadPool.h"
#include "ReceiverSocket.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <thread>

namespace aura {

// -----------------------------------------------------------------------------
UDPServerThreadPool::UDPServerThreadPool(std::shared_ptr<ReceiverSocket> socket, int numThreads)
    : m_socket(std::move(socket))
    , m_numThreads(numThreads > 0 ? numThreads : detectOptimalThreadCount()) {}

// -----------------------------------------------------------------------------
UDPServerThreadPool::~UDPServerThreadPool() {
    stop();
}

// -----------------------------------------------------------------------------
void UDPServerThreadPool::start() {
    if (m_running.exchange(true)) return;

    AURA_LOG_INFO("UDPServerThreadPool", "Starting {} IO threads...", m_numThreads);

    for (int i = 0; i < m_numThreads; ++i) {
        m_threads.emplace_back([this, i]() { workerLoop(i); });

        // Keep IO threads responsive without starving the UI/message pump.
        SetThreadPriority(m_threads.back().native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);

        // Give each thread a descriptive name (visible in debugger)
        const std::wstring name = std::format(L"aura-io-{}", i);
        SetThreadDescription(m_threads.back().native_handle(), name.c_str());
    }

    AURA_LOG_INFO("UDPServerThreadPool", "{} IO threads running.", m_numThreads);
}

// -----------------------------------------------------------------------------
void UDPServerThreadPool::stop() {
    if (!m_running.exchange(false)) return;

    AURA_LOG_INFO("UDPServerThreadPool", "Stopping IO threads...");

    // m_running = false signals all worker loops to exit.
    // Workers poll this flag in their loop condition.
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    }
    m_threads.clear();

    AURA_LOG_INFO("UDPServerThreadPool", "All IO threads stopped. Total packets: {}",
                  m_totalPackets.load());
}

// -----------------------------------------------------------------------------
void UDPServerThreadPool::workerLoop(int threadId) {
    AURA_LOG_DEBUG("UDPServerThreadPool", "IO thread {} started.", threadId);

    while (m_running.load(std::memory_order_relaxed)) {
        try {
            // receiveOnce() blocks up to 5ms via select(), then returns.
            // This prevents 100% CPU spin while still having < 5ms wake latency.
            const bool gotPacket = m_socket->receiveOnce(5 /*timeoutMs*/);
            if (gotPacket) {
                m_totalPackets.fetch_add(1, std::memory_order_relaxed);
            }
        } catch (const std::exception& e) {
            AURA_LOG_ERROR("UDPServerThreadPool",
                "IO thread {} caught exception: {} -- continuing.", threadId, e.what());
            // Brief pause to avoid tight error loops on persistent failures
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (...) {
            AURA_LOG_ERROR("UDPServerThreadPool",
                "IO thread {} caught unknown exception -- continuing.", threadId);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    AURA_LOG_DEBUG("UDPServerThreadPool", "IO thread {} exiting.", threadId);
}

// -----------------------------------------------------------------------------
int UDPServerThreadPool::detectOptimalThreadCount() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const int cores = static_cast<int>(si.dwNumberOfProcessors);
    // Use at most 4 IO threads. More than 4 rarely helps because the
    // bottleneck shifts to the PacketReorderCache mutex at high packet rates.
    return std::max(1, std::min(4, cores / 2));
}

} // namespace aura

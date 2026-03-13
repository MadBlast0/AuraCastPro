#include "../pch.h"  // PCH
#include "ReconnectManager.h"
#include "../utils/Logger.h"
#include <chrono>

ReconnectManager::ReconnectManager(Config cfg) : m_cfg(cfg) {}

ReconnectManager::~ReconnectManager() { stop(); }

int64_t ReconnectManager::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void ReconnectManager::onPacketReceived() {
    m_lastPacketMs.store(nowMs());
    if (!m_connected.load()) {
        // First packet after reconnect
        m_connected  = true;
        m_retryCount = 0;
        LOG_INFO("ReconnectManager: Connection restored (device: {})", m_deviceId);
        if (m_reconnectedCb) m_reconnectedCb();
    }
}

void ReconnectManager::onConnected(const std::string& deviceId) {
    m_deviceId   = deviceId;
    m_connected  = true;
    m_retryCount = 0;
    m_lastPacketMs.store(nowMs());
    LOG_INFO("ReconnectManager: Connected to {}", deviceId);
}

void ReconnectManager::onDisconnected() {
    m_connected = false;
    LOG_INFO("ReconnectManager: Disconnected from {}", m_deviceId);
}

void ReconnectManager::start() {
    if (m_running.exchange(true)) return;
    m_thread = std::thread([this]{ monitorLoop(); });
}

void ReconnectManager::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void ReconnectManager::monitorLoop() {
    bool wasConnected = false;
    int  retrying = 0;

    while (m_running.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(m_cfg.checkIntervalMs));

        bool connected = m_connected.load();
        int64_t silenceMs = nowMs() - m_lastPacketMs.load();

        if (connected && silenceMs > m_cfg.silenceTimeoutSec * 1000LL) {
            // Timeout -- looks like connection dropped
            LOG_WARN("ReconnectManager: No packets for {}ms -- triggering reconnect",
                     silenceMs);
            m_connected = false;
            retrying    = 0;
            if (m_timeoutCb) m_timeoutCb();
        }

        if (!connected && m_lastPacketMs.load() > 0) {
            // We were previously connected -- attempt reconnect with exponential backoff.
            // Backoff: retryIntervalSec * 2^(attempt-1), capped at 60 s.
            // Attempt 1 -> base interval, 2 -> 2x, 3 -> 4x, ... max 60 s.
            if (retrying < m_cfg.maxRetries) {
                int64_t sinceLastAttempt = nowMs() - m_lastPacketMs.load();
                int64_t backoffMs = static_cast<int64_t>(
                    m_cfg.retryIntervalSec * 1000LL *
                    std::min(1 << retrying, 64)); // 2^retrying capped at 64x
                if (sinceLastAttempt > backoffMs) {
                    retrying++;
                    m_retryCount.store(retrying);
                    LOG_INFO("ReconnectManager: Reconnect attempt {}/{} (backoff {}ms)",
                             retrying, m_cfg.maxRetries, backoffMs);
                    if (m_reconnectCb) m_reconnectCb();
                    m_lastPacketMs.store(nowMs()); // reset timer for next attempt
                }
            } else {
                LOG_ERROR("ReconnectManager: Gave up after {} attempts", retrying);
                retrying = 0;
                m_lastPacketMs = 0;  // stop retrying
                if (m_giveUpCb) m_giveUpCb();
            }
        }

        wasConnected = connected;
    }
}

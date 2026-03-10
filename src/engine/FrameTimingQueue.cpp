// =============================================================================
// FrameTimingQueue.cpp — Frame pacing for smooth 120fps playback
// =============================================================================
#include "FrameTimingQueue.h"
#include "../utils/Logger.h"
#include <algorithm>

namespace aura {

FrameTimingQueue::FrameTimingQueue() {}

void FrameTimingQueue::init() {
    AURA_LOG_INFO("FrameTimingQueue",
        "Initialised. Manages decode→present timing. "
        "Works with temporal_frame_pacing.hlsl for micro-stutter elimination.");
}

void FrameTimingQueue::reset() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queue.clear();
    m_lastDecodedUs = m_lastPresentedUs = 0;
    m_avgDecodeMs = m_avgJitterMs = 0;
    m_displayPeriodUs = 16666.67;
}

void FrameTimingQueue::pushFrame(uint32_t frameIndex, uint64_t decodedAtUs,
                                  uint64_t presentationTimeUs) {
    std::lock_guard<std::mutex> lk(m_mutex);

    // Compute blend weight relative to previous frame timing
    float blendWeight = 1.0f;
    if (!m_queue.empty()) {
        const uint64_t prevPts = m_queue.back().targetPresentUs;
        const uint64_t delta   = presentationTimeUs > prevPts
                                 ? presentationTimeUs - prevPts : 0;
        // If frames arrive faster than display period, blend toward previous
        blendWeight = static_cast<float>(
            std::clamp(delta / m_displayPeriodUs, 0.0, 1.0));
    }

    // Update decode latency EMA
    if (m_lastDecodedUs > 0) {
        const double decodeMs = (decodedAtUs - m_lastDecodedUs) / 1000.0;
        m_avgDecodeMs = 0.9 * m_avgDecodeMs + 0.1 * decodeMs;
    }
    m_lastDecodedUs = decodedAtUs;

    m_queue.push_back({frameIndex, decodedAtUs, presentationTimeUs, blendWeight});

    // Cap queue depth to 4 frames — beyond that we're falling behind
    while (m_queue.size() > 4) {
        AURA_LOG_WARN("FrameTimingQueue", "Dropping old frame — decoder ahead of renderer.");
        m_queue.pop_front();
    }
}

bool FrameTimingQueue::popReadyFrame(FrameTimingEntry& out) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_queue.empty()) return false;

    out = m_queue.front();
    m_queue.pop_front();

    // Track present jitter
    if (m_lastPresentedUs > 0) {
        const double periodMs = (out.targetPresentUs - m_lastPresentedUs) / 1000.0;
        const double jitter   = std::abs(periodMs - m_displayPeriodUs / 1000.0);
        m_avgJitterMs = 0.9 * m_avgJitterMs + 0.1 * jitter;
    }
    m_lastPresentedUs = out.targetPresentUs;

    return true;
}

size_t FrameTimingQueue::queueDepth() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_queue.size();
}

void FrameTimingQueue::updateDisplayPeriod(uint64_t presentUs) {
    if (m_lastPresentedUs > 0 && presentUs > m_lastPresentedUs) {
        const double period = static_cast<double>(presentUs - m_lastPresentedUs);
        m_displayPeriodUs = 0.95 * m_displayPeriodUs + 0.05 * period;
    }
}

} // namespace aura

#pragma once
// =============================================================================
// FrameTimingQueue.h — Frame presentation timing + blend weight calculator
// =============================================================================
#include <cstdint>
#include <deque>
#include <mutex>

namespace aura {

struct FrameTimingEntry {
    uint32_t frameIndex;
    uint64_t decodedAtUs;       // When decoder finished this frame
    uint64_t targetPresentUs;   // When renderer should display it
    float    blendWeight{1.0f}; // For temporal_frame_pacing.hlsl (0=prev, 1=curr)
};

class FrameTimingQueue {
public:
    FrameTimingQueue();

    void init();
    void reset();

    // Called by decoder thread after each frame is decoded
    void pushFrame(uint32_t frameIndex, uint64_t decodedAtUs,
                   uint64_t presentationTimeUs);

    // Called by render thread to get the next frame to display
    // Returns false if no frame is ready
    bool popReadyFrame(FrameTimingEntry& out);

    // How many frames are waiting
    size_t queueDepth() const;

    // Statistics
    double averageDecodeMs() const { return m_avgDecodeMs; }
    double averagePresentJitterMs() const { return m_avgJitterMs; }

private:
    mutable std::mutex     m_mutex;
    std::deque<FrameTimingEntry> m_queue;
    uint64_t               m_lastDecodedUs{0};
    uint64_t               m_lastPresentedUs{0};
    double                 m_avgDecodeMs{0};
    double                 m_avgJitterMs{0};

    // Display refresh period estimate (starts at 60fps)
    double m_displayPeriodUs{16666.67};
    void   updateDisplayPeriod(uint64_t presentUs);
};

} // namespace aura

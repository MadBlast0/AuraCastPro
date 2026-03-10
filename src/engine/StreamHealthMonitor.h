#pragma once
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>

namespace aura {
class StreamHealthMonitor {
public:
    struct Snapshot {
        double   lossRate{0.0};
        uint32_t avgJitterUs{0};
        double   avgFrameMs{0.0};
        uint64_t decodeErrors{0};
    };

    StreamHealthMonitor();
    ~StreamHealthMonitor();

    void init();
    void start();
    void stop();

    void recordPacket(bool lost, uint32_t jitterUs);
    void recordDecodeError();
    void recordFrameTime(double frameMs);

    Snapshot snapshot() const;
    void setDegradedCallback(std::function<void(Snapshot)> cb);

private:
    void monitorLoop();

    struct RollingStats {
        uint64_t totalPackets{0};
        uint64_t lostPackets{0};
        uint64_t jitterAccum{0};
        uint64_t jitterSamples{0};
        double   frameTimeAccum{0.0};
        uint64_t frameTimeSamples{0};
        uint64_t decodeErrors{0};
    };

    mutable std::mutex    m_mtx;
    std::condition_variable m_cv;
    std::thread           m_thread;
    std::atomic<bool>     m_running{false};
    RollingStats          m_stats;
    std::function<void(Snapshot)> m_degradedCb;
};
} // namespace aura

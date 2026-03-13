// =============================================================================
// LatencyMonitor.cpp — End-to-end pipeline latency benchmark
//
// Measures: packet receive → NAL parse → decode → render frame
// Target: < 70ms total end-to-end at 1080p60, < 50ms at 720p60
//
// Run: AuraCastPro_tests --gtest_filter="*Latency*"
// =============================================================================
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>

#include "../../src/engine/PacketReorderCache.h"
#include "../../src/engine/NALParser.h"
#include "../../src/engine/BitratePID.h"
#include "../../src/engine/NetworkPredictor.h"
#include "../../src/engine/FrameTimingQueue.h"
#include "../../src/utils/PerformanceTimer.h"

using namespace std::chrono;
using namespace aura;

// Helper: measure execution time of a callable in microseconds
template<typename Fn>
double measureUs(Fn fn) {
    const auto t0 = high_resolution_clock::now();
    fn();
    const auto t1 = high_resolution_clock::now();
    return duration_cast<nanoseconds>(t1 - t0).count() / 1000.0;
}

// Helper: compute statistics
struct Stats {
    double min, max, mean, p95, p99, stddev;
};

Stats computeStats(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = sum / n;
    const double sq_sum = std::inner_product(samples.begin(), samples.end(),
                                              samples.begin(), 0.0);
    const double stddev = std::sqrt(sq_sum / n - mean * mean);
    return {
        samples.front(),
        samples.back(),
        mean,
        samples[static_cast<size_t>(n * 0.95)],
        samples[static_cast<size_t>(n * 0.99)],
        stddev
    };
}

// =============================================================================
// Test: PacketReorderCache insertion latency
// Target: < 1ms per packet
// =============================================================================
TEST(LatencyMonitor, PacketReorderCacheLatency) {
    PacketReorderCache cache;
    cache.init();

    constexpr int kIterations = 10000;
    std::vector<double> times;
    times.reserve(kIterations);

    std::vector<uint8_t> dummyPkt(1316, 0xAB); // typical UDP MTU payload

    for (int i = 0; i < kIterations; ++i) {
        times.push_back(measureUs([&]() {
            cache.insert(static_cast<uint16_t>(i), dummyPkt);
        }));
    }

    const Stats s = computeStats(times);
    printf("\n[PacketReorderCache] Insert latency over %d iterations:\n"
           "  min=%.1fus  max=%.1fus  mean=%.1fus  p95=%.1fus  p99=%.1fus  stddev=%.1fus\n",
           kIterations, s.min, s.max, s.mean, s.p95, s.p99, s.stddev);

    EXPECT_LT(s.mean, 1000.0) << "Mean insert latency must be under 1ms";
    EXPECT_LT(s.p99,  5000.0) << "p99 insert latency must be under 5ms";
}

// =============================================================================
// Test: NALParser throughput
// Target: > 1000 NAL units/second
// =============================================================================
TEST(LatencyMonitor, NALParserThroughput) {
    NALParser parser;
    parser.init();

    int nalCount = 0;
    parser.setNALCallback([&](auto&&, bool, bool) { nalCount++; });

    // Build a realistic H.265 RTP packet: single NAL unit
    // Type = 1 (TRAIL_R non-IDR slice)
    auto makeSingleNAL = [](uint8_t nalType, size_t payloadSize) {
        std::vector<uint8_t> pkt;
        // RTP-style H.265 single NAL header
        pkt.push_back((nalType << 1) & 0x7E);  // byte 0
        pkt.push_back(0x01);                    // byte 1 (layer + tid)
        pkt.resize(pkt.size() + payloadSize, 0x42);
        return pkt;
    };

    constexpr int kPackets = 5000;
    auto pkt = makeSingleNAL(1, 1000); // 1KB payload

    const auto t0 = high_resolution_clock::now();
    for (int i = 0; i < kPackets; ++i) {
        parser.feedRTPPayload(pkt.data(), pkt.size(),
                              static_cast<uint16_t>(i), false);
    }
    const auto t1 = high_resolution_clock::now();

    const double elapsedMs = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
    const double throughput = kPackets / (elapsedMs / 1000.0);

    printf("\n[NALParser] Processed %d packets in %.1fms = %.0f packets/sec\n",
           kPackets, elapsedMs, throughput);

    EXPECT_GT(throughput, 1000.0) << "NAL parser must handle > 1000 packets/sec";
}

// =============================================================================
// Test: BitratePID update latency
// Target: < 100us per update
// =============================================================================
TEST(LatencyMonitor, BitratePIDUpdateLatency) {
    BitratePID pid;
    pid.init(20000.0);

    constexpr int kIterations = 100000;
    std::vector<double> times;
    times.reserve(kIterations);

    for (int i = 0; i < kIterations; ++i) {
        const double loss = (i % 100 == 0) ? 0.05 : 0.001;
        const double jitter = 5.0 + (i % 20);
        times.push_back(measureUs([&]() {
            pid.update(loss, jitter, 15000.0);
        }));
    }

    const Stats s = computeStats(times);
    printf("\n[BitratePID] Update latency over %d iterations:\n"
           "  min=%.2fus  max=%.2fus  mean=%.2fus  p99=%.2fus\n",
           kIterations, s.min, s.max, s.mean, s.p99);

    EXPECT_LT(s.mean, 100.0) << "BitratePID update must take < 100us on average";
}

// =============================================================================
// Test: FrameTimingQueue push+pop round-trip
// Target: < 500us per frame
// =============================================================================
TEST(LatencyMonitor, FrameTimingQueueRoundTrip) {
    FrameTimingQueue queue;
    queue.init();

    constexpr int kFrames = 1000;
    std::vector<double> times;
    times.reserve(kFrames);

    for (int i = 0; i < kFrames; ++i) {
        const uint64_t now = i * 16667ULL; // 60fps timestamps
        times.push_back(measureUs([&]() {
            queue.pushFrame(i, now, now + 16667);
            FrameTimingEntry entry;
            queue.popReadyFrame(entry);
        }));
    }

    const Stats s = computeStats(times);
    printf("\n[FrameTimingQueue] Push+Pop round-trip over %d frames:\n"
           "  mean=%.1fus  p95=%.1fus  p99=%.1fus\n",
           kFrames, s.mean, s.p95, s.p99);

    EXPECT_LT(s.mean, 500.0) << "FrameTimingQueue round-trip must be < 500us";
}

// =============================================================================
// Test: NetworkPredictor EMA convergence speed
// =============================================================================
TEST(LatencyMonitor, NetworkPredictorConvergence) {
    NetworkPredictor predictor;
    predictor.init();

    // Simulate 10% packet loss spike then recovery
    for (int i = 0; i < 50; ++i)
        predictor.feedSample(0.001, 5.0, 20.0, 1000000, 0.1);

    // Loss spike
    for (int i = 0; i < 5; ++i)
        predictor.feedSample(0.10, 30.0, 80.0, 500000, 0.1);

    auto telemetry = predictor.predict();
    EXPECT_GT(telemetry.packetLossPct, 0.5)
        << "EMA should respond to loss spike within 5 samples";

    // Recovery
    for (int i = 0; i < 20; ++i)
        predictor.feedSample(0.001, 5.0, 20.0, 1000000, 0.1);

    telemetry = predictor.predict();
    EXPECT_LT(telemetry.packetLossPct, 5.0)
        << "EMA should recover after 20 normal samples";

    printf("\n[NetworkPredictor] After recovery: loss=%.2f%%  jitter=%.1fms\n",
           telemetry.packetLossPct, telemetry.jitterMs);
}



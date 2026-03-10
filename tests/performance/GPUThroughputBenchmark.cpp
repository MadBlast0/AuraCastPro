// =============================================================================
// GPUThroughputBenchmark.cpp — GPU pipeline throughput tests
//
// Benchmarks the DX12 pipeline components:
//   - Descriptor heap allocation speed
//   - Command queue submission latency
//   - Fence signal/wait round-trip
//   - Simulated frame rate capacity
//
// These run on CPU only (no GPU device needed) to test the management overhead.
// Full GPU tests require a DX12 device and run as integration tests.
// =============================================================================
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <atomic>
#include <thread>

using namespace std::chrono;

// ── Helper ────────────────────────────────────────────────────────────────────
template<typename Fn>
double measureUs(Fn fn) {
    const auto t0 = high_resolution_clock::now();
    fn();
    return duration_cast<nanoseconds>(
        high_resolution_clock::now() - t0).count() / 1000.0;
}

// =============================================================================
// Test: Atomic fence counter throughput (simulates DX12 fence signaling)
// Target: > 10 million signals/second (CPU-only baseline)
// =============================================================================
TEST(GPUThroughput, AtomicFenceCounterThroughput) {
    std::atomic<uint64_t> fenceValue{0};
    constexpr int kSignals = 1'000'000;

    const auto t0 = high_resolution_clock::now();
    for (int i = 0; i < kSignals; ++i)
        fenceValue.fetch_add(1, std::memory_order_release);
    const double elapsedMs =
        duration_cast<microseconds>(high_resolution_clock::now() - t0).count() / 1000.0;

    const double signalsPerSec = kSignals / (elapsedMs / 1000.0);
    printf("\n[Fence] %d atomic increments in %.2fms = %.0f signals/sec\n",
           kSignals, elapsedMs, signalsPerSec);

    EXPECT_GT(signalsPerSec, 10'000'000.0)
        << "Atomic fence should signal > 10M times/sec";
    EXPECT_EQ(fenceValue.load(), static_cast<uint64_t>(kSignals));
}

// =============================================================================
// Test: Frame pacing simulation — can we sustain 120 FPS scheduling?
// =============================================================================
TEST(GPUThroughput, FramePacingAt120FPS) {
    constexpr int    kFrames        = 240;    // 2 seconds at 120fps
    constexpr double kFramePeriodUs = 8333.0; // 120fps = 8.333ms/frame

    std::vector<double> frameTimes;
    frameTimes.reserve(kFrames);

    auto lastFrame = high_resolution_clock::now();

    for (int i = 0; i < kFrames; ++i) {
        // Simulate minimal per-frame work (command list recording overhead)
        volatile double work = 0;
        for (int j = 0; j < 100; ++j) work += j * 0.001;

        const auto now     = high_resolution_clock::now();
        const double dtUs  = duration_cast<nanoseconds>(now - lastFrame).count() / 1000.0;
        frameTimes.push_back(dtUs);
        lastFrame = now;

        // Busy-wait to simulate vsync pacing (in real code this is DX12 Present)
        const auto targetTime = lastFrame + microseconds(static_cast<int64_t>(kFramePeriodUs));
        while (high_resolution_clock::now() < targetTime) {
            std::this_thread::yield();
        }
    }

    // Skip first frame (warmup)
    frameTimes.erase(frameTimes.begin());

    std::sort(frameTimes.begin(), frameTimes.end());
    const size_t n    = frameTimes.size();
    const double mean = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / n;
    const double p95  = frameTimes[static_cast<size_t>(n * 0.95)];
    const double p99  = frameTimes[static_cast<size_t>(n * 0.99)];
    const double fps  = 1'000'000.0 / mean;

    printf("\n[Frame Pacing] %d frames @ target 120fps:\n"
           "  mean=%.1fus (%.1f fps)  p95=%.1fus  p99=%.1fus\n",
           static_cast<int>(n), mean, fps, p95, p99);

    // Allow ±20% jitter from target
    EXPECT_GT(fps, 96.0)  << "Should achieve at least 96fps (80% of 120)";
    EXPECT_LT(p99, kFramePeriodUs * 3.0) << "p99 frame time should be < 3x target period";
}

// =============================================================================
// Test: Command buffer allocation pool throughput
// Simulates allocating and recycling DX12 command allocators
// =============================================================================
TEST(GPUThroughput, CommandBufferAllocationThroughput) {
    // Simulate a pool of 3 command allocators (as in DX12CommandQueue)
    constexpr int kPoolSize = 3;
    constexpr int kIterations = 100000;

    struct FakeAllocator { uint64_t fenceValue{0}; bool inUse{false}; };
    std::vector<FakeAllocator> pool(kPoolSize);
    std::atomic<uint64_t> completedFence{0};
    uint64_t submitFence = 0;

    const auto t0 = high_resolution_clock::now();

    for (int i = 0; i < kIterations; ++i) {
        // Find a free allocator (fence completed)
        FakeAllocator* alloc = nullptr;
        for (auto& a : pool) {
            if (!a.inUse || a.fenceValue <= completedFence.load()) {
                a.inUse = true;
                alloc = &a;
                break;
            }
        }
        if (!alloc) continue; // pool exhausted — skip

        // Simulate submit
        alloc->fenceValue = ++submitFence;

        // Simulate GPU completing every 3 submits
        if (i % 3 == 2) completedFence.fetch_add(3);
    }

    const double elapsedMs =
        duration_cast<microseconds>(high_resolution_clock::now() - t0).count() / 1000.0;
    const double opsPerSec = kIterations / (elapsedMs / 1000.0);

    printf("\n[CmdBuffer] %d alloc/submit cycles in %.2fms = %.0f ops/sec\n",
           kIterations, elapsedMs, opsPerSec);

    EXPECT_GT(opsPerSec, 500'000.0)
        << "Command buffer pool should handle > 500K ops/sec";
}

// =============================================================================
// Test: Bitrate calculation throughput (stats update path)
// =============================================================================
TEST(GPUThroughput, StatsUpdateThroughput) {
    // Simulates the stats thread updating network metrics at high frequency
    std::atomic<double> latency{0}, bitrate{0}, fps{0}, loss{0};
    constexpr int kUpdates = 1'000'000;

    const auto t0 = high_resolution_clock::now();

    for (int i = 0; i < kUpdates; ++i) {
        latency.store(20.0 + (i % 50),  std::memory_order_relaxed);
        bitrate.store(8000.0 + (i % 4000), std::memory_order_relaxed);
        fps.store(59.5 + (i % 2) * 0.5,  std::memory_order_relaxed);
        loss.store((i % 1000 == 0) ? 0.01 : 0.0, std::memory_order_relaxed);
    }

    const double elapsedMs =
        duration_cast<microseconds>(high_resolution_clock::now() - t0).count() / 1000.0;
    const double updatesPerSec = kUpdates / (elapsedMs / 1000.0);

    printf("\n[Stats] %d atomic updates in %.2fms = %.0f updates/sec\n",
           kUpdates, elapsedMs, updatesPerSec);

    EXPECT_GT(updatesPerSec, 5'000'000.0)
        << "Stats updates should be > 5M/sec (lock-free atomics)";
}

// =============================================================================
// Test: Verify 4K frame buffer memory throughput (memcpy baseline)
// 4K BGRA = 3840 * 2160 * 4 = 33,177,600 bytes ≈ 31.6 MB per frame
// At 60fps we need to move ~1.9 GB/sec
// =============================================================================
TEST(GPUThroughput, FrameBufferMemoryThroughput) {
    constexpr size_t k4KFrameBytes = 3840ULL * 2160 * 4;
    constexpr int    kFrames = 60; // 1 second

    std::vector<uint8_t> src(k4KFrameBytes, 0xAB);
    std::vector<uint8_t> dst(k4KFrameBytes);

    const auto t0 = high_resolution_clock::now();
    for (int i = 0; i < kFrames; ++i)
        memcpy(dst.data(), src.data(), k4KFrameBytes);
    const double elapsedMs =
        duration_cast<microseconds>(high_resolution_clock::now() - t0).count() / 1000.0;

    const double gbPerSec =
        (k4KFrameBytes * kFrames) / (elapsedMs / 1000.0) / (1024.0 * 1024 * 1024);
    const double fpsPossible = kFrames / (elapsedMs / 1000.0);

    printf("\n[Memory] 4K frame copy (%.1f MB) x%d in %.1fms:\n"
           "  Throughput: %.1f GB/sec, supports %.0f fps\n",
           k4KFrameBytes / (1024.0 * 1024), kFrames, elapsedMs, gbPerSec, fpsPossible);

    // Most modern CPUs have > 20 GB/sec memory bandwidth
    EXPECT_GT(gbPerSec, 5.0) << "Should achieve > 5 GB/sec memcpy for 4K frames";
    EXPECT_GT(fpsPossible, 60.0) << "Should support at least 60fps 4K on CPU path";
}


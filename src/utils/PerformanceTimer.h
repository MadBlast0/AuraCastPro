#pragma once
// =============================================================================
// PerformanceTimer.h — Sub-microsecond wall-clock timer using
//                       Windows QueryPerformanceCounter (QPC).
//
// QPC is the highest-resolution timer available on Windows.
// It does NOT depend on the system clock and is monotonic.
//
// Usage:
//   PerformanceTimer t;
//   t.start();
//   // ... work ...
//   double ms = t.elapsedMs();
//
//   // One-shot inline measurement:
//   auto guard = PerformanceTimer::scope("FrameDecode");
//   // guard destructs and logs elapsed time automatically
// =============================================================================

#include <string>
#include <chrono>

// Windows.h must be included before using LARGE_INTEGER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aura {

class PerformanceTimer {
public:
    // -----------------------------------------------------------------------
    // init() — caches the QPC frequency once at startup.
    // Must be called from main() before any timer is used.
    // -----------------------------------------------------------------------
    static void init();

    // -----------------------------------------------------------------------
    // Instance timer
    // -----------------------------------------------------------------------
    PerformanceTimer();

    // Record the current timestamp as t0
    void start();

    // Return elapsed time since start() in milliseconds (double precision)
    double elapsedMs() const;

    // Return elapsed time since start() in microseconds
    double elapsedUs() const;

    // Return elapsed time since start() in nanoseconds
    double elapsedNs() const;

    // Restart timer and return the elapsed time before reset
    double restartMs();

    // -----------------------------------------------------------------------
    // RAII scope guard — logs elapsed time on destruction
    // -----------------------------------------------------------------------
    class ScopeGuard {
    public:
        explicit ScopeGuard(const std::string& name);
        ~ScopeGuard();

        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;

    private:
        std::string  m_name;
        PerformanceTimer m_timer;
    };

    // Factory: auto guard = PerformanceTimer::scope("MySection");
    static ScopeGuard scope(const std::string& name);

    // -----------------------------------------------------------------------
    // Lap / split timing  [ADDED — Task 052 gap fix]
    // lap() records a split time from the last start() or lap().
    // getLaps() returns all recorded splits as (name, elapsedMs) pairs.
    // Used by LatencyMonitor to measure per-pipeline-stage latency.
    // -----------------------------------------------------------------------
    void lap(const std::string& name = "");
    const std::vector<std::pair<std::string, double>>& getLaps() const;
    void clearLaps();

    // Static wall-clock timestamp in microseconds  [ADDED — Task 052 gap fix]
    // Used for absolute timestamps (packet arrival, frame decode, etc.)
    static int64_t now_us();

private:
    static LARGE_INTEGER s_frequency; // QPC ticks per second
    static bool          s_initialised;

    LARGE_INTEGER m_start{}; // Timestamp captured at start()

    // Lap state
    LARGE_INTEGER m_lapBase{};
    std::vector<std::pair<std::string, double>> m_laps;
};

} // namespace aura

// NOTE: The following section was ADDED to fix Task 052 gap.
// lap() / getLaps() split-timing system and now_us() static.
// Previously missing — required by LatencyMonitor stage breakdown.

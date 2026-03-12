// =============================================================================
// PerformanceTimer.cpp — Windows QPC-based sub-microsecond timer
// =============================================================================

#include "../pch.h"  // PCH
#include "PerformanceTimer.h"
#include "Logger.h"

#include <stdexcept>

namespace aura {

// Static members
LARGE_INTEGER PerformanceTimer::s_frequency{};
bool          PerformanceTimer::s_initialised = false;

// -----------------------------------------------------------------------------
void PerformanceTimer::init() {
    if (!QueryPerformanceFrequency(&s_frequency)) {
        throw std::runtime_error("QueryPerformanceFrequency failed — "
                                 "QPC not available on this hardware.");
    }
    s_initialised = true;
    AURA_LOG_INFO("PerformanceTimer", "QPC frequency: {} Hz ({:.3f} ns/tick)",
                  s_frequency.QuadPart,
                  1.0e9 / static_cast<double>(s_frequency.QuadPart));
}

// -----------------------------------------------------------------------------
PerformanceTimer::PerformanceTimer() {
    QueryPerformanceCounter(&m_start);
}

// -----------------------------------------------------------------------------
void PerformanceTimer::start() {
    QueryPerformanceCounter(&m_start);
}

// -----------------------------------------------------------------------------
double PerformanceTimer::elapsedMs() const {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    const double ticks = static_cast<double>(now.QuadPart - m_start.QuadPart);
    return (ticks / static_cast<double>(s_frequency.QuadPart)) * 1000.0;
}

// -----------------------------------------------------------------------------
double PerformanceTimer::elapsedUs() const {
    return elapsedMs() * 1000.0;
}

// -----------------------------------------------------------------------------
double PerformanceTimer::elapsedNs() const {
    return elapsedMs() * 1.0e6;
}

// -----------------------------------------------------------------------------
double PerformanceTimer::restartMs() {
    const double elapsed = elapsedMs();
    start();
    return elapsed;
}

// -----------------------------------------------------------------------------
// ScopeGuard
// -----------------------------------------------------------------------------
PerformanceTimer::ScopeGuard::ScopeGuard(const std::string& name)
    : m_name(name)
    , m_start(std::chrono::high_resolution_clock::now()) {
}

PerformanceTimer::ScopeGuard::~ScopeGuard() {
    auto end = std::chrono::high_resolution_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(end - m_start).count();
    AURA_LOG_DEBUG("PerformanceTimer", "[scope] {} took {:.3f} ms", m_name, ms);
}

// -----------------------------------------------------------------------------
PerformanceTimer::ScopeGuard PerformanceTimer::scope(const std::string& name) {
    return ScopeGuard(name);
}

} // namespace aura

// =============================================================================
// Lap / split timing  [ADDED — Task 052 gap fix]
// =============================================================================

namespace aura {

void PerformanceTimer::lap(const std::string& name) {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);

    // First lap: measure from start()
    const LARGE_INTEGER base = m_laps.empty() ? m_start : m_lapBase;
    const double ticks = static_cast<double>(now.QuadPart - base.QuadPart);
    const double ms    = (ticks / static_cast<double>(s_frequency.QuadPart)) * 1000.0;

    m_laps.emplace_back(name.empty() ? ("lap_" + std::to_string(m_laps.size())) : name, ms);
    m_lapBase = now;
}

const std::vector<std::pair<std::string, double>>& PerformanceTimer::getLaps() const {
    return m_laps;
}

void PerformanceTimer::clearLaps() {
    m_laps.clear();
    m_lapBase = m_start;
}

int64_t PerformanceTimer::now_us() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (s_frequency.QuadPart == 0) return 0;
    // Convert QPC ticks → microseconds
    return static_cast<int64_t>(
        (static_cast<double>(now.QuadPart) / static_cast<double>(s_frequency.QuadPart)) * 1'000'000.0
    );
}

} // namespace aura

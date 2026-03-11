// =============================================================================
// SessionLogger.cpp — Per-session structured event log (JSON lines)
// Task 211: Writes timestamped session events to %APPDATA%\AuraCastPro\sessions\
//           for post-session diagnostics and support.
// =============================================================================
#include "../pch.h"  // PCH
#include "SessionLogger.h"
#include "../utils/Logger.h"
#include "../utils/AppDataInit.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>

namespace aura {

using json = nlohmann::json;
namespace fs = std::filesystem;

SessionLogger::SessionLogger() = default;
SessionLogger::~SessionLogger() { endSession(); }

void SessionLogger::beginSession(const std::string& deviceName, const std::string& protocol) {
    std::lock_guard lk(m_mtx);
    m_sessionId = generateSessionId();
    m_deviceName = deviceName;
    m_protocol = protocol;
    m_startTime = std::chrono::steady_clock::now();

    fs::path dir = AppDataInit::sessionLogDir();
    fs::create_directories(dir);
    m_filePath = (dir / (m_sessionId + ".jsonl")).string();

    m_file.open(m_filePath, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        AURA_LOG_WARN("SessionLogger", "Cannot open session log: {}", m_filePath);
        return;
    }

    writeEvent("session_start", {
        {"device", deviceName},
        {"protocol", protocol}
    });
    AURA_LOG_INFO("SessionLogger", "Session {} started for {} via {}", m_sessionId, deviceName, protocol);
}

void SessionLogger::endSession() {
    std::lock_guard lk(m_mtx);
    if (!m_file.is_open()) return;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_startTime).count();
    writeEvent("session_end", { {"duration_s", elapsed} });
    m_file.close();
    AURA_LOG_INFO("SessionLogger", "Session {} ended ({}s).", m_sessionId, elapsed);
}

void SessionLogger::logEvent(const std::string& event, nlohmann::json extra) {
    std::lock_guard lk(m_mtx);
    if (!m_file.is_open()) return;
    writeEvent(event, std::move(extra));
}

void SessionLogger::logStats(double fps, double latencyMs, double bitrateKbps) {
    std::lock_guard lk(m_mtx);
    if (!m_file.is_open()) return;
    writeEvent("stats", {
        {"fps",         fps},
        {"latency_ms",  latencyMs},
        {"bitrate_kbps",bitrateKbps}
    });
}

std::string SessionLogger::currentSessionId() const {
    std::lock_guard lk(m_mtx);
    return m_sessionId;
}

// ── private ──────────────────────────────────────────────────────────────────

void SessionLogger::writeEvent(const std::string& type, nlohmann::json extra) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto t   = system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_s(&tm_buf, &t);

    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    std::string timestamp = std::string(ts) + "." +
        (ms.count() < 100 ? "0" : "") + (ms.count() < 10 ? "0" : "") +
        std::to_string(ms.count()) + "Z";

    json entry = { {"t", timestamp}, {"event", type} };
    for (auto& [k, v] : extra.items()) entry[k] = v;

    m_file << entry.dump() << "\n";
    m_file.flush();
}

std::string SessionLogger::generateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()).count();
    std::ostringstream oss;
    oss << "session_" << ms;
    return oss.str();
}

} // namespace aura

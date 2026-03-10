#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>

namespace aura {
class SessionLogger {
public:
    SessionLogger();
    ~SessionLogger();

    void beginSession(const std::string& deviceName, const std::string& protocol);
    void endSession();
    void logEvent(const std::string& event, nlohmann::json extra = {});
    void logStats(double fps, double latencyMs, double bitrateKbps);
    std::string currentSessionId() const;

private:
    void writeEvent(const std::string& type, nlohmann::json extra);
    static std::string generateSessionId();

    mutable std::mutex m_mtx;
    std::ofstream      m_file;
    std::string        m_filePath;
    std::string        m_sessionId;
    std::string        m_deviceName;
    std::string        m_protocol;
    std::chrono::steady_clock::time_point m_startTime;
};
} // namespace aura

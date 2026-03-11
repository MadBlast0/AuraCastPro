#pragma once
// =============================================================================
// Logger.h — Centralised high-performance logging facade
//
// Wraps spdlog. All modules call Logger::get() to obtain the shared logger.
// Thread-safe. Async queue backend so logging never blocks the hot path.
//
// Usage:
//   AURA_LOG_INFO("ReceiverSocket", "Bound to port {}", port);
//   AURA_LOG_WARN("BitratePID", "Packet loss {}%", loss);
//   AURA_LOG_ERROR("DX12Manager", "Device lost: {}", hr);
// =============================================================================

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace aura {

class Logger {
public:
    // -------------------------------------------------------------------------
    // init() — call once from main() before any other subsystem
    //   logDir   : directory where rotating log files are written
    //   maxSizeMB: rotate file when it exceeds this size
    //   maxFiles : keep this many rotated files before deleting the oldest
    // -------------------------------------------------------------------------
    static void init(const std::string& logDir = "logs",
                     std::size_t maxSizeMB = 10,
                     std::size_t maxFiles = 5);

    // Retrieve the shared logger instance (valid after init())
    static std::shared_ptr<spdlog::logger> get();

    // Flush all pending log messages and shut down the async queue.
    // Call from the shutdown sequence before the process exits.
    static void shutdown();

private:
    Logger() = delete;
};

} // namespace aura

// =============================================================================
// Convenience macros — prepend the subsystem name automatically.
// Using macros preserves __FILE__ / __LINE__ source location.
// =============================================================================
#define AURA_LOG_TRACE(subsystem, fmt_str, ...) \
    ::aura::Logger::get()->log(spdlog::level::trace, \
        fmt::runtime(std::string("[") + subsystem + "] " + fmt_str), ##__VA_ARGS__)

#define AURA_LOG_DEBUG(subsystem, fmt_str, ...) \
    ::aura::Logger::get()->log(spdlog::level::debug, \
        fmt::runtime(std::string("[") + subsystem + "] " + fmt_str), ##__VA_ARGS__)

#define AURA_LOG_INFO(subsystem, fmt_str, ...) \
    ::aura::Logger::get()->log(spdlog::level::info, \
        fmt::runtime(std::string("[") + subsystem + "] " + fmt_str), ##__VA_ARGS__)

#define AURA_LOG_WARN(subsystem, fmt_str, ...) \
    ::aura::Logger::get()->log(spdlog::level::warn, \
        fmt::runtime(std::string("[") + subsystem + "] " + fmt_str), ##__VA_ARGS__)

#define AURA_LOG_ERROR(subsystem, fmt_str, ...) \
    ::aura::Logger::get()->log(spdlog::level::err, \
        fmt::runtime(std::string("[") + subsystem + "] " + fmt_str), ##__VA_ARGS__)

#define AURA_LOG_CRITICAL(subsystem, fmt_str, ...) \
    ::aura::Logger::get()->log(spdlog::level::critical, \
        fmt::runtime(std::string("[") + subsystem + "] " + fmt_str), ##__VA_ARGS__)

// =============================================================================
// Backward-compatible shorthands (single-string form — component embedded):
//   LOG_INFO("ReceiverSocket: Bound to port {}", port);
// These are kept for files that predate the AURA_LOG_* naming convention.
// =============================================================================
#define LOG_TRACE(fmt_str, ...)    ::aura::Logger::get()->log(spdlog::level::trace,    fmt::runtime(fmt_str), ##__VA_ARGS__)
#define LOG_DEBUG(fmt_str, ...)    ::aura::Logger::get()->log(spdlog::level::debug,    fmt::runtime(fmt_str), ##__VA_ARGS__)
#define LOG_INFO(fmt_str, ...)     ::aura::Logger::get()->log(spdlog::level::info,     fmt::runtime(fmt_str), ##__VA_ARGS__)
#define LOG_WARN(fmt_str, ...)     ::aura::Logger::get()->log(spdlog::level::warn,     fmt::runtime(fmt_str), ##__VA_ARGS__)
#define LOG_ERROR(fmt_str, ...)    ::aura::Logger::get()->log(spdlog::level::err,      fmt::runtime(fmt_str), ##__VA_ARGS__)
#define LOG_CRITICAL(fmt_str, ...) ::aura::Logger::get()->log(spdlog::level::critical, fmt::runtime(fmt_str), ##__VA_ARGS__)
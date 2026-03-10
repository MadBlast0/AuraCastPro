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
#define AURA_LOG_TRACE(subsystem, ...) \
    SPDLOG_LOGGER_TRACE(::aura::Logger::get(), "[" subsystem "] " __VA_ARGS__)

#define AURA_LOG_DEBUG(subsystem, ...) \
    SPDLOG_LOGGER_DEBUG(::aura::Logger::get(), "[" subsystem "] " __VA_ARGS__)

#define AURA_LOG_INFO(subsystem, ...) \
    SPDLOG_LOGGER_INFO(::aura::Logger::get(), "[" subsystem "] " __VA_ARGS__)

#define AURA_LOG_WARN(subsystem, ...) \
    SPDLOG_LOGGER_WARN(::aura::Logger::get(), "[" subsystem "] " __VA_ARGS__)

#define AURA_LOG_ERROR(subsystem, ...) \
    SPDLOG_LOGGER_ERROR(::aura::Logger::get(), "[" subsystem "] " __VA_ARGS__)

#define AURA_LOG_CRITICAL(subsystem, ...) \
    SPDLOG_LOGGER_CRITICAL(::aura::Logger::get(), "[" subsystem "] " __VA_ARGS__)

// =============================================================================
// Backward-compatible shorthands (single-string form — component embedded):
//   LOG_INFO("ReceiverSocket: Bound to port {}", port);
// These are kept for files that predate the AURA_LOG_* naming convention.
// =============================================================================
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(::aura::Logger::get(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(::aura::Logger::get(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(::aura::Logger::get(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(::aura::Logger::get(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(::aura::Logger::get(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(::aura::Logger::get(), __VA_ARGS__)

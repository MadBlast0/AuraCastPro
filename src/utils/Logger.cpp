// =============================================================================
// Logger.cpp -- Implementation of the centralised async spdlog logger
// =============================================================================

#include "../pch.h"  // PCH
#include "Logger.h"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace aura {

// Module-level singleton -- valid after init(), cleared by shutdown()
static std::shared_ptr<spdlog::logger> s_logger;

void Logger::init(const std::string& logDir, std::size_t maxSizeMB, std::size_t maxFiles) {
    // Create log directory if it does not exist
    std::filesystem::create_directories(logDir);

    // Initialise spdlog async thread pool:
    //   queue capacity = 8192 log records
    //   1 background thread drains the queue to the sinks
    spdlog::init_thread_pool(8192, 1);

    const std::string logPath = logDir + "/auracastpro.log";
    const std::size_t maxBytes = maxSizeMB * 1024 * 1024;

    // Two sinks: coloured stdout + rotating file
    std::vector<spdlog::sink_ptr> sinks;

    // Console sink (coloured) -- useful during development
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::debug);
    sinks.push_back(consoleSink);

    // Rotating file sink -- keeps the last N files, each ≤ maxSizeMB
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logPath, maxBytes, maxFiles);
    fileSink->set_level(spdlog::level::trace);
    sinks.push_back(fileSink);

    // Async logger -- writing never blocks the calling thread
    s_logger = std::make_shared<spdlog::async_logger>(
        "aura",
        sinks.begin(), sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    s_logger->set_level(spdlog::level::trace);

    // Pattern: [2025-01-01 12:34:56.789] [level] message
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    spdlog::register_logger(s_logger);
    spdlog::set_default_logger(s_logger);

    s_logger->info("[Logger] Initialised -- log file: {}", logPath);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!s_logger) {
        // Safety fallback: if init() was not called, return a basic console logger.
        // This avoids null-pointer crashes during early startup.
        return spdlog::default_logger();
    }
    return s_logger;
}

void Logger::shutdown() {
    if (s_logger) {
        s_logger->info("[Logger] Shutting down.");
        s_logger->flush();
    }
    spdlog::shutdown();
    s_logger.reset();
}

} // namespace aura

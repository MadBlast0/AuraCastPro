#pragma once
extern "C" {
#include <libavformat/avformat.h>
}
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

// ─── MP4 Fragmented Muxer Options ────────────────────────────────────────────

// Applies crash-safe fragmented MP4 options to an AVFormatContext.
// Must be called BEFORE avformat_write_header().
class MP4MuxerFragments {
public:
    // Apply fragmented MP4 options: frag_keyframe + empty_moov + default_base_moof
    static bool apply(AVFormatContext* fmtCtx);

    // Recommended: fragment every 2 seconds
    static constexpr int kFragIntervalSec = 2;
};

// ─── Disk Space Monitor ───────────────────────────────────────────────────────

enum class DiskWarningLevel { None, Low, Critical };

// Note: This is a lightweight internal monitor for RecordingController.
// For the full aura::DiskSpaceMonitor see integration/DiskSpaceMonitor.h
class RecordingDiskGuard {
public:
    struct Thresholds {
        uint64_t warnBytes     = 2ULL * 1024 * 1024 * 1024;  // 2 GB
        uint64_t criticalBytes = 500ULL * 1024 * 1024;        // 500 MB
        uint64_t minimumToStartBytes = 1ULL * 1024 * 1024 * 1024; // 1 GB
    };

    void setPath(const std::string& path) { m_path = path; }
    void setThresholds(Thresholds t)       { m_t = t; }

    // Check if there's enough space to start recording
    // Returns empty string if OK, else error message
    std::string checkCanStart();

    // Get free bytes on the recording drive
    uint64_t getFreeBytes();

    // Start background monitoring (callback fires on main thread via Qt)
    void startMonitoring(std::function<void(DiskWarningLevel, uint64_t)> cb);
    void stopMonitoring();

    // Format bytes as human-readable: "142 GB free"
    static std::string formatFree(uint64_t bytes);

private:
    void monitorLoop(std::function<void(DiskWarningLevel, uint64_t)> cb);

    std::string m_path;
    Thresholds  m_t;
    std::atomic<bool> m_running { false };
    std::thread m_thread;
};

// ─── Recording Controller ─────────────────────────────────────────────────────

namespace aura { class StreamRecorder; }
using aura::StreamRecorder;

class RecordingController {
public:
    enum class State { Idle, Recording, Paused };

    void setRecorder(StreamRecorder* rec) { m_recorder = rec; }
    void setDiskMonitor(RecordingDiskGuard* mon) { m_diskMon = mon; }
    void setOutputFolder(const std::string& folder) { m_outputFolder = folder; }

    // Video stream parameters — set before startRecording()
    struct VideoConfig {
        uint32_t    width  = 1920;
        uint32_t    height = 1080;
        uint32_t    fps    = 30;
        std::string codec  = "h264";  // "h264" or "hevc"
    };
    void setVideoConfig(VideoConfig cfg) { m_videoConfig = std::move(cfg); }

    void setOnStateChanged(std::function<void(State)> cb) { m_onState = std::move(cb); }
    void setOnError(std::function<void(std::string)> cb) { m_onError = std::move(cb); }
    void setOnComplete(std::function<void(std::string, uint64_t)> cb) { m_onComplete = std::move(cb); }

    // Returns empty string on success, error message on failure
    std::string startRecording();
    void pauseRecording();
    void resumeRecording();
    void stopRecording();

    State state() const { return m_state; }
    std::string currentPath() const { return m_currentPath; }

    // Time elapsed since recording started (0 if not recording)
    int64_t elapsedSeconds() const;

private:
    std::string generatePath();

    StreamRecorder*    m_recorder    = nullptr;
    RecordingDiskGuard*  m_diskMon     = nullptr;
    std::string        m_outputFolder;
    std::string        m_currentPath;
    VideoConfig        m_videoConfig;
    State              m_state       = State::Idle;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::duration<double> m_pausedDuration {};

    std::function<void(State)>                m_onState;
    std::function<void(std::string)>          m_onError;
    std::function<void(std::string, uint64_t)> m_onComplete;
};

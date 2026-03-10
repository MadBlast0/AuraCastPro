#include "RecordingSystem.h"
#include "StreamRecorder.h"
#include "../utils/Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <ctime>

// ─── MP4MuxerFragments ────────────────────────────────────────────────────────

bool MP4MuxerFragments::apply(AVFormatContext* fmtCtx) {
    if (!fmtCtx || !fmtCtx->priv_data) {
        LOG_ERROR("MP4MuxerFragments: Invalid AVFormatContext");
        return false;
    }
    int ret = av_opt_set(fmtCtx->priv_data, "movflags",
                         "frag_keyframe+empty_moov+default_base_moof", 0);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOG_ERROR("MP4MuxerFragments: Failed to set movflags: {}", errbuf);
        return false;
    }
    LOG_DEBUG("MP4MuxerFragments: Fragmented MP4 enabled");
    return true;
}

// ─── DiskSpaceMonitor ────────────────────────────────────────────────────────

uint64_t RecordingDiskGuard::getFreeBytes() {
    ULARGE_INTEGER free, total, totalFree;
    std::wstring wp(m_path.begin(), m_path.end());
    if (!GetDiskFreeSpaceExW(wp.c_str(), &free, &total, &totalFree)) {
        // Try drive root
        std::wstring drive = wp.substr(0, 3);
        if (!GetDiskFreeSpaceExW(drive.c_str(), &free, &total, &totalFree))
            return UINT64_MAX;
    }
    return free.QuadPart;
}

std::string RecordingDiskGuard::checkCanStart() {
    uint64_t freeBytes = getFreeBytes();
    if (freeBytes == UINT64_MAX)
        return "Cannot determine disk space. Please check the recording folder.";
    if (freeBytes < m_t.minimumToStartBytes) {
        return "Not enough disk space to start recording. Need at least 1 GB free. "
               "Current: " + formatFree(freeBytes) + ".";
    }
    return "";
}

void RecordingDiskGuard::startMonitoring(
    std::function<void(DiskWarningLevel, uint64_t)> cb) {
    if (m_running.exchange(true)) return;
    m_thread = std::thread([this, cb]{ monitorLoop(cb); });
}

void RecordingDiskGuard::stopMonitoring() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void RecordingDiskGuard::monitorLoop(
    std::function<void(DiskWarningLevel, uint64_t)> cb) {
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (!m_running.load()) break;

        uint64_t free = getFreeBytes();
        DiskWarningLevel level = DiskWarningLevel::None;

        if (free < m_t.criticalBytes)
            level = DiskWarningLevel::Critical;
        else if (free < m_t.warnBytes)
            level = DiskWarningLevel::Low;

        if (level != DiskWarningLevel::None) {
            LOG_WARN("DiskSpaceMonitor: {} free ({})",
                     formatFree(free),
                     level == DiskWarningLevel::Critical ? "CRITICAL" : "LOW");
            if (cb) cb(level, free);
        }
    }
}

std::string RecordingDiskGuard::formatFree(uint64_t bytes) {
    if (bytes == UINT64_MAX) return "unknown";
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= 1ULL * 1024 * 1024 * 1024)
        ss << (bytes / (1024.0 * 1024 * 1024)) << " GB";
    else if (bytes >= 1024 * 1024)
        ss << (bytes / (1024.0 * 1024)) << " MB";
    else
        ss << (bytes / 1024.0) << " KB";
    ss << " free";
    return ss.str();
}

// ─── RecordingController ─────────────────────────────────────────────────────

std::string RecordingController::startRecording() {
    if (m_state != State::Idle) return "Already recording";

    // Check disk space
    if (m_diskMon) {
        std::string err = m_diskMon->checkCanStart();
        if (!err.empty()) return err;
    }

    m_currentPath = generatePath();
    if (m_recorder) m_recorder->startRecording(
        m_currentPath,
        m_videoConfig.width,
        m_videoConfig.height,
        m_videoConfig.fps,
        m_videoConfig.codec);

    m_state     = State::Recording;
    m_startTime = std::chrono::steady_clock::now();
    m_pausedDuration = {};

    LOG_INFO("RecordingController: Started → {}", m_currentPath);
    if (m_onState) m_onState(m_state);
    return "";
}

void RecordingController::pauseRecording() {
    if (m_state != State::Recording) return;
    m_state = State::Paused;
    LOG_INFO("RecordingController: Paused");
    if (m_onState) m_onState(m_state);
}

void RecordingController::resumeRecording() {
    if (m_state != State::Paused) return;
    m_state = State::Recording;
    LOG_INFO("RecordingController: Resumed");
    if (m_onState) m_onState(m_state);
}

void RecordingController::stopRecording() {
    if (m_state == State::Idle) return;
    if (m_recorder) m_recorder->stopRecording();
    m_state = State::Idle;

    // Get file size
    uint64_t size = 0;
    std::error_code ec;
    auto sz = std::filesystem::file_size(m_currentPath, ec);
    if (!ec) size = sz;

    LOG_INFO("RecordingController: Stopped → {} ({} bytes)", m_currentPath, size);
    if (m_onState)   m_onState(m_state);
    if (m_onComplete) m_onComplete(m_currentPath, size);
}

int64_t RecordingController::elapsedSeconds() const {
    if (m_state == State::Idle) return 0;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - m_startTime - m_pausedDuration;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
}

std::string RecordingController::generatePath() {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_s(&tm_buf, &time);

    std::ostringstream ss;
    ss << m_outputFolder << "\\AuraCastPro_"
       << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    std::string path = ss.str();

    // Collision handling
    if (!std::filesystem::exists(path)) return path;
    for (int i = 1; i < 100; i++) {
        std::ostringstream s2;
        s2 << m_outputFolder << "\\AuraCastPro_"
           << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S")
           << "_" << i << ".mp4";
        if (!std::filesystem::exists(s2.str())) return s2.str();
    }
    return path; // fallback
}

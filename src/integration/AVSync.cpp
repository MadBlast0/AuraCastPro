#include "../pch.h"  // PCH
#include "AVSync.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>

static int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

AVSync::AVSync(Config cfg) : m_cfg(cfg) {
    int maxDelaySamples = (cfg.delayBufferMs * cfg.sampleRate / 1000) * cfg.channels;
    m_delayBuf.assign(maxDelaySamples, 0.0f);
}

void AVSync::onVideoFrame(int64_t ptsUs) {
    m_lastVideoPtsUs.store(ptsUs);
    int64_t audioPts = m_lastAudioPtsUs.load();
    if (audioPts > 0 && ptsUs > 0) {
        float offsetMs = (float)(audioPts - ptsUs) / 1000.0f;
        updateOffset(offsetMs);
    }
}

void AVSync::onAudioBuffer(int64_t ptsUs) {
    m_lastAudioPtsUs.store(ptsUs);
}

void AVSync::updateOffset(float newOffsetMs) {
    float current = m_currentOffsetMs.load();
    float smoothed = current + m_cfg.smoothingAlpha * (newOffsetMs - current);
    m_currentOffsetMs.store(smoothed);

    // Update delay target
    int totalOffsetMs = (int)smoothed + m_manualOffsetMs;
    int delayMs = std::max(0, -totalOffsetMs); // delay audio if it's ahead
    m_targetDelayFrames = (delayMs * m_cfg.sampleRate / 1000) * m_cfg.channels;

    // Periodic log
    int64_t now = nowUs();
    if (now - m_lastReportUs > m_cfg.reportIntervalSec * 1000000LL) {
        LOG_INFO("AVSync: offset={:.1f}ms correction={}ms",
                 smoothed, totalOffsetMs);
        m_lastReportUs = now;
    }
}

int AVSync::processSamples(const float* input,  int numFrames,
                             float* output, int maxOutputFrames) {
    std::lock_guard<std::mutex> lock(m_bufMutex);

    int samplesIn = numFrames * m_cfg.channels;
    // Push input into delay buffer
    for (int i = 0; i < samplesIn; i++)
        m_delayBuf.push_back(input[i]);

    // How many samples to hold back
    int holdSamples = std::min((int)m_delayBuf.size(), m_targetDelayFrames);

    int availSamples = (int)m_delayBuf.size() - holdSamples;
    int outSamples   = std::min(availSamples, maxOutputFrames * m_cfg.channels);
    outSamples = (outSamples / m_cfg.channels) * m_cfg.channels; // align to frames

    for (int i = 0; i < outSamples; i++) {
        output[i] = m_delayBuf.front();
        m_delayBuf.pop_front();
    }

    // Fill remainder with silence
    int remaining = (maxOutputFrames * m_cfg.channels) - outSamples;
    std::memset(output + outSamples, 0, remaining * sizeof(float));

    return outSamples / m_cfg.channels;
}

void AVSync::reset() {
    std::lock_guard<std::mutex> lock(m_bufMutex);
    m_delayBuf.clear();
    m_currentOffsetMs  = 0.0f;
    m_lastVideoPtsUs   = 0;
    m_lastAudioPtsUs   = 0;
    m_targetDelayFrames = 0;
}

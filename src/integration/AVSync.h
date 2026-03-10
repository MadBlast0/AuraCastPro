#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <functional>
#include <atomic>
#include <mutex>

// Measures and corrects A/V sync offset.
// Audio is delayed to match video presentation timestamps.
// Target: maintain sync within ±10ms.
class AVSync {
public:
    struct Config {
        int   delayBufferMs   = 200;   // max audio delay buffer
        int   sampleRate      = 48000;
        int   channels        = 2;
        float smoothingAlpha  = 0.05f; // EMA for offset estimate
        int   reportIntervalSec = 10;
    };

    explicit AVSync(Config cfg = {});

    // Call from video renderer with frame PTS
    void onVideoFrame(int64_t ptsUs);

    // Call from audio mixer with audio buffer PTS
    void onAudioBuffer(int64_t ptsUs);

    // Feed PCM samples in, get delayed output (same number of frames)
    // Returns number of valid output frames (may be less than input if buffering)
    int processSamples(const float* input,  int numFrames,
                             float* output, int maxOutputFrames);

    // Manual offset in ms (-200 to +200), from Settings slider
    void setManualOffsetMs(int ms) { m_manualOffsetMs = ms; }
    int  manualOffsetMs() const    { return m_manualOffsetMs; }

    // Current measured offset (positive = audio ahead of video)
    float currentOffsetMs() const { return m_currentOffsetMs.load(); }

    void reset();

private:
    void updateOffset(float newOffsetMs);

    Config  m_cfg;
    int     m_manualOffsetMs = 0;

    std::atomic<float>   m_currentOffsetMs { 0.0f };
    std::atomic<int64_t> m_lastVideoPtsUs  { 0 };
    std::atomic<int64_t> m_lastAudioPtsUs  { 0 };

    // Delay ring buffer (float PCM samples)
    std::mutex           m_bufMutex;
    std::deque<float>    m_delayBuf;
    int                  m_targetDelayFrames = 0;

    int64_t              m_lastReportUs = 0;
};

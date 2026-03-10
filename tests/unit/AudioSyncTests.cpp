#include <gtest/gtest.h>
#include "../../src/integration/AVSync.h"

// ─── AVSync Tests ─────────────────────────────────────────────────────────────

TEST(AudioMixer, AVOffsetMeasuredCorrectly) {
    AVSync::Config cfg;
    cfg.sampleRate = 48000;
    cfg.channels   = 2;
    AVSync sync(cfg);

    // Video at 1000ms, audio at 980ms → expected offset ≈ -20ms (audio behind)
    sync.onVideoFrame(1000 * 1000);
    sync.onAudioBuffer(980 * 1000);

    // Trigger update (call several more to let EMA stabilise)
    for (int i = 0; i < 50; i++) {
        sync.onVideoFrame((1000 + i * 16) * 1000LL);
        sync.onAudioBuffer((980 + i * 16) * 1000LL);
    }

    float offset = sync.currentOffsetMs();
    EXPECT_NEAR(offset, -20.0f, 3.0f)
        << "Expected A/V offset near -20ms, got " << offset;
}

TEST(AudioMixer, DelayBufferAppliesCorrection) {
    AVSync::Config cfg;
    cfg.sampleRate = 48000;
    cfg.channels   = 2;
    AVSync sync(cfg);

    // Set manual offset = +30ms (delay audio by 30ms)
    sync.setManualOffsetMs(30);

    // Create 480 frames (10ms at 48kHz) of test audio
    const int frames     = 480;
    const int totalSamps = frames * 2;
    std::vector<float> input(totalSamps, 0.5f);
    std::vector<float> output(totalSamps * 2, 0.0f); // extra space

    // First call: all samples should be buffered (delay applied)
    int out1 = sync.processSamples(input.data(), frames,
                                    output.data(), frames * 2);

    // With 30ms delay at 48kHz: 1440 samples held back.
    // First call → output should be silent (all samples delayed)
    for (int i = 0; i < out1 * 2; i++)
        EXPECT_NEAR(output[i], 0.0f, 0.01f)
            << "Expected silence in first delayed output at sample " << i;
}

TEST(AudioMixer, SyncUnderSimulatedJitter) {
    AVSync::Config cfg;
    cfg.sampleRate = 48000;
    cfg.channels   = 2;
    cfg.smoothingAlpha = 0.1f;
    AVSync sync(cfg);

    std::srand(42);
    for (int i = 0; i < 100; i++) {
        float jitter = ((std::rand() % 41) - 20) * 1000.0f; // ±20ms in us
        sync.onVideoFrame((int64_t)(i * 16666.0f));
        sync.onAudioBuffer((int64_t)(i * 16666.0f + jitter));
    }

    float offset = std::abs(sync.currentOffsetMs());
    EXPECT_LE(offset, 25.0f)
        << "A/V offset drifted beyond 25ms under jitter: " << offset;
}

TEST(AudioMixer, ResetClearsState) {
    AVSync::Config cfg;
    cfg.sampleRate = 48000; cfg.channels = 2;
    AVSync sync(cfg);

    sync.onVideoFrame(1000000);
    sync.onAudioBuffer(800000);
    sync.reset();

    EXPECT_NEAR(sync.currentOffsetMs(), 0.0f, 0.1f);
}

// =============================================================================
// BitratePIDTests.cpp — Unit tests for the BitratePID controller
// =============================================================================

#include <gtest/gtest.h>
#include "../../src/engine/BitratePID.h"

#include <thread>
#include <chrono>

using namespace aura;

// Helper: wait long enough for the PID update interval to elapse
static void waitForUpdate(int ms = 120) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ─── Basic construction ───────────────────────────────────────────────────────

TEST(BitratePIDTest, InitialisesToFiftyPercentOfMax) {
    BitratePID::Config cfg;
    cfg.maxBitrateBps = 10'000'000.0f;
    BitratePID pid(cfg);

    const float initial = pid.currentBitrateBps();
    EXPECT_FLOAT_EQ(initial, cfg.maxBitrateBps * 0.5f);
}

TEST(BitratePIDTest, ResetSetsExplicitValue) {
    BitratePID pid;
    pid.reset(2'000'000.0f);
    EXPECT_FLOAT_EQ(pid.currentBitrateBps(), 2'000'000.0f);
}

// ─── No-loss scenario: bitrate should increase (or stay at max) ───────────────

TEST(BitratePIDTest, ZeroLossAllowsBitrateToIncrease) {
    BitratePID::Config cfg;
    cfg.maxBitrateBps = 50'000'000.0f;
    BitratePID pid(cfg);

    pid.reset(5'000'000.0f); // Start low

    NetworkTelemetry t;
    t.packetLossRate    = 0.0f;   // No loss
    t.jitterMs          = 0.0f;
    t.rttMs             = 10.0f;
    t.bandwidthEstBps   = 50'000'000.0f; // Plenty of bandwidth

    waitForUpdate();
    const float newRate = pid.update(t);

    // With zero loss, bitrate should not decrease
    EXPECT_GE(newRate, 5'000'000.0f);
}

// ─── Critical loss: emergency cut to 50% ─────────────────────────────────────

TEST(BitratePIDTest, CriticalLossTriggersEmergencyCut) {
    BitratePID::Config cfg;
    cfg.criticalLossRate = 0.05f; // 5%
    BitratePID pid(cfg);
    pid.reset(20'000'000.0f); // Start at 20 Mbps

    NetworkTelemetry t;
    t.packetLossRate  = 0.10f; // 10% — above critical threshold
    t.jitterMs        = 5.0f;
    t.rttMs           = 50.0f;
    t.bandwidthEstBps = 0.0f;

    waitForUpdate();
    const float newRate = pid.update(t);

    // Should be cut by at least 40% from the starting value
    EXPECT_LT(newRate, 20'000'000.0f * 0.6f);
}

// ─── Bitrate always stays within configured bounds ───────────────────────────

TEST(BitratePIDTest, NeverExceedsMaxBitrate) {
    BitratePID::Config cfg;
    cfg.maxBitrateBps = 5'000'000.0f;
    BitratePID pid(cfg);

    // Run many iterations with zero loss
    for (int i = 0; i < 20; ++i) {
        waitForUpdate();
        NetworkTelemetry t;
        t.packetLossRate  = 0.0f;
        t.bandwidthEstBps = 100'000'000.0f;
        pid.update(t);
    }

    EXPECT_LE(pid.currentBitrateBps(), cfg.maxBitrateBps);
}

TEST(BitratePIDTest, NeverDropsBelowMinBitrate) {
    BitratePID::Config cfg;
    cfg.minBitrateBps = 500'000.0f;
    BitratePID pid(cfg);

    // Simulate catastrophic loss
    for (int i = 0; i < 10; ++i) {
        waitForUpdate();
        NetworkTelemetry t;
        t.packetLossRate  = 1.0f; // 100% loss
        t.bandwidthEstBps = 0.0f;
        pid.update(t);
    }

    EXPECT_GE(pid.currentBitrateBps(), cfg.minBitrateBps);
}

// ─── Bandwidth ceiling ────────────────────────────────────────────────────────

TEST(BitratePIDTest, RespectsEstimatedBandwidthCeiling) {
    BitratePID::Config cfg;
    cfg.maxBitrateBps       = 50'000'000.0f;
    cfg.bandwidthSafetyFactor = 0.85f;
    BitratePID pid(cfg);
    pid.reset(40'000'000.0f);

    NetworkTelemetry t;
    t.packetLossRate  = 0.0f;  // No loss
    t.bandwidthEstBps = 10'000'000.0f; // Only 10 Mbps available

    waitForUpdate();
    const float newRate = pid.update(t);

    // Must not exceed 85% of 10 Mbps = 8.5 Mbps
    EXPECT_LE(newRate, 10'000'000.0f * 0.85f + 1000.0f); // +1kbps tolerance
}

// ─── Callback notification ────────────────────────────────────────────────────

TEST(BitratePIDTest, CallbackFiredOnSignificantChange) {
    BitratePID pid;
    pid.reset(20'000'000.0f);

    float callbackValue = 0.0f;
    pid.setOnBitrateChanged([&](float v) { callbackValue = v; });

    NetworkTelemetry t;
    t.packetLossRate  = 0.5f; // High loss — should trigger significant change
    t.bandwidthEstBps = 0.0f;

    waitForUpdate();
    pid.update(t);

    EXPECT_GT(callbackValue, 0.0f);
    EXPECT_LT(callbackValue, 20'000'000.0f);
}

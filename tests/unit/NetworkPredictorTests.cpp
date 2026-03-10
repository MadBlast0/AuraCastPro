// NetworkPredictorTests.cpp — Unit tests for NetworkPredictor
// Task 207
#include <gtest/gtest.h>
#include "../../src/engine/NetworkPredictor.h"

using namespace aura;

class NetworkPredictorTest : public ::testing::Test {
protected:
    NetworkPredictor predictor;
    void SetUp() override { predictor.init(); }
};

TEST_F(NetworkPredictorTest, InitialBandwidthIsZero) {
    EXPECT_EQ(predictor.estimatedBandwidthKbps(), 0.0);
}

TEST_F(NetworkPredictorTest, SingleSampleUpdates) {
    predictor.recordSample(10'000, 8);  // 10000 bytes in 8ms
    EXPECT_GT(predictor.estimatedBandwidthKbps(), 0.0);
}

TEST_F(NetworkPredictorTest, BandwidthIncreasesWithHighThroughput) {
    // Feed many large packets quickly
    for (int i = 0; i < 20; i++) {
        predictor.recordSample(50'000, 5);  // 50KB in 5ms = high throughput
    }
    double bw = predictor.estimatedBandwidthKbps();
    EXPECT_GT(bw, 1'000.0);  // Should estimate well over 1 Mbps
}

TEST_F(NetworkPredictorTest, JitterTracked) {
    predictor.recordSample(1000, 10);
    predictor.recordSample(1000, 20);
    predictor.recordSample(1000, 15);
    EXPECT_GE(predictor.jitterMs(), 0.0);
}

TEST_F(NetworkPredictorTest, ResetClearsState) {
    predictor.recordSample(50'000, 5);
    predictor.reset();
    EXPECT_EQ(predictor.estimatedBandwidthKbps(), 0.0);
}

// AudioMixerTests.cpp — Unit tests for AudioMixer
// Task 205
#include <gtest/gtest.h>
#include "../../src/integration/AudioMixer.h"
#include <vector>

using namespace aura;

class AudioMixerTest : public ::testing::Test {
protected:
    AudioMixer mixer;
    void SetUp() override { mixer.init(); }
    void TearDown() override { mixer.shutdown(); }
};

TEST_F(AudioMixerTest, DefaultVolumeIsOne) {
    EXPECT_FLOAT_EQ(mixer.masterVolume(), 1.0f);
}

TEST_F(AudioMixerTest, SetMasterVolume) {
    mixer.setMasterVolume(0.5f);
    EXPECT_NEAR(mixer.masterVolume(), 0.5f, 0.001f);
}

TEST_F(AudioMixerTest, VolumeClampsToZero) {
    mixer.setMasterVolume(-0.5f);
    EXPECT_GE(mixer.masterVolume(), 0.0f);
}

TEST_F(AudioMixerTest, VolumeClampsToOne) {
    mixer.setMasterVolume(2.0f);
    EXPECT_LE(mixer.masterVolume(), 1.0f);
}

TEST_F(AudioMixerTest, MixSilentBuffers) {
    std::vector<float> ch1(512, 0.0f);
    std::vector<float> ch2(512, 0.0f);
    auto out = mixer.mix(ch1.data(), ch2.data(), 512);
    for (auto s : out) EXPECT_FLOAT_EQ(s, 0.0f);
}

TEST_F(AudioMixerTest, MixAddsChannels) {
    std::vector<float> ch1(8, 0.3f);
    std::vector<float> ch2(8, 0.2f);
    mixer.setMasterVolume(1.0f);
    auto out = mixer.mix(ch1.data(), ch2.data(), 8);
    ASSERT_EQ(out.size(), 8u);
    // Mixed output should be sum clamped to [-1,1]
    for (auto s : out) {
        EXPECT_GE(s, -1.0f);
        EXPECT_LE(s, 1.0f);
    }
}

TEST_F(AudioMixerTest, MuteProducesSilence) {
    mixer.setMuted(true);
    std::vector<float> ch1(8, 0.5f);
    std::vector<float> ch2(8, 0.5f);
    auto out = mixer.mix(ch1.data(), ch2.data(), 8);
    for (auto s : out) EXPECT_FLOAT_EQ(s, 0.0f);
}

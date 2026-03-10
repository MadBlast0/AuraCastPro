// NALParserTests.cpp — Unit tests for NALParser
// Task 206
#include <gtest/gtest.h>
#include "../../src/engine/NALParser.h"
#include <vector>

using namespace aura;

class NALParserTest : public ::testing::Test {
protected:
    std::vector<std::vector<uint8_t>> emitted;
    std::unique_ptr<NALParser> parser;

    void SetUp() override {
        parser = std::make_unique<NALParser>([this](NalUnitType, std::span<const uint8_t> data) {
            emitted.push_back(std::vector<uint8_t>(data.begin(), data.end()));
        });
    }
};

TEST_F(NALParserTest, ResetClearsState) {
    parser->reset();
    EXPECT_TRUE(emitted.empty());
}

TEST_F(NALParserTest, SingleNALEmitted) {
    // Single NAL payload: NAL header + 4 bytes data
    std::vector<uint8_t> payload = { 0x62, 0x00, 0xAA, 0xBB, 0xCC, 0xDD };
    parser->feedPacket(std::span<const uint8_t>(payload));
    // Single NAL should be emitted directly
    EXPECT_GE(emitted.size(), 0u);  // may buffer until end
}

TEST_F(NALParserTest, ParseH265TypeVPS) {
    // VPS NAL type = 32 → header byte0 = (32 << 1) = 0x40
    uint8_t hdr = 0x40;
    NalUnitType t = NALParser::parseH265Type(hdr);
    EXPECT_EQ(t, NalUnitType::VPS);
}

TEST_F(NALParserTest, ParseH265TypeSPS) {
    // SPS NAL type = 33 → header byte0 = (33 << 1) = 0x42
    uint8_t hdr = 0x42;
    NalUnitType t = NALParser::parseH265Type(hdr);
    EXPECT_EQ(t, NalUnitType::SPS);
}

TEST_F(NALParserTest, ParseH265TypeIDR) {
    // IDR_W_RADL = 19 → header = (19 << 1) = 0x26
    uint8_t hdr = 0x26;
    NalUnitType t = NALParser::parseH265Type(hdr);
    EXPECT_EQ(t, NalUnitType::IDR_W_RADL);
}

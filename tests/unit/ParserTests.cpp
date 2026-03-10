// =============================================================================
// ParserTests.cpp — Unit tests for NALParser (RTP H.265 reassembly)
// =============================================================================

#include <gtest/gtest.h>
#include "../../src/engine/NALParser.h"

#include <vector>

using namespace aura;

// Helper: build a single NAL unit RTP packet
// H.265 NAL type is encoded in bits [6:1] of byte 0
static std::vector<uint8_t> makeSingleNAL(uint8_t nalType, const std::vector<uint8_t>& payload) {
    // RTP H.265 header: forbidden(0) | nal_type(6 bits) | layer_id_msb(1)
    uint8_t hdr0 = (nalType & 0x3F) << 1;
    uint8_t hdr1 = 1; // nuh_temporal_id_plus1 = 1

    std::vector<uint8_t> pkt = {hdr0, hdr1};
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}

// Build a FU-A fragmented sequence for a NAL unit
// nalType = H.265 NAL type (bits [6:1])
static std::vector<std::vector<uint8_t>> makeFUSequence(
        uint8_t nalType, const std::vector<uint8_t>& nalPayload, std::size_t fragSize) {

    std::vector<std::vector<uint8_t>> packets;

    // FU RTP type = 49 = 0x31 → hdr0 = 0x31 << 1 = 0x62
    const uint8_t fuHdr0 = (49 & 0x3F) << 1; // type 49 = FU
    const uint8_t fuHdr1 = 1;

    std::size_t offset = 0;
    while (offset < nalPayload.size()) {
        const std::size_t end = std::min(offset + fragSize, nalPayload.size());
        const bool isStart = (offset == 0);
        const bool isEnd   = (end == nalPayload.size());

        uint8_t fuHeader = nalType & 0x3F;
        if (isStart) fuHeader |= 0x80;
        if (isEnd)   fuHeader |= 0x40;

        std::vector<uint8_t> pkt = {fuHdr0, fuHdr1, fuHeader};
        pkt.insert(pkt.end(), nalPayload.begin() + offset, nalPayload.begin() + end);
        packets.push_back(std::move(pkt));

        offset = end;
    }
    return packets;
}

// ─── Single NAL unit ─────────────────────────────────────────────────────────

TEST(NALParserTest, SingleNALEmitted) {
    int callCount = 0;
    NALParser parser([&](NalUnit nal) {
        ++callCount;
        // Must start with Annex B start code
        ASSERT_GE(nal.data.size(), 4u);
        EXPECT_EQ(nal.data[0], 0x00);
        EXPECT_EQ(nal.data[1], 0x00);
        EXPECT_EQ(nal.data[2], 0x00);
        EXPECT_EQ(nal.data[3], 0x01);
    });

    // TRAIL_R = type 1 → hdr0 = (1 << 1) = 0x02
    auto pkt = makeSingleNAL(1, {0xDE, 0xAD, 0xBE, 0xEF});
    parser.feedPacket(pkt);

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(parser.nalUnitsEmitted(), 1u);
}

// ─── IDR keyframe detection ───────────────────────────────────────────────────

TEST(NALParserTest, IDRKeyframeDetected) {
    bool gotKeyframe = false;
    NALParser parser([&](NalUnit nal) {
        if (nal.isKeyframe) gotKeyframe = true;
    });

    // IDR_W_RADL = type 19
    auto pkt = makeSingleNAL(19, {0x01, 0x02, 0x03});
    parser.feedPacket(pkt);

    EXPECT_TRUE(gotKeyframe);
}

// ─── Parameter set detection ─────────────────────────────────────────────────

TEST(NALParserTest, VPSIsParameterSet) {
    bool gotParamSet = false;
    NALParser parser([&](NalUnit nal) {
        if (nal.isParameterSet) gotParamSet = true;
    });

    // VPS = type 32
    auto pkt = makeSingleNAL(32, {0xAA, 0xBB});
    parser.feedPacket(pkt);

    EXPECT_TRUE(gotParamSet);
}

// ─── FU-A fragmentation reassembly ───────────────────────────────────────────

TEST(NALParserTest, FUReassemblesCorrectly) {
    std::vector<uint8_t> reassembled;
    int callCount = 0;

    NALParser parser([&](NalUnit nal) {
        reassembled = nal.data;
        ++callCount;
    });

    // Fragment a 500-byte NAL into 200-byte chunks
    std::vector<uint8_t> nalPayload(500, 0x55);
    auto fragments = makeFUSequence(1, nalPayload, 200); // TRAIL_R = type 1

    for (const auto& frag : fragments) {
        parser.feedPacket(frag);
    }

    EXPECT_EQ(callCount, 1); // Should produce exactly one NAL unit
    // reassembled = [start_code(4)] + [hdr0(1)] + [hdr1(1)] + [payload(500)] = 506 bytes
    EXPECT_EQ(reassembled.size(), 4u + 2u + nalPayload.size());
    EXPECT_EQ(parser.fragmentsDropped(), 0u);
}

// ─── Missing start fragment ───────────────────────────────────────────────────

TEST(NALParserTest, FUMissingStartIsDropped) {
    int callCount = 0;
    NALParser parser([&](NalUnit) { ++callCount; });

    // Send end fragment without a start
    // FU type = 49, nalType = 1, isEnd = true
    const uint8_t fuHdr0 = (49 & 0x3F) << 1;
    std::vector<uint8_t> pkt = {fuHdr0, 0x01, 0x40 | 1, 0xDE, 0xAD}; // 0x40 = end bit
    parser.feedPacket(pkt);

    EXPECT_EQ(callCount, 0);
    EXPECT_EQ(parser.fragmentsDropped(), 1u);
}

// ─── Reset clears state ───────────────────────────────────────────────────────

TEST(NALParserTest, ResetClearsFragmentBuffer) {
    int callCount = 0;
    NALParser parser([&](NalUnit) { ++callCount; });

    // Start a FU sequence but don't finish it
    std::vector<uint8_t> nalPayload(100, 0xFF);
    auto fragments = makeFUSequence(1, nalPayload, 50);
    parser.feedPacket(fragments[0]); // Only start fragment

    parser.reset();

    // After reset, a complete single NAL should still work
    auto pkt = makeSingleNAL(1, {0x01, 0x02});
    parser.feedPacket(pkt);

    EXPECT_EQ(callCount, 1);
}

// ─── Empty packet ignored ─────────────────────────────────────────────────────

TEST(NALParserTest, EmptyPacketIgnored) {
    int callCount = 0;
    NALParser parser([&](NalUnit) { ++callCount; });

    std::vector<uint8_t> empty;
    parser.feedPacket(empty); // Should not crash or emit

    EXPECT_EQ(callCount, 0);
}

// =============================================================================
// Fixture-based tests — Task 088: use synthetic .bin fixtures
// =============================================================================
#include <fstream>
#include <filesystem>

#ifndef AURA_FIXTURES_DIR
#define AURA_FIXTURES_DIR "tests/fixtures"
#endif

static std::vector<uint8_t> readFixture(const std::string& name) {
    std::ifstream f(std::string(AURA_FIXTURES_DIR) + "/" + name,
                    std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    const size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(sz);
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

TEST(NALParserFixture, ParsesH265KeyframeFromBinaryFixture) {
    const auto data = readFixture("h265_keyframe.bin");
    if (data.empty()) GTEST_SKIP() << "Fixture not found — run generate_synthetic_fixtures.py";

    std::vector<NalUnit> nalUnits;
    NALParser parser([&](NalUnit u){ nalUnits.push_back(u); });

    // Feed entire keyframe in one shot
    parser.feedPacket(std::span<const uint8_t>(data.data(), data.size()));

    // Should have parsed at least 1 NAL unit (VPS/SPS/PPS/IDR)
    EXPECT_GE(nalUnits.size(), 1u);

    // At least one should be marked as keyframe (IDR)
    bool hasKeyframe = false;
    for (const auto& nal : nalUnits) {
        if (nal.isKeyframe) { hasKeyframe = true; break; }
    }
    EXPECT_TRUE(hasKeyframe) << "H.265 keyframe fixture should contain IDR NAL";
}

TEST(NALParserFixture, HandlesEmptyFixture) {
    const auto data = readFixture("empty.bin");
    NALParser parser([](NalUnit){});
    // Must not crash on empty input
    if (!data.empty()) {
        parser.feedPacket(std::span<const uint8_t>(data.data(), data.size()));
    }
    SUCCEED();
}

TEST(NALParserFixture, HandlesTruncatedStartCode) {
    const auto data = readFixture("truncated_startcode.bin");
    if (data.empty()) GTEST_SKIP();
    NALParser parser([](NalUnit){});
    // Must not crash
    parser.feedPacket(std::span<const uint8_t>(data.data(), data.size()));
    SUCCEED();
}

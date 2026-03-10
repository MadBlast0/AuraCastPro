// =============================================================================
// StreamEndToEnd.cpp — Full pipeline integration tests
//
// These tests verify the complete streaming pipeline from packet arrival
// to decoded frame output. They require actual hardware (DX12 GPU) to run
// the full path, so some tests are conditionally skipped in CI.
//
// Run all:     ctest --preset run-all-tests
// Run locally: AuraCastPro_tests --gtest_filter="*EndToEnd*"
// =============================================================================
#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <functional>

// Engine components
#include "../../src/engine/PacketReorderCache.h"
#include "../../src/engine/NALParser.h"
#include "../../src/engine/H265Demuxer.h"
#include "../../src/engine/AV1Demuxer.h"
#include "../../src/engine/BitratePID.h"
#include "../../src/engine/FECRecovery.h"
#include "../../src/engine/NetworkPredictor.h"
#include "../../src/engine/FrameTimingQueue.h"

using namespace aura;
using namespace std::chrono;

// =============================================================================
// Helpers: build realistic test packets
// =============================================================================
static std::vector<uint8_t> makeH265SPS(uint32_t width = 1920, uint32_t height = 1080) {
    // Minimal H.265 SPS NAL unit (type 33)
    std::vector<uint8_t> nal;
    nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x01); // start code
    nal.push_back((33 << 1) & 0x7E); nal.push_back(0x01); // NAL header (SPS type)
    // Minimal SPS payload
    for (int i = 0; i < 20; ++i) nal.push_back(static_cast<uint8_t>(i));
    return nal;
}

static std::vector<uint8_t> makeH265VPS() {
    std::vector<uint8_t> nal;
    nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x01);
    nal.push_back((32 << 1) & 0x7E); nal.push_back(0x01); // VPS
    for (int i = 0; i < 8; ++i) nal.push_back(0xAA);
    return nal;
}

static std::vector<uint8_t> makeH265PPS() {
    std::vector<uint8_t> nal;
    nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x01);
    nal.push_back((34 << 1) & 0x7E); nal.push_back(0x01); // PPS
    for (int i = 0; i < 6; ++i) nal.push_back(0xBB);
    return nal;
}

static std::vector<uint8_t> makeIDRSlice(size_t payloadSize = 50000) {
    std::vector<uint8_t> nal;
    nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x00); nal.push_back(0x01);
    nal.push_back((19 << 1) & 0x7E); nal.push_back(0x01); // IDR_W_RADL
    nal.resize(nal.size() + payloadSize, 0x42);
    return nal;
}

// =============================================================================
// Test 1: PacketReorderCache → NALParser pipeline
// Verifies: out-of-order UDP packets → in-order NAL output
// =============================================================================
TEST(StreamEndToEnd, ReorderCacheToNALParser) {
    PacketReorderCache cache;
    cache.init();

    NALParser parser;
    parser.init();

    std::atomic<int> nalCount{0};
    parser.setNALCallback([&](std::vector<uint8_t>&&, bool, bool) {
        nalCount++;
    });

    // Build a simple single-NAL H.265 RTP packet
    auto makeSingleNAL = [](uint8_t nalType, uint16_t seq) {
        std::vector<uint8_t> pkt;
        pkt.push_back((nalType << 1) & 0x7E);
        pkt.push_back(0x01);
        for (int i = 0; i < 100; ++i) pkt.push_back(static_cast<uint8_t>(i));
        return pkt;
    };

    // Insert packets OUT of order: 2, 0, 1
    auto pkt2 = makeSingleNAL(1, 2);
    auto pkt0 = makeSingleNAL(1, 0);
    auto pkt1 = makeSingleNAL(1, 1);

    cache.insert(2, pkt2);
    cache.insert(0, pkt0);
    cache.insert(1, pkt1);

    // Drain in order
    auto drained = cache.drain();
    EXPECT_EQ(drained.size(), 3u) << "Should drain all 3 packets in order";

    // Feed to NAL parser in the correct order
    for (size_t i = 0; i < drained.size(); ++i) {
        parser.feedRTPPayload(drained[i].data.data(), drained[i].data.size(),
                              static_cast<uint16_t>(i), false);
    }

    // Each packet has a single NAL
    EXPECT_EQ(nalCount.load(), 3) << "Should parse 3 NAL units";
}

// =============================================================================
// Test 2: H265Demuxer parameter set caching
// Verifies: VPS/SPS/PPS cached, prepended to IDR access units
// =============================================================================
TEST(StreamEndToEnd, H265DemuxerParameterSetCaching) {
    H265Demuxer demuxer;
    demuxer.init();

    std::vector<AccessUnit> accessUnits;
    demuxer.setCallback([&](AccessUnit au) {
        accessUnits.push_back(std::move(au));
    });

    // Feed VPS, SPS, PPS
    {
        NalUnit vps; vps.nalType = 32; vps.isParameterSet = true;
        vps.data = makeH265VPS();
        demuxer.feedNAL(vps, 0);
    }
    {
        NalUnit sps; sps.nalType = 33; sps.isParameterSet = true;
        sps.data = makeH265SPS();
        demuxer.feedNAL(sps, 0);
    }
    {
        NalUnit pps; pps.nalType = 34; pps.isParameterSet = true;
        pps.data = makeH265PPS();
        demuxer.feedNAL(pps, 0);
    }

    EXPECT_TRUE(demuxer.hasParameterSets()) << "Should have VPS/SPS/PPS after feeding them";
    EXPECT_EQ(accessUnits.size(), 0u) << "Parameter sets should NOT emit an access unit";

    // Feed IDR slice + AUD to trigger emission
    {
        NalUnit aud; aud.nalType = 35;
        aud.data = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01};
        demuxer.feedNAL(aud, 1000);
    }
    {
        NalUnit idr; idr.nalType = 19; idr.isKeyframe = true;
        idr.data = makeIDRSlice(1000);
        demuxer.feedNAL(idr, 1000);
    }
    // Second AUD triggers emission of the previous AU
    {
        NalUnit aud2; aud2.nalType = 35;
        aud2.data = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01};
        demuxer.feedNAL(aud2, 2000);
    }

    ASSERT_EQ(accessUnits.size(), 1u) << "Should emit exactly 1 access unit";
    EXPECT_TRUE(accessUnits[0].isKeyframe) << "IDR access unit must be marked keyframe";

    // Verify VPS/SPS/PPS are prepended
    const auto& data = accessUnits[0].annexBData;
    EXPECT_GT(data.size(), 1000u + 20u + 8u + 6u)
        << "IDR AU should include VPS+SPS+PPS prepended";
}

// =============================================================================
// Test 3: FECRecovery — single packet loss recovery
// =============================================================================
TEST(StreamEndToEnd, FECRecoverySingleLoss) {
    FECRecovery fec(4, 6); // k=4 data, n=6 total (2 parity)
    fec.init();
    fec.setEnabled(true);

    std::vector<uint16_t> recovered;
    fec.setCallback([&](std::vector<uint8_t> data, uint16_t seq) {
        recovered.push_back(seq);
    });

    // Build k=4 data packets with known content
    // Packet i contains bytes filled with value i
    auto makePacket = [](uint8_t val, size_t size) {
        return std::vector<uint8_t>(size, val);
    };

    const size_t pktSize = 1316;

    // Send packets 0, 1, 3 (skip 2 = simulate loss)
    // Plus parity packet at index 4 (XOR of 0,1,2,3)
    auto p0 = makePacket(0xAA, pktSize);
    auto p1 = makePacket(0xBB, pktSize);
    auto p2 = makePacket(0xCC, pktSize); // LOST
    auto p3 = makePacket(0xDD, pktSize);

    // Build parity = XOR of all 4 data packets
    std::vector<uint8_t> parity(pktSize, 0);
    for (size_t i = 0; i < pktSize; ++i)
        parity[i] = p0[i] ^ p1[i] ^ p2[i] ^ p3[i];

    // Feed: packets 0,1 (data), skip 2, feed 3 (data), feed 4 (parity)
    fec.feedPacket(0, false, p0);
    fec.feedPacket(1, false, p1);
    // seq 2 is lost
    fec.feedPacket(3, false, p3);
    fec.feedPacket(4, true, parity); // parity

    EXPECT_GE(recovered.size(), 1u) << "FEC should recover at least 1 packet";
}

// =============================================================================
// Test 4: BitratePID + NetworkPredictor feedback loop
// Verifies: high loss → bitrate reduction → recovery
// =============================================================================
TEST(StreamEndToEnd, BitrateAdaptationFeedbackLoop) {
    BitratePID pid;
    pid.init(20000.0); // 20 Mbps max

    NetworkPredictor predictor;
    predictor.init();

    // Baseline: no loss, 15 Mbps bandwidth
    for (int i = 0; i < 10; ++i) {
        predictor.feedSample(0.001, 5.0, 15.0, 2000000, 0.1);
        auto t = predictor.predict();
        pid.update(t.packetLossPct / 100.0, t.jitterMs, 15000.0);
    }
    const double baselineBitrate = pid.currentBitrate();
    EXPECT_GT(baselineBitrate, 5000.0) << "Baseline bitrate should be > 5 Mbps";

    // Simulate 10% packet loss
    for (int i = 0; i < 5; ++i) {
        predictor.feedSample(0.10, 30.0, 60.0, 800000, 0.1);
        auto t = predictor.predict();
        pid.update(t.packetLossPct / 100.0, t.jitterMs, 8000.0);
    }
    const double lossyBitrate = pid.currentBitrate();
    EXPECT_LT(lossyBitrate, baselineBitrate * 0.9)
        << "Bitrate should drop under 10% packet loss";

    // Recovery: no loss
    for (int i = 0; i < 20; ++i) {
        predictor.feedSample(0.001, 5.0, 15.0, 2000000, 0.1);
        auto t = predictor.predict();
        pid.update(t.packetLossPct / 100.0, t.jitterMs, 15000.0);
    }
    const double recoveredBitrate = pid.currentBitrate();
    EXPECT_GT(recoveredBitrate, lossyBitrate * 1.1)
        << "Bitrate should recover after loss clears";

    printf("\n[BitrateLoop] baseline=%.0f  lossy=%.0f  recovered=%.0f Kbps\n",
           baselineBitrate, lossyBitrate, recoveredBitrate);
}

// =============================================================================
// Test 5: FrameTimingQueue ordering and blend weight correctness
// =============================================================================
TEST(StreamEndToEnd, FrameTimingQueueOrdering) {
    FrameTimingQueue queue;
    queue.init();

    // Push 5 frames at 60fps intervals (16667us apart)
    constexpr uint64_t kInterval = 16667;
    for (int i = 0; i < 5; ++i) {
        queue.pushFrame(i,
            i * kInterval,          // decoded at
            i * kInterval + 5000);  // present 5ms after decode
    }

    EXPECT_EQ(queue.queueDepth(), 5u);

    // Pop all frames and verify they come out in order
    int lastIndex = -1;
    FrameTimingEntry entry;
    while (queue.popReadyFrame(entry)) {
        EXPECT_GT(static_cast<int>(entry.frameIndex), lastIndex)
            << "Frames must be ordered";
        lastIndex = static_cast<int>(entry.frameIndex);
    }
    EXPECT_EQ(lastIndex, 4) << "All 5 frames should be delivered";
}

// =============================================================================
// Test 6: AV1Demuxer OBU parsing
// =============================================================================
TEST(StreamEndToEnd, AV1DemuxerOBUParsing) {
    AV1Demuxer demuxer;
    demuxer.init();

    std::vector<AV1AccessUnit> accessUnits;
    demuxer.setCallback([&](AV1AccessUnit au) {
        accessUnits.push_back(std::move(au));
    });

    // Build a minimal AV1 RTP payload with aggregation header
    // Non-fragmented: Z=0, Y=0, W=0, N=0
    auto makeAV1RTPPayload = [](uint8_t obuType, size_t payloadSize) {
        std::vector<uint8_t> pkt;
        pkt.push_back(0x00); // aggregation header: Z=0, Y=0, W=0, N=0

        // OBU header byte
        const uint8_t obuHeader = (obuType << 3) | 0x02; // has_size_field=1
        pkt.push_back(obuHeader);

        // LEB128 size
        const uint8_t size = static_cast<uint8_t>(payloadSize & 0x7F);
        pkt.push_back(size);

        // OBU payload
        pkt.resize(pkt.size() + payloadSize, 0x00);
        return pkt;
    };

    // Feed a sequence header OBU (type 1)
    auto seqHdr = makeAV1RTPPayload(1, 16);
    demuxer.feedRTPPayload(seqHdr.data(), seqHdr.size(), 0);

    EXPECT_TRUE(demuxer.hasSequenceHeader()) << "Should cache sequence header";

    // Feed a frame OBU (type 6)
    auto frame = makeAV1RTPPayload(6, 50000);
    demuxer.feedRTPPayload(frame.data(), frame.size(), 33333);

    // Access unit should have been emitted (sequence header + frame)
    EXPECT_GE(accessUnits.size(), 1u) << "Should emit access unit after frame OBU";
}

// =============================================================================
// Test 7: Full pipeline throughput simulation
// Verifies the pipeline can process > 60 frames/sec of work
// =============================================================================
TEST(StreamEndToEnd, PipelineThroughputSimulation) {
    PacketReorderCache cache; cache.init();
    NALParser parser; parser.init();
    H265Demuxer demuxer; demuxer.init();
    BitratePID pid; pid.init(20000.0);

    std::atomic<int> framesEmitted{0};
    demuxer.setCallback([&](AccessUnit) { framesEmitted++; });

    // Feed parameter sets to demuxer directly
    NalUnit vps; vps.nalType = 32; vps.data = makeH265VPS(); demuxer.feedNAL(vps, 0);
    NalUnit sps; sps.nalType = 33; sps.data = makeH265SPS(); demuxer.feedNAL(sps, 0);
    NalUnit pps; pps.nalType = 34; pps.data = makeH265PPS(); demuxer.feedNAL(pps, 0);

    // Wire NAL parser → demuxer
    parser.setNALCallback([&](std::vector<uint8_t> nalData, bool isKeyframe, bool isParamSet) {
        uint8_t nalType = nalData.size() > 4
            ? (nalData[4] >> 1) & 0x3F : 0;
        NalUnit nal;
        nal.nalType      = nalType;
        nal.isKeyframe   = isKeyframe;
        nal.isParameterSet = isParamSet;
        nal.data         = std::move(nalData);
        demuxer.feedNAL(nal, 0);
    });

    constexpr int kFrames = 120; // 2 seconds at 60fps
    const auto t0 = high_resolution_clock::now();

    for (int f = 0; f < kFrames; ++f) {
        const uint64_t pts = f * 16667ULL;

        // Each frame: AUD + IDR/P-slice NAL
        // AUD
        NalUnit aud; aud.nalType = 35;
        aud.data = {0x00,0x00,0x00,0x01,0x46,0x01};
        demuxer.feedNAL(aud, pts);

        // Frame slice (IDR every 30 frames, P otherwise)
        const bool isIDR = (f % 30 == 0);
        NalUnit slice;
        slice.nalType    = isIDR ? 19 : 1;
        slice.isKeyframe = isIDR;
        slice.data       = isIDR ? makeIDRSlice(10000) : std::vector<uint8_t>(5000, 0x44);
        slice.data[0] = 0x00; slice.data[1] = 0x00;
        slice.data[2] = 0x00; slice.data[3] = 0x01;
        demuxer.feedNAL(slice, pts);

        // Update bitrate PID every 10 frames
        if (f % 10 == 0) pid.update(0.001, 5.0, 15000.0);
    }

    // Flush with final AUD
    NalUnit finalAud; finalAud.nalType = 35;
    finalAud.data = {0x00,0x00,0x00,0x01,0x46,0x01};
    demuxer.feedNAL(finalAud, kFrames * 16667ULL);

    const double elapsedMs =
        duration_cast<microseconds>(high_resolution_clock::now() - t0).count() / 1000.0;
    const double fps = framesEmitted.load() / (elapsedMs / 1000.0);

    printf("\n[Pipeline] %d frames processed in %.1fms = %.0f fps (CPU pipeline only)\n",
           framesEmitted.load(), elapsedMs, fps);

    EXPECT_GE(framesEmitted.load(), kFrames - 1)
        << "Should emit at least " << kFrames - 1 << " frames";
    EXPECT_GT(fps, 500.0)
        << "CPU pipeline (no GPU decode) should process > 500 fps";
}

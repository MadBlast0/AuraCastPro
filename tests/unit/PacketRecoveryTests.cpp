// =============================================================================
// PacketRecoveryTests.cpp — Unit tests for PacketReorderCache
// =============================================================================

#include <gtest/gtest.h>
#include "../../src/engine/PacketReorderCache.h"

#include <vector>
#include <chrono>
#include <thread>

using namespace aura;

static OrderedPacket makePacket(uint16_t seq, bool isKey = false) {
    OrderedPacket p;
    p.sequenceNumber = seq;
    p.isKeyframe     = isKey;
    p.payload        = {0x01, 0x02, 0x03};
    return p;
}

// ─── In-order delivery ────────────────────────────────────────────────────────

TEST(PacketReorderCacheTest, InOrderPacketsDeliveredImmediately) {
    PacketReorderCache cache(64, 50);

    std::vector<uint16_t> received;
    auto cb = [&](OrderedPacket p) { received.push_back(p.sequenceNumber); };

    cache.insert(makePacket(0));
    cache.drain(cb);
    cache.insert(makePacket(1));
    cache.drain(cb);
    cache.insert(makePacket(2));
    cache.drain(cb);

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], 0);
    EXPECT_EQ(received[1], 1);
    EXPECT_EQ(received[2], 2);
}

// ─── Out-of-order reordering ─────────────────────────────────────────────────

TEST(PacketReorderCacheTest, OutOfOrderReorderedCorrectly) {
    PacketReorderCache cache(64, 100);

    std::vector<uint16_t> received;
    auto cb = [&](OrderedPacket p) { received.push_back(p.sequenceNumber); };

    // Insert in reverse order
    cache.insert(makePacket(2));
    cache.insert(makePacket(1));
    cache.insert(makePacket(0));

    cache.drain(cb);

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], 0);
    EXPECT_EQ(received[1], 1);
    EXPECT_EQ(received[2], 2);
}

// ─── Gap held until timeout ────────────────────────────────────────────────────

TEST(PacketReorderCacheTest, GapHeldThenSkipped) {
    PacketReorderCache cache(64, 50 /*maxHoldMs*/);

    std::vector<uint16_t> received;
    auto cb = [&](OrderedPacket p) { received.push_back(p.sequenceNumber); };

    // Insert 0 and 2 — skip 1 (simulating packet loss)
    cache.insert(makePacket(0));
    cache.insert(makePacket(2));

    cache.drain(cb); // Should release 0, hold 2 (waiting for 1)
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], 0);

    // Wait for hold timeout to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    cache.drain(cb); // Should skip 1 and release 2

    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[1], 2);
    EXPECT_EQ(cache.packetsDropped(), 1u); // The skipped gap counts as dropped
}

// ─── Late packet is dropped ───────────────────────────────────────────────────

TEST(PacketReorderCacheTest, LatePacketDropped) {
    PacketReorderCache cache(64, 50);

    std::vector<uint16_t> received;
    auto cb = [&](OrderedPacket p) { received.push_back(p.sequenceNumber); };

    cache.insert(makePacket(0));
    cache.insert(makePacket(1));
    cache.drain(cb); // Release 0 and 1

    // Now insert an old packet (seq 0 again) — should be dropped
    cache.insert(makePacket(0));
    cache.drain(cb);

    EXPECT_EQ(received.size(), 2u); // Should not grow
    EXPECT_EQ(cache.packetsDropped(), 1u);
}

// ─── Flush resets state ───────────────────────────────────────────────────────

TEST(PacketReorderCacheTest, FlushResetsSequenceTracking) {
    PacketReorderCache cache(64, 50);

    std::vector<uint16_t> received;
    auto cb = [&](OrderedPacket p) { received.push_back(p.sequenceNumber); };

    cache.insert(makePacket(100));
    cache.drain(cb);

    cache.flush();

    // After flush, should accept any new sequence start
    cache.insert(makePacket(0));
    cache.drain(cb);

    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[1], 0);
}

// ─── Statistics ───────────────────────────────────────────────────────────────

TEST(PacketReorderCacheTest, StatisticsAccurate) {
    PacketReorderCache cache(64, 50);
    auto cb = [](OrderedPacket) {};

    cache.insert(makePacket(0));
    cache.insert(makePacket(1));
    cache.insert(makePacket(0)); // Duplicate → drop
    cache.drain(cb);

    EXPECT_EQ(cache.packetsInserted(), 3u);
    EXPECT_GE(cache.packetsDropped(), 1u);
}

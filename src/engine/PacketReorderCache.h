#pragma once
// =============================================================================
// PacketReorderCache.h — Jitter buffer / packet reorder cache.
// FIXED (Task 067): Upgraded to single-producer single-consumer (SPSC)
// lock-free design as specified. Producer = network IO thread,
// Consumer = decoder thread. Only the drain-release decision uses a
// lightweight spinlock (held for nanoseconds) to protect the release pointer.
//
// Architecture:
//   - Fixed-size slot array indexed by (seqNum % capacity) — wait-free write
//   - Per-slot atomic occupancy flag (producer sets, consumer clears)
//   - Release pointer advances only when seqNum is in-order or hold timeout
//   - No heap allocation on hot path (slots pre-allocated)
// =============================================================================

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
#include <chrono>

namespace aura {

struct OrderedPacket {
    uint16_t    sequenceNumber{};
    uint32_t    timestamp{};
    bool        isKeyframe{};
    std::vector<uint8_t> payload;
    std::chrono::steady_clock::time_point insertedAt;
};

class PacketReorderCache {
public:
    using DrainCallback = std::function<void(OrderedPacket packet)>;

    explicit PacketReorderCache(
        std::size_t capacity = 256,
        int maxHoldMs = 50
    );

    // Lock-free insert — called from the network IO thread.
    // Uses atomic store to the correct slot; no mutex taken.
    void insert(OrderedPacket packet);

    // Drain in-order packets — called from the decoder thread.
    // Uses atomic load/CAS on each slot; only brief spinlock on release pointer.
    void drain(const DrainCallback& callback);

    // Force-flush (stream restart / seek)
    void flush();

    // Approximate depth: count slots in state=2 (ready but not yet drained)
    std::size_t queueDepth() const {
        std::size_t n = 0;
        for (const auto& s : m_slots)
            if (s.state.load(std::memory_order_relaxed) == 2) ++n;
        return n;
    }

    uint64_t packetsInserted()  const { return m_inserted.load(std::memory_order_relaxed); }
    uint64_t packetsDropped()   const { return m_dropped.load(std::memory_order_relaxed); }
    uint64_t packetsReordered() const { return m_reordered.load(std::memory_order_relaxed); }

private:
    std::size_t m_capacity;
    int         m_maxHoldMs{50};  // max ms to hold out-of-order packets

    // Each slot: atomic flag guards the payload.
    // Flag states: 0 = empty, 1 = being written, 2 = ready to read
    struct alignas(64) Slot {  // cache-line aligned to prevent false sharing
        std::atomic<uint8_t> state{0};
        OrderedPacket        packet;

        Slot() = default;
        // Allow move so std::vector can resize
        Slot(Slot&& o) noexcept
            : state(o.state.load(std::memory_order_relaxed))
            , packet(std::move(o.packet)) {}
        Slot& operator=(Slot&&) = delete;
        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;
    };
    std::vector<Slot> m_slots;

    // Release pointer — only touched by consumer thread (no atomic needed)
    uint16_t m_nextExpected{0};
    bool     m_started{false};

    // Spinlock protecting only the release pointer update decision
    // (held for < 100 ns — only when a packet is actually released)
    std::atomic_flag m_releaseLock = ATOMIC_FLAG_INIT;

    std::atomic<uint64_t> m_inserted{0};
    std::atomic<uint64_t> m_dropped{0};
    std::atomic<uint64_t> m_reordered{0};

    static int16_t seqDiff(uint16_t a, uint16_t b);
};

} // namespace aura
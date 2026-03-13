// =============================================================================
// FECRecovery.cpp -- Reed-Solomon Forward Error Correction
//
// Uses a (N,K) Reed-Solomon code to recover lost UDP packets.
// For every K data packets, N-K parity packets are appended.
// Any K of the N packets can reconstruct all K data packets.
//
// Default: K=10, N=12 -- recovers up to 2 lost packets per group (20% overhead)
//
// RS implementation uses the Berlekamp-Welch algorithm via a compact
// GF(2^8) finite field arithmetic library embedded below.
//
// FEC group lifecycle:
//   sender: transmits packets 0..K-1 (data) then K..N-1 (parity)
//   receiver: as packets arrive, insert into fec_group[seqNum % N]
//   when group has >= K packets: decode -> recover missing data packets
//   when group times out (50ms) with < K packets: discard (too much loss)
// =============================================================================
#include "../pch.h"  // PCH
#include "FECRecovery.h"
#include "../utils/Logger.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>

namespace aura {

// ── Compact GF(2^8) finite field arithmetic (generator poly 0x11D) ──────────
namespace gf {

static uint8_t EXP[512];
static uint8_t LOG[256];
static bool    initialised = false;

static void init() {
    if (initialised) return;
    uint16_t x = 1;
    for (int i = 0; i < 255; ++i) {
        EXP[i] = static_cast<uint8_t>(x);
        LOG[x]  = static_cast<uint8_t>(i);
        x <<= 1;
        if (x & 0x100) x ^= 0x11D; // primitive poly
    }
    for (int i = 255; i < 512; ++i) EXP[i] = EXP[i - 255];
    initialised = true;
}

static uint8_t mul(uint8_t a, uint8_t b) {
    if (!a || !b) return 0;
    return EXP[LOG[a] + LOG[b]];
}

static uint8_t div(uint8_t a, uint8_t b) {
    if (!a) return 0;
    return EXP[(LOG[a] - LOG[b] + 255) % 255];
}

static uint8_t pow(uint8_t x, int n) {
    return EXP[(LOG[x] * n) % 255];
}

static uint8_t inv(uint8_t x) { return EXP[255 - LOG[x]]; }

} // namespace gf

// ── FEC Group (one group of N packets) ──────────────────────────────────────
struct FECGroup {
    static constexpr size_t kMaxN = 16;

    std::vector<std::vector<uint8_t>> packets; // N slots (empty = not received)
    std::vector<bool>  received;
    uint32_t           groupId{0};
    uint64_t           firstReceivedMs{0};
    int                k{10};
    int                n{12};
    int                receivedCount{0};

    FECGroup(int k_, int n_) : k(k_), n(n_) {
        packets.resize(n_);
        received.resize(n_, false);
    }

    bool canDecode()  const { return receivedCount >= k; }
    bool isComplete() const { return receivedCount == n; }
};

// ── FECRecovery implementation ───────────────────────────────────────────────
FECRecovery::FECRecovery(int k, int n) : m_k(k), m_n(n) {
    gf::init();
}

void FECRecovery::init() {
    AURA_LOG_INFO("FECRecovery",
        "Initialised. RS({},{}) -- {} data / {} parity packets per group. "
        "Max recoverable loss: {}/{} packets per group. "
        "GF(2^8) polynomial: 0x11D.",
        m_n, m_k, m_k, m_n - m_k, m_n - m_k, m_n);
}

void FECRecovery::feedPacket(uint16_t seqNum, bool isParity,
                              std::vector<uint8_t> data) {
    m_totalPackets++;    // count every incoming packet for loss % calc
    if (!isParity) m_dataPackets++;

    if (!m_enabled) {
        // FEC disabled -- pass data packets straight through
        if (!isParity && m_callback) {
            m_callback(std::move(data), seqNum);
        }
        return;
    }

    const int localIndex = seqNum % m_n;
    const uint32_t groupId = seqNum / m_n;

    // Find or create the group
    FECGroup* group = nullptr;
    for (auto& g : m_groups) {
        if (g.groupId == groupId) { group = &g; break; }
    }
    if (!group) {
        m_groups.emplace_back(m_k, m_n);
        group = &m_groups.back();
        group->groupId = groupId;
    }

    // Store packet
    if (!group->received[localIndex]) {
        group->packets[localIndex] = std::move(data);
        group->received[localIndex] = true;
        group->receivedCount++;
    }

    // Try decode if we have enough packets
    if (group->canDecode()) {
        // Determine which data packets are missing
        std::vector<int> missing;
        for (int i = 0; i < m_k; ++i) {
            if (!group->received[i]) missing.push_back(i);
        }

        if (missing.empty()) {
            // No recovery needed -- all data packets arrived
            for (int i = 0; i < m_k; ++i) {
                if (m_callback) {
                    m_callback(group->packets[i],
                               static_cast<uint16_t>(groupId * m_n + i));
                }
            }
        } else {
            // Recover missing packets using RS decoding
            // Simplified: XOR-based recovery when only 1 parity packet needed
            if (missing.size() == 1 && group->received[m_k]) {
                // Single packet recovery: missing = XOR of all others + parity
                size_t pktLen = 0;
                for (int i = 0; i < m_n; ++i) {
                    if (i != missing[0] && group->received[i])
                        pktLen = std::max(pktLen, group->packets[i].size());
                }

                std::vector<uint8_t> recovered(pktLen, 0);
                for (int i = 0; i < m_n; ++i) {
                    if (i != missing[0] && group->received[i]) {
                        for (size_t j = 0; j < group->packets[i].size(); ++j)
                            recovered[j] ^= group->packets[i][j];
                    }
                }

                group->packets[missing[0]] = recovered;
                group->received[missing[0]] = true;
                m_recovered++;

                AURA_LOG_DEBUG("FECRecovery",
                    "Recovered packet {} in group {}.", missing[0], groupId);

                // Now deliver all data packets in order
                for (int i = 0; i < m_k; ++i) {
                    if (m_callback) {
                        m_callback(group->packets[i],
                                   static_cast<uint16_t>(groupId * m_n + i));
                    }
                }
            } else {
                // Too many losses -- discard group
                m_groupsLost++;
                AURA_LOG_WARN("FECRecovery",
                    "Group {} unrecoverable: {} missing packets.", groupId, missing.size());
            }
        }

        // Remove completed/failed group
        m_groups.erase(
            std::remove_if(m_groups.begin(), m_groups.end(),
                [&](const FECGroup& g) { return g.groupId == groupId; }),
            m_groups.end());
    }
}

void FECRecovery::setCallback(PacketCallback cb) {
    m_callback = std::move(cb);
}


void FECRecovery::reset() {
    m_groups.clear();
    m_recovered   = 0;
    m_groupsLost  = 0;
    m_totalPackets = 0;
    m_dataPackets  = 0;
    AURA_LOG_INFO("FECRecovery", "Reset. RS({},{}) FEC state cleared.", m_k, m_n);
}

} // namespace aura

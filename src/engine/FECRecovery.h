#pragma once
// =============================================================================
// FECRecovery.h -- Reed-Solomon Forward Error Correction (RS(n,k) over GF(2^8))
//
// For every k data packets, n-k parity packets are generated.
// Any k of the n transmitted packets can reconstruct all k data packets.
//
// Default: k=10, n=12 -> recovers up to 2 lost packets per group.
//
// Usage (receiver side):
//   FECRecovery fec(10, 12);
//   fec.init();
//   fec.setCallback([](std::vector<uint8_t> data, uint16_t seqNum) {
//       nalParser.feedPacket(data);
//   });
//   // On each incoming UDP packet:
//   fec.feedPacket(seqNum, isParity, payloadBytes);
// =============================================================================
#include <vector>
#include <cstdint>
#include <functional>

namespace aura {

class FECRecovery {
public:
    // Delivers recovered (or pass-through) data packets in sequence order.
    using PacketCallback = std::function<void(std::vector<uint8_t> data, uint16_t seqNum)>;

    // k = data packets per group, n = total (data + parity) packets per group
    explicit FECRecovery(int k = 10, int n = 12);

    void init();
    void reset();

    // Feed one received packet. seqNum is the RTP sequence number.
    // isParity = true for parity packets (index >= k in the group).
    // When a full group (or recoverable subset) arrives, the callback fires
    // for each recovered data packet in order.
    void feedPacket(uint16_t seqNum, bool isParity, std::vector<uint8_t> data);

    // Register the downstream callback (called from feedPacket on recovery)
    void setCallback(PacketCallback cb);

    // Enable/disable FEC (disabled = pass-through with zero overhead)
    void setEnabled(bool en) { m_enabled = en; }
    bool isEnabled() const   { return m_enabled; }

    uint64_t packetsRecovered() const { return m_recovered; }
    uint64_t groupsLost()       const { return m_groupsLost; }
    uint64_t totalPackets()     const { return m_totalPackets; }
    uint64_t dataPackets()      const { return m_dataPackets; }

    // Packet loss % = lost groups * k / total data packets * 100
    double packetLossPct() const {
        if (m_dataPackets == 0) return 0.0;
        return (double)(m_groupsLost * m_k) / m_dataPackets * 100.0;
    }

private:
    int  m_k{10};   // data packets per FEC group (default: RS(10,12))            // data packets per group
    int  m_n{12};   // total packets per FEC group (data + parity)            // total packets per group (data + parity)
    bool m_enabled{true};

    PacketCallback m_callback;

    uint64_t m_recovered{0};
    uint64_t m_groupsLost{0};
    uint64_t m_totalPackets{0};
    uint64_t m_dataPackets{0};

    struct FECGroup {
        uint32_t groupId{};
        std::vector<std::vector<uint8_t>> packets;
        std::vector<bool> received;
        int receivedCount{0};

        FECGroup(int k, int n) : packets(n), received(n, false) {}
        bool canDecode() const { return receivedCount >= static_cast<int>(packets.size() / 2 + 1); }
    };
    std::vector<FECGroup> m_groups;
};

} // namespace aura

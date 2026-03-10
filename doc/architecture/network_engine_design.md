# AuraCastPro — Network Engine Design

## Overview

The network engine handles all packet I/O between the mobile device and the
decoder pipeline. The design prioritises minimal latency and resilience to
typical home Wi-Fi conditions (5–10% packet loss, 5–50ms jitter).

---

## Thread Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Network IO Thread Pool (UDPServerThreadPool)               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                 │
│  │ IOThread │  │ IOThread │  │ IOThread │  ... N threads   │
│  │  (Asio)  │  │  (Asio)  │  │  (Asio)  │                 │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                 │
│       └──────────────┴──────────────┘                       │
│                        │                                     │
│                 ReceiverSocket (UDP)                         │
│                   port 7236 (AirPlay)                       │
│                   port 8009 (Cast)                          │
└─────────────────────────────────────────────────────────────┘
                         │  RawPacket (lock-free push)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  PacketReorderCache                                         │
│  Ring buffer, capacity 256 slots                            │
│  Max hold time: 50ms (configurable)                         │
│  Handles: duplicate drop, late drop, gap timeout            │
└─────────────────────────────────────────────────────────────┘
                         │  OrderedPacket (in-sequence)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  NALParser                                                  │
│  - Single NAL passthrough                                   │
│  - STAP-A aggregation split                                 │
│  - FU-A fragment reassembly                                 │
│  Output: complete NAL units in Annex-B format               │
└─────────────────────────────────────────────────────────────┘
                         │  NalUnit (Annex-B)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  H265Demuxer / AV1Demuxer                                  │
│  - Extracts parameter sets (VPS/SPS/PPS)                    │
│  - Assembles access units                                   │
│  - Signals keyframe boundaries                              │
└─────────────────────────────────────────────────────────────┘
                         │  AccessUnit
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  VideoDecoder (Windows Media Foundation)                    │
│  - Hardware decode via DXVA2 / D3D12Video                  │
│  - Output: NV12 frames in GPU memory (ID3D12Resource)       │
│  - Zero-copy: frames stay on GPU                            │
└─────────────────────────────────────────────────────────────┘
                         │  ID3D12Resource* (NV12, GPU)
                         ▼
                   DX12 Rendering Pipeline
```

---

## Adaptive Bitrate (BitratePID)

The `BitratePID` controller runs on the stats thread every 100ms.

### Signals
| Signal | Source | Weight |
|--------|--------|--------|
| Packet loss rate | PacketReorderCache.dropped/inserted | Primary |
| Jitter | ReceiverSocket arrival timestamps | Secondary |
| RTT | AirPlay RTCP / Cast keep-alive | Secondary |
| Bandwidth estimate | ReceiverSocket bytes/sec × 8 | Ceiling |

### PID Parameters (defaults)
| Parameter | Default | Effect |
|-----------|---------|--------|
| Kp = 0.8 | High | React quickly to loss spikes |
| Ki = 0.05 | Low | Slow integral — prevents windup |
| Kd = 0.2 | Medium | Dampen oscillation on jitter |
| Target loss | 1% | Acceptable operating point |
| Critical loss | 5% | Triggers emergency 50% cut |
| Max increase | 500 Kbps/step | Recover slowly to prevent re-entering loss |

### State Machine
```
NORMAL   →  (loss > critical)  →  EMERGENCY CUT (×0.5)
NORMAL   →  (loss > target)    →  REDUCE (PID output)
NORMAL   →  (loss < target)    →  INCREASE (slow ramp)
EMERGENCY → (loss < target ×3) →  NORMAL (after 3 stable intervals)
```

---

## Forward Error Correction (FECRecovery)

Optional XOR-based FEC for high-loss environments.

- Every N data packets → 1 parity packet
- Parity = XOR of the N packets
- If one of the N+1 packets is lost, it can be reconstructed
- N is configurable (default: 10 data + 1 parity)
- FEC adds ~10% bandwidth overhead

FEC is disabled by default and enabled when:
- User manually enables in Settings → Network → FEC
- BitratePID detects sustained loss > 3% (auto-enable option)

---

## Socket Buffer Sizing

The kernel receive buffer is set to **8 MB** (`SO_RCVBUF`).

Rationale: 4K@120fps H.265 can burst at ~50 Mbps = 6.25 MB/s.
At 120fps, each frame is ~52 KB. A 8 MB buffer holds ~150 frames worth
of data, enough to absorb a 1.25 second burst without dropping packets.

Without this large buffer, the kernel drops packets during bursts before
our IO thread has a chance to read them. This manifests as sudden frame
drops even when the network is otherwise healthy.

---

## Port Assignments

| Port | Protocol | Description |
|------|----------|-------------|
| 7236 | UDP | AirPlay RTSP data channel |
| 7000 | TCP | AirPlay RTSP control channel |
| 8009 | TCP/UDP | Google Cast channel |
| 5555 | TCP | ADB over TCP/IP |
| mDNS | UDP 5353 | Device discovery (multicast) |

All ports are configurable via Settings → Network → Ports.
Firewall rules are added by the installer automatically.

## Packet Loss Recovery (FEC)

AuraCastPro uses a simple **XOR-based Forward Error Correction** scheme:

- The mobile device sends `k` data packets + 1 parity packet per FEC group
- The parity packet = XOR of all `k` data packets
- If any 1 packet is lost, it can be reconstructed: `lost = parity XOR all_others`
- Groups with 2+ lost packets fall back to keyframe request (RTCP FIR)

At 20 Mbps, each FEC group covers ~16ms of video. This recovers from random
packet loss up to ~6% without visible glitches.

## Adaptive Jitter Buffer

PacketReorderCache uses a dynamic hold window:

```
initial_hold_ms = 50ms      # conservative start
target_hold_ms  = 2 * jitter_estimate_ms
min_hold_ms     = 5ms       # never go below (reorder window)
max_hold_ms     = 200ms     # cap (user-visible lag above this)

Adjustment: every 1 second
  if (late_arrivals_last_sec > 2): hold_ms += 10ms  (slow increase)
  if (late_arrivals_last_sec == 0): hold_ms -= 5ms  (fast decrease)
```

This self-tunes to network conditions: fast Wi-Fi sees ~10ms hold,
congested networks see up to 200ms.

## RTCP Feedback Messages

AuraCastPro sends RTCP feedback to control the mobile encoder:

| Message | Trigger | Effect |
|---------|---------|--------|
| REMB    | Every 100ms | Set max bitrate (PID output) |
| PLI     | Decoder error or > 3 missing frames | Request new keyframe |
| FIR     | Full decoder reset required | Request full intra frame |
| NACK    | Sequence gap detected | Request packet retransmit |

## Connection State Machine

```
DISCONNECTED
    │ mDNS / device connects
    ▼
HANDSHAKING ──── auth failure ──→ DISCONNECTED
    │ auth OK
    ▼
STREAMING ──── timeout / error ──→ RECONNECTING
    │                                    │
    │                              max retries exceeded
    │                                    ▼
    │                              DISCONNECTED
    │ user stops
    ▼
DISCONNECTED
```

Reconnect uses **exponential backoff**: 5s, 10s, 20s, 40s, 60s, 60s…
(capped at 60s, gives up after 12 attempts = ~10 minutes total)

## Protocol Port Map

| Port | Protocol | Purpose |
|------|----------|---------|
| 7000 | TCP | AirPlay RTSP control |
| 7100 | TCP | AirPlay mirroring data |
| 7236 | UDP | AirPlay video/audio stream |
| 8009 | TCP | Google Cast receiver |
| 5555 | TCP | ADB wireless (Android) |
| 5353 | UDP multicast | mDNS / Bonjour discovery |

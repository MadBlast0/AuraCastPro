# AuraCastPro — Low Latency 120fps Specification

## Latency Budget

End-to-end latency target: **30–70ms** (competitive with wired USB mirroring).
The budget is broken down as follows:

```
Mobile device screen capture:    0–8ms   (device-side, outside our control)
H.265/AV1 encoding (on device):  5–15ms  (hardware encoder latency)
Wi-Fi transmission:              2–10ms  (depends on AP/channel)
UDP receive + reorder:           0–50ms  (jitter buffer hold time)
NAL reassembly:                  0–2ms
Hardware decode (MF/DXVA):       2–8ms
GPU render (all passes):         1–4ms
Display vsync wait:              0–8ms   (at 120Hz, max 8.3ms wait)
────────────────────────────────────────
Total (typical Wi-Fi, 120Hz):    10–55ms ✓
Total (busy 2.4GHz Wi-Fi):       20–75ms ✓ (within spec)
```

## 120fps Requirements

Delivering 120fps requires all pipeline stages to complete within **8.3ms/frame**.

| Stage | Time Budget | How We Meet It |
|-------|------------|----------------|
| Receive + Reorder | < 1ms | Large socket buffer + lock-free ring |
| NAL Reassembly | < 0.5ms | Zero-copy span operations |
| Hardware Decode | < 3ms | DXVA2 / D3D12Video (GPU decode) |
| GPU Render | < 2ms | Pre-compiled PSOs, no state changes |
| Present | < 1ms | Tearing-free with DXGI_PRESENT_ALLOW_TEARING |
| **Total** | **< 7.5ms** | **0.8ms headroom** |

## Threading Model

```
Thread 1 (IO):      ReceiverSocket.receiveOnce() tight loop
                    → lock-free push to PacketReorderCache

Thread 2 (Decode):  PacketReorderCache.drain() → NALParser
                    → H265Demuxer → VideoDecoder
                    Runs at video frame rate (60/120fps)

Thread 3 (Render):  FrameTimingQueue.dequeue() → DX12 render passes
                    → SwapChain.Present()
                    Tied to display vsync (VBlankEvent)

Thread 4 (Stats):   BitratePID.update() every 100ms
                    NetworkStatsModel signals to Qt UI

Thread 5+ (Qt):     Qt event loop + QML rendering
                    (separate from video pipeline threads)
```

## Network Configuration Requirements

For 120fps at 4K, the Wi-Fi link must support:

- Bandwidth: ≥ 25 Mbps sustained (H.265 at 4K120)
- Latency: ≤ 10ms RTT to router
- **5 GHz band strongly recommended** — 2.4 GHz rarely achieves < 20ms jitter

For guaranteed performance, use USB wired ADB mirroring (Android only).
Wired connections have < 1ms transport latency.

## Keyframe Recovery

When a keyframe is missed (e.g. after packet loss), the video decoder
produces corrupt frames until the next keyframe arrives. AuraCastPro
handles this by:

1. Detecting decoder errors (`MF_E_NOTACCEPTING` or output glitch)
2. Sending an RTCP FIR (Full Intra Request) to the mobile device
3. Device responds with a keyframe within 1–3 frames
4. Total recovery time: typically < 50ms

## Comparison with Competitor Products

| Product | Typical Latency | Max FPS | Notes |
|---------|----------------|---------|-------|
| AuraCastPro | 30–70ms | 120 | This product |
| Reflector 4 | 80–150ms | 60 | Software decode |
| LonelyScreen | 100–200ms | 30 | No GPU pipeline |
| ApowerMirror | 80–180ms | 60 | Various |
| USB wired (ADB) | 10–30ms | 120 | Wired baseline |

## Adaptive Bitrate Control

AuraCastPro uses a **PID controller** (BitratePID) to continuously tune the
requested bitrate sent back to the mobile device via RTCP feedback.

```
Target bitrate = Kp * error + Ki * ∫error + Kd * d(error)/dt

Where:
  error = (measured_throughput - current_bitrate) / current_bitrate
  Kp    = 0.4  (proportional — fast response to bandwidth changes)
  Ki    = 0.05 (integral     — eliminates steady-state offset)
  Kd    = 0.1  (derivative   — dampens oscillation)
```

The PID output is clamped to [1 Mbps, 50 Mbps] and sent to the device
as an RTCP REMB (Receiver Estimated Maximum Bitrate) message.

NetworkPredictor feeds the PID with an EMA-smoothed (α=0.15) estimate
of available bandwidth, packet loss rate, and jitter, updated every
100ms from incoming RTP packet statistics.

## Frame Pacing (temporal_frame_pacing.hlsl)

At 120fps, even 1ms of GPU jitter causes visible stutter. The
temporal frame pacing compute shader:

1. Runs after decode, before present
2. Computes the ideal present timestamp using a Kalman-filtered 
   arrival time predictor
3. Holds the frame in a GPU-resident buffer if it arrives early
4. Drops duplicate frames if the device sends faster than display rate

This ensures frames are presented exactly on VBlank boundaries,
regardless of network jitter.

## Memory Bandwidth

At 4K120 with NV12 pixel format:
```
Frame size:  3840 × 2160 × 1.5 bytes = 11.9 MB/frame
Frame rate:  120 fps
Raw BW:      120 × 11.9 = 1,428 MB/s
```

This exceeds PCIe Gen 3 x4 bandwidth (~3.5 GB/s total). We keep frames
GPU-resident (D3D12 resources) and never copy to system memory, reducing
effective bandwidth to ~200 MB/s (only CBV/constant buffer updates).

## GPU Pipeline Passes

```
Pass 1: chroma_upsample.hlsl  (Compute)
        NV12 UV plane 1920×1080 → 3840×2160 (Lanczos-2, optional)
        Skipped at 1080p (negligible visual difference)

Pass 2: nv12_to_rgb.hlsl      (Pixel)
        NV12 → RGBA16F (BT.2020 matrix for HDR, BT.709 for SDR)

Pass 3: hdr10_tonemap.hlsl    (Pixel)  [HDR monitors only]
        RGBA16F ST.2084 → display-referred nits
        Skipped on SDR monitors

Pass 4: lanczos8.hlsl         (Pixel)  [optional, 4K→1080p]
        High-quality downscale when window < source resolution

Pass 5: fullscreen_vs.hlsl + present
        Full-screen triangle → swapchain backbuffer → Present()
```

Total GPU time budget: **< 2ms at 4K** on RTX 2060 / RX 5600 XT.

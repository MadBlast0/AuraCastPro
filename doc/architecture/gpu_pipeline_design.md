# AuraCastPro — GPU Rendering Pipeline Design

## Overview

All video rendering is performed on the GPU using DirectX 12.
The CPU never touches pixel data after the decoder hands off the frame.
This keeps CPU usage below 5% even at 4K@120fps.

---

## Frame Lifecycle

```
VideoDecoder output
   │
   │  ID3D12Resource* (NV12 format, GPU memory)
   ▼
┌──────────────────────────────────────────────────────────────┐
│  PASS 1 — Chroma Upsample (optional, 4K quality mode)       │
│  Shader: chroma_upsample.hlsl (compute shader)              │
│  Input:  NV12 UV plane (half res)                           │
│  Output: Full-res UV plane                                   │
│  Cost:   ~0.05ms at 4K on RTX 3070                         │
└──────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────┐
│  PASS 2 — NV12 → RGB Conversion                             │
│  Shader: nv12_to_rgb.hlsl (pixel shader)                    │
│  Input:  Y plane (t0) + UV plane (t1)                       │
│  Output: RGBA8 or RGBA16F (HDR path)                        │
│  Colour space: BT.709 YCbCr studio swing                    │
│  Cost:   ~0.1ms at 4K                                       │
└──────────────────────────────────────────────────────────────┘
   │
   ├─── [SDR path] ────────────────────────────────────────────▶ Pass 4
   │
   └─── [HDR path: device reports HDR10 stream]
   │
   ▼
┌──────────────────────────────────────────────────────────────┐
│  PASS 3 — HDR10 Tone Mapping (HDR path only)                │
│  Shader: hdr10_tonemap.hlsl (pixel shader)                  │
│  Input:  Linear HDR (BT.2020, PQ)                           │
│  Output: SDR RGBA8 (BT.709, gamma 2.2)                      │
│  Algorithm: ACES Filmic tone curve                          │
│  Cost:   ~0.15ms at 4K                                      │
└──────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────┐
│  PASS 4 — Lanczos Scaling (if frame ≠ window size)          │
│  Shader: lanczos8.hlsl (pixel shader)                       │
│  Input:  Any resolution RGB texture                         │
│  Output: Window-size RGBA8                                   │
│  Algorithm: Lanczos-4 (8 taps per axis, 64 total samples)   │
│  Cost:   ~0.2ms at 4K output                               │
└──────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────┐
│  PASS 5 — Frame Pacing (compute shader, every display vsync) │
│  Shader: temporal_frame_pacing.hlsl                         │
│  Input:  Current + previous frame                           │
│  Output: Blended frame for smooth display                   │
│  Purpose: Eliminate micro-stutter when display Hz ≠ video Hz│
│  Cost:   ~0.05ms                                            │
└──────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────┐
│  SWAPCHAIN PRESENT                                          │
│  IDXGISwapChain4::Present(1, 0)  — vsync'd                  │
│  Format: DXGI_FORMAT_R8G8B8A8_UNORM (SDR)                  │
│          DXGI_FORMAT_R10G10B10A2_UNORM (HDR)               │
└──────────────────────────────────────────────────────────────┘
```

---

## Memory Management

### GPU Frame Buffer Pool

Decoded frames are pooled to avoid repeated allocation/deallocation:

- Pool size: 8 frames (configurable)
- Format: NV12 (DXGI_FORMAT_NV12)
- Max resolution: 7680×4320 (8K) — supports the full spec
- Allocation: on init, not per-frame

At 4K NV12: 3840×2160 × 1.5 bytes = ~11.8 MB per frame × 8 = ~94 MB VRAM

This is well within the VRAM budget of any GPU that supports D3D12 (≥2 GB).

### Zero-Copy Design

The decoder outputs directly into the pool frames. The rendering pipeline
reads from the same VRAM resource. No PCIe transfers occur after decode.

Only the final swapchain present involves VRAM→display memory copy,
which is handled internally by the GPU display engine.

---

## Root Signature Layout

```
Root Parameter [0]: CBV (b0) — Per-draw constants
  - float2 TexelSize
  - float  Brightness / Contrast / Exposure / etc.
  - float4x4 Transform (for rotation/flip)

Root Parameter [1]: Descriptor Table — Shader resources
  - SRV t0: Y plane or RGB input
  - SRV t1: UV plane (NV12 path only)

Static Sampler [0]: s0 — Linear Clamp
  - Filter: MIN_MAG_MIP_LINEAR
  - Address: CLAMP (prevents edge artefacts)
```

---

## Performance Targets

| Scenario | GPU Time Budget | Target FPS |
|----------|----------------|------------|
| 1080p60 SDR | 3.5ms/frame | 60 fps |
| 1080p120 SDR | 1.75ms/frame | 120 fps |
| 4K60 SDR | 3.5ms/frame | 60 fps |
| 4K60 HDR | 4.0ms/frame | 60 fps |
| 4K120 SDR | 1.75ms/frame | 120 fps |

All targets assume a mid-range GPU (e.g. RTX 3060 or RX 6600).
High-end GPUs achieve these budgets with headroom to spare.

---

## Shader Compilation

Shaders are compiled at **build time** by `dxc.exe` (DirectX Shader Compiler).
The `.cso` compiled shader objects are deployed alongside the `.exe`.

Shader model: **SM 6.0** minimum.
- SM 6.0: available on all D3D12 GPUs (Maxwell/GCN2 and later)
- SM 6.6: enables additional mesh shader features (future use)

Runtime recompilation is NOT used — `.hlsl` source is not shipped to end users.

## D3D12 Resource Lifecycle

```
Frame N arrives from decoder (NV12, D3D12 texture in DECODE state)
    │
    ▼ Resource barrier: DECODE → SHADER_RESOURCE
    │
    ▼ Pass 1: chroma_upsample (Compute dispatch, optional)
    │         Input:  NV12 UV plane (half-res)
    │         Output: Full-res UV plane (UAV)
    │
    ▼ Pass 2: nv12_to_rgb (PS fullscreen triangle)
    │         Input:  NV12 Y plane + upsampled UV
    │         Output: RGBA16F render target (HDR colour space)
    │
    ▼ Pass 3: hdr10_tonemap (PS, HDR displays only)
    │         Input:  RGBA16F (scene-referred, PQ encoded)
    │         Output: Display-referred nits mapped to swapchain format
    │
    ▼ Pass 4: lanczos8 (PS, downscale only when window < source)
    │         Input:  RGBA16F full resolution
    │         Output: RGBA8 at window resolution
    │
    ▼ Resource barrier: RENDER_TARGET → PRESENT
    │
    ▼ IDXGISwapChain4::Present(1, DXGI_PRESENT_ALLOW_TEARING)
```

## Descriptor Heaps

All shader-visible descriptors are sub-allocated from two heaps
created at startup (DX12DeviceContext):

| Heap | Type | Size | Usage |
|------|------|------|-------|
| `m_srvHeap` | CBV_SRV_UAV | 1024 | Texture SRVs, UAVs, constant buffers |
| `m_rtvHeap` | RTV | 32 | Render target views for each pass |

Heaps are never resized at runtime. The 1024-descriptor limit supports
up to 64 frames in flight × 16 textures/frame with headroom to spare.

## GPU Memory Budget

| Resource | Format | Size (4K) | Count | Total |
|----------|--------|-----------|-------|-------|
| NV12 decode buffers | NV12 | 11.9 MB | 3 | 35.7 MB |
| RGBA16F render targets | RGBA16F | 31.6 MB | 2 | 63.2 MB |
| Swapchain backbuffers | RGBA8 | 7.9 MB | 2 | 15.8 MB |
| Constant buffers | — | 4 KB | 8 | 0.03 MB |
| **Total** | | | | **~115 MB** |

This fits comfortably within the 2 GB VRAM minimum requirement.

## GPU Synchronisation

Double-buffered rendering using `DX12Fence`:

```
Frame N:   Submit command list → Signal fence(N) → Present
Frame N+1: Wait fence(N-1) → Record new command list
```

`waitForGPUIdle()` is called during window resize and device lost events to
ensure all GPU work is finished before releasing resources.

## HDR / SDR Swapchain Selection

`HDRDetection::supportsHDR10()` queries `IDXGIOutput6::GetDesc1()` and
checks `ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020`.

- **HDR display detected:** swapchain format = `DXGI_FORMAT_R16G16B16A16_FLOAT`,
  color space = `DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020`, tonemapping enabled
- **SDR display:** swapchain format = `DXGI_FORMAT_R8G8B8A8_UNORM`,
  color space = `DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709`, no tonemapping

The swapchain is recreated on `WM_DISPLAYCHANGE` in case the user connects
or disconnects an HDR monitor while the app is running.

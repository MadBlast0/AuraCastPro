# Encoder Architecture - Mirroring vs Recording

## Overview

AuraCastPro has TWO separate encoding pipelines:

1. **MIRRORING** (Live Display) - Decoding only, no encoding
2. **RECORDING** (Save to File) - Encoding with hardware acceleration

## 1. MIRRORING Pipeline (Live Display)

### Purpose
Display iPad screen in real-time on PC with minimal latency.

### Flow
```
iPad → AirPlay (H.264/H.265) → VideoDecoder → GPU Texture → Mirror Window
```

### Components
- **VideoDecoder** (`src/engine/VideoDecoder.cpp`)
  - Decodes H.264 or H.265 from iPad
  - Uses Windows Media Foundation (MF)
  - Hardware accelerated (NVIDIA/AMD/Intel)
  - Output: NV12 texture in GPU memory

- **MirrorWindow** (`src/display/MirrorWindow.cpp`)
  - Renders decoded frames to window
  - Uses Direct3D 12 for rendering
  - No encoding involved

### Settings (Video Section)
- **Preferred Codec**: h264 or h265 (what iPad sends)
  - Setting: `video.preferredCodec`
  - Used by: VideoDecoder initialization
  - Options: "h264", "h265"

- **Hardware Encoder**: MISLEADING NAME!
  - Setting: `video.hardwareEncoder`
  - Actually controls: RENDERING backend (not encoding!)
  - Options: "Auto", "Direct3D 12", "Direct3D 11", "OpenGL", "Vulkan", "Software (CPU)"
  - Used by: MirrorWindow rendering (NOT CURRENTLY IMPLEMENTED)
  - Should be renamed to: "Rendering Backend"

### Key Point
**Mirroring does NOT use encoding** - it only DECODES video from iPad and displays it.

---

## 2. RECORDING Pipeline (Save to File)

### Purpose
Encode and save mirrored video to file with user-selected codec and quality.

### Flow
```
GPU Texture → HardwareEncoder → Encoded Packets → StreamRecorder → File
```

### Components
- **HardwareEncoder** (`src/streaming/HardwareEncoder.cpp`)
  - Encodes frames using FFmpeg
  - Supports multiple hardware encoders:
    - NVIDIA NVENC (h264_nvenc, hevc_nvenc)
    - AMD AMF (h264_amf)
    - Intel QuickSync (h264_qsv)
    - Software (libx264, libx265)

- **StreamRecorder** (`src/integration/StreamRecorder.cpp`)
  - Muxes encoded video + audio into file
  - Supports formats: MP4, MKV, FLV, MOV

### Settings (Recording Section)
- **Video Encoder**: Actual encoding hardware
  - Setting: `recording.videoEncoder`
  - Options:
    - "NVIDIA NVENC H.264" → h264_nvenc
    - "NVIDIA NVENC H.265" → hevc_nvenc
    - "x264" → libx264 (software)
    - "x265" → libx265 (software)
    - "AMD AMF H.264" → h264_amf
    - "Intel QSV H.264" → h264_qsv

- **Encoder Preset**: Quality vs Speed
  - Setting: `recording.encoderPreset`
  - Options: P1 (Fastest) to P7 (Slowest/Best Quality)

- **Rate Control**: Bitrate mode
  - Setting: `recording.rateControl`
  - Options: CBR, VBR, CQP

- **Bitrate**: Target bitrate in kbps
  - Setting: `recording.bitrate`

---

## Current Issues

### Issue 1: "Hardware Encoder" is Misleading
**Problem**: The "Hardware Encoder" dropdown in Video settings suggests it controls encoding, but it actually controls the RENDERING backend.

**Impact**: Users think they're selecting encoding hardware, but they're not.

**Solution**: 
- Rename to "Rendering Backend" in UI
- Or remove it entirely (Direct3D 12 is the only working option)

### Issue 2: Mirroring Codec Not Used
**Problem**: The `preferredCodec` setting (h264/h265) is saved but VideoDecoder was hardcoded to H.265.

**Status**: FIXED - VideoDecoder now reads from settings

### Issue 3: Recording Encoder Not Wired
**Problem**: The `videoEncoder` setting is saved but may not be properly used by HardwareEncoder.

**Status**: NEEDS VERIFICATION

---

## What Needs to be Fixed

### 1. Ensure VideoDecoder Uses Codec Setting ✅
**Status**: DONE
- VideoDecoder now reads `preferredCodec` from settings
- Defaults to H.264 for better compatibility
- Supports H.264, H.265, and AV1

### 2. Ensure HardwareEncoder Uses Encoder Setting ⏳
**Status**: NEEDS IMPLEMENTATION
- HardwareEncoder should read `videoEncoder` from settings
- Map UI strings to FFmpeg codec names:
  - "NVIDIA NVENC H.264" → "h264_nvenc"
  - "NVIDIA NVENC H.265" → "hevc_nvenc"
  - "x264" → "libx264"
  - "x265" → "libx265"
  - "AMD AMF H.264" → "h264_amf"
  - "Intel QSV H.264" → "h264_qsv"

### 3. Rename "Hardware Encoder" to "Rendering Backend" ⏳
**Status**: NEEDS UI UPDATE
- Update SettingsPage.qml
- Update SettingsModel property name
- Or remove entirely if not used

### 4. Wire Recording to Use Selected Encoder ⏳
**Status**: NEEDS VERIFICATION
- StreamRecorder should use HardwareEncoder
- HardwareEncoder should respect settings
- Test all encoder options work

---

## Testing Checklist

### Mirroring (Decoding)
- [ ] H.264 from iPad displays correctly
- [ ] H.265 from iPad displays correctly (if codec installed)
- [ ] Settings change takes effect after restart
- [ ] Hardware decode works (GPU)
- [ ] Software fallback works (CPU)

### Recording (Encoding)
- [ ] NVIDIA NVENC H.264 works
- [ ] NVIDIA NVENC H.265 works
- [ ] x264 software works
- [ ] x265 software works
- [ ] AMD AMF H.264 works (on AMD GPU)
- [ ] Intel QSV H.264 works (on Intel GPU)
- [ ] Settings change takes effect immediately
- [ ] Recorded file plays correctly

### Integration
- [ ] Can mirror H.264 and record with H.265
- [ ] Can mirror H.265 and record with H.264
- [ ] Mirroring and recording are independent
- [ ] Changing mirroring codec doesn't affect recording
- [ ] Changing recording encoder doesn't affect mirroring

---

## Summary

**Mirroring** = DECODING (H.264/H.265 from iPad) → Display
**Recording** = ENCODING (NVENC/AMF/QSV/x264/x265) → File

These are SEPARATE pipelines with SEPARATE settings.

The "Hardware Encoder" in Video settings is MISLEADING - it's actually the rendering backend, not encoding.

The actual video encoder for recording is in the Recording section.

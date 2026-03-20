# Mirroring vs Recording Settings - Clarification

## Overview
Mirroring and Recording settings are now properly separated and serve different purposes.

---

## Mirroring Settings (Live Display)

### Purpose
Controls what the iPad/iPhone sees and sends during live screen mirroring.

### Location
Settings → Mirroring tab

### Key Settings

#### Resolution
- **Default**: Native (Auto) - Advertises 1080p as safe default
- **Options**: Native, 4K, 2K, 1080p, 720p, 360p
- **What it does**: Tells the device what resolution to send
- **Note**: Device may send lower resolution if it doesn't support the requested one

#### Frame Rate
- **Default**: Auto (60 FPS)
- **Options**: Auto, 120, 90, 60, 30, 24 FPS
- **What it does**: Tells the device what frame rate to send

#### Bitrate
- **Default**: 20 Mbps
- **Range**: 2-200 Mbps
- **What it does**: Controls the quality of the live stream

#### Color Space
- **Default**: sRGB
- **Status**: Stored but not yet applied (future feature)

#### Hardware Encoder
- **Default**: Auto
- **Status**: Auto-detection used (NVENC > AMF > QuickSync > Software)

---

## Recording Settings (Saved File)

### Purpose
Controls how the received stream is saved to a file.

### Location
Settings → Recording tab

### Key Settings

#### Recording Path
- **Default**: %USERPROFILE%\Videos\AuraCastPro
- **What it does**: Where recordings are saved
- **UI**: Folder browser button

#### Resolution (Rescale)
- **Default**: Source (use whatever the device sends)
- **Options**: Source, 1920x1080, 1280x720, 2560x1440, 3840x2160
- **What it does**: Rescales the recording to a different resolution
- **Note**: "Source" means no rescaling - record at the resolution the device sends

#### Video Encoder
- **Default**: Stream Copy (No Re-encode)
- **Options**: Stream Copy, NVENC H.264/H.265, x264, x265, AMD AMF, Intel QSV
- **What it does**: 
  - Stream Copy = Zero CPU, saves exactly what device sends
  - Others = Re-encode to different codec (uses CPU/GPU)

#### Recording Format
- **Default**: MP4
- **Options**: MP4, MKV, FLV, MOV, AVI
- **What it does**: Container format for the saved file

#### All Other Settings
- Rate control, encoder preset, profile, tuning, multipass, keyframe interval, etc.
- Only apply when using a re-encoding video encoder (not Stream Copy)

---

## How They Work Together

### Example 1: Default Settings
```
Mirroring:
  - Resolution: Native (Auto) → Advertises 1080p
  - FPS: Auto → Advertises 60 FPS
  - Bitrate: 20 Mbps

Recording:
  - Rescale Resolution: Source → Records at 1080p (what device sends)
  - Video Encoder: Stream Copy → No re-encoding
  - Format: MP4

Result: iPad sends 1080p@60fps → Recorded as 1080p@60fps MP4 (zero CPU)
```

### Example 2: High Quality Mirroring, Lower Quality Recording
```
Mirroring:
  - Resolution: 4K → Advertises 4K
  - FPS: 60 FPS
  - Bitrate: 50 Mbps

Recording:
  - Rescale Resolution: 1920x1080 → Downscale to 1080p
  - Video Encoder: NVENC H.264 → Re-encode with GPU
  - Format: MP4

Result: iPad sends 4K@60fps → Displayed at 4K → Recorded as 1080p@60fps MP4
```

### Example 3: Lower Quality Mirroring, Source Recording
```
Mirroring:
  - Resolution: 720p → Advertises 720p
  - FPS: 30 FPS
  - Bitrate: 10 Mbps

Recording:
  - Rescale Resolution: Source → Records at 720p
  - Video Encoder: Stream Copy
  - Format: MP4

Result: iPad sends 720p@30fps → Displayed at 720p → Recorded as 720p@30fps MP4
```

---

## Important Notes

### Mirroring Resolution = What Device Sends
- The mirroring resolution setting tells the device what to send
- The device may send lower if it doesn't support the requested resolution
- "Native (Auto)" is safest - works with all devices

### Recording Resolution = What Gets Saved
- "Source" means record whatever the device sends (no rescaling)
- Other options rescale the recording to that resolution
- Rescaling requires re-encoding (can't use Stream Copy)

### Stream Copy vs Re-encoding
- **Stream Copy**: Zero CPU, saves exactly what device sends, fastest
- **Re-encoding**: Uses CPU/GPU, allows format conversion, rescaling, quality adjustment

### When to Use What

**Use Stream Copy when:**
- You want zero CPU usage
- You want to record exactly what the device sends
- You don't need to change resolution or codec

**Use Re-encoding when:**
- You want to rescale to a different resolution
- You want to convert codec (H.265 → H.264)
- You want to adjust quality/bitrate
- You want to apply filters

---

## Default Behavior (After This Fix)

### Mirroring
- Resolution: **Native (Auto)** - Advertises 1080p as safe default
- FPS: **Auto** - Advertises 60 FPS
- Bitrate: **20 Mbps**

### Recording
- Rescale Resolution: **Source** - Records at whatever device sends
- Video Encoder: **Stream Copy** - No re-encoding
- Format: **MP4**

This gives the best compatibility and performance out of the box. Users can adjust both independently based on their needs.

---

## Changes Made

### Files Modified
1. **src/manager/SettingsModel.cpp**
   - Changed default `m_maxResolutionIndex` from 1 (4K) to 0 (Native)
   - Changed default `m_fpsCapIndex` from 1 (120fps) to 0 (Auto/60fps)

2. **src/manager/SettingsModel.h**
   - Changed default `m_rescaleResolution` from "1920x1080" to "Source"

3. **src/main.cpp**
   - Updated case 0 to use 1080p (was 8K) for Native/Auto mode
   - This advertises a safe default that all devices support

4. **src/manager/HubWindow.cpp**
   - Removed dependency on `maxResolutionIndex` for recording
   - Recording now uses 1080p as initial default (updated when stream starts)
   - User can override via `rescaleResolution` setting

### Build Status
✅ Compiles successfully with no errors
✅ All settings properly separated
✅ Default behavior is now safe and compatible

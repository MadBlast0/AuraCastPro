# White Screen Fix - Root Cause Found

## Problem
iPad connects successfully, mirror window opens, but shows white screen. Connection stays alive (POST /feedback keeps happening), but no video is displayed.

## Root Cause
**H.265/HEVC decoder is not available on this system.**

From logs:
```
[error] [main] VideoDecoder init failed: VideoDecoder: No H.265/HEVC decoder available (hardware or software). Install codec pack or update GPU drivers.
```

The iPad was sending H.265 video (type=110), but the VideoDecoder failed to initialize because:
- Windows doesn't include HEVC codec by default (costs $0.99 in Microsoft Store)
- No hardware H.265 decoder available
- No software H.265 decoder fallback configured

## Secondary Issue
MirroringListener is receiving video data on TCP port 7010, but showing:
```
[warning] [MirroringListener] Resyncing stream (invalid frames: 78000, buffered: 12945 bytes)
```

This means 78,000 frames couldn't be parsed - likely because the framing detection is failing or the data format doesn't match expectations.

## Solution Applied
Changed codec preference from H.265 to H.264:
```powershell
$settings.video.preferredCodec = "h264"
```

H.264/AVC decoder is available on all Windows systems and works reliably.

## Why White Screen (Not Black)
The MirrorWindow clears to BLACK (`clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }`), but only draws if `m_currentTexture` is set. Since no frames are being decoded:
1. `m_currentTexture` remains NULL
2. The draw call is skipped
3. The window shows the default Win32 background (COLOR_WINDOWFRAME)
4. On some systems this appears white/light gray

## Test Instructions

1. **Kill any running AuraCastPro**
   ```powershell
   Get-Process AuraCastPro -ErrorAction SilentlyContinue | Stop-Process -Force
   ```

2. **Start AuraCastPro**
   - The app will now initialize H.264 decoder instead of H.265
   - Should see: `[info] [VideoDecoder] Initialising H.264/AVC hardware decoder...`
   - Should see: `[info] [VideoDecoder] H.264/AVC decoder ready.`

3. **Connect iPad**
   - iPad → Control Center → Screen Mirroring → AuraCastPro
   - iPad will negotiate H.264 codec
   - Video should now decode and display

4. **Check logs for success**
   ```powershell
   Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" -Tail 50 | Select-String "H.264|Decoded frame|VideoDecoder"
   ```

   Should see:
   ```
   [info] [VideoDecoder] H.264/AVC decoder ready.
   [info] [VideoDecoder] Decoded frame #1: 1920x1080 pts=0 key=false
   [info] [VideoDecoder] Decoded frame #30: 1920x1080 pts=500000 key=false
   ```

## If Still White Screen

### Check 1: Decoder Initialized
```powershell
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" | Select-String "VideoDecoder.*ready|VideoDecoder.*failed"
```

Should see: `H.264/AVC decoder ready`
Should NOT see: `VideoDecoder init failed`

### Check 2: Frames Being Received
```powershell
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" | Select-String "MirroringListener.*processed|invalid frames"
```

Should see: `Interleaved packets processed: 120`
Should NOT see high invalid frame counts

### Check 3: Frames Being Decoded
```powershell
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" | Select-String "Decoded frame"
```

Should see multiple "Decoded frame" entries

### Check 4: Codec Negotiation
```powershell
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" | Select-String "Stream active.*Codec"
```

Should see: `Stream active. Codec: H264`
Should NOT see: `Codec: H265`

## Alternative Solutions

### Option 1: Install HEVC Codec (Costs Money)
1. Open Microsoft Store
2. Search for "HEVC Video Extensions"
3. Purchase and install ($0.99)
4. Change settings back to H.265 if desired

### Option 2: Use Software H.265 Decoder
- Requires FFmpeg with libx265
- More CPU intensive
- Not currently implemented in AuraCastPro

### Option 3: Force H.264 on All Devices
- Keep `preferredCodec = "h264"` in settings
- Most compatible option
- Works on all Windows systems
- Slightly lower quality than H.265 at same bitrate

## Technical Details

### Codec Negotiation Flow
1. iPad sends SETUP request
2. AuraCastPro checks `preferredCodec` setting
3. Responds with codec preference in SETUP response
4. iPad sends video in requested codec
5. VideoDecoder must support that codec

### Why H.265 Failed
- Windows Media Foundation (MF) requires HEVC codec pack
- Hardware decoders (NVDEC, QuickSync, AMF) may not support H.265
- Software fallback (FFmpeg) not configured

### Why H.264 Works
- H.264/AVC is included in Windows by default
- All GPUs support H.264 hardware decode
- FFmpeg software fallback available
- Universal compatibility

## Success Criteria
✓ VideoDecoder initializes successfully (H.264)
✓ iPad negotiates H.264 codec
✓ MirroringListener receives and parses frames
✓ VideoDecoder decodes frames
✓ Mirror window displays iPad screen
✓ No white screen
✓ Connection stays active

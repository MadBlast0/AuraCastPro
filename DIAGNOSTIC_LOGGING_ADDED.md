# Diagnostic Logging Added - Media Pipeline Tracing

## Changes Made

### 1. Enhanced MirroringListener Callback (src/main.cpp)
Added comprehensive logging to trace the TCP video stream:
- Packet reception counter (rx)
- Decryption counter (decrypt) - placeholder for future
- NAL emission counter (nal)
- Hex dump of first 32 bytes of first 3 packets
- Better error tracking

### 2. NALParser Logging (src/engine/NALParser.cpp)
Added detailed packet and NAL unit logging:
- First 5 packets: full hex dump of first 16 bytes
- Packet size, type, and RTP header analysis
- NAL emission logging (first 10, then every 30th)
- NAL type, size, keyframe status, parameter set status

### 3. VideoDecoder Logging (src/engine/VideoDecoder.cpp)
Added submitNAL and processOutput logging:
- NAL submission counter and details
- ProcessInput error logging with context
- Decoded frame counter and details
- Frame dimensions, PTS, keyframe status

## What to Look For in Logs

### Successful Pipeline Flow
```
[info] [main] TCP video: rx=120 decrypt=0 nal=120 (latest: hdr=0 payload=1234)
[info] [main] TCP packet #1 first 32 bytes: 80 60 00 01 ...
[info] [NALParser] Packet #1: size=1234 type=49 first16: 62 01 ...
[info] [NALParser] Emitted NAL #1: type=33 size=45 key=false paramSet=true
[info] [VideoDecoder] submitNAL #1: size=45 key=false pts=0
[info] [VideoDecoder] Decoded frame #1: 1920x1080 pts=0 key=false
```

### Problem Indicators

**No TCP packets arriving:**
```
[info] [main] TCP video: rx=0 decrypt=0 nal=0
```
→ iPad not sending to TCP port 7010, or MirroringListener not accepting connection

**Packets arriving but NALParser not receiving:**
```
[info] [main] TCP video: rx=120 decrypt=0 nal=0
```
→ Callback logic not feeding NALParser (check framing detection)

**NALParser receiving but not emitting:**
```
[info] [NALParser] Packet #1: size=1234 type=255 first16: xx xx ...
[warn] [NALParser] STAP-A: NAL size ... overflows packet
```
→ Encrypted data or wrong framing (need decryption)

**NALs emitted but decoder not receiving:**
```
[info] [NALParser] Emitted NAL #1: type=33 size=45
(no VideoDecoder submitNAL logs)
```
→ NALParser callback not wired to decoder

**Decoder receiving but not decoding:**
```
[info] [VideoDecoder] submitNAL #30: size=1234 key=true pts=0
[warn] [VideoDecoder] ProcessInput failed: C00D36B4
(no "Decoded frame" logs)
```
→ Invalid NAL data, missing parameter sets, or codec mismatch

## Encryption Detection

Check the hex dump of first packet:
- **Encrypted**: Random-looking bytes, no recognizable patterns
- **Unencrypted RTP**: Starts with `80` or `90` (RTP version 2)
- **Unencrypted Annex-B**: Starts with `00 00 00 01` (start code)

Example encrypted:
```
TCP packet #1 first 32 bytes: 3f a2 8b 12 9c 44 f1 ...
```

Example unencrypted RTP:
```
TCP packet #1 first 32 bytes: 80 60 00 01 00 00 00 00 ...
```

## Next Steps

### If Encrypted (Most Likely)
1. Add decryption using AirPlay session AES key
2. Wire AirPlay2Host session context to MirroringListener
3. Decrypt payload before feeding to NALParser

### If Unencrypted But Wrong Framing
1. Check if payload is RTSP interleaved (`$` prefix)
2. Check if payload is legacy 128-byte header format
3. Adjust framing detection logic

### If NALParser Issues
1. Check if H.264 instead of H.265 (different NAL types)
2. Verify RTP payload type matches codec
3. Check for fragmentation issues (FU-A)

### If Decoder Issues
1. Verify parameter sets (VPS/SPS/PPS) are sent first
2. Check codec negotiation in SETUP response
3. Try software decoder fallback

## Testing Instructions

1. Kill any running AuraCastPro processes
2. Start AuraCastPro (may need to disable Application Control temporarily)
3. Connect iPad via Screen Mirroring
4. Wait for mirror window to open
5. Check logs:
   ```powershell
   Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" -Tail 200 | Select-String "TCP video|NALParser|VideoDecoder|Decoded frame"
   ```

## Application Control Issue

The newly built exe is being blocked by Windows Application Control policy. To test:

**Option 1: Disable temporarily**
```powershell
# Run as Administrator
Set-MpPreference -DisableRealtimeMonitoring $true
# Test the app
# Re-enable
Set-MpPreference -DisableRealtimeMonitoring $false
```

**Option 2: Add exclusion**
```powershell
# Run as Administrator
Add-MpPreference -ExclusionPath "C:\Users\MadBlast\Documents\GitHub\AuraCastPro\build\Release"
```

**Option 3: Sign the executable**
Use the existing `sign_executable.ps1` script

## Expected Outcome

With diagnostic logging, we'll be able to pinpoint exactly where the pipeline breaks:
1. TCP reception
2. Framing detection
3. Decryption (if needed)
4. NAL parsing
5. Decoder submission
6. Frame output

The logs will show which stage is failing and guide the fix.

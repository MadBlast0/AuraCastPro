# White Screen Issue - Comprehensive Debug Summary

## Current Status
iPad connects successfully, mirror window opens, but shows WHITE SCREEN.

## What We've Tried (Chronologically)

### 1. ✅ Port Configuration (FIXED)
- Changed AirPlay control port 7100 → 7000
- Fixed SETUP response ports (timing, event, data)
- iPad completes full handshake (PHASES 1-6)

### 2. ✅ Codec Selection (FIXED)
- Forced H.264 only (disabled H.265 to avoid HEVC codec requirement)
- Updated mDNS features: `0x5A7FFFF7` → `0x5A7FFEE7` (bit 27 cleared)
- iPad now sends H.264 instead of H.265

### 3. ✅ NAL Parser (FIXED)
- Fixed H.264 NAL type parsing (was using H.265 logic)
- Now correctly parses H.264 NAL types (1, 5, 7, 8, 24, 28)

### 4. ⚠️ Video Decryption (ATTEMPTED)
- Added AES-128-CTR key derivation from pair-verify shared secret
- Implemented decryption in MirroringListener callback
- **Result**: Decryption made things WORSE (turned valid data into garbage)
- **Conclusion**: Video stream is NOT encrypted (disabled decryption)

### 5. ⚠️ AVCC Format Conversion (IN PROGRESS)
- Discovered iPad sends AVCC format (length-prefixed), not Annex-B
- Added AVCC → Annex-B conversion
- Extracts SPS/PPS from avcC decoder config
- **Status**: Conversion code added but not fully working

## Current Problem

**Logs show**:
```
[13:47:47.000] Parsed avcC decoder config (SPS/PPS)
[13:47:56.900] Legacy frames processed: 120 (latest 8952 bytes)
[13:47:48.603] Resyncing stream (invalid frames: 2000)
```

**Analysis**:
1. ✅ avcC config is parsed (SPS/PPS extracted)
2. ❌ Subsequent AVCC packets NOT being converted
3. ❌ VideoDecoder receives NO frames (no "Decoded" messages)
4. ❌ MirroringListener keeps resyncing (invalid frame format)

## Root Cause Hypothesis

The video stream format is **NOT** what we expect:
- First packet: AVCC decoder config (avcC atom)
- Subsequent packets: Unknown format (not AVCC, not Annex-B, not RTP)

The MirroringListener is trying to parse as "legacy frames" but failing.

## What Xindawn Does Differently

Xindawn uses a **closed-source SDK** (`libairplay.dll`) that:
1. Handles all AirPlay protocol internally
2. Provides callbacks with **already-decoded video frames**
3. They just display frames using VLC/SDL

We're implementing the ENTIRE protocol from scratch, which is why we're hitting these low-level format issues.

## Next Steps to Try

### Option A: Fix AVCC Conversion
1. Add detailed logging to see what format subsequent packets are
2. Check if packets are interleaved format vs legacy format
3. Verify AVCC length field parsing (might be 2-byte, not 4-byte)

### Option B: Bypass NALParser Entirely
1. Feed raw packets directly to VideoDecoder
2. Let Windows Media Foundation handle format detection
3. Remove all our custom parsing logic

### Option C: Study MirroringListener Format
1. The "Legacy frames" and "Interleaved packets" suggest two different formats
2. Check MirroringListener.cpp to understand expected format
3. Packets might have custom headers we're not parsing

### Option D: Use Xindawn's Approach
1. Consider using their SDK instead of reimplementing
2. Or study their packet captures to understand format
3. Check if they have documentation on packet format

## Key Files to Check

1. `src/engine/MirroringListener.cpp` - Handles TCP video stream, has "legacy" and "interleaved" parsing
2. `src/main.cpp` lines 1014-1100 - Video data callback with AVCC conversion
3. `src/engine/VideoDecoder.cpp` - H.264 decoder (should show "Decoded" messages)
4. `src/engine/NALParser.cpp` - NAL unit parser (might not be needed)

## Diagnostic Commands

```powershell
# Copy latest log
Copy-Item "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" -Destination ".\debug.log"

# Check if decoder is working
Get-Content ".\debug.log" | Select-String "VideoDecoder.*Decoded|State reset.*Decoded"

# Check packet format
Get-Content ".\debug.log" | Select-String "Parsed avcC|AVCC: Converted|Legacy frames|Interleaved"

# Check NAL types
Get-Content ".\debug.log" | Select-String "NAL.*type=" | Select-Object -Last 20
```

## Critical Question

**Is the VideoDecoder even capable of decoding?**
- Logs show: "No hardware H.264/AVC decoder found, falling back to software decoder"
- But we never see "Decoded: X frames" messages
- The software decoder might not be working at all!

Check `src/engine/VideoDecoder.cpp` - the software decoder fallback might be broken.

# Connection Timeout Fix - Summary

## Changes Made

I've disabled the connection timeout that was causing disconnections after 6 seconds.

### Files Modified:

1. **src/engine/ReconnectManager.h**
   - Changed `silenceTimeoutSec` from `3` to `0` (disabled)
   - Comment updated to indicate `0 = disabled, no timeout`

2. **src/engine/ReconnectManager.cpp**
   - Added check: `if (connected && m_cfg.silenceTimeoutSec > 0 && ...)`
   - Timeout logic now only runs if `silenceTimeoutSec > 0`
   - When set to `0`, the connection will never timeout due to lack of packets

## What This Fixes

**Before:** Connection would disconnect after 3-6 seconds if no video packets were received
**After:** Connection stays active indefinitely until manually disconnected

## Why It Was Disconnecting

The logs showed:
```
[2026-03-19 20:20:38.698] [warning] ReconnectManager: No packets for 5907ms -- triggering reconnect
[2026-03-19 20:20:39.699] [info] [VideoDecoder] State reset. Decoded so far: 0 frames.
```

The ReconnectManager was timing out because:
1. UDP port 7010 was blocked by firewall (now fixed)
2. No video packets were being received
3. After 3 seconds of silence, it triggered a disconnect

## Current Status

### ✅ Fixed:
- TCP 7000 firewall rule (AirPlay control)
- UDP 7010 firewall rule (video data)
- UDP 5353 firewall rule (mDNS)
- Connection timeout disabled

### ⚠️ Needs Rebuild:
The code changes are complete, but the app needs to be rebuilt for them to take effect.

## To Rebuild:

Since the build folder was accidentally deleted, you'll need to:

1. Restore OpenSSL paths (or reinstall vcpkg dependencies)
2. Run: `cmake -B build -G "Visual Studio 17 2022" -A x64`
3. Run: `cmake --build build --config Release`

OR

If you have a backup of the build folder, restore it and just rebuild:
```powershell
cmake --build build --config Release --target AuraCastPro
```

## Expected Behavior After Rebuild:

1. iPad connects successfully
2. Mirror window opens
3. Connection stays active indefinitely
4. Video frames are received on UDP 7010
5. Video appears in mirror window
6. No automatic disconnection

## Firewall Rules Added:

```
✅ TCP 7000 - AirPlay control (RTSP handshake)
✅ UDP 7010 - Video data (RTP packets)  
✅ TCP 7001 - Timing sync
✅ UDP 5353 - mDNS discovery
```

All firewall rules are in place and ready.

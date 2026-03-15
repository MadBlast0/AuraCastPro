# 🚨 CRITICAL: Why iPad Can't See AuraCastPro Anymore

## The Problem

After analyzing all 5 reference implementations, I found the ROOT CAUSE:

**We're advertising the wrong port for _raop._tcp service!**

## Current Configuration (WRONG)

```cpp
// src/discovery/MDNSService.cpp:372-373
services.push_back({displayName, "_airplay._tcp", ..., 7000, ...});           // ✅ OK
services.push_back({mac + "@" + displayName, "_raop._tcp", ..., 7000, ...});  // ❌ WRONG!
```

**Problem:** iOS devices look for _raop._tcp on port 5000, not 7000!

## What Working Implementations Do

### AirPlay-master (C - PROVEN WORKING)
```c
// main.c:98-99
opt->port_raop = 5000;      // RAOP RTSP control
opt->port_airplay = 7000;   // AirPlay data

// dnssd.c
dnssd_register_raop(dnssd, name, 5000, ...);     // _raop._tcp on 5000 ✅
dnssd_register_airplay(dnssd, name, 7000, ...);  // _airplay._tcp on 7000 ✅
```

### Airplay2OnWindows (C# - MODERN)
```csharp
// AirPlayReceiver.cs:37-38
_airTunesPort = 5000;  // RAOP
_airPlayPort = 7000;   // AirPlay

// mDNS registration
var airTunes = new ServiceProfile(..., AirTunesType, _airTunesPort);  // 5000 ✅
var airPlay = new ServiceProfile(..., AirPlayType, _airPlayPort);     // 7000 ✅
```

### airplayreceiver (C# - CROSS-PLATFORM)
```csharp
_airTunesPort = 5000;  // ✅
_airPlayPort = 7000;   // ✅
```

## Why This Matters

**iOS Connection Flow:**
```
1. iPad scans network for _raop._tcp services
2. Finds "AuraCastPro" advertising port 7000
3. Tries to connect to port 7000
4. But iOS EXPECTS _raop._tcp on port 5000!
5. Connection fails → Service disappears from list
```

**When we advertise on the WRONG port:**
- iOS can't find the service properly
- Service may appear briefly then disappear
- Connection attempts fail silently

## The Fix

We need to change TWO things:

### 1. Change mDNS Port Advertisement
```cpp
// src/discovery/MDNSService.cpp:372-373
services.push_back({displayName, "_airplay._tcp", ..., 7000, ...});           // Keep 7000
services.push_back({mac + "@" + displayName, "_raop._tcp", ..., 5000, ...});  // Change to 5000!
```

### 2. Change RTSP Listener Port
```cpp
// src/engine/AirPlay2Host.cpp:59
constexpr uint16_t AIRPLAY_CONTROL_PORT = 5000;  // Change from 7000 to 5000
```

## Port Roles Explained

| Port | Service | Purpose | Protocol |
|------|---------|---------|----------|
| **5000** | _raop._tcp | RTSP control (all requests: /info, OPTIONS, SETUP, etc.) | TCP |
| **7000** | _airplay._tcp | Video/Audio data streaming | TCP/UDP |
| 7001 | - | Timing sync | UDP |
| 7002 | - | Timing sync | UDP |
| 7010 | - | Video RTP data | UDP |
| 7011 | - | Audio RTP data | UDP |

**Key Point:** The RTSP control channel (where /info, pair-setup, pair-verify happen) uses port 5000, NOT 7000!

## Why We Got Confused

Looking at the code history:
1. We saw "AirPlay port 7000" and thought that's the main port
2. But 7000 is for DATA streaming, not RTSP control
3. RTSP control (the handshake) happens on port 5000
4. This is why ALL working implementations use 5000 for _raop._tcp

## Evidence from Reference Implementations

| Implementation | _raop._tcp Port | _airplay._tcp Port | Works? |
|---------------|-----------------|-------------------|--------|
| AirPlay-master | **5000** ✅ | 7000 | ✅ YES |
| AirPlayServer | 5001 | 7001 | ✅ YES (custom) |
| Airplay2OnWindows | **5000** ✅ | 7000 | ✅ YES |
| airplayreceiver | **5000** ✅ | 7000 | ✅ YES |
| WindowPlay | 7000 ❌ | 7000 | ❌ NO (incomplete) |
| **Our Current** | **7000** ❌ | 7000 | ❌ NO |

## What Happens After Fix

**Before Fix:**
```
iPad: "Looking for _raop._tcp on port 5000..."
Our App: "I'm advertising _raop._tcp on port 7000!"
iPad: "Not found. Service disappeared."
```

**After Fix:**
```
iPad: "Looking for _raop._tcp on port 5000..."
Our App: "I'm advertising _raop._tcp on port 5000!"
iPad: "Found it! Connecting..."
iPad: "GET /info RTSP/1.0"
Our App: "Here's my /info response with all fields!"
iPad: "Perfect! Let's pair..."
```

## Firewall Impact

We'll need to update firewall rules:
```powershell
# Remove old rule for port 7000 (if it was for RTSP)
# Add new rule for port 5000
New-NetFirewallRule -DisplayName "AuraCastPro AirPlay RTSP" `
    -Direction Inbound -Protocol TCP -LocalPort 5000 -Action Allow
```

## Testing Plan

1. **Stop current app**
2. **Make the changes** (ports 5000 for RTSP, 7000 for data)
3. **Update firewall rules**
4. **Rebuild app**
5. **Start app**
6. **Check logs**: Should see "Listening on TCP 0.0.0.0:5000"
7. **Test from iPad**: Should see "AuraCastPro" in Screen Mirroring list
8. **Monitor logs**: Should see "GET /info" request

## Recommendation

**BEFORE BUILDING:**
1. Let's discuss if we want to use standard port 5000
2. Or use custom ports like AirPlayServer (5001/7001)
3. Standard ports (5000/7000) are recommended for compatibility

**AFTER AGREEMENT:**
1. Make the port changes
2. Update firewall scripts
3. Rebuild and test

## Why AirPlay-master is Our Best Reference

After comparing all 5 implementations:

🥇 **AirPlay-master** wins because:
- ✅ Native C/C++ (matches our codebase)
- ✅ Uses standard ports (5000/7000)
- ✅ Proven to work with iOS 9.3.2 - 10.2.1
- ✅ Complete protocol implementation
- ✅ Has working DLL and examples
- ✅ Includes packet captures for debugging

See `AIRPLAY_IMPLEMENTATIONS_COMPARISON.md` for full analysis.


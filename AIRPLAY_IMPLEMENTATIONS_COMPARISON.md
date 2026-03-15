# 📊 AirPlay Reference Implementations - Complete Comparison

## Executive Summary

After analyzing all 5 implementations, here's the verdict:

🥇 **BEST FOR US: AirPlay-master (C/C++)**
- Native C implementation (matches our codebase)
- Proven to work with iOS 9.3.2 to iOS 10.2.1
- Uses standard ports (5000 RAOP, 7000 AirPlay)
- Complete protocol implementation
- Has working examples and DLL

🥈 **RUNNER-UP: AirPlayServer-master (C++/Windows)**
- Modern C++ implementation
- Active development
- Windows-specific optimizations
- Good documentation

## Detailed Comparison Table

| Feature | AirPlay-master | AirPlayServer-master | Airplay2OnWindows | airplayreceiver | WindowPlay |
|---------|---------------|---------------------|-------------------|-----------------|------------|
| **Language** | C/C++ | C++ | C# (.NET 8) | C# (.NET Core) | C# |
| **Platform** | Windows | Windows | Windows | Cross-platform | Windows |
| **License** | Open Source | Open Source | Open Source | Open Source | Open Source |
| **Last Updated** | ~2017 | 2024 (Active) | 2024 (Active) | 2021 | 2023 |
| **iOS Support** | 9.3.2 - 10.2.1 | Modern iOS | Modern iOS | iOS 14+ | Basic |
| **Code Maturity** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |

## Port Configuration

| Implementation | RAOP Port | AirPlay Port | Notes |
|---------------|-----------|--------------|-------|
| **AirPlay-master** | **5000** | **7000** | ✅ Standard ports |
| **AirPlayServer-master** | 5001 | 7001 | Custom ports |
| **Airplay2OnWindows** | 5000 | 7000 | ✅ Standard ports |
| **airplayreceiver** | 5000 | 7000 | ✅ Standard ports |
| **WindowPlay** | 7000 | 7000 | ❌ Same port for both |

## Protocol Support

| Feature | AirPlay-master | AirPlayServer | Airplay2OnWindows | airplayreceiver | WindowPlay |
|---------|---------------|---------------|-------------------|-----------------|------------|
| **Screen Mirroring** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No |
| **Audio Streaming** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ⚠️ Basic |
| **H.264 Decoding** | ✅ FFmpeg | ✅ FFmpeg | ✅ FFmpeg | ✅ FFmpeg | ❌ No |
| **AAC-ELD Audio** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No |
| **ALAC Audio** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No |
| **FairPlay** | ✅ Yes | ✅ Yes | ⚠️ Partial | ⚠️ Partial | ❌ No |
| **Pair-Setup** | ✅ SRP-6a | ✅ Full | ✅ Ed25519 | ✅ Ed25519 | ❌ No |
| **Pair-Verify** | ✅ Yes | ✅ Yes | ✅ Curve25519 | ✅ Curve25519 | ❌ No |
| **/info Endpoint** | ✅ Complete | ✅ Complete | ✅ Complete | ✅ Complete | ❌ Missing |

## mDNS/Service Discovery

| Implementation | Service Type | Port Advertised | TXT Records |
|---------------|--------------|-----------------|-------------|
| **AirPlay-master** | _raop._tcp<br>_airplay._tcp | 5000<br>7000 | ✅ Complete |
| **AirPlayServer** | _airplay._tcp | 7001 | ✅ Complete |
| **Airplay2OnWindows** | _raop._tcp<br>_airplay._tcp | 5000<br>7000 | ✅ Complete |
| **airplayreceiver** | _raop._tcp<br>_airplay._tcp | 5000<br>7000 | ✅ Complete |
| **WindowPlay** | _airplay._tcp | 7000 | ⚠️ Basic |

## Architecture & Code Quality

### AirPlay-master (C/C++)
**Pros:**
- ✅ Native C implementation (easy to integrate with our C++ code)
- ✅ Proven to work with real iOS devices
- ✅ Complete protocol implementation
- ✅ Includes working DLL and examples
- ✅ Has packet captures for debugging
- ✅ FairPlay support via external server
- ✅ Standard port configuration (5000/7000)
- ✅ Well-structured library design

**Cons:**
- ❌ Older codebase (~2017)
- ❌ Limited to iOS 9.3.2 - 10.2.1 (may need updates for newer iOS)
- ❌ Chinese comments in some parts
- ❌ Less documentation

**Key Files:**
- `lib/raop.c` - RAOP protocol handler
- `lib/airplay.c` - AirPlay protocol handler
- `lib/dnssd.c` - mDNS service discovery
- `lib/raop_rtp.c` - RTP streaming
- `mediaserver.h` - Public API

**Port Configuration:**
```c
opt->port_raop = 5000;      // RAOP/AirTunes
opt->port_airplay = 7000;   // AirPlay
```

---

### AirPlayServer-master (C++/Windows)
**Pros:**
- ✅ Modern C++ implementation
- ✅ Active development (2024)
- ✅ Excellent documentation
- ✅ Windows-optimized (SDL2, ImGui)
- ✅ Complete protocol support
- ✅ Good error handling
- ✅ Performance monitoring
- ✅ Quality presets (30/60 FPS)

**Cons:**
- ❌ Uses custom ports (5001/7001) instead of standard
- ❌ More complex architecture
- ❌ Requires Bonjour for Windows
- ❌ Larger codebase

**Key Files:**
- `AirPlayServerLib/airplay2.cpp` - Core protocol
- `AirPlayServer/CSDLPlayer.cpp` - Video rendering
- `dnssd/` - mDNS implementation

**Port Configuration:**
```cpp
// Uses ports 5001 and 7001 (non-standard)
```

---

### Airplay2OnWindows (C# .NET 8)
**Pros:**
- ✅ Modern .NET 8 implementation
- ✅ Active development (2024)
- ✅ Auto-launches ffplay for video
- ✅ Clean architecture
- ✅ Good documentation
- ✅ Standard ports (5000/7000)
- ✅ Proper Curve25519/Ed25519 crypto

**Cons:**
- ❌ C# (harder to integrate with our C++ code)
- ❌ Requires .NET runtime
- ❌ External ffplay dependency
- ❌ More memory overhead

**Key Files:**
- `AirPlay/Listeners/AirTunesListener.cs` - Main RTSP handler
- `AirPlay/AirPlayReceiver.cs` - Service setup
- `AirPlay/Decoders/` - Audio/video decoders

**Port Configuration:**
```csharp
_airTunesPort = 5000;  // RAOP
_airPlayPort = 7000;   // AirPlay
```

---

### airplayreceiver (C# .NET Core)
**Pros:**
- ✅ Cross-platform (.NET Core)
- ✅ Good documentation
- ✅ Wiki with protocol explanation
- ✅ Standard ports (5000/7000)
- ✅ Clean code structure
- ✅ Proper crypto implementation

**Cons:**
- ❌ C# (harder to integrate)
- ❌ Last updated 2021
- ❌ Requires building native codecs (fdk-aac, alac)
- ❌ Complex setup process

**Key Files:**
- `AirPlay/Listeners/AirTunesListener.cs` - RTSP handler
- `AirPlay/Crypto/` - Cryptographic functions
- `AirPlay/Decoders/` - Audio decoders

**Port Configuration:**
```csharp
_airTunesPort = 5000;
_airPlayPort = 7000;
```

---

### WindowPlay (C#)
**Pros:**
- ✅ Simple implementation
- ✅ Easy to understand
- ✅ Minimal dependencies

**Cons:**
- ❌ Incomplete implementation
- ❌ No screen mirroring support
- ❌ No /info endpoint
- ❌ Basic RTSP only
- ❌ No proper pairing
- ❌ Uses same port for both services

**Key Files:**
- `WindowPlay/RTSPServer.cs` - Basic RTSP server

**Port Configuration:**
```csharp
// Uses port 7000 for everything (incorrect)
```

---

## Critical Findings

### Why iPad Can't See AuraCastPro Anymore

After our changes, the iPad can't see the service because:

1. **Port Mismatch**: We're advertising _raop._tcp on port 7000, but iOS expects it on port 5000
2. **Service Discovery**: iOS looks for _raop._tcp on 5000 for RTSP control
3. **Our Current Setup**:
   ```cpp
   // MDNSService.cpp - WRONG!
   services.push_back({..., "_raop._tcp", ..., 7000, ...});  // Should be 5000!
   services.push_back({..., "_airplay._tcp", ..., 7000, ...}); // This is OK
   ```

### What Working Implementations Do

**AirPlay-master (CORRECT):**
```c
// main.c
opt->port_raop = 5000;      // RAOP RTSP control
opt->port_airplay = 7000;   // AirPlay data

// dnssd.c
dnssd_register_raop(dnssd, name, 5000, ...);     // _raop._tcp on 5000
dnssd_register_airplay(dnssd, name, 7000, ...);  // _airplay._tcp on 7000
```

**Airplay2OnWindows (CORRECT):**
```csharp
// AirPlayReceiver.cs
_airTunesPort = 5000;  // RTSP listener
_airPlayPort = 7000;   // Data port

// mDNS
var airTunes = new ServiceProfile(..., AirTunesType, _airTunesPort);  // 5000
var airPlay = new ServiceProfile(..., AirPlayType, _airPlayPort);     // 7000
```

## Recommendation

### 🎯 Use AirPlay-master as Primary Reference

**Reasons:**
1. **Native C/C++** - Direct integration with our codebase
2. **Proven Working** - Tested with real iOS devices
3. **Complete Implementation** - All protocol phases
4. **Standard Ports** - Uses 5000/7000 correctly
5. **Library Design** - Clean API we can adapt

### 📋 Action Plan

1. **Fix Port Configuration** (IMMEDIATE):
   ```cpp
   // Change in MDNSService.cpp:
   services.push_back({..., "_raop._tcp", ..., 5000, ...});  // NOT 7000!
   
   // Change in AirPlay2Host.cpp:
   constexpr uint16_t AIRPLAY_CONTROL_PORT = 5000;  // NOT 7000!
   ```

2. **Study AirPlay-master Implementation**:
   - `lib/raop.c` - RTSP request handling
   - `lib/dnssd.c` - mDNS TXT records
   - `lib/raop_rtp.c` - RTP streaming

3. **Verify /info Endpoint** (Already Fixed):
   - Our fix matches working implementations ✅

4. **Test Connection**:
   - Restart app with port 5000
   - iPad should see "AuraCastPro" again
   - Monitor for /info request

## Comparison Summary

| Rank | Implementation | Best For | Integration Difficulty |
|------|---------------|----------|----------------------|
| 🥇 1 | **AirPlay-master** | Our project (C++) | ⭐ Easy |
| 🥈 2 | **AirPlayServer-master** | Reference/Learning | ⭐⭐ Medium |
| 🥉 3 | **Airplay2OnWindows** | Protocol Understanding | ⭐⭐⭐ Hard (C#) |
| 4 | **airplayreceiver** | Cross-platform | ⭐⭐⭐ Hard (C#) |
| 5 | **WindowPlay** | Basic concepts only | ⭐⭐⭐⭐ Not useful |

## Next Steps

1. **DO NOT BUILD YET** - Let's discuss the port change first
2. Change RTSP listener from port 7000 → 5000
3. Update mDNS to advertise _raop._tcp on 5000
4. Keep _airplay._tcp on 7000
5. Test if iPad can see the service again


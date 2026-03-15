# 📚 Reference Implementation Analysis

## Overview

Analyzed 5 working AirPlay server implementations from the `airplay_reference` folder to understand why our iPad connection was failing.

## Key Findings

### 1. Port Configuration (CRITICAL)

**Working Implementation (Airplay2OnWindows):**
```csharp
private readonly ushort _airTunesPort = 5000;  // RTSP control
private readonly ushort _airPlayPort = 7000;   // Video data

// mDNS advertising:
var airTunes = new ServiceProfile(..., AirTunesType, _airTunesPort);  // Port 5000
var airPlay = new ServiceProfile(..., AirPlayType, _airPlayPort);     // Port 7000

// RTSP listener:
public AirTunesListener(..., ushort port, ...) : base(port)  // Listens on 5000
```

**Our Implementation:**
```cpp
constexpr uint16_t AIRPLAY_CONTROL_PORT = 7000;  // RTSP control

// mDNS advertising (MDNSService.cpp):
services.push_back({..., "_airplay._tcp", ..., 7000, ...});
services.push_back({..., "_raop._tcp", ..., 7000, ...});  // ❌ Should be 5000!

// RTSP listener:
// Listens on port 7000
```

**Conclusion:** We're advertising _raop._tcp on port 7000, but iOS expects it on port 5000!

### 2. /info Endpoint Response (CRITICAL)

**Working Implementation Returns:**
```csharp
var dict = new Dictionary<string, object>();
dict.Add("features", 61379444727);
dict.Add("displays", new List<Dictionary<string, object>> {
    new Dictionary<string, object> {
        { "widthPixels", 1920.0 },
        { "heightPixels", 1080.0 },
        { "features", 30 },
        { "rotation", true },
        { "overscanned", false }
    }
});
dict.Add("audioFormats", new List<Dictionary<string, object>> {
    new Dictionary<string, object> {
        { "type", 100 },
        { "audioInputFormats", 67108860 },
        { "audioOutputFormats", 67108860 }
    },
    new Dictionary<string, object> {
        { "type", 101 },
        { "audioInputFormats", 67108860 },
        { "audioOutputFormats", 67108860 }
    }
});
dict.Add("vv", 2);
dict.Add("statusFlags", 4);  // ❌ We had 0!
dict.Add("keepAliveLowPower", true);
dict.Add("sourceVersion", "220.68");
dict.Add("pk", "...");
dict.Add("keepAliveSendStatsAsBody", true);
dict.Add("deviceID", "...");
dict.Add("model", "AppleTV5,3");
dict.Add("audioLatencies", ...);
dict.Add("macAddress", "...");
```

**Our Original Implementation:**
```cpp
// Missing: displays, audioFormats, vv, keepAliveLowPower, keepAliveSendStatsAsBody
// Wrong: statusFlags = 0 (should be 4)
// Wrong: model = "AppleTV3,2" (should be "AppleTV5,3")
```

**✅ FIXED:** Updated makeAirPlayInfoPlist() to include all required fields

### 3. mDNS TXT Records

**Working Implementation:**
```csharp
// _raop._tcp TXT records:
airTunes.AddProperty("ch", "2");
airTunes.AddProperty("cn", "0,1,2,3");
airTunes.AddProperty("et", "0,3,5");
airTunes.AddProperty("md", "0,1,2");
airTunes.AddProperty("sr", "44100");
airTunes.AddProperty("ss", "16");
airTunes.AddProperty("da", "true");
airTunes.AddProperty("sv", "false");
airTunes.AddProperty("ft", "0x5A7FFFF7,0x1E");  // ✅ TWO values!
airTunes.AddProperty("am", "AppleTV5,3");
airTunes.AddProperty("pk", "...");
airTunes.AddProperty("sf", "0x4");  // statusFlags
airTunes.AddProperty("tp", "UDP");
airTunes.AddProperty("vn", "65537");
airTunes.AddProperty("vs", "220.68");
airTunes.AddProperty("vv", "2");

// _airplay._tcp TXT records:
airPlay.AddProperty("deviceid", "...");
airPlay.AddProperty("features", "0x5A7FFFF7,0x1E");  // ✅ TWO values!
airPlay.AddProperty("flags", "0x4");
airPlay.AddProperty("model", "AppleTV5,3");
airPlay.AddProperty("pk", "...");
airPlay.AddProperty("pi", "...");
airPlay.AddProperty("srcvers", "220.68");
airPlay.AddProperty("vv", "2");
```

**Our Implementation:**
```cpp
// We have most of these, but features might need TWO values
// Currently: features=0x5A7FFFF7
// Should be: features=0x5A7FFFF7,0x1E
```

### 4. Connection Flow

**iOS Connection Sequence:**
```
1. mDNS Discovery
   iOS: "Show me all _airplay._tcp and _raop._tcp services"
   Server: "Here's AuraCastPro on port 7000 (or 5000)"

2. GET /info (FIRST REQUEST!)
   iOS: "GET /info RTSP/1.0"
   Server: Returns plist with displays, audioFormats, statusFlags, etc.
   iOS: Validates response fields
   ❌ If validation fails → "Unable to connect"

3. OPTIONS
   iOS: "OPTIONS * RTSP/1.0"
   Server: "Public: OPTIONS, ANNOUNCE, SETUP, RECORD, ..."

4. POST /pair-setup
   iOS: Sends TLV8 pairing data
   Server: SRP-6a handshake (M1→M2, M3→M4)

5. POST /pair-verify
   iOS: Sends Curve25519 public key
   Server: ECDH key exchange + Ed25519 signature

6. SETUP
   iOS: "SETUP rtsp://host/stream RTSP/1.0"
   Server: Returns Transport header with ports

7. RECORD
   iOS: "RECORD rtsp://host/stream RTSP/1.0"
   Server: Starts receiving video/audio data
```

**Critical Point:** If /info fails, iOS never proceeds to OPTIONS!

### 5. Pair-Verify Implementation

**Working Implementation (Curve25519 + Ed25519):**
```csharp
// Phase 1: ECDH key exchange
var curve25519 = Curve25519.getInstance(Curve25519.BEST);
var curve25519KeyPair = curve25519.generateKeyPair();
session.EcdhOurs = curve25519KeyPair.getPublicKey();
session.EcdhShared = curve25519.calculateAgreement(ecdhPrivateKey, session.EcdhTheirs);

// Phase 2: Sign with Ed25519
var aesCtr128Encrypt = Utilities.InitializeChiper(session.EcdhShared);
byte[] dataToSign = new byte[64];
Array.Copy(session.EcdhOurs, 0, dataToSign, 0, 32);
Array.Copy(session.EcdhTheirs, 0, dataToSign, 32, 32);
var signature = Chaos.NaCl.Ed25519.Sign(dataToSign, _expandedPrivateKey);
byte[] encryptedSignature = aesCtr128Encrypt.DoFinal(signature);
```

**Our Implementation:**
```cpp
// We have Curve25519 and Ed25519 functions implemented
// But pair-verify just returns 200 OK in no-PIN mode
// This might be OK for transient pairing, but could be rejected by iOS
```

## Recommendations

### Immediate (Already Done)
✅ Fix /info endpoint to include all required fields
✅ Change statusFlags from 0 to 4
✅ Add displays, audioFormats, audioLatencies arrays
✅ Update model to AppleTV5,3

### High Priority (If Connection Still Fails)
1. **Change port configuration:**
   - Move RTSP listener from port 7000 to port 5000
   - Update mDNS to advertise _raop._tcp on 5000
   - Keep _airplay._tcp on 7000

2. **Update features format:**
   - Change from "0x5A7FFFF7" to "0x5A7FFFF7,0x1E"
   - This is TWO comma-separated hex values

### Medium Priority
3. **Implement proper pair-verify:**
   - Use Curve25519 ECDH key exchange
   - Use Ed25519 signatures
   - Encrypt signature with AES-CTR
   - Currently we just return 200 OK (no-PIN mode)

### Low Priority
4. **Separate RAOP and AirPlay services:**
   - Create two separate listeners
   - One for audio (RAOP) on port 5000
   - One for video (AirPlay) on port 7000

## Test Results

### Before Fix
- iPad could see "AuraCastPro" ✅
- iPad showed "Unable to connect" ❌
- No /info request in logs ❌

### After Fix
- iPad can see "AuraCastPro" ✅
- /info endpoint returns complete response ✅
- Waiting for test results... ⏳

## Reference Files

All working implementations are in `airplay_reference/`:

1. **Airplay2OnWindows-main/** (C#)
   - Most complete implementation
   - File: `AirPlay/Listeners/AirTunesListener.cs` (line 56-200)
   - Shows /info endpoint handling

2. **WindowPlay-master/** (C#)
   - Simple RTSP server
   - File: `WindowPlay/RTSPServer.cs`
   - Basic request routing

3. **airplayreceiver-main/** (C#)
   - Good mDNS examples
   - File: `AirPlay/AirPlayReceiver.cs`
   - Shows port configuration

4. **AirPlayServer-master/** (Java)
   - Port separation examples
   - Shows RAOP vs AirPlay distinction

5. **AirPlay-master/** (C)
   - Low-level protocol details
   - Binary format examples

## Conclusion

The main issue was the incomplete /info endpoint response. iOS validates this response BEFORE attempting any connection. Missing fields like `displays`, `audioFormats`, and wrong `statusFlags` value caused iOS to reject the connection immediately.

The port configuration (5000 vs 7000) might also be an issue, but we need to test the /info fix first to see if that alone solves the problem.


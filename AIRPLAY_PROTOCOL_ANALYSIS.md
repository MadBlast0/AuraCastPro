# AirPlay Protocol Analysis - Key Findings

## Critical Issues Found in Our Implementation

After analyzing 5 working AirPlay server implementations, I found several critical differences:

### 1. **Missing /info Endpoint** ⚠️ CRITICAL
**What iOS expects:**
- iOS devices send `GET /info` request BEFORE attempting to connect
- This returns device capabilities, display info, audio formats
- Our implementation doesn't handle this endpoint!

**Working implementation returns:**
```
GET /info HTTP/1.1
Response: application/x-apple-binary-plist with:
- features: 61379444727
- displays: [{widthPixels: 1920, heightPixels: 1080, features: 30, ...}]
- audioFormats: [{type: 100, audioInputFormats: 67108860, ...}]
- vv: 2
- statusFlags: 4
- sourceVersion: "220.68"
- pk: <32-byte public key>
- deviceID: <MAC address>
- model: "AppleTV5,3"
```

### 2. **Incorrect Port Advertising**
**What we're doing:**
- Advertising port 7000 for both _airplay._tcp and _raop._tcp

**What working implementations do:**
- _raop._tcp (AirTunes): port 5000 (audio streaming)
- _airplay._tcp: port 7000 (video/mirroring)
- These are DIFFERENT services on DIFFERENT ports!

### 3. **Missing TXT Record Properties**
**Our mDNS TXT records are missing:**
- `statusFlags=0x4` (required!)
- `keepAliveLowPower=true`
- `keepAliveSendStatsAsBody=true`
- Proper `features` format: should be TWO values like `0x5A7FFFF7,0x1E`

### 4. **Pair-Verify Implementation**
**Working implementations:**
- Use Curve25519 ECDH key exchange
- Use Ed25519 signatures
- Implement proper AES-CTR encryption for signature
- Handle two-phase verification (flag 0 and flag 1)

**Our implementation:**
- Returns empty 200 OK (no-PIN mode)
- Doesn't do proper cryptographic handshake
- iOS might reject this as insecure

### 5. **RTSP Response Headers**
**Working implementations include:**
```
Server: AirTunes/220.68
Content-Type: application/x-apple-binary-plist (for /info)
Content-Type: application/octet-stream (for pair-setup/verify)
```

## Immediate Fixes Needed

### Fix 1: Add /info Endpoint Handler
```cpp
// In AirPlay2Host::handleSession(), add before OPTIONS:
else if (method == "GET" && urlPath == "/info") {
    const auto body = makeAirPlayInfoPlist("AuraCastPro");
    response = makeRTSP(200, "OK", cseq,
                        "Content-Type: application/x-apple-binary-plist\r\n"
                        "Server: AirTunes/220.68\r\n",
                        body);
    AURA_LOG_INFO("AirPlay2Host", "Returned /info plist to {}", clientIp);
}
```

### Fix 2: Separate AirTunes Port
- Change _raop._tcp to advertise port 5000 (not 7000)
- Keep _airplay._tcp on port 7000
- Start TWO listeners: one for audio (5000), one for video (7000)

### Fix 3: Update mDNS TXT Records
Add to _airplay._tcp:
```
statusFlags=0x4
keepAliveLowPower=true
keepAliveSendStatsAsBody=true
features=0x5A7FFFF7,0x1E  (not just 0x5A7FFFF7)
```

### Fix 4: Proper Pair-Verify
Implement Curve25519 + Ed25519 cryptographic handshake instead of just returning 200 OK.

## Why iPad Can See But Not Connect

**Current behavior:**
1. iPad sees "AuraCastPro" via mDNS ✅
2. iPad sends `GET /info` to check capabilities ❌ (we don't respond)
3. iPad gives up with "Unable to connect" error

**The /info endpoint is the FIRST thing iOS checks!**

Without a proper /info response, iOS doesn't even attempt the RTSP handshake.

## Port Configuration

**Correct setup:**
- Port 5000 (TCP): RAOP/AirTunes audio streaming
- Port 7000 (TCP): AirPlay video/mirroring control
- Port 7001 (UDP): Timing sync
- Port 7002 (UDP): Timing sync  
- Port 7010 (UDP): Video RTP data
- Port 7011 (UDP): Audio RTP data
- Port 5353 (UDP): mDNS discovery

## References

Working implementations analyzed:
1. **Airplay2OnWindows** (C#) - Most complete, handles /info properly
2. **WindowPlay** (C#) - Simple RTSP implementation
3. **airplayreceiver** (C#) - Good mDNS examples
4. **AirPlayServer** (Java) - Shows proper port separation
5. **AirPlay-master** (C) - Low-level protocol details

## Next Steps

1. ✅ Add /info endpoint handler (CRITICAL - do this first!)
2. ✅ Fix mDNS TXT records with statusFlags
3. ✅ Separate RAOP (5000) and AirPlay (7000) ports
4. ⏳ Implement proper Curve25519/Ed25519 pair-verify
5. ⏳ Test connection from iPad

The /info endpoint is likely the main blocker. iOS checks this BEFORE attempting any connection.

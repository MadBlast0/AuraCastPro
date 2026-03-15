# 💻 Code-Level Comparison: Line-by-Line Analysis

## File Count & Structure

```
AirPlay-master:          105 files (20 .c, 85 .h)
AirPlayServer-master:    525 files (238 .c/.cpp, 287 .h/.hpp)
Airplay2OnWindows:       61 .cs files
airplayreceiver:         56 .cs files
────────────────────────────────────────────────
TOTAL:                   747 files analyzed
```

## Critical Code Comparisons

### 1. Port Configuration (THE ROOT CAUSE)

**AirPlay-master (CORRECT):**
```c
// main.c:98-99
opt->port_raop = 5000;      // ✅ RAOP on 5000
opt->port_airplay = 7000;   // ✅ AirPlay on 7000

// dnssd.c:183-184
dnssd_register_raop(dnssd, name, 5000, hwaddr, hwaddrlen, password);
dnssd_register_airplay(dnssd, name, 7000, hwaddr, hwaddrlen);
```

**Our Current Code (WRONG):**
```cpp
// MDNSService.cpp:372-373
services.push_back({..., "_raop._tcp", ..., 7000, ...});  // ❌ Should be 5000!
services.push_back({..., "_airplay._tcp", ..., 7000, ...}); // ✅ OK

// AirPlay2Host.cpp:59
constexpr uint16_t AIRPLAY_CONTROL_PORT = 7000;  // ❌ Should be 5000!
```

**Fix Required:**
```cpp
// Change to:
services.push_back({..., "_raop._tcp", ..., 5000, ...});  // ✅ FIXED!
constexpr uint16_t AIRPLAY_CONTROL_PORT = 5000;  // ✅ FIXED!
```

### 2. /info Endpoint Response

**AirPlay-master (INCOMPLETE - 40%):**
```c
// lib/airplay.c:129-143
#define SERVER_INFO  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "\
"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>deviceid</key>\r\n"\
"<string>%s</string>\r\n"\
"<key>features</key>\r\n"\
"<integer>119</integer>\r\n"\          // ❌ Wrong value
"<key>model</key>\r\n"\
"<string>Kodi,1</string>\r\n"\
"<key>protovers</key>\r\n"\
"<string>1.0</string>\r\n"\
"<key>srcvers</key>\r\n"\
"<string>"AIRPLAY_SERVER_VERSION_STR"</string>\r\n"\
"</dict>\r\n"\
"</plist>\r\n"

// Missing:
// ❌ displays
// ❌ audioFormats
// ❌ audioLatencies
// ❌ statusFlags
// ❌ vv
// ❌ keepAliveLowPower
// ❌ keepAliveSendStatsAsBody
```

**Airplay2OnWindows (COMPLETE - 100%):**
```csharp
// AirTunesListener.cs:56-200
var dict = new Dictionary<string, object>();
dict.Add("features", 61379444727);  // ✅ Correct value
dict.Add("name", "airserver");
dict.Add("displays", new List<Dictionary<string, object>> {
    new Dictionary<string, object> {
        { "primaryInputDevice", 1 },
        { "rotation", true },
        { "widthPhysical", 0 },
        { "widthPixels", 1920.0 },
        { "uuid", "061013ae-7b0f-4305-984b-974f677a150b" },
        { "heightPhysical", 0 },
        { "features", 30 },
        { "heightPixels", 1080.0 },
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
dict.Add("vv", 2);  // ✅ Required!
dict.Add("statusFlags", 4);  // ✅ Required!
dict.Add("keepAliveLowPower", true);
dict.Add("sourceVersion", "220.68");
dict.Add("pk", "29fbb183a58b466e05b9ab667b3c429d18a6b785637333d3f0f3a34baa89f45c");
dict.Add("keepAliveSendStatsAsBody", true);
dict.Add("deviceID", "78:7B:8A:BD:C9:4D");
dict.Add("model", "AppleTV5,3");
dict.Add("audioLatencies", new List<Dictionary<string, object>> {
    new Dictionary<string, object> {
        { "outputLatencyMicros", 0 },
        { "type", 100 },
        { "audioType", "default" },
        { "inputLatencyMicros", 0 }
    },
    new Dictionary<string, object> {
        { "outputLatencyMicros", 0 },
        { "type", 101 },
        { "audioType", "default" },
        { "inputLatencyMicros", 0 }
    }
});
dict.Add("macAddress", "78:7B:8A:BD:C9:4D");
```

**Our Current Code (FIXED - 100%):**
```cpp
// We already fixed this! ✅
// See CRITICAL_FIX_INFO_ENDPOINT.md
```

### 3. mDNS TXT Records

**AirPlay-master:**
```c
// dnssd.c:221-285 (raop_register)
TXTRecordSetValue(&txtRecord, "ch", 1, "2");
TXTRecordSetValue(&txtRecord, "cn", 7, "0,1,2,3");
TXTRecordSetValue(&txtRecord, "et", 5, "0,3,5");
TXTRecordSetValue(&txtRecord, "md", 5, "0,1,2");
TXTRecordSetValue(&txtRecord, "sr", 5, "44100");
TXTRecordSetValue(&txtRecord, "ss", 2, "16");
TXTRecordSetValue(&txtRecord, "tp", 3, "UDP");
TXTRecordSetValue(&txtRecord, "vn", 1, "3");
TXTRecordSetValue(&txtRecord, "txtvers", 1, "1");
TXTRecordSetValue(&txtRecord, "pw", 5, "false");

// dnssd.c:287-330 (airplay_register)
TXTRecordSetValue(&txtRecord, "deviceid", hwaddrlen*3-1, hwaddrstr);
TXTRecordSetValue(&txtRecord, "features", 10, "0x5A7FFFF7");  // ❌ Single value
TXTRecordSetValue(&txtRecord, "model", 11, "AppleTV3,2");
TXTRecordSetValue(&txtRecord, "srcvers", 6, "220.68");
TXTRecordSetValue(&txtRecord, "flags", 3, "0x4");
TXTRecordSetValue(&txtRecord, "vv", 1, "2");
TXTRecordSetValue(&txtRecord, "pk", 64, pkstr);
TXTRecordSetValue(&txtRecord, "pi", 36, pistr);
```

**Airplay2OnWindows (ENHANCED):**
```csharp
// AirPlayReceiver.cs:85-125
var airTunes = new ServiceProfile($"{deviceIdInstance}@{_instance}", AirTunesType, _airTunesPort);
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
airTunes.AddProperty("pk", "29fbb183a58b466e05b9ab667b3c429d18a6b785637333d3f0f3a34baa89f45e");
airTunes.AddProperty("sf", "0x4");  // statusFlags
airTunes.AddProperty("tp", "UDP");
airTunes.AddProperty("vn", "65537");
airTunes.AddProperty("vs", "220.68");
airTunes.AddProperty("vv", "2");

var airPlay = new ServiceProfile(_instance, AirPlayType, _airPlayPort);
airPlay.AddProperty("deviceid", _deviceId);
airPlay.AddProperty("features", "0x5A7FFFF7,0x1E");  // ✅ TWO values!
airPlay.AddProperty("flags", "0x4");
airPlay.AddProperty("model", "AppleTV5,3");
airPlay.AddProperty("pk", "29fbb183a58b466e05b9ab667b3c429d18a6b785637333d3f0f3a34baa89f45e");
airPlay.AddProperty("pi", "aa072a95-0318-4ec3-b042-4992495877d3");
airPlay.AddProperty("srcvers", "220.68");
airPlay.AddProperty("vv", "2");
```

**Key Difference:**
```
AirPlay-master:  features=0x5A7FFFF7        (single value)
Airplay2OnWindows: features=0x5A7FFFF7,0x1E  (TWO values!)
```

### 4. Pairing Implementation

**AirPlay-master (NO PAIRING):**
```c
// No pair-setup or pair-verify implementation
// Uses RSA encryption only
// Password-based auth (optional)
```

**AirPlayServer (FULL PAIRING):**
```c
// lib/pairing.c:29-500
struct pairing_s {
    unsigned char ed_private[64];
    unsigned char ed_public[32];
    unsigned char ecdh_private[32];
    unsigned char ecdh_public[32];
    unsigned char ecdh_secret[32];
};

// Pair-Setup (SRP-6a style)
static void pairing_session_init(pairing_session_t *session) {
    // Generate Ed25519 keypair
    ed25519_create_keypair(session->ed_public, session->ed_private, seed);
    
    // Generate Curve25519 keypair
    curve25519_donna(session->ecdh_public, session->ecdh_private);
}

// Pair-Verify (Curve25519 + Ed25519)
static int pairing_session_finish(pairing_session_t *session, 
                                   const unsigned char *their_public) {
    // ECDH shared secret
    curve25519_donna(session->ecdh_secret, session->ecdh_private, their_public);
    
    // Derive AES keys
    pairing_derive_key(session->ecdh_secret, "Pair-Verify-AES-Key", aes_key);
    pairing_derive_key(session->ecdh_secret, "Pair-Verify-AES-IV", aes_iv);
    
    // Sign with Ed25519
    ed25519_sign(signature, message, message_len, 
                 session->ed_public, session->ed_private);
    
    // Encrypt signature with AES-CTR
    aes_ctr_encrypt(signature, 64, aes_key, aes_iv);
    
    return 0;
}
```

**Airplay2OnWindows (CLEAN PAIRING):**
```csharp
// Listeners/AirTunesListener.cs:200-300
if (flag > 0) {
    // Phase 1: ECDH key exchange
    session.EcdhTheirs = reader.ReadBytes(32);
    session.EdTheirs = reader.ReadBytes(32);
    
    var curve25519 = Curve25519.getInstance(Curve25519.BEST);
    var curve25519KeyPair = curve25519.generateKeyPair();
    
    session.EcdhOurs = curve25519KeyPair.getPublicKey();
    var ecdhPrivateKey = curve25519KeyPair.getPrivateKey();
    
    session.EcdhShared = curve25519.calculateAgreement(ecdhPrivateKey, session.EcdhTheirs);
    
    // Phase 2: Sign with Ed25519
    var aesCtr128Encrypt = Utilities.InitializeChiper(session.EcdhShared);
    
    byte[] dataToSign = new byte[64];
    Array.Copy(session.EcdhOurs, 0, dataToSign, 0, 32);
    Array.Copy(session.EcdhTheirs, 0, dataToSign, 32, 32);
    
    var signature = Chaos.NaCl.Ed25519.Sign(dataToSign, _expandedPrivateKey);
    
    byte[] encryptedSignature = aesCtr128Encrypt.DoFinal(signature);
    
    byte[] output = new byte[session.EcdhOurs.Length + encryptedSignature.Length];
    Array.Copy(session.EcdhOurs, 0, output, 0, session.EcdhOurs.Length);
    Array.Copy(encryptedSignature, 0, output, session.EcdhOurs.Length, encryptedSignature.Length);
    
    response.Headers.Add("Content-Type", "application/octet-stream");
    await response.WriteAsync(output, 0, output.Length);
}
```

**Our Current Code (SIMPLIFIED):**
```cpp
// We use no-PIN mode (transient pairing)
// Returns 200 OK without full crypto
// This might be OK for basic use
```

### 5. RTSP Request Routing

**AirPlay-master:**
```c
// lib/airplay.c:400-600
static void conn_request(httpd_conn_t *conn, http_request_t *request, 
                         http_response_t **response, void **opaque) {
    const char *method = http_request_get_method(request);
    const char *url = http_request_get_url(request);
    
    if (!strcmp(method, "GET") && !strcmp(url, "/server-info")) {
        // Handle /server-info
    } else if (!strcmp(method, "POST") && !strcmp(url, "/reverse")) {
        // Handle /reverse
    } else if (!strcmp(method, "POST") && !strcmp(url, "/play")) {
        // Handle /play
    } else if (!strcmp(method, "POST") && !strcmp(url, "/scrub")) {
        // Handle /scrub
    } else if (!strcmp(method, "POST") && !strcmp(url, "/rate")) {
        // Handle /rate
    } else if (!strcmp(method, "POST") && !strcmp(url, "/stop")) {
        // Handle /stop
    } else if (!strcmp(method, "POST") && !strcmp(url, "/photo")) {
        // Handle /photo
    } else if (!strcmp(method, "GET") && !strcmp(url, "/playback-info")) {
        // Handle /playback-info
    }
}
```

**AirPlayServer:**
```c
// lib/airplay.c:300-400
airplay_handler_t handler = NULL;

if (!strcmp(method, "POST") && !strcmp(url, "/pair-setup")) {
    handler = &airplay_handler_pairsetup;
} else if (!strcmp(method, "POST") && !strcmp(url, "/pair-verify")) {
    handler = &airplay_handler_pairverify;
} else if (!strcmp(method, "GET") && !strcmp(url, "/server-info")) {
    handler = &airplay_handler_serverinfo;
} else if (!strcmp(method, "POST") && !strcmp(url, "/play")) {
    handler = &airplay_handler_play;
} else if (!strcmp(method, "POST") && !strcmp(url, "/scrub")) {
    handler = &airplay_handler_scrub;
} else if (!strcmp(method, "POST") && !strcmp(url, "/rate")) {
    handler = &airplay_handler_rate;
} else if (!strcmp(method, "POST") && !strcmp(url, "/stop")) {
    handler = &airplay_handler_stop;
} else if (!strcmp(method, "POST") && !strcmp(url, "/photo")) {
    handler = &airplay_handler_photo;
} else if (!strcmp(method, "GET") && !strcmp(url, "/playback-info")) {
    handler = &airplay_handler_playbackinfo;
}

if (handler) {
    handler(conn, request, response);
}
```

**Airplay2OnWindows:**
```csharp
// Listeners/AirTunesListener.cs:50-500
public override async Task OnDataReceivedAsync(Request request, Response response, 
                                                CancellationToken cancellationToken) {
    var sessionId = request.Headers["Active-Remote"];
    var session = await SessionManager.Current.GetSessionAsync(sessionId);
    
    if (request.Type == RequestType.GET && "/info".Equals(request.Path, StringComparison.OrdinalIgnoreCase)) {
        // Handle /info
    }
    if (request.Type == RequestType.POST && "/pair-setup".Equals(request.Path, StringComparison.OrdinalIgnoreCase)) {
        // Handle /pair-setup
    }
    if (request.Type == RequestType.POST && "/pair-verify".Equals(request.Path, StringComparison.OrdinalIgnoreCase)) {
        // Handle /pair-verify
    }
    if (request.Type == RequestType.POST && "/fp-setup".Equals(request.Path, StringComparison.OrdinalIgnoreCase)) {
        // Handle /fp-setup (FairPlay)
    }
    if (request.Type == RequestType.GET && "/feedback".Equals(request.Path, StringComparison.OrdinalIgnoreCase)) {
        // Handle /feedback
    }
    if (request.Type == RequestType.OPTIONS) {
        // Handle OPTIONS
    }
    if (request.Type == RequestType.SETUP) {
        // Handle SETUP
    }
    if (request.Type == RequestType.GET_PARAMETER) {
        // Handle GET_PARAMETER
    }
    if (request.Type == RequestType.SET_PARAMETER) {
        // Handle SET_PARAMETER
    }
    if (request.Type == RequestType.RECORD) {
        // Handle RECORD
    }
    if (request.Type == RequestType.TEARDOWN) {
        // Handle TEARDOWN
    }
}
```

### 6. Audio Callback Structure

**AirPlay-master:**
```c
// lib/raop.h:30-50
struct raop_callbacks_s {
    void* cls;
    
    void (*audio_init)(void *cls, int bits, int channels, int samplerate, int isaudio);
    void (*audio_process)(void *cls, const void *buffer, int buflen, 
                          double timestamp, unsigned int seqnum);
    void (*audio_destroy)(void *cls);
    void (*audio_flush)(void *cls);
    void (*audio_set_volume)(void *cls, int volume);
    void (*audio_set_metadata)(void *cls, const void *buffer, int buflen);
    void (*audio_set_coverart)(void *cls, const void *buffer, int buflen);
    
    void (*mirroring_play)(void *cls, int width, int height, 
                           const void *buffer, int buflen, 
                           int payloadtype, double timestamp);
    void (*mirroring_process)(void *cls, const void *buffer, int buflen, 
                              int payloadtype, double timestamp);
    void (*mirroring_stop)(void *cls);
};
```

**AirPlayServer:**
```c
// include/raop.h:35-60
struct raop_callbacks_s {
    void* cls;
    
    void (*connected)(void* cls, const char* remoteName, const char* remoteDeviceId);
    void (*disconnected)(void* cls, const char* remoteName, const char* remoteDeviceId);
    void (*audio_process)(void *cls, pcm_data_struct *data, 
                          const char* remoteName, const char* remoteDeviceId);
    void (*video_process)(void *cls, h264_decode_struct *data, 
                          const char* remoteName, const char* remoteDeviceId);
    void (*audio_flush)(void *cls, void *session, 
                        const char* remoteName, const char* remoteDeviceId);
    void (*audio_set_volume)(void *cls, void *session, float volume, 
                             const char* remoteName, const char* remoteDeviceId);
    void (*audio_set_metadata)(void *cls, void *session, 
                               const void *buffer, int buflen, 
                               const char* remoteName, const char* remoteDeviceId);
    void (*audio_set_coverart)(void *cls, void *session, 
                               const void *buffer, int buflen, 
                               const char* remoteName, const char* remoteDeviceId);
    void (*audio_remote_control_id)(void *cls, const char *dacp_id, 
                                     const char *active_remote_header, 
                                     const char* remoteName, const char* remoteDeviceId);
    void (*audio_set_progress)(void *cls, void *session, 
                               unsigned int start, unsigned int curr, unsigned int end, 
                               const char* remoteName, const char* remoteDeviceId);
};
```

**Key Differences:**
- AirPlayServer includes device identification in callbacks
- AirPlayServer has connected/disconnected events
- AirPlayServer passes decoded H.264 (h264_decode_struct)
- AirPlay-master passes raw H.264 NAL units

## Summary of Critical Findings

### ✅ What AirPlay-master Does RIGHT:
1. **Port Configuration**: 5000/7000 (correct!)
2. **Simple Architecture**: Easy to understand
3. **Native C**: Direct integration
4. **Proven Working**: iOS 9.3.2 - 10.2.1
5. **DLL Export**: Clean API

### ❌ What AirPlay-master Does WRONG:
1. **Incomplete /info**: Missing 60% of required fields
2. **No Pairing**: No pair-setup/pair-verify
3. **Single TXT Value**: features=0x5A7FFFF7 (should be two values)
4. **Old Model**: AppleTV3,2 (should be AppleTV5,3)

### 🎯 What We Need to Do:
1. **Keep**: Port configuration (5000/7000)
2. **Keep**: Simple architecture
3. **Fix**: /info endpoint (add all fields) ✅ DONE
4. **Fix**: TXT records (add second features value)
5. **Consider**: Adding pair-verify (from AirPlayServer)

### 📊 Final Verdict:

**Best Implementation: AirPlay-master**
- Simplest to integrate (C/C++)
- Correct port configuration
- Proven working
- Just needs /info fix (which we did!)

**Runner-up: AirPlayServer**
- Most complete
- Best documentation
- But too complex (525 files!)

**Learning Resources: Airplay2OnWindows**
- Clean pairing code
- Good documentation
- Modern approach


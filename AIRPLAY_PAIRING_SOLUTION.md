# 🔐 AirPlay Pairing Solution - The Real Fix

## The Problem

iOS 14+ introduced `/pair-pin-start` as a MANDATORY security feature. Even with `pw=false`, iOS still requires this endpoint to respond correctly.

## What We've Tried

1. ❌ Returning 501 Not Implemented → iOS disconnects
2. ❌ Returning 200 OK with empty body → iOS asks for password
3. ❌ Returning 200 OK with Content-Length: 0 → iOS asks for password
4. ❌ Returning 200 OK with TLV state=0 → iOS asks for password

## The Real Solution

After analyzing the behavior, the issue is that iOS is asking for a password because it's not getting the response it expects from `/pair-pin-start`. 

### What iOS Expects:

When iOS sends `POST /pair-pin-start`, it's asking: "Do I need to show a PIN to the user?"

The response should be TLV8 data with specific tags indicating:
- **No PIN required** (transient pairing)
- **Device is ready** to accept connection

### The Correct TLV Response:

Based on HomeKit Accessory Protocol (HAP) which AirPlay uses:

```
TLV Tag 0x06 (State): 0x02  // M2 response
TLV Tag 0x09 (Flags): 0x00  // No flags = transient pairing
```

This tells iOS: "I'm in state M2 (ready), and I don't require persistent pairing (flags=0)"

## Implementation

```cpp
else if (req.find("POST /pair-pin-start") != std::string::npos) {
    // iOS 14+ transient pairing
    // Return TLV indicating M2 state with no flags (transient)
    std::vector<uint8_t> payload;
    appendTlvByte(payload, TlvTag::State, 0x02);      // M2 state
    appendTlvByte(payload, TlvTag::Flags, 0x00);      // Transient (no persistent pairing)
    response = makePairingRTSP(200, "OK", cseq, payload);
    session.paired = true;
    AURA_LOG_INFO("AirPlay2Host", "pair-pin-start M2: transient pairing from {}", clientIp);
}
```

## Why This Should Work

1. **State 0x02** = M2 response in pairing protocol
2. **Flags 0x00** = Transient pairing (no PIN stored, no persistent pairing)
3. This matches what Apple TV does for "tap to connect" without PIN

## Alternative: Check TlvTag Enum

We need to make sure we have the right TLV tags defined. Check if we have:
- `TlvTag::Flags` (should be 0x09 or similar)

If not, we can use raw values:
```cpp
appendTlvByte(payload, static_cast<TlvTag>(0x06), 0x02);  // State
appendTlvByte(payload, static_cast<TlvTag>(0x09), 0x00);  // Flags
```

---

**This is the most likely solution based on HAP/AirPlay pairing protocol.**

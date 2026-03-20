# iPad Connection Diagnosis

## Current Status
The iPad **IS** connecting and completing most of the AirPlay handshake, but then times out after 30 seconds.

## What's Working ✅
1. Port 7000 is listening and accepting connections
2. Firewall rules are in place
3. mDNS is advertising correctly
4. iPad successfully completes:
   - Phase 1: /info (capabilities exchange)
   - Phase 2: pair-setup (Ed25519 key exchange)
   - Phase 3: pair-verify (ECDH + signature verification - 2 steps)
   - Phase 3.5: FairPlay setup (2 steps)
   - Phase 5: Initial SETUP (timing/event ports)

## What's Failing ❌
After Phase 5 (initial SETUP), the iPad waits 30 seconds and then closes the connection with "Unable to connect" error.

## Root Cause Analysis

### Timeline from Logs:
```
20:15:27.592 - PHASE 5 COMPLETE: Initial SETUP response sent (77 bytes)
20:15:27.593 - Waiting for request #8 from 192.168.1.68...
[30 seconds of silence]
20:15:57.683 - Client 192.168.1.68 closed connection gracefully
```

### The Problem:
The iPad is expecting **additional SETUP requests** for the actual video and audio streams after the initial SETUP, but:
1. The iPad never sends them (or they're not being received)
2. The app times out waiting
3. After 30 seconds, the iPad gives up

### Possible Causes:
1. **Binary Plist Response Malformed**: The initial SETUP response might be incorrectly formatted, causing the iPad to not proceed
2. **Missing ANNOUNCE**: The iPad might be waiting for an ANNOUNCE request that never comes
3. **Port Mismatch**: The SETUP response advertises port 7100 for video, but this might be wrong
4. **Timeout Too Long**: The recv() call has no timeout, so it blocks forever waiting for data

## Next Steps to Fix

### Option 1: Add Logging to See What iPad Sends Next
Add more detailed logging after SETUP to see if the iPad sends anything else.

### Option 2: Fix the SETUP Response
The initial SETUP response should return:
- `timingPort`: Port for timing sync (usually 7001)
- `eventPort`: Port for events (usually 7000)

Currently returning both as 7100, which might be wrong.

### Option 3: Check if ANNOUNCE Should Come Before SETUP
Some AirPlay implementations expect ANNOUNCE (codec negotiation) before SETUP. Our code has ANNOUNCE after SETUP.

### Option 4: Add Timeout to Prevent 30-Second Hang
Add a shorter timeout (5-10 seconds) so the connection fails faster and provides better error messages.

## Recommended Fix
1. Change the initial SETUP response to use correct ports:
   - `timingPort`: 7001
   - `eventPort`: 7000
2. Add debug logging to see what the iPad sends after SETUP
3. Add a 10-second timeout to the recv() call
4. Check if the binary plist response is correctly formatted

## Log Evidence
```
[2026-03-19 20:15:27.592] [info] [AirPlay2Host] PHASE 5: Initial SETUP (timing/event ports)
[2026-03-19 20:15:27.592] [info] [AirPlay2Host] PHASE 5 COMPLETE: Initial SETUP response sent (77 bytes)
[2026-03-19 20:15:27.593] [info] [AirPlay2Host] >>> responded to 192.168.1.68 with 210 bytes
[2026-03-19 20:15:27.593] [debug] [AirPlay2Host] Waiting for request #8 from 192.168.1.68...
[30 seconds pass]
[2026-03-19 20:15:57.683] [info] [AirPlay2Host] Client 192.168.1.68 closed connection gracefully
```

The iPad successfully received the SETUP response (210 bytes) but then waited 30 seconds before giving up.

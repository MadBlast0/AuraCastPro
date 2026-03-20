# Video Packets Not Arriving - Root Cause & Fix

## Problem Summary
iPad connects successfully and completes all AirPlay handshake phases (1-6), mirror window opens, but shows white screen with "Not Responding". After ~10 seconds, iPad sends TEARDOWN and disconnects. Logs show "Decoded so far: 0 frames" - NO video packets arriving on UDP 7010.

## Root Cause: Missing Firewall Rule for UDP 7001

### Why This Matters
AirPlay uses a timing synchronization protocol (NTP-style) on UDP port 7001 to establish precise clock sync between the iPad and receiver. This timing sync is **REQUIRED** before the iPad will send any video packets.

**The flow is:**
1. iPad connects to TCP 7000 (RTSP control) ✓
2. iPad completes pairing and SETUP phases ✓
3. iPad receives SETUP response telling it to use:
   - `timingPort: 7001` (UDP) - for NTP timing sync
   - `eventPort: 7000` (TCP) - for control events
   - `dataPort: 7010` (UDP) - for video RTP packets
4. **iPad tries to establish timing sync on UDP 7001** ← BLOCKED BY FIREWALL
5. Without timing sync, iPad refuses to send video packets
6. After 10 seconds of no video, iPad gives up and sends TEARDOWN

### Evidence from Logs
```
[2026-03-19 21:00:23.315] [info] [AirPlay2Host] PHASE 5 COMPLETE: Initial SETUP response sent (77 bytes)
[2026-03-19 21:00:23.336] [info] [AirPlay2Host] PHASE 6: RECORD from 192.168.1.68 - STREAMING STARTS!
[2026-03-19 21:00:33.394] [info] [AirPlay2Host] <<< TEARDOWN
[2026-03-19 21:00:33.425] [info] [VideoDecoder] State reset. Decoded so far: 0 frames.
[2026-03-19 21:00:33.425] [debug] [PacketReorderCache] Flushed. Stats: inserted=0 dropped=0 reordered=0
```

**Key observation:** `inserted=0` means ReceiverSocket received ZERO UDP packets on port 7010.

### Current Firewall Status
```powershell
✓ TCP 7000 - AirPlay control (RTSP)
✗ UDP 7001 - Timing sync (NTP) ← MISSING!
✓ UDP 7010 - Video data (RTP)
✓ UDP 5353 - mDNS discovery
```

## Fix: Add UDP 7001 Firewall Rule

### Step 1: Run the Firewall Script
```powershell
# Right-click PowerShell and select "Run as Administrator"
cd C:\Users\MadBlast\Documents\GitHub\AuraCastPro
.\add_udp_7001_firewall.ps1
```

### Step 2: Restart AuraCastPro
Kill any running instances and start fresh:
```powershell
Get-Process AuraCastPro -ErrorAction SilentlyContinue | Stop-Process -Force
.\build\Release\AuraCastPro.exe
```

### Step 3: Test iPad Connection
1. Open Control Center on iPad
2. Tap "Screen Mirroring"
3. Select "AuraCastPro"
4. Enter PIN if prompted
5. Mirror window should open and show iPad screen (not white)

### Expected Behavior After Fix
1. iPad connects to TCP 7000 ✓
2. iPad establishes timing sync on UDP 7001 ✓ (NEW!)
3. iPad starts sending video packets to UDP 7010 ✓
4. ReceiverSocket receives packets → PacketReorderCache → FECRecovery → NALParser → VideoDecoder
5. Frames decoded and displayed in mirror window ✓
6. Connection stays active indefinitely ✓

## Why This Wasn't Caught Earlier

The TimingServer was correctly configured to listen on UDP 7001 (fixed in previous session), but the firewall rule was never added. The app logs don't show firewall blocks - they just show silence (no packets arriving).

## Verification Commands

### Check if firewall rule exists:
```powershell
netsh advfirewall firewall show rule name="AuraCastPro UDP 7001"
```

### Check if TimingServer is listening:
```powershell
netstat -an | findstr ":7001"
# Should show: UDP 0.0.0.0:7001
```

### Monitor timing sync activity:
```powershell
# Watch logs for TimingServer messages
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" -Wait | Select-String "TimingServer"
```

## Technical Details

### AirPlay Port Architecture
- **TCP 7000**: RTSP control channel (OPTIONS, SETUP, RECORD, TEARDOWN)
- **UDP 7001**: NTP timing sync (clock synchronization)
- **UDP 7010**: RTP video data (H.264/H.265 packets)
- **UDP 7011**: RTP audio data (AAC/ALAC packets)
- **UDP 5353**: mDNS service discovery

### Why Timing Sync is Required
AirPlay uses precise timestamps for:
- Audio/video synchronization
- Frame pacing (smooth playback)
- Latency compensation
- Jitter buffer management

Without timing sync, the iPad cannot calculate proper presentation timestamps, so it refuses to send video data.

### Code References
- `src/main.cpp:958` - TimingServer initialization (port 7001)
- `src/engine/TimingServer.cpp` - NTP sync handler
- `src/engine/AirPlay2Host.cpp:1565` - SETUP response (timingPort: 7001)
- `src/engine/ReceiverSocket.cpp` - UDP packet receiver (port 7010)

## Next Steps After Fix

Once the firewall rule is added and iPad connects successfully:

1. **Verify video packets arriving:**
   ```
   [info] [ReceiverSocket] Received 1316 bytes from 192.168.1.68:xxxxx
   [debug] [PacketReorderCache] Inserted packet seq=1234
   [info] [VideoDecoder] Decoded frame 1 (1920x1080)
   ```

2. **Monitor frame rate:**
   - Should see 30-60 FPS depending on iPad settings
   - Check performance overlay (Ctrl+Shift+O)

3. **Test stability:**
   - Connection should stay active indefinitely
   - No automatic disconnections
   - Only disconnects when user stops mirroring

## Troubleshooting

### If still no video after adding firewall rule:

1. **Check Windows Firewall is enabled:**
   ```powershell
   Get-NetFirewallProfile | Select-Object Name, Enabled
   ```

2. **Verify all ports are listening:**
   ```powershell
   netstat -an | findstr ":7000 :7001 :7010 :5353"
   ```

3. **Check for port conflicts:**
   ```powershell
   Get-Process -Id (Get-NetTCPConnection -LocalPort 7000).OwningProcess
   Get-Process -Id (Get-NetUDPEndpoint -LocalPort 7001).OwningProcess
   Get-Process -Id (Get-NetUDPEndpoint -LocalPort 7010).OwningProcess
   ```

4. **Disable third-party firewalls temporarily:**
   - Norton, McAfee, Kaspersky, etc. may block even with Windows Firewall rules

5. **Check router/network firewall:**
   - Some routers have client isolation enabled
   - iPad and PC must be on same subnet
   - Try disabling AP isolation in router settings

## Success Criteria

✓ Firewall rule for UDP 7001 exists
✓ TimingServer receives NTP requests from iPad
✓ ReceiverSocket receives video packets on UDP 7010
✓ VideoDecoder shows "Decoded frame X" messages
✓ Mirror window displays iPad screen content
✓ Connection stays active > 1 minute
✓ No automatic disconnections

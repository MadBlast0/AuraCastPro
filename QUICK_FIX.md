# Quick Fix Guide - iPhone Sees AuraCastPro But Won't Connect

## The Problem
Your iPhone can see "AuraCastPro" in Screen Mirroring, but when you tap it, the connection fails.

## Most Likely Causes

### 1. Windows Firewall Blocking Incoming Connections
Even though the app is listening on port 7236, Windows Firewall might be blocking incoming connections from your iPhone.

**Fix:**
```bash
# Run as Administrator
scripts\fix_firewall.bat
```

### 2. Network Isolation / AP Isolation
Your router might have "AP Isolation" enabled, which prevents devices from talking to each other.

**Check:**
- Log into your router (usually 192.168.1.1 or 192.168.0.1)
- Look for "AP Isolation", "Client Isolation", or "Guest Network" settings
- Disable it
- Restart router

### 3. VPN or Proxy Interfering
If you have a VPN running on PC or iPhone, it might be blocking local network connections.

**Fix:**
- Disable VPN on both devices
- Try connecting again

### 4. Different Subnets
PC and iPhone must be on the same subnet (e.g., both 192.168.1.x).

**Check:**
```bash
# On PC
ipconfig

# On iPhone
Settings → Wi-Fi → (i) → IP Address
```

Both should start with the same numbers (e.g., 192.168.1.x).

## Debugging Steps

### Step 1: Verify Ports Are Open
```bash
netstat -ano | findstr "7236"
```

You should see:
```
TCP    0.0.0.0:7236           0.0.0.0:0              LISTENING
```

### Step 2: Monitor Connections in Real-Time
```powershell
# Run this in PowerShell
.\scripts\debug_connection.ps1
```

Then try connecting from iPhone. You should see:
```
[14:23:45] NEW CONNECTION:
  TCP    192.168.1.66:7236    192.168.1.100:52341    ESTABLISHED
  Remote device: 192.168.1.100:52341
```

If you DON'T see this, the connection isn't reaching your PC at all.

### Step 3: Check AuraCastPro Logs
Look for these messages in the app logs:

**Good signs:**
```
[AirPlay2Host] Listening on TCP 0.0.0.0:7236
[AirPlay2Host] Connection from 192.168.1.100
[AirPlay2Host] ANNOUNCE: codec=H265 device=...
[AirPlay2Host] SETUP (video): client_port=...
[AirPlay2Host] RECORD -- stream flowing from ...
```

**Bad signs:**
```
[AirPlay2Host] bind/listen failed on port 7236
[AirPlay2Host] Connection refused -- mirroring inactive
```

### Step 4: Test with Another Device
Try connecting from:
- Another iPhone/iPad
- Android device (use Cast instead of AirPlay)
- Mac (use AirPlay)

If NONE work, it's a PC/network issue.
If SOME work, it's a device-specific issue.

## Common Error Patterns

### Pattern 1: Connection Appears Then Immediately Disappears
**Symptom:** iPhone shows "Connecting..." then fails after 2-3 seconds.

**Cause:** Firewall is blocking the connection after initial handshake.

**Fix:**
1. Run `scripts\fix_firewall.bat` as Administrator
2. Or manually add exception in Windows Defender Firewall:
   - Control Panel → Windows Defender Firewall → Advanced Settings
   - Inbound Rules → New Rule
   - Port → TCP → 7236
   - Allow the connection
   - Apply to all profiles

### Pattern 2: iPhone Shows "Unable to Connect"
**Symptom:** iPhone immediately shows error without trying to connect.

**Cause:** mDNS advertisement is wrong or iPhone cached old info.

**Fix:**
1. On iPhone: Settings → General → Reset → Reset Network Settings
2. Reconnect to Wi-Fi
3. Try again

### Pattern 3: Stuck on "Connecting..."
**Symptom:** iPhone shows "Connecting..." forever, never succeeds or fails.

**Cause:** RTSP handshake is timing out.

**Fix:**
1. Check if port 7236 is actually listening: `netstat -ano | findstr "7236"`
2. Restart AuraCastPro
3. Check logs for errors during startup

## Advanced Debugging

### Capture Network Traffic
```bash
# Run as Administrator
scripts\capture_airplay_traffic.bat
```

This will capture all network traffic. Try connecting from iPhone, then stop the capture. Look for:
- TCP SYN packets from iPhone to PC:7236
- TCP SYN-ACK responses from PC
- RTSP requests (OPTIONS, ANNOUNCE, SETUP, RECORD)

### Check if Another Service is Using Port 7236
```bash
netstat -ano | findstr "7236"
```

If you see multiple processes using port 7236, that's the problem.

**Fix:**
1. Find the process ID (PID) in the last column
2. `tasklist | findstr "<PID>"`
3. Close that application
4. Restart AuraCastPro

### Test with Telnet
```bash
# From another computer on same network
telnet 192.168.1.66 7236
```

If it connects, the port is open and reachable.
If it times out, firewall or network issue.

## Still Not Working?

### Collect Diagnostic Info
1. Run `scripts\check_network.bat` and save output
2. Run `scripts\debug_connection.ps1` while trying to connect
3. Check AuraCastPro logs in `%APPDATA%\AuraCastPro\logs\`
4. Take screenshot of iPhone error message

### Temporary Workaround
If you need it working NOW:

1. **Disable Windows Firewall temporarily:**
   ```bash
   # Run as Administrator
   netsh advfirewall set allprofiles state off
   ```
   
2. Try connecting from iPhone

3. **Re-enable firewall after testing:**
   ```bash
   netsh advfirewall set allprofiles state on
   ```

If it works with firewall off, the issue is definitely firewall rules.

## Expected Behavior

When working correctly:

1. iPhone: Open Control Center → Screen Mirroring
2. iPhone: See "AuraCastPro" in list
3. iPhone: Tap "AuraCastPro"
4. iPhone: Shows "Connecting..." for 1-2 seconds
5. PC: AuraCastPro shows device in list as "Connected"
6. iPhone: Screen appears on PC
7. PC: Stats show bitrate, latency, FPS

Total time: 2-5 seconds from tap to streaming.

## Prevention

After fixing, prevent future issues:

1. Add permanent firewall rules (done by `fix_firewall.bat`)
2. Set AuraCastPro to start with Windows (in Settings)
3. Use static IP for PC (prevents IP changes)
4. Disable AP Isolation on router
5. Keep PC and iPhone on same Wi-Fi network (not guest network)

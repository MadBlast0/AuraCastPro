# ✅ AirPlay Ports Updated to Standard iOS Ports

## What Was Changed

All AirPlay ports have been updated from custom ports to **standard iOS AirPlay ports**:

### Old Configuration (Custom Ports)
- ❌ Port 7236 (TCP) - Custom RTSP control port
- ❌ Port 7238 (UDP) - Custom timing port

### New Configuration (Standard iOS Ports)
- ✅ Port 7000 (TCP) - Standard AirPlay RTSP control port
- ✅ Port 7001 (UDP) - Standard timing sync port
- ✅ Port 7002 (UDP) - Standard timing sync port
- ✅ Port 7010 (UDP) - Video RTP data
- ✅ Port 7011 (UDP) - Audio RTP data
- ✅ Port 5353 (UDP) - mDNS discovery (unchanged)

## Files Updated

1. **src/engine/AirPlay2Host.cpp** - Already using port 7000 ✅
2. **src/discovery/MDNSService.cpp** - Already advertising port 7000 ✅
3. **src/manager/SettingsModel.cpp** - Changed default from 7236 to 7000 ✅
4. **scripts/add_firewall_rules.ps1** - Updated firewall rules ✅
5. **scripts/debug_connection.ps1** - Updated monitoring script ✅
6. **test_connection.ps1** - Updated test script ✅
7. **AIRPLAY_TROUBLESHOOTING.md** - Updated documentation ✅
8. **CONNECTION_STATUS.md** - Updated status ✅

## Next Steps

### 1. Update Firewall Rules (REQUIRED)

Run this command as Administrator in PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File update_firewall_ports.ps1
```

This will:
- Remove old firewall rules for port 7236
- Add new firewall rules for ports 7000, 7001, 7002
- Keep existing rules for mDNS (5353) and Cast (8009)

### 2. Start the Application

The app has been rebuilt with the correct ports. Start it:

```
build\release_x64\Release\AuraCastPro.exe
```

### 3. Test the Connection

From your iPad:
1. Open Control Center (swipe down from top-right)
2. Tap "Screen Mirroring"
3. Look for "AuraCastPro" in the list
4. Tap it to connect

### 4. Verify Port is Listening

After starting the app, verify port 7000 is listening:

```powershell
netstat -ano | Select-String ":7000"
```

You should see:
```
TCP    0.0.0.0:7000           0.0.0.0:0              LISTENING       [PID]
```

### 5. Test Port Connectivity

Run this to test if the port is accessible:

```powershell
Test-NetConnection -ComputerName localhost -Port 7000
```

Should show: `TcpTestSucceeded : True`

## Why Standard Ports Matter

iOS devices expect AirPlay receivers to use **standard ports**:
- Port 7000 is the official AirPlay RTSP control port
- Ports 7001/7002 are standard timing sync ports
- Using custom ports (like 7236) can cause connection failures

Apple's AirPlay protocol specification requires these specific ports for proper device discovery and connection establishment.

## Troubleshooting

If iPad still can't connect after updating:

1. **Restart everything in order:**
   - Close AuraCastPro
   - Restart your router
   - Restart your PC
   - Restart your iPad
   - Start AuraCastPro
   - Try connecting

2. **Check firewall rules:**
   ```powershell
   Get-NetFirewallRule -DisplayName "*AuraCastPro*" | Select-Object DisplayName, Enabled, Direction, Action
   ```
   All rules should show `Enabled: True` and `Action: Allow`

3. **Verify network profile is Private:**
   - Settings → Network & Internet → Wi-Fi
   - Click your network
   - Under "Network profile type", select "Private"

4. **Check for port conflicts:**
   ```powershell
   netstat -ano | Select-String ":7000"
   ```
   Only AuraCastPro should be using port 7000

5. **Monitor connection attempts:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/debug_connection.ps1
   ```
   This will show any connection attempts from your iPad

## Expected Behavior

When working correctly:
1. iPad sees "AuraCastPro" in Screen Mirroring list ✅ (Already working)
2. Tapping it initiates TCP connection to port 7000
3. RTSP handshake completes
4. Video/audio streams start on UDP ports 7010/7011
5. iPad screen appears on PC

## Current Status

- ✅ Code updated to use standard ports
- ✅ Application rebuilt successfully
- ✅ Documentation updated
- ⏳ Firewall rules need to be updated (run update_firewall_ports.ps1)
- ⏳ Application needs to be started
- ⏳ Connection needs to be tested from iPad

## Support

If you continue to have issues after following these steps, check:
- Router AP Isolation settings (should be DISABLED)
- Both devices on same Wi-Fi network
- No VPN running on either device
- Windows Firewall is not blocking the connection

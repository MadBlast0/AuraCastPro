# AuraCastPro Troubleshooting Guide

## Quick Checklist

Before diving into detailed troubleshooting, verify these basics:

- [ ] PC and phone are on the **SAME Wi-Fi network** (not guest network)
- [ ] Windows Firewall allows AuraCastPro (run `scripts\fix_firewall.bat`)
- [ ] No VPN is running on PC or phone
- [ ] iTunes/Bonjour service is not running (conflicts with port 5353)
- [ ] PC has a valid IP address (not 169.254.x.x or 127.0.0.1)

## Problem: "Waiting for devices..." Never Ends

### Symptoms
- Dashboard shows "Waiting for devices..."
- No devices appear in the list
- Stats show 0 kbps, 0 ms, 0.0 FPS

### Diagnosis
1. Run `scripts\check_network.bat` to check your network setup
2. Check AuraCastPro logs for these messages:
   - ✅ GOOD: "Selected network interface: Wi-Fi (Wireless) - IP: 192.168.x.x"
   - ❌ BAD: "No usable LAN IPv4 address found"
   - ❌ BAD: "bind(5353) failed"

### Solutions

#### Solution 1: Fix Network Connection
```bash
# Check if you have a valid IP
ipconfig

# Look for "IPv4 Address" that starts with 192.168.x.x or 10.x.x.x
# If you see 169.254.x.x, your network connection is broken
```

**Fix:** Reconnect to Wi-Fi or restart your router

#### Solution 2: Fix Port 5353 Conflict
```bash
# Check what's using port 5353
netstat -ano | findstr ":5353"

# If you see output, something is using the port
# Usually it's Bonjour service from iTunes
```

**Fix:** 
```bash
# Stop Bonjour service
net stop "Bonjour Service"

# Or just close iTunes and restart AuraCastPro
```

#### Solution 3: Fix Firewall
```bash
# Run as Administrator
scripts\fix_firewall.bat
```

This adds rules for:
- UDP 5353 (mDNS discovery)
- TCP 7236 (AirPlay control)
- UDP 7236 (AirPlay video)
- TCP 8009 (Google Cast)

#### Solution 4: Check Router Settings
Some routers have "AP Isolation" or "Client Isolation" enabled, which prevents devices from seeing each other.

**Fix:**
1. Log into your router admin panel (usually 192.168.1.1 or 192.168.0.1)
2. Look for "AP Isolation", "Client Isolation", or "Guest Network"
3. Disable it
4. Restart router

## Problem: Devices Appear But Won't Connect

### Symptoms
- Device shows in list as "Idle"
- Clicking "Connect" does nothing
- Or connection starts but immediately fails

### Solutions

#### Solution 1: Check AirPlay Settings on iPhone
1. Open Control Center (swipe down from top-right)
2. Long-press on Screen Mirroring
3. Look for "AuraCastPro" in the list
4. Tap it

**Note:** Don't use the "Connect" button in AuraCastPro UI - devices connect automatically when you select them in AirPlay/Cast menu.

#### Solution 2: Check Android Cast Settings
1. Open Quick Settings (swipe down from top)
2. Tap "Cast" or "Screen Cast"
3. Look for "AuraCastPro" in the list
4. Tap it

#### Solution 3: Restart Both Devices
Sometimes the simplest solution works:
1. Close AuraCastPro
2. Restart your phone
3. Restart AuraCastPro
4. Try connecting again

## Problem: Connection Drops or Stutters

### Symptoms
- Video freezes or stutters
- High latency (>100ms)
- Packet loss >5%

### Solutions

#### Solution 1: Check Wi-Fi Signal Strength
- Move closer to router
- Use 5GHz Wi-Fi instead of 2.4GHz (less interference)
- Reduce distance between phone and PC

#### Solution 2: Close Bandwidth-Heavy Apps
- Stop downloads/uploads
- Close streaming services (Netflix, YouTube)
- Pause cloud sync (OneDrive, Dropbox)

#### Solution 3: Check Network Adapter Settings
```bash
# Open Device Manager
devmgmt.msc

# Find your network adapter
# Right-click → Properties → Power Management
# Uncheck "Allow the computer to turn off this device to save power"
```

## Problem: UI is Broken or Unresponsive

### Symptoms
- Dashboard looks weird
- Buttons don't work
- Text is missing or overlapping

### Solutions

#### Solution 1: Update Graphics Drivers
AuraCastPro uses DirectX 12 for GPU rendering.

1. Press Win+R, type `dxdiag`, press Enter
2. Check "DirectX Version" (should be 12)
3. Update your graphics drivers:
   - NVIDIA: https://www.nvidia.com/Download/index.aspx
   - AMD: https://www.amd.com/en/support
   - Intel: https://www.intel.com/content/www/us/en/download-center/home.html

#### Solution 2: Reset Settings
```bash
# Delete settings file
del %APPDATA%\AuraCastPro\settings.json

# Restart AuraCastPro
```

## Problem: "License Invalid" or Feature Locked

### Symptoms
- Some features are grayed out
- "Pro features require license" message

### Solutions

#### Solution 1: Activate License
1. Open Settings
2. Enter your license key and email
3. Click "Activate"

#### Solution 2: Check Internet Connection
License validation requires internet access.

```bash
# Test connection to license server
ping license.auracastpro.com
```

## Advanced Diagnostics

### Enable Debug Logging
1. Open `%APPDATA%\AuraCastPro\settings.json`
2. Add: `"logLevel": "debug"`
3. Restart AuraCastPro
4. Check logs in `%APPDATA%\AuraCastPro\logs\`

### Collect Diagnostic Info
```bash
# Run this and save output
scripts\check_network.bat > network_info.txt

# Also collect:
# - AuraCastPro logs from %APPDATA%\AuraCastPro\logs\
# - Windows Event Viewer → Application logs
# - Screenshot of the issue
```

### Check System Requirements
Minimum:
- Windows 10 version 1903 or later
- DirectX 12 compatible GPU
- 4GB RAM
- 1Gbps network adapter (for 4K streaming)

Recommended:
- Windows 11
- NVIDIA GTX 1060 or AMD RX 580
- 8GB RAM
- Wi-Fi 6 (802.11ax) or Gigabit Ethernet

## Still Having Issues?

If none of these solutions work:

1. **Check the logs:**
   - Location: `%APPDATA%\AuraCastPro\logs\`
   - Look for ERROR or WARN messages
   - Share relevant log excerpts when asking for help

2. **Verify your setup:**
   - Run `scripts\check_network.bat`
   - Take screenshots of any errors
   - Note your Windows version and network adapter model

3. **Get help:**
   - GitHub Issues: [link to your repo]
   - Discord: [link to your Discord]
   - Email: support@auracastpro.com

Include in your support request:
- Windows version
- Network adapter model
- Phone/tablet model and OS version
- Output from `scripts\check_network.bat`
- Relevant log excerpts
- Screenshots of the issue

## Common Error Messages

### "No usable LAN IPv4 address found"
**Meaning:** PC is not connected to a network
**Fix:** Connect to Wi-Fi or Ethernet

### "bind(5353) failed"
**Meaning:** Port 5353 is already in use
**Fix:** Stop Bonjour service or close iTunes

### "GetAdaptersAddresses failed"
**Meaning:** Windows network API error
**Fix:** Restart PC, update network drivers

### "DirectX 12 device creation failed"
**Meaning:** GPU doesn't support DX12 or drivers are outdated
**Fix:** Update graphics drivers

### "License validation failed"
**Meaning:** Can't reach license server or key is invalid
**Fix:** Check internet connection, verify license key

## Tips for Best Performance

1. **Use 5GHz Wi-Fi** - Less interference, higher bandwidth
2. **Connect PC via Ethernet** - Most stable connection
3. **Close unnecessary apps** - Free up CPU and network
4. **Update everything** - Windows, drivers, AuraCastPro
5. **Use Quality of Service (QoS)** - Prioritize AuraCastPro traffic in router settings

## Network Requirements

### Bandwidth
- 720p @ 30fps: ~5 Mbps
- 1080p @ 60fps: ~15 Mbps
- 4K @ 60fps: ~50 Mbps

### Latency
- Acceptable: <70ms
- Good: <40ms
- Excellent: <30ms

### Packet Loss
- Acceptable: <2%
- Good: <0.5%
- Excellent: <0.1%

If your network doesn't meet these requirements, try:
- Lowering resolution in phone settings
- Moving closer to router
- Using Ethernet instead of Wi-Fi
- Upgrading router to Wi-Fi 6

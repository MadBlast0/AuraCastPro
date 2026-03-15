# AirPlay Connection Troubleshooting Guide

## ✅ Verified Working
- mDNS service is broadcasting correctly on 192.168.1.66
- AirPlay (_airplay._tcp) and RAOP (_raop._tcp) services are being advertised
- Firewall rules are in place
- AirPlay2Host is listening on port 7000 (standard iOS AirPlay port)

## Common Issues and Solutions

### 1. Network Isolation (Most Common)
**Problem:** Your router may have "AP Isolation" or "Client Isolation" enabled, which prevents devices from seeing each other.

**Solution:**
- Log into your router's admin panel (usually 192.168.1.1 or 192.168.0.1)
- Look for settings like:
  - "AP Isolation" - DISABLE this
  - "Client Isolation" - DISABLE this  
  - "Wireless Isolation" - DISABLE this
  - "Guest Network" - Make sure your iPad is NOT on a guest network
- Restart your router after making changes

### 2. Different Subnets
**Problem:** Your PC and iPad might be on different network segments.

**Check:**
- PC IP: 192.168.1.66
- iPad IP: Should be 192.168.1.x (same subnet)
- On iPad: Settings → Wi-Fi → (i) next to your network → Check IP address

### 3. Windows Network Profile
**Problem:** Windows network is set to "Public" instead of "Private"

**Solution:**
1. Open Settings → Network & Internet
2. Click on your Wi-Fi network
3. Under "Network profile type", select "Private"
4. Restart AuraCastPro

### 4. iPad Restrictions
**Problem:** AirPlay might be restricted on your iPad

**Check:**
- Settings → Screen Time → Content & Privacy Restrictions
- Make sure AirPlay is allowed

### 5. Bonjour Service Conflict
**Problem:** Another service (iTunes, Bonjour Print Services) might be using port 5353 or 7000

**Solution:**
1. Close iTunes, Apple Music, and any Apple services
2. Restart AuraCastPro
3. Try connecting from iPad

### 6. Force iPad to Refresh
**Problem:** iPad's mDNS cache might be stale

**Solution:**
1. On iPad: Settings → Wi-Fi → Turn Wi-Fi OFF
2. Wait 10 seconds
3. Turn Wi-Fi back ON
4. Wait for connection
5. Try Screen Mirroring again

### 7. Restart Everything
Sometimes a simple restart fixes everything:
1. Close AuraCastPro
2. Restart your PC
3. Restart your iPad
4. Restart your router
5. Start AuraCastPro
6. Try connecting

## Advanced Diagnostics

### Check if iPad can see the PC
On iPad, install a network scanner app like "Fing" or "Network Analyzer" and see if it can detect your PC at 192.168.1.66

### Check Windows Firewall
Run as Administrator:
```powershell
Get-NetFirewallRule -DisplayName "*AuraCastPro*" | Select-Object DisplayName, Enabled, Direction, Action
```

All rules should show "Enabled: True" and "Action: Allow"

### Test mDNS Broadcasting
Run the test_mdns.ps1 script to verify mDNS packets are being sent:
```powershell
powershell -ExecutionPolicy Bypass -File test_mdns.ps1
```

You should see "Found AuraCastPro mDNS packet"

## Still Not Working?

### Try These Alternative Methods:

1. **Use a Different Network**
   - Try connecting both devices to a mobile hotspot
   - This bypasses router isolation issues

2. **Check Router Firmware**
   - Update your router's firmware to the latest version
   - Some older routers have buggy mDNS implementations

3. **Disable VPN**
   - If you have a VPN running on either device, disable it
   - VPNs can block local network discovery

4. **Check Antivirus/Security Software**
   - Temporarily disable antivirus to test
   - Some security software blocks mDNS packets

## What Should Happen When It Works

1. Open Control Center on iPad (swipe down from top-right)
2. Tap "Screen Mirroring"
3. You should see "AuraCastPro" in the list
4. Tap it
5. A 4-digit PIN may appear on your PC
6. Enter the PIN on your iPad
7. Your iPad screen should appear on your PC

## Current Status
- ✅ AuraCastPro is running
- ✅ mDNS is broadcasting
- ✅ Firewall rules are configured
- ✅ Listening on correct ports
- ❓ iPad not detecting the service

**Most likely cause:** Router AP Isolation or network profile set to Public

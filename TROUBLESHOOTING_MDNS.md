# 🔍 mDNS Troubleshooting Guide

## Current Status ✅

### App Status
- ✅ AuraCastPro is running (PID: varies)
- ✅ Listening on port 5000 (RAOP RTSP)
- ✅ Listening on port 7000 (AirPlay)
- ✅ Broadcasting on 192.168.1.66
- ✅ mDNS service active
- ✅ Bonjour Service running

### Network Configuration
- **PC IP**: 192.168.1.66
- **Interface**: Wi-Fi
- **mDNS Port**: 5353 (active)
- **Multicast**: 224.0.0.251

### Firewall
- ✅ Windows Firewall rules exist for AuraCastPro
- ✅ All profiles enabled (Domain, Private, Public)

## Possible Issues & Solutions

### 1. iPad and PC on Different Networks ⚠️
**Problem**: mDNS only works on the same local network

**Check**:
- Is your iPad connected to the same Wi-Fi network as your PC?
- PC is on: 192.168.1.x network
- iPad should be on: 192.168.1.x network (same subnet)

**Solution**:
- Connect both devices to the same Wi-Fi network
- Avoid using VPN on either device

### 2. mDNS Propagation Delay ⏱️
**Problem**: It can take 10-30 seconds for mDNS to propagate

**Solution**:
- Wait 30 seconds after starting the app
- Try toggling Wi-Fi off/on on your iPad
- Swipe down Control Center again to refresh the list

### 3. iOS Cache Issue 📱
**Problem**: iOS may have cached the old service

**Solution**:
1. On iPad: Settings → General → Transfer or Reset iPad → Reset → Reset Network Settings
2. Or simpler: Turn iPad Wi-Fi off for 10 seconds, then back on
3. Wait 30 seconds
4. Check Screen Mirroring again

### 4. Bonjour Service Conflict 🔄
**Problem**: Multiple mDNS services competing

**Check**:
```powershell
Get-Service | Where-Object {$_.DisplayName -like "*Bonjour*" -or $_.DisplayName -like "*mDNS*"}
```

**Solution**:
- If you have iTunes or other Apple software, try closing them
- Restart AuraCastPro

### 5. Windows Firewall Blocking mDNS 🛡️
**Problem**: Firewall may block multicast packets

**Solution**:
Run these commands in PowerShell (as Administrator):
```powershell
# Allow mDNS multicast
New-NetFirewallRule -DisplayName "mDNS Multicast" -Direction Inbound -Protocol UDP -LocalPort 5353 -Action Allow -Profile Any

# Allow AirPlay ports
New-NetFirewallRule -DisplayName "AirPlay RAOP" -Direction Inbound -Protocol TCP -LocalPort 5000 -Action Allow -Profile Any
New-NetFirewallRule -DisplayName "AirPlay Data" -Direction Inbound -Protocol TCP -LocalPort 7000 -Action Allow -Profile Any
```

### 6. Router Blocking Multicast 🌐
**Problem**: Some routers block mDNS/multicast traffic

**Check**:
- Log into your router settings
- Look for "Multicast" or "IGMP Snooping" settings
- Look for "AP Isolation" or "Client Isolation"

**Solution**:
- Enable "Multicast" or "IGMP Snooping"
- Disable "AP Isolation" or "Client Isolation"
- Some routers call this "Allow clients to communicate"

### 7. Antivirus/Security Software 🦠
**Problem**: Third-party security software blocking mDNS

**Solution**:
- Temporarily disable antivirus
- Add AuraCastPro.exe to whitelist
- Check if you have any network security software

## Quick Test Steps

### Step 1: Verify Network
```powershell
# On PC - check your IP
ipconfig | Select-String "IPv4"

# Should show 192.168.1.66
```

On iPad:
- Settings → Wi-Fi → (i) next to your network
- Check IP address starts with 192.168.1.x

### Step 2: Restart Everything
1. Stop AuraCastPro
2. Wait 5 seconds
3. Start AuraCastPro
4. Wait 30 seconds
5. On iPad: Toggle Wi-Fi off/on
6. Wait 10 seconds
7. Check Screen Mirroring

### Step 3: Test with Another Device
- Try from another iOS device if available
- Try from a Mac if available
- This helps determine if it's an iPad-specific issue

### Step 4: Check Logs
Look at the app output:
```powershell
Get-Content "build\Release\output.log" -Tail 50
```

Look for:
- "Broadcasting AirPlay/Cast via built-in mDNS"
- "Listening on TCP 0.0.0.0:5000"
- Any error messages

## Advanced Diagnostics

### Capture mDNS Packets
Use Wireshark to see if mDNS packets are being sent:
1. Install Wireshark
2. Capture on your Wi-Fi interface
3. Filter: `udp.port == 5353`
4. Look for packets to 224.0.0.251
5. Check if you see `_raop._tcp` and `_airplay._tcp` services

### Test with dns-sd Command
On Mac or Linux:
```bash
dns-sd -B _raop._tcp
dns-sd -B _airplay._tcp
```

Should show "AuraCastPro" in the list

## What Should Work

When everything is correct:
1. Open iPad Control Center
2. Tap "Screen Mirroring"
3. See "AuraCastPro" in the list (may take 10-30 seconds)
4. Tap to connect
5. Connection succeeds

## Still Not Working?

If none of the above works, the issue might be:
1. **iOS Version**: Very old iOS versions may not support newer AirPlay features
2. **Network Equipment**: Some enterprise/hotel Wi-Fi blocks mDNS
3. **VPN**: VPN on either device will break mDNS
4. **Subnet Mask**: Non-standard subnet masks can cause issues

## Next Steps

1. Confirm iPad and PC are on same network (192.168.1.x)
2. Wait 30 seconds after starting app
3. Toggle iPad Wi-Fi off/on
4. Check Screen Mirroring again
5. If still not working, try the firewall commands above
6. Check router settings for AP Isolation

---

**Current Configuration**:
- RAOP Port: 5000 ✅
- AirPlay Port: 7000 ✅
- Features: 0x5A7FFFF7,0x1E ✅
- Model: AppleTV5,3 ✅
- Display Name: AuraCastPro ✅

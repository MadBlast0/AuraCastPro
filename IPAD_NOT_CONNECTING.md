# iPad Not Connecting - Troubleshooting

## Current Status
✓ AuraCastPro is running (PID: 17332)
✓ TCP port 7000 is listening (AirPlay control)
✓ UDP port 5353 is listening (mDNS discovery)
✓ mDNS announcements are being broadcast
✓ AirPlay2Host is ready to accept connections

## Why iPad Might Not See AuraCastPro

### 1. iPad Cached Old Connection
The iPad may have cached the previous connection state and needs to forget it.

**Fix:**
1. On iPad: Settings → General → AirPlay & Handoff
2. Find "AuraCastPro" in the list
3. Tap (i) icon → Forget This Device
4. Go back to Control Center → Screen Mirroring
5. AuraCastPro should appear fresh

### 2. Different Network/Subnet
iPad and PC must be on the SAME Wi-Fi network.

**Check:**
- PC IP: 192.168.1.64
- iPad IP: Should be 192.168.1.x (same subnet)
- If iPad is on different network (e.g., 192.168.0.x or 10.0.0.x), it won't see mDNS

**Fix:**
- Connect iPad to the same Wi-Fi network as PC
- Check iPad IP: Settings → Wi-Fi → (i) icon next to network name

### 3. Router AP Isolation Enabled
Some routers have "AP Isolation" or "Client Isolation" that prevents devices from seeing each other.

**Check:**
- Router settings → Wireless → Advanced
- Look for "AP Isolation", "Client Isolation", or "Station Isolation"
- Should be DISABLED

### 4. Windows Firewall Blocking mDNS
mDNS uses UDP port 5353 for discovery.

**Check:**
```powershell
Get-NetFirewallRule -DisplayName "*5353*" | Select-Object DisplayName, Enabled
```

**Fix:**
```powershell
netsh advfirewall firewall add rule name="mDNS UDP 5353" dir=in action=allow protocol=UDP localport=5353
```

### 5. iPad Needs Restart
Sometimes iOS caches network services and needs a restart.

**Fix:**
1. Restart iPad (hold power + volume, slide to power off)
2. Wait 10 seconds
3. Power on
4. Try connecting again

### 6. PC Firewall Profile
Windows may be using "Public" network profile which blocks discovery.

**Check:**
```powershell
Get-NetConnectionProfile | Select-Object Name, NetworkCategory
```

Should show "Private" not "Public"

**Fix:**
```powershell
Set-NetConnectionProfile -NetworkCategory Private
```

## Quick Test: Manual IP Connection

If mDNS discovery isn't working, try connecting directly by IP:

1. On iPad: Settings → Wi-Fi → (i) icon
2. Scroll down to "Configure Proxy" → Manual
3. Server: 192.168.1.64
4. Port: 7000
5. Save
6. Try Screen Mirroring again

(This bypasses mDNS discovery)

## Check Logs for Connection Attempts

```powershell
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" -Wait | Select-String "Accepted|connection from|OPTIONS|SETUP"
```

Leave this running and try connecting from iPad. You should see:
```
[info] [AirPlay2Host] Accepted connection from 192.168.1.x
[info] [AirPlay2Host] <<< OPTIONS * RTSP/1.0
```

If you see nothing, the iPad isn't reaching the PC.

## Network Diagnostics

### From PC: Can you ping iPad?
```powershell
# Get iPad IP from iPad: Settings → Wi-Fi → (i) icon
ping 192.168.1.x
```

### From iPad: Can you reach PC?
1. Open Safari on iPad
2. Go to: http://192.168.1.64:7000
3. Should see error or response (proves connectivity)

## Most Common Causes (in order)

1. **iPad cached old connection** (90% of cases)
   - Fix: Forget device in iPad settings

2. **Different Wi-Fi networks** (5% of cases)
   - Fix: Connect to same network

3. **Router AP isolation** (3% of cases)
   - Fix: Disable in router settings

4. **Windows Public network profile** (2% of cases)
   - Fix: Change to Private

## Success Indicators

When iPad connects, you'll see in logs:
```
[info] [AirPlay2Host] Accepted connection from 192.168.1.68
[info] [AirPlay2Host] <<< OPTIONS * RTSP/1.0
[info] [AirPlay2Host] >>> responded to 192.168.1.68 with 123 bytes
[info] [AirPlay2Host] <<< POST /pair-setup RTSP/1.0
```

## If Still Not Working

1. Restart PC
2. Restart iPad
3. Restart router
4. Try from a different iOS device (if available)
5. Check Windows Event Viewer for firewall blocks

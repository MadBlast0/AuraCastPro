# Fix: iPad Connects But No Video Packets

## Quick Fix (2 steps)

### 1. Add Missing Firewall Rule
```powershell
# Right-click PowerShell → Run as Administrator
.\add_udp_7001_firewall.ps1
```

### 2. Restart AuraCastPro
```powershell
Get-Process AuraCastPro -ErrorAction SilentlyContinue | Stop-Process -Force
.\build\Release\AuraCastPro.exe
```

## What Was Wrong?

The firewall was blocking UDP port 7001 (timing sync). Without timing sync, iPad refuses to send video packets.

## Verify the Fix

```powershell
.\verify_airplay_ports.ps1
```

Should show all green checkmarks ✓

## Test Connection

1. iPad → Control Center → Screen Mirroring
2. Select "AuraCastPro"
3. Mirror window should show iPad screen (not white)
4. Connection stays active indefinitely

## Still Not Working?

See `VIDEO_PACKETS_FIX.md` for detailed troubleshooting.

## Port Configuration

| Port | Protocol | Purpose |
|------|----------|---------|
| 7000 | TCP | AirPlay control (RTSP) |
| 7001 | UDP | Timing sync (NTP) ← **THIS WAS MISSING** |
| 7010 | UDP | Video data (RTP) |
| 5353 | UDP | mDNS discovery |

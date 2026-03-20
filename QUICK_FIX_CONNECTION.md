# Quick Fix - iPad Not Connecting

## Immediate Steps to Try

### 1. Change Resolution to 1080p
The default resolution might be too high (4K). Try this:

1. Open AuraCastPro
2. Go to Settings → Mirroring tab
3. Change "Resolution" to **1080p**
4. Restart the app
5. Try connecting from iPad again

### 2. Check Network
- iPad and PC must be on the **SAME Wi-Fi network**
- Both devices should have Wi-Fi enabled (not Ethernet for PC, not cellular for iPad)
- Router must allow device-to-device communication (disable AP Isolation if enabled)

### 3. Check Firewall
Run this to verify firewall rules:
```powershell
.\check_connection.ps1
```

If firewall rules are missing, run as Administrator:
```powershell
.\add_firewall_rule.ps1
```

### 4. Restart Everything
1. Close AuraCastPro completely
2. On iPad: Settings → Wi-Fi → Forget network → Reconnect
3. Restart AuraCastPro
4. On iPad: Control Center → Screen Mirroring → Look for "AuraCastPro"

### 5. Check Logs
Look for errors in: `%LOCALAPPDATA%\AuraCastPro\logs\`

Common errors to look for:
- "Port 7000 already in use" → Another app is using the port
- "mDNS advertisement failed" → Network discovery issue
- "Failed to bind socket" → Firewall or port conflict

### 6. Verify App is Running
```powershell
Get-Process -Name "AuraCastPro"
netstat -an | findstr "7000"
```

Should show:
- AuraCastPro.exe process running
- Port 7000 in LISTENING state

## Common Issues

### Issue: "AuraCastPro" doesn't appear in Screen Mirroring list
**Causes:**
- Different Wi-Fi networks
- Firewall blocking port 7000
- mDNS not advertising
- Router AP Isolation enabled

**Fix:**
1. Verify same network
2. Run `add_firewall_rule.ps1` as Admin
3. Check router settings (disable AP Isolation)
4. Restart app

### Issue: Appears in list but won't connect
**Causes:**
- Resolution too high for iPad
- Pairing issue
- Network timeout

**Fix:**
1. Change resolution to 1080p in Settings
2. Delete pairing: Delete `%LOCALAPPDATA%\AuraCastPro\airplay_identity.bin`
3. Restart app and try again

### Issue: Connects but immediately disconnects
**Causes:**
- Unsupported resolution/FPS combination
- Network instability
- Codec mismatch

**Fix:**
1. Use 1080p @ 60fps (most compatible)
2. Check Wi-Fi signal strength
3. Disable VPN on both devices

## Debug Mode

To see detailed connection logs:

1. Open app
2. Watch the log window at bottom
3. On iPad, try to connect
4. Look for these phases:
   - PHASE 1: /info request (capabilities)
   - PHASE 2: /pair-verify (authentication)
   - PHASE 3: /fp-setup (FairPlay)
   - PHASE 4: SETUP (stream setup)
   - PHASE 5: RECORD (start streaming)

If it fails at any phase, note which one and check the error message.

## Still Not Working?

1. Run diagnostics:
   ```powershell
   .\check_connection.ps1
   ```

2. Check if port 7000 is blocked:
   ```powershell
   Test-NetConnection -ComputerName localhost -Port 7000
   ```

3. Try with a different iPad/iPhone if available

4. Check Windows Event Viewer for firewall blocks:
   - Event Viewer → Windows Logs → Security
   - Look for "Windows Filtering Platform" blocks

5. Temporarily disable Windows Firewall to test:
   ```powershell
   # As Administrator
   Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled False
   # Test connection
   # Then re-enable:
   Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled True
   ```

## Quick Test

Run this one-liner to check everything:
```powershell
Write-Host "App Running: $((Get-Process AuraCastPro -EA SilentlyContinue) -ne $null)"; Write-Host "Port 7000: $((Get-NetTCPConnection -LocalPort 7000 -State Listen -EA SilentlyContinue) -ne $null)"; Write-Host "Firewall: $((Get-NetFirewallRule -DisplayName '*AuraCastPro*' -EA SilentlyContinue) -ne $null)"
```

Should show all True.

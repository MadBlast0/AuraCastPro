# Test Your Connection - Step by Step

Follow these steps IN ORDER to diagnose why your iPhone can't connect.

## Step 1: Verify AuraCastPro is Running

```bash
# Check if process is running
tasklist | findstr "AuraCastPro"
```

**Expected:** You should see `AuraCastPro.exe` with a process ID.

**If not:** Start AuraCastPro first!

---

## Step 2: Verify Ports Are Listening

```bash
netstat -ano | findstr "7236"
```

**Expected output:**
```
TCP    0.0.0.0:7236           0.0.0.0:0              LISTENING       19972
UDP    0.0.0.0:5353           *:*                                    19972
```

**If you see this:** ✅ Ports are open, continue to Step 3

**If you DON'T see TCP 7236 LISTENING:** ❌ AuraCastPro failed to start properly
- Check logs in `%APPDATA%\AuraCastPro\logs\`
- Look for errors like "bind/listen failed"
- Another app might be using port 7236

---

## Step 3: Test Firewall

```powershell
# Run as Administrator
Test-NetConnection -ComputerName localhost -Port 7236
```

**Expected:**
```
TcpTestSucceeded : True
```

**If False:** Firewall is blocking the port
- Run `scripts\fix_firewall.bat` as Administrator
- Or temporarily disable firewall to test

---

## Step 4: Monitor Connections in Real-Time

```powershell
# Open PowerShell and run:
.\scripts\debug_connection.ps1
```

**Now try connecting from your iPhone.**

### What You Should See:

**✅ SUCCESS - Connection reaches PC:**
```
[14:23:45] NEW CONNECTION:
  TCP    192.168.1.66:7236    192.168.1.100:52341    ESTABLISHED
  Remote device: 192.168.1.100:52341
```

**❌ FAILURE - No connection appears:**
- Firewall is blocking
- Router has AP Isolation enabled
- iPhone and PC on different subnets

---

## Step 5: Check AuraCastPro Logs

Open: `%APPDATA%\AuraCastPro\logs\latest.log`

### Look for these messages:

**✅ GOOD - Everything working:**
```
[AirPlay2Host] Listening on TCP 0.0.0.0:7236
[AirPlay2Host] Connection from 192.168.1.100
[AirPlay2Host] pair-setup no-PIN: auto-accepted from 192.168.1.100
[AirPlay2Host] ANNOUNCE: codec=H265 device=...
[AirPlay2Host] SETUP (video): client_port=49152, server_port=7010
[AirPlay2Host] RECORD -- stream flowing from 192.168.1.100
```

**❌ BAD - Connection not reaching:**
```
[AirPlay2Host] Listening on TCP 0.0.0.0:7236
(no "Connection from" messages)
```
→ Firewall or network issue

**❌ BAD - Connection drops immediately:**
```
[AirPlay2Host] Connection from 192.168.1.100
[AirPlay2Host] Session ended: 192.168.1.100
```
→ Handshake failing, check next section

---

## Step 6: Verify Network Configuration

### On PC:
```bash
ipconfig
```

Look for your Wi-Fi adapter's IPv4 address. Example:
```
IPv4 Address. . . . . . . . . . . : 192.168.1.66
```

### On iPhone:
1. Settings → Wi-Fi
2. Tap (i) next to your network name
3. Check IP Address

**Both should be on same subnet:**
- PC: 192.168.1.66
- iPhone: 192.168.1.100
- ✅ Same subnet (192.168.1.x)

**If different subnets:**
- PC: 192.168.1.66
- iPhone: 192.168.0.100
- ❌ Different subnets (1.x vs 0.x)
- → Reconnect iPhone to correct Wi-Fi network

---

## Step 7: Test from Another Device

Try connecting from:
- [ ] Another iPhone/iPad
- [ ] Android device (use Cast)
- [ ] Mac (use AirPlay)

**If NONE work:** PC/network issue (firewall, router settings)

**If SOME work:** Device-specific issue (reset network settings on failing device)

---

## Common Issues & Fixes

### Issue 1: "Connection from X.X.X.X" appears in logs but immediately disconnects

**Cause:** RTSP handshake failing

**Fix:**
1. Check if you're running any proxy/VPN software
2. Disable antivirus temporarily to test
3. Check if another AirPlay receiver is running (iTunes, etc.)

### Issue 2: No "Connection from" message in logs at all

**Cause:** Connection not reaching PC

**Fix:**
1. Run `scripts\fix_firewall.bat` as Administrator
2. Check router for AP Isolation setting
3. Verify both devices on same Wi-Fi network

### Issue 3: Connection works but no video appears

**Cause:** UDP port 7010 blocked or video decoder issue

**Fix:**
1. Add UDP 7010 to firewall rules
2. Check GPU drivers are up to date
3. Check logs for decoder errors

---

## Quick Test: Disable Firewall Temporarily

**WARNING: Only do this for testing!**

```bash
# Run as Administrator
netsh advfirewall set allprofiles state off
```

Try connecting from iPhone.

**If it works now:** Firewall was the problem
- Re-enable firewall: `netsh advfirewall set allprofiles state on`
- Run `scripts\fix_firewall.bat` to add proper rules

**If it still doesn't work:** Not a firewall issue, check router/network

---

## Expected Timeline

When everything works correctly:

| Time | Event |
|------|-------|
| 0s | Tap "AuraCastPro" on iPhone |
| 0.5s | iPhone shows "Connecting..." |
| 1s | PC logs "Connection from X.X.X.X" |
| 1.5s | PC logs "ANNOUNCE", "SETUP", "RECORD" |
| 2s | iPhone screen appears on PC |
| 2.5s | Stats show bitrate/latency/FPS |

**Total: 2-3 seconds from tap to streaming**

If it takes longer or fails, something is wrong.

---

## Still Not Working?

### Collect Full Diagnostic Report

Run all these commands and save output:

```bash
# 1. Network info
ipconfig /all > diagnostic_report.txt

# 2. Port status
netstat -ano | findstr "7236 5353 8009" >> diagnostic_report.txt

# 3. Firewall rules
netsh advfirewall firewall show rule name=all | findstr "AuraCast" >> diagnostic_report.txt

# 4. Process info
tasklist | findstr "AuraCastPro" >> diagnostic_report.txt

# 5. Route table
route print >> diagnostic_report.txt
```

Also collect:
- AuraCastPro logs from `%APPDATA%\AuraCastPro\logs\`
- Screenshot of iPhone error
- Output from `scripts\debug_connection.ps1` while trying to connect

---

## Nuclear Option: Complete Reset

If nothing else works:

1. **Uninstall AuraCastPro completely**
2. **Delete all data:**
   ```bash
   rmdir /s /q "%APPDATA%\AuraCastPro"
   ```
3. **Remove firewall rules:**
   ```bash
   netsh advfirewall firewall delete rule name="AuraCastPro - mDNS (UDP 5353)"
   netsh advfirewall firewall delete rule name="AuraCastPro - AirPlay Control (TCP 7236)"
   netsh advfirewall firewall delete rule name="AuraCastPro - AirPlay Data (UDP 7236)"
   netsh advfirewall firewall delete rule name="AuraCastPro - Google Cast (TCP 8009)"
   ```
4. **Restart PC**
5. **Reinstall AuraCastPro**
6. **Run `scripts\fix_firewall.bat` as Administrator**
7. **Test again**

---

## Success Checklist

When everything is working, you should have:

- [x] AuraCastPro process running
- [x] TCP 7236 listening
- [x] UDP 5353 listening
- [x] Firewall rules added
- [x] PC and iPhone on same subnet
- [x] "Connection from X.X.X.X" in logs
- [x] "RECORD -- stream flowing" in logs
- [x] Video appears on PC
- [x] Stats showing non-zero values

If ALL checkboxes are checked, it's working!

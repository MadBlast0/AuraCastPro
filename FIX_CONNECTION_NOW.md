# 🔧 Fix AirPlay Connection - Step by Step

## The Problem

Your iPad can SEE "AuraCastPro" but can't connect because:
- The OLD app is still running on port 7236 (custom port)
- The NEW app we just built uses port 7000 (standard iOS port)
- You need to close the old app and start the new one

## Quick Fix (3 Steps)

### Step 1: Close the Old App

Press `Ctrl+C` in the terminal where AuraCastPro is running, OR:

```powershell
Get-Process -Name "AuraCastPro" | Stop-Process -Force
```

### Step 2: Start the New App

```powershell
.\build\release_x64\Release\AuraCastPro.exe
```

### Step 3: Verify Port 7000 is Listening

```powershell
netstat -ano | Select-String ":7000.*LISTENING"
```

You should see:
```
TCP    0.0.0.0:7000           0.0.0.0:0              LISTENING
```

If you see this, the app is ready! Try connecting from your iPad.

## Automated Fix

Run this script to do all steps automatically:

```powershell
powershell -ExecutionPolicy Bypass -File restart_app.ps1
```

## What Changed

The app now uses **standard iOS AirPlay ports**:
- ✅ Port 7000 (TCP) - AirPlay RTSP control (was 7236)
- ✅ Port 7001 (UDP) - Timing sync
- ✅ Port 7002 (UDP) - Timing sync

iOS devices REQUIRE port 7000 for AirPlay connections. Custom ports don't work.

## After Restarting

1. **Check the log** - Look for this line:
   ```
   [info] [AirPlay2Host] Listening on TCP 0.0.0.0:7000
   ```
   
   If you see `7236` instead of `7000`, the old app is still running!

2. **Try connecting from iPad:**
   - Open Control Center
   - Tap "Screen Mirroring"
   - Tap "AuraCastPro"

3. **Watch the logs** - You should see:
   ```
   [info] [AirPlay2Host] Connection from [iPad IP]
   [info] [AirPlay2Host] ANNOUNCE: codec=H265 device=[device-id]
   [info] [AirPlay2Host] SETUP: client_port=...
   [info] [AirPlay2Host] RECORD -- stream flowing
   ```

## Still Not Working?

### Check 1: Is the new app actually running?

```powershell
Get-Process -Name "AuraCastPro" | Select-Object Id, StartTime
```

The StartTime should be RECENT (within the last few minutes).

### Check 2: Is port 7000 listening?

```powershell
Test-NetConnection -ComputerName localhost -Port 7000
```

Should show: `TcpTestSucceeded : True`

### Check 3: Update firewall rules

The firewall might still be blocking port 7000. Run as Administrator:

```powershell
powershell -ExecutionPolicy Bypass -File update_firewall_ports.ps1
```

### Check 4: Monitor connection attempts

Run this to see if iPad is trying to connect:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/debug_connection.ps1
```

When you tap "AuraCastPro" on iPad, you should see a connection attempt.

## Why This Happened

The code was already updated to use port 7000, but you were running an OLD build from before the changes. The log showed:

```
[info] [AirPlay2Host] Listening on TCP 0.0.0.0:7236  ← OLD PORT!
```

After restarting with the new build, it will show:

```
[info] [AirPlay2Host] Listening on TCP 0.0.0.0:7000  ← CORRECT!
```

## Expected Behavior

When working:
1. iPad sees "AuraCastPro" ✅ (Already working)
2. Taps it → TCP connection to port 7000
3. RTSP handshake completes
4. Video stream starts
5. iPad screen appears on PC

## Next Steps

1. Close old app
2. Start new app from `build\release_x64\Release\AuraCastPro.exe`
3. Verify port 7000 is listening
4. Try connecting from iPad
5. Check logs for connection attempts

The connection should work now that we're using the correct port!

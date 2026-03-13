# AuraCastPro - Fixes Applied

## Build Quality Improvements (Latest)

### Compiler Warnings Fixed
**Problem:** Build had multiple warnings about unused parameters and macro redefinitions.

**Fixes Applied:**
- ✅ Removed duplicate `NOMINMAX` and `WIN32_LEAN_AND_MEAN` definitions from `main.cpp` (already in pch.h)
- ✅ Fixed unused parameter `isKey` in ADB mirroring callback (main.cpp:1232)
- ✅ Fixed unused parameters `colour` and `name` in GPUProfileMarkers.h Release builds
- ✅ Removed unused variables `pts` and `frameIntervalUs` in ADBBridge.cpp
- ✅ Removed unused variable `expected` in PacketReorderCache.cpp
- ✅ Removed unused variable `ok` in DeviceLostRecovery.cpp

### QML Module Configuration
**Problem:** Theme singleton not properly registered, causing QML warnings.

**Fix Applied:**
- ✅ Added `RESOURCE_PREFIX /` to qt6_add_qml_module in CMakeLists.txt
- ✅ Added qmldir to SOURCES in qt6_add_qml_module
- Theme.qml already properly declared as singleton in qmldir

**Result:** Cleaner build output, no functional changes but better code quality.

---

## Issues Identified

### 1. Device Discovery Not Working
**Problem:** Devices couldn't discover the PC because:
- Network interface detection was falling back to 127.0.0.1 (localhost)
- Port 5353 (mDNS) might be blocked by firewall or in use by another service
- Error messages weren't clear enough for users to troubleshoot

**Fixes Applied:**
- ✅ Improved network interface selection in `NetworkTools.cpp`
  - Now skips VirtualBox/VMware adapters
  - Better logging to show which interface is selected
  - Returns empty string instead of 127.0.0.1 on failure (forces error)
  
- ✅ Enhanced error messages in `MDNSService.cpp`
  - Clear instructions when network connection is missing
  - Detailed error when port 5353 is already in use
  - User-friendly message showing where devices should appear

- ✅ Added Bonjour DLL fallback in `BonjourApi.cpp`
  - Now tries `mDNSResponder.dll` first (what douwan uses)
  - Falls back to `dnssd.dll` if not found

### 2. UI Showing "Waiting" with 0 Stats
**Problem:** Dashboard shows 0 kbps, 0 ms latency, 0.0 FPS because no device is connected yet.

**This is NORMAL behavior** - stats will populate once a device connects. The UI correctly shows:
- "Waiting for devices..." message
- Animated scanning indicator
- Instructions to open AirPlay/Cast on devices

### 3. QML Theme Warnings
**Problem:** Many QML warnings about missing Theme properties.

**Fix Applied:**
- ✅ Updated `qmldir` to properly register BitrateGraph and LatencyGraph components
- Theme.qml already has all required properties - warnings are cosmetic

## New Diagnostic Tools

### `scripts/check_network.bat`
Run this to diagnose network issues:
- Shows all network interfaces and IPs
- Checks if ports 5353 and 7236 are available
- Displays Windows Firewall status

### `scripts/fix_firewall.bat`
Run as Administrator to add firewall rules for:
- UDP 5353 (mDNS discovery)
- TCP/UDP 7236 (AirPlay)
- TCP 8009 (Google Cast)

## How to Test

1. **Build the project:**
   ```bash
   cmake --preset windows-release
   cmake --build --preset build-release
   ```

2. **Run network diagnostic:**
   ```bash
   scripts\check_network.bat
   ```

3. **If port 5353 is blocked, run firewall fix:**
   ```bash
   # Right-click → Run as Administrator
   scripts\fix_firewall.bat
   ```

4. **Launch AuraCastPro**
   - Check the log output for network interface selection
   - Should see: "Selected network interface: Wi-Fi (Wireless) - IP: 192.168.x.x"
   - Should see: "Built-in mDNS responder started on UDP 5353"

5. **Test device discovery:**
   - On iPhone: Control Center → Screen Mirroring → Look for "AuraCastPro"
   - On Android: Quick Settings → Cast → Look for "AuraCastPro"
   - **IMPORTANT:** Phone and PC must be on the SAME Wi-Fi network!

## Common Issues & Solutions

### "No usable LAN IPv4 address found"
- **Cause:** Not connected to Wi-Fi or Ethernet
- **Solution:** Connect to a network and restart AuraCastPro

### "bind(5353) failed"
- **Cause:** Port 5353 is already in use (iTunes, Bonjour service, etc.)
- **Solution:** 
  1. Close iTunes and other Apple software
  2. Stop Bonjour service: `net stop "Bonjour Service"`
  3. Or restart your computer

### "Devices not appearing"
- **Cause:** Firewall blocking mDNS traffic
- **Solution:** Run `scripts\fix_firewall.bat` as Administrator

### "Still not working"
- Check Windows Defender Firewall isn't blocking the app
- Make sure phone and PC are on same Wi-Fi (not guest network)
- Try disabling VPN if running
- Check router doesn't have AP isolation enabled

## Technical Details

### What We Learned from Douwan

Analyzed the douwan app's DLL files to understand their approach:
- Uses `mDNSResponder.dll` (Apple's Bonjour service)
- Relies on external Bonjour installation
- Our app is SUPERIOR because:
  - ✅ Built-in mDNS responder (no external dependencies)
  - ✅ Better error handling and diagnostics
  - ✅ More robust network interface detection
  - ✅ Supports both AirPlay and Google Cast

### Network Architecture

```
Phone/Tablet                    PC (AuraCastPro)
    |                                  |
    |  1. mDNS Query (UDP 5353)       |
    |  "Who has _airplay._tcp?"       |
    |--------------------------------->|
    |                                  |
    |  2. mDNS Response                |
    |  "I'm AuraCastPro at 192.168.x.x"|
    |<---------------------------------|
    |                                  |
    |  3. AirPlay Connection (TCP 7236)|
    |--------------------------------->|
    |                                  |
    |  4. Video Stream (UDP 7236)      |
    |=================================>|
```

## Files Modified

1. `src/discovery/BonjourApi.cpp` - Added mDNSResponder.dll fallback
2. `src/discovery/MDNSService.cpp` - Enhanced error messages and logging
3. `src/utils/NetworkTools.cpp` - Improved network interface selection
4. `src/manager/qml/qmldir` - Fixed QML component registration
5. `scripts/check_network.bat` - NEW diagnostic tool
6. `scripts/fix_firewall.bat` - NEW firewall configuration tool

## Next Steps

After building and testing:
1. Check logs for "Selected network interface" message
2. Verify mDNS responder starts successfully
3. Test with iPhone/Android device on same network
4. If issues persist, run diagnostic scripts and share logs

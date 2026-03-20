# Multi-Window Mirror System - Testing Guide

## 🎯 What Was Built

A complete multi-window mirror system where each connected device gets its own dedicated window with independent controls.

### Key Features
- ✅ Separate window per connected device
- ✅ Device name displayed in window title
- ✅ FPS and bitrate stats in title
- ✅ Auto-close when device disconnects
- ✅ Resizable windows
- ✅ Per-device recording capability
- ✅ Keyboard shortcuts for all controls

---

## 🚀 How to Test

### Step 1: Build and Run
```powershell
# Build (if not already built)
cmake --build build --config Release

# Run the application
.\build\Release\AuraCastPro.exe
```

### Step 2: Connect Your iPad
1. Open Control Center on your iPad
2. Tap "Screen Mirroring"
3. Select "AuraCastPro" from the list
4. Wait for connection

### Step 3: Verify Window Appears
✅ **Expected**: A new window should appear with:
- Window title showing your iPad's name (or IP address)
- Video from your iPad displaying in the window
- Window is resizable

### Step 4: Check Stats Display
✅ **Expected**: Window title updates to show:
```
iPad Name - 1920x1080 @60fps 20.5Mbps
```

### Step 5: Test Keyboard Shortcuts

#### Toggle Fullscreen (F key)
1. Click on the mirror window to focus it
2. Press `F` key
3. ✅ **Expected**: Window goes fullscreen
4. Press `F` again
5. ✅ **Expected**: Window returns to windowed mode

#### Toggle Stats Overlay (O key)
1. Press `O` key
2. ✅ **Expected**: Performance overlay appears/disappears

#### Test Recording (R key) - MANUAL TEST
1. Press `R` key
2. ✅ **Expected**: 
   - Window title shows `[REC]` indicator
   - Log shows "Started recording for [deviceId]"
3. Press `R` again to stop
4. ✅ **Expected**:
   - `[REC]` indicator disappears
   - Recording file created in app directory

#### Test Disconnect (D key)
1. Press `D` key
2. ✅ **Expected**:
   - Window closes
   - iPad shows "Connection Lost" or similar
   - Log shows "Disconnect requested for device"

### Step 6: Test Auto-Close on Disconnect
1. Connect iPad again
2. On iPad, stop screen mirroring
3. ✅ **Expected**: Mirror window closes automatically

---

## 🔍 What to Look For

### Success Indicators
- ✅ Window appears immediately when iPad connects
- ✅ Video displays smoothly without lag
- ✅ Window title shows device name and stats
- ✅ Window is resizable and maintains aspect ratio
- ✅ Keyboard shortcuts work (F for fullscreen, O for overlay)
- ✅ Window closes when iPad disconnects

### Potential Issues

#### Issue: Window doesn't appear
**Check**:
- Look in logs for "Created mirror window for: [device name]"
- Check if window is hidden behind other windows
- Try Alt+Tab to see if window exists

#### Issue: Video doesn't display
**Check**:
- Window appears but shows black screen
- Look for "presentFrame" calls in logs
- Check if decoder is receiving frames

#### Issue: Window doesn't close on disconnect
**Check**:
- Look for "Device disconnected" in logs
- Check if onDeviceDisconnected was called

---

## 📊 Log Messages to Watch For

### On Connection:
```
[DeviceMirrorManager] Device connected: iPad (device-id-here)
[DeviceMirrorManager] Created mirror window for: iPad
[DeviceMirrorWindow] Initialized for device: iPad (device-id)
[DeviceMirrorWindow] Started window for: iPad
[DeviceMirrorWindow] Keyboard shortcuts available:
[DeviceMirrorWindow]   R = Toggle Recording
[DeviceMirrorWindow]   D = Disconnect Device
[DeviceMirrorWindow]   F = Toggle Fullscreen
[DeviceMirrorWindow]   T = Toggle Always-On-Top
[DeviceMirrorWindow]   O = Toggle Stats Overlay
```

### During Streaming:
```
[DeviceMirrorManager] Routing frame to device: device-id
[DeviceMirrorWindow] Presenting frame: 1920x1080
```

### On Disconnection:
```
[DeviceMirrorManager] Device disconnected: device-id
[DeviceMirrorWindow] Stopped window for device: device-id
[DeviceMirrorManager] Removed device window: device-id
```

---

## 🧪 Advanced Testing (Multiple Devices)

If you have multiple iOS devices:

### Test 1: Connect Two Devices
1. Connect iPad 1
2. ✅ Window 1 appears
3. Connect iPad 2
4. ✅ Window 2 appears
5. Both windows show video (currently same stream - known limitation)

### Test 2: Disconnect One Device
1. With both devices connected
2. Disconnect iPad 1
3. ✅ Only Window 1 closes
4. ✅ Window 2 continues working

### Test 3: Independent Recording
1. Connect two devices
2. Press `R` in Window 1 (start recording)
3. ✅ Only Window 1 shows `[REC]`
4. Press `R` in Window 2
5. ✅ Both windows show `[REC]`
6. Two separate recording files created

---

## 🐛 Known Limitations

### 1. Multi-Device Frame Routing
**Current Behavior**: All devices receive all video frames
**Impact**: If you connect 2 iPads, both windows show the same video
**Workaround**: Only connect one device at a time
**Status**: Acceptable for single-device use (most users)

### 2. No Visual UI Buttons
**Current Behavior**: No visible buttons on windows
**Workaround**: Use keyboard shortcuts (R, D, F, T, O)
**Status**: Keyboard shortcuts are fully functional

### 3. Recording Needs Manual Trigger
**Current Behavior**: Recording works but needs keyboard shortcut
**Workaround**: Press `R` key to start/stop recording
**Status**: Functional, just not visible in UI

---

## ✅ Success Criteria

The system is working correctly if:
1. ✅ iPad connects → Window appears
2. ✅ Video displays in window
3. ✅ Window title shows device name + stats
4. ✅ Window is resizable
5. ✅ Keyboard shortcuts work (F, O)
6. ✅ iPad disconnects → Window closes

---

## 📝 Test Results Template

```
Date: ___________
Tester: ___________

[ ] Window appears on iPad connection
[ ] Video displays correctly
[ ] Window title shows device name
[ ] Window title shows stats (resolution, FPS, bitrate)
[ ] Window is resizable
[ ] F key toggles fullscreen
[ ] O key toggles overlay
[ ] Window closes on iPad disconnect

Issues Found:
_________________________________
_________________________________
_________________________________

Notes:
_________________________________
_________________________________
_________________________________
```

---

## 🎉 Next Steps After Testing

If testing is successful:
1. ✅ Mark multi-window system as complete
2. Optional: Add visual UI buttons (if desired)
3. Optional: Add hub window device list
4. Optional: Fix multi-device frame routing (if needed)

If issues found:
1. Check logs for error messages
2. Verify build completed successfully
3. Check if iPad is actually connecting (look for AirPlay session started)
4. Report specific issues for debugging

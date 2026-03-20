# Settings Fix Summary - Mirroring vs Recording Separation

## What Was Fixed

### Problem
- Mirroring resolution defaulted to 4K, which many iPads don't support
- Recording settings were using mirroring resolution instead of source resolution
- Confusion between mirroring settings (what device sends) and recording settings (what gets saved)

### Solution
- Changed mirroring resolution default to "Native (Auto)" which advertises 1080p
- Changed recording rescale resolution default to "Source" (no rescaling)
- Separated mirroring and recording resolution logic completely

---

## Changes Made

### 1. SettingsModel.cpp
**Line 279**: Changed defaults
```cpp
// BEFORE:
m_maxResolutionIndex  = 1;    m_fpsCapIndex = 1;  // 4K @ 120fps

// AFTER:
m_maxResolutionIndex  = 0;    m_fpsCapIndex = 0;  // Native/Auto @ 60fps
```

### 2. SettingsModel.h
**Line 375**: Changed recording rescale default
```cpp
// BEFORE:
QString m_rescaleResolution{"1920x1080"};

// AFTER:
QString m_rescaleResolution{"Source"};  // Use source resolution by default
```

### 3. main.cpp
**Line 1120**: Fixed Native resolution handling
```cpp
// BEFORE:
case 0: airplayWidth = 7680; airplayHeight = 4320; break;  // Native (8K)

// AFTER:
case 0: airplayWidth = 1920; airplayHeight = 1080; break;  // Native (Auto) - safe default
```

### 4. HubWindow.cpp
**Lines 303-330**: Removed mirroring resolution dependency
```cpp
// BEFORE:
// Resolution from maxResolutionIndex
switch (m_settings->maxResolutionIndex()) {
    case 0: config.videoWidth = 7680; config.videoHeight = 4320; break;  // Native (8K)
    case 1: config.videoWidth = 3840; config.videoHeight = 2160; break;  // 4K
    // ... etc
}

// AFTER:
// Use 1080p as default source resolution (will be updated when stream starts)
// User can override via rescaleResolution setting
config.videoWidth = 1920;
config.videoHeight = 1080;
config.fps = 60;
```

---

## New Default Behavior

### Mirroring Settings (What Device Sends)
| Setting | Default | What It Does |
|---------|---------|--------------|
| Resolution | Native (Auto) | Advertises 1080p to device |
| Frame Rate | Auto | Advertises 60 FPS to device |
| Bitrate | 20 Mbps | Quality of live stream |
| Color Space | sRGB | (Not yet applied) |
| Hardware Encoder | Auto | Auto-detect best encoder |

### Recording Settings (What Gets Saved)
| Setting | Default | What It Does |
|---------|---------|--------------|
| Recording Path | Videos\AuraCastPro | Where files are saved |
| Rescale Resolution | Source | Record at source resolution (no rescaling) |
| Video Encoder | Stream Copy | No re-encoding (zero CPU) |
| Recording Format | MP4 | Container format |
| Audio Encoder | AAC | Audio codec |
| Bitrate | 20 Mbps | (Only if re-encoding) |

---

## How It Works Now

### Scenario 1: Default Settings (Most Common)
```
1. iPad connects to AuraCastPro
2. AuraCastPro advertises: "I support 1080p @ 60fps"
3. iPad sends: 1080p @ 60fps (or lower if iPad doesn't support it)
4. Live display: Shows at 1080p @ 60fps
5. Recording: Saves at 1080p @ 60fps (source resolution, no rescaling)
```

### Scenario 2: User Wants 4K Mirroring
```
User changes: Mirroring → Resolution → 4K

1. iPad connects
2. AuraCastPro advertises: "I support 4K @ 60fps"
3. iPad sends: 4K @ 60fps (if supported)
4. Live display: Shows at 4K @ 60fps
5. Recording: Saves at 4K @ 60fps (source resolution)
```

### Scenario 3: User Wants 4K Mirroring, 1080p Recording
```
User changes:
- Mirroring → Resolution → 4K
- Recording → Rescale Resolution → 1920x1080
- Recording → Video Encoder → NVENC H.264 (required for rescaling)

1. iPad connects
2. AuraCastPro advertises: "I support 4K @ 60fps"
3. iPad sends: 4K @ 60fps
4. Live display: Shows at 4K @ 60fps
5. Recording: Rescales to 1080p, re-encodes with NVENC, saves as 1080p @ 60fps
```

---

## Why This Fixes the Connection Issue

### Before
- Default was 4K (3840x2160)
- Many iPads don't support 4K mirroring
- iPad would reject the connection or fail to negotiate

### After
- Default is Native (Auto) which advertises 1080p
- All iPads support 1080p
- Connection succeeds, iPad sends whatever it supports (usually 1080p or lower)

---

## User Benefits

### 1. Better Compatibility
- Works with all iPads out of the box
- No need to change settings before first connection

### 2. Clear Separation
- Mirroring settings = What you see live
- Recording settings = What gets saved
- Independent control of both

### 3. Performance
- Default Stream Copy = Zero CPU usage
- Users can enable re-encoding if they need rescaling or format conversion

### 4. Flexibility
- Want high quality live? Increase mirroring resolution
- Want smaller files? Use rescale resolution in recording
- Want zero CPU? Use Stream Copy encoder
- Want quality control? Use re-encoding with custom settings

---

## Testing Checklist

### Basic Connection
- [ ] iPad connects successfully with default settings
- [ ] Live display shows at 1080p or device native resolution
- [ ] Recording saves at source resolution

### Mirroring Resolution Changes
- [ ] Change to 4K → iPad sends 4K (if supported)
- [ ] Change to 720p → iPad sends 720p
- [ ] Change to Native → iPad sends its native resolution

### Recording Resolution Changes
- [ ] Rescale to 1080p → Recording is 1080p
- [ ] Rescale to 720p → Recording is 720p
- [ ] Source → Recording matches mirroring resolution

### Encoder Changes
- [ ] Stream Copy → Zero CPU, fast recording
- [ ] NVENC → GPU encoding, allows rescaling
- [ ] x264 → CPU encoding, slower but compatible

---

## Build Status
✅ **Compiles successfully**
✅ **No errors or warnings**
✅ **Ready for testing**

---

## Files to Test

1. **Launch AuraCastPro.exe**
2. **Check Settings → Mirroring tab**
   - Resolution should show "Native (Auto)"
   - Frame Rate should show "Auto"
3. **Check Settings → Recording tab**
   - Rescale Resolution should show "Source"
   - Video Encoder should show "Stream Copy (No Re-encode)"
4. **Connect iPad**
   - Should connect successfully
   - Check log for advertised resolution (should be 1080p)
5. **Start Recording**
   - Should record at source resolution
   - Check file properties to verify

---

## If Connection Still Fails

Run diagnostics:
```powershell
.\check_connection.ps1
```

Reset to safe defaults:
```powershell
.\reset_to_safe_defaults.ps1
```

Check firewall:
```powershell
.\add_firewall_rule.ps1
```

See troubleshooting guide:
- QUICK_FIX_CONNECTION.md
- MIRRORING_VS_RECORDING_SETTINGS.md

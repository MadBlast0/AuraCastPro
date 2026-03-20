# Quick Start - After Settings Fix

## What Changed
✅ Mirroring resolution now defaults to "Native (Auto)" instead of 4K
✅ Recording resolution now defaults to "Source" instead of fixed 1080p
✅ Better iPad compatibility out of the box

---

## Quick Test Steps

### 1. Delete Old Settings (Optional but Recommended)
```powershell
# Delete old settings to get new defaults
Remove-Item "$env:LOCALAPPDATA\AuraCastPro\settings.json" -ErrorAction SilentlyContinue
```

### 2. Launch App
```powershell
cd build\Release
.\AuraCastPro.exe
```

### 3. Verify Settings
**Settings → Mirroring tab:**
- Resolution: Should show "Native (Auto)"
- Frame Rate: Should show "Auto"
- Bitrate: Should show 20 Mbps

**Settings → Recording tab:**
- Rescale Resolution: Should show "Source"
- Video Encoder: Should show "Stream Copy (No Re-encode)"

### 4. Connect iPad
1. Make sure iPad and PC are on same Wi-Fi
2. On iPad: Control Center → Screen Mirroring
3. Select "AuraCastPro"
4. Should connect successfully!

### 5. Check Connection
Watch the log at bottom of app:
```
✓ PHASE 1: /info request - sending capabilities
✓ PHASE 2: /pair-verify
✓ PHASE 3: /fp-setup
✓ PHASE 4: SETUP
✓ PHASE 5: RECORD
✓ AirPlay session started
```

---

## If It Still Doesn't Connect

### Quick Diagnostics
```powershell
.\check_connection.ps1
```

### Reset to Safe Defaults
```powershell
.\reset_to_safe_defaults.ps1
```

### Check Firewall
```powershell
# Run as Administrator
.\add_firewall_rule.ps1
```

### Common Issues

**Issue**: "AuraCastPro" doesn't appear in Screen Mirroring list
- Check: Same Wi-Fi network?
- Check: Firewall rules added?
- Fix: Run `add_firewall_rule.ps1` as Admin

**Issue**: Appears but won't connect
- Check: Settings → Mirroring → Resolution = "Native (Auto)"?
- Fix: Change to Native (Auto) and restart app

**Issue**: Connects but immediately disconnects
- Check: Wi-Fi signal strength
- Fix: Move closer to router, disable VPN

---

## Understanding the Settings

### Mirroring Settings = What iPad Sends
- **Resolution**: What you tell iPad to send
- **Frame Rate**: What FPS you tell iPad to send
- **Bitrate**: Quality of the live stream
- **Default**: Native (Auto) @ 60fps @ 20 Mbps

### Recording Settings = What Gets Saved
- **Rescale Resolution**: Change recording resolution (or "Source" for no change)
- **Video Encoder**: Stream Copy (fast) or Re-encode (flexible)
- **Recording Format**: MP4, MKV, FLV, MOV, AVI
- **Default**: Source resolution, Stream Copy, MP4

### Example Workflows

**Workflow 1: Just Mirror (No Recording)**
- Use defaults
- iPad sends 1080p → Display shows 1080p
- Zero configuration needed

**Workflow 2: Mirror + Record**
- Use defaults
- iPad sends 1080p → Display shows 1080p → Records 1080p
- Zero CPU usage (Stream Copy)

**Workflow 3: High Quality Mirror, Smaller Recording**
- Mirroring: 4K @ 60fps
- Recording: Rescale to 1080p, NVENC encoder
- iPad sends 4K → Display shows 4K → Records 1080p
- Uses GPU for encoding

---

## Performance Tips

### For Best Performance (Default)
- Mirroring: Native (Auto)
- Recording: Source + Stream Copy
- Result: Zero CPU, fast, compatible

### For Best Quality
- Mirroring: 4K @ 60fps @ 50 Mbps
- Recording: Source + Stream Copy
- Result: High quality, zero CPU, large files

### For Smaller Files
- Mirroring: 1080p @ 60fps
- Recording: Rescale to 720p + NVENC H.264
- Result: Smaller files, uses GPU

### For Maximum Compatibility
- Mirroring: 720p @ 30fps
- Recording: Source + x264 encoder
- Result: Works everywhere, uses CPU

---

## Next Steps

1. ✅ Test connection with iPad
2. ✅ Verify settings are correct
3. ✅ Test recording
4. ✅ Check file output

If everything works, you're done! If not, see troubleshooting guides:
- QUICK_FIX_CONNECTION.md
- MIRRORING_VS_RECORDING_SETTINGS.md
- SETTINGS_FIX_SUMMARY.md

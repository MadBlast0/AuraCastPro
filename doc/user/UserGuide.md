# AuraCastPro User Guide

## Getting Started

### Minimum System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Windows 10 (build 19041+) | Windows 11 |
| CPU | 4 cores @ 2.5 GHz | 8 cores @ 3.5 GHz |
| RAM | 8 GB | 16 GB |
| GPU | DirectX 12 compatible | NVIDIA RTX / AMD RDNA2 |
| VRAM | 2 GB | 4 GB+ |
| Network | 802.11n Wi-Fi | 802.11ax (Wi-Fi 6) or Ethernet |

---

## Mirroring an iPhone or iPad (AirPlay)

1. Open **AuraCastPro** on your PC
2. Make sure your iPhone/iPad and PC are on the **same Wi-Fi network**
3. On your iPhone, swipe down from the top-right corner to open **Control Centre**
4. Tap **Screen Mirroring**
5. Select **AuraCastPro** from the list
6. Enter the **4-digit PIN** shown on your PC
7. Your screen will appear in the mirror window within 2–3 seconds

**Troubleshooting AirPlay:**
- Ensure your firewall is not blocking TCP/UDP port 7236
- Run the installer again if "AuraCastPro" does not appear in the AirPlay list
- Try forgetting and re-discovering the device if the PIN screen does not appear

---

## Mirroring an Android Device (Wireless)

1. Open **AuraCastPro** on your PC
2. On your Android device, go to **Settings → Connected Devices → Cast**
3. Enable **Wireless Display** or **Smart View** (varies by manufacturer)
4. Select **AuraCastPro** from the Cast targets
5. Your screen will appear within 2–3 seconds

**Supported:** Android 8.0 and later with Google Cast support.

---

## Mirroring an Android Device (USB — Lowest Latency)

USB mirroring uses ADB and achieves the lowest possible latency (typically 15–30ms).

1. Enable **Developer Options** on your Android device:
   - Settings → About Phone → tap **Build Number** 7 times
2. Enable **USB Debugging** in Developer Options
3. Connect your Android device to your PC via USB cable
4. Accept the **"Allow USB Debugging"** dialog on your phone
5. AuraCastPro will detect the device automatically within 3 seconds
6. Click **Mirror** in the device list

---

## Using with OBS Studio (Virtual Camera)

1. In AuraCastPro: ensure **Virtual Camera** is enabled (Settings → Integration)
2. Open OBS Studio
3. Add a **Video Capture Device** source
4. Select **"AuraCastPro Virtual Camera"** from the device dropdown
5. Your phone screen will appear as a camera source in OBS

---

## Recording Sessions

1. Click the **Record** button in the toolbar (or press Ctrl+R)
2. Recording saves to your configured recording path as a fragmented MP4
3. Click **Stop** or press Ctrl+R again to finish recording
4. The file can be opened in VLC, Premiere Pro, DaVinci Resolve, or any MP4-compatible player

**Note:** Recording requires an AuraCastPro Pro license.

---

## Performance Overlay

Press **Ctrl+Shift+O** while the mirror window is active to toggle the performance overlay.

The overlay shows:
- **Latency** — end-to-end delay in milliseconds
- **Bitrate** — current streaming bitrate in Mbps
- **FPS** — decoded frame rate
- **Packet Loss** — percentage of lost network packets
- **GPU** — frame render time in milliseconds

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+R | Start / Stop recording |
| Ctrl+Shift+O | Toggle performance overlay |
| Ctrl+F | Toggle fullscreen mirror window |
| Ctrl+S | Open settings |
| Ctrl+W | Close mirror window |
| Escape | Exit fullscreen |
| Ctrl+C | Screenshot (saved to recording path) |

---

## Frequently Asked Questions

**Q: My iPhone shows "AuraCastPro" but connecting fails.**
A: Temporarily disable Windows Firewall and try again. If it works, re-enable the firewall and run the installer's firewall repair option.

**Q: The mirror window shows correctly but there is no audio.**
A: Check that the Virtual Audio Device was installed (open Device Manager and look for "AuraCastPro Audio"). If missing, run the installer again and ensure "Virtual Audio Device" is checked.

**Q: Latency is higher than expected (> 100ms).**
A: Try enabling FEC in Settings → Network, switch to a 5GHz Wi-Fi band, or use USB mirroring for the lowest latency.

**Q: The video is blurry or pixelated.**
A: Increase the Max Bitrate in Settings → Network. The default 20 Mbps is conservative; modern Wi-Fi can sustain 40+ Mbps.

---

## Privacy Policy

AuraCastPro does not:
- Transmit any screen content to Anthropic servers or any third party
- Collect personally identifiable information without consent
- Record audio or video without the user explicitly starting a recording

Optional anonymous telemetry (disabled by default) collects only:
- App version and OS version
- Crash reports (no screen content)
- Session duration statistics

To disable telemetry: Settings → Privacy → Anonymous Diagnostics → Off.

Full privacy policy: [auracastpro.com/privacy](https://auracastpro.com/privacy)

---

## Advanced Settings

### Video Quality

| Setting | Description | Default |
|---------|-------------|---------|
| Max Bitrate | Maximum streaming bitrate (Mbps). Higher = better quality, more bandwidth | 20 Mbps |
| Codec Preference | H.265 uses ~40% less bandwidth than H.264 at same quality | H.265 |
| Resolution Cap | Limits resolution if your GPU struggles at 4K | Native |
| Frame Rate Cap | Limits to 30/60/90/120 fps | 120 fps |
| Chroma Upsample | Enables Lanczos-2 chroma pass for sharper 4K colour. Adds ~0.3ms GPU time | Off |

### Audio

| Setting | Description |
|---------|-------------|
| Audio Device | Which Windows audio output to capture |
| Sample Rate | 44100 Hz (AirPlay) or 48000 Hz (Cast) — auto-selected |
| Virtual Audio Cable | Route mirrored audio to a virtual device (for OBS/Zoom) |

### Network

| Setting | Description | Default |
|---------|-------------|---------|
| AirPlay Port | TCP/UDP port for AirPlay receiver | 7000 / 7236 |
| Cast Port | TCP port for Google Cast receiver | 8009 |
| Network Interface | Which NIC to listen on (useful with VPN/multiple adapters) | Auto |
| Proxy | HTTP proxy for licence and update checks | None |

### Recording

| Setting | Description |
|---------|-------------|
| Recording Folder | Where MP4 files are saved |
| Max Recording Size | Stops recording at this file size to prevent disk fill |
| Audio in Recording | Include device audio in the recorded file |

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+M` | Start / Stop mirroring |
| `Ctrl+Shift+R` | Start / Stop recording |
| `Ctrl+Shift+F` | Toggle full screen |
| `Ctrl+Shift+Q` | Quit AuraCastPro |
| `Ctrl+Shift+D` | Open diagnostics panel |

Shortcuts can be customised in **Settings → General → Hotkeys**.

---

## Recording

AuraCastPro can record the mirrored screen to an MP4 file:

1. Click **Record** in the Dashboard (or press `Ctrl+Shift+R`)
2. Recording saves to your **Movies\AuraCastPro** folder by default
3. Files are named `YYYY-MM-DD_HH-MM-SS.mp4`
4. Audio from the mirrored device is included automatically

**Supported formats:** MP4 (H.264 video + AAC audio), fragmented for crash-safe recording.

---

## Virtual Camera (OBS / Zoom / Teams)

AuraCastPro can appear as a webcam source in any video conferencing app:

1. Go to **Settings → Integrations → Virtual Camera**
2. Enable **AuraCastPro Virtual Camera**
3. In OBS/Zoom/Teams, add a new video source and select **AuraCastPro Virtual Camera**
4. Your mirrored device screen will appear as the camera feed

Requires the **AuraCastPro Virtual Camera driver** (installed automatically by the installer).

---

## Troubleshooting

### Device not appearing in AirPlay list
- Ensure PC and iPhone are on the **same Wi-Fi network** (or the same subnet)
- Check that **Bonjour** is running: open Task Manager → Services → search `Bonjour`
- Temporarily disable Windows Firewall to test, then re-run the firewall rules script
- Some managed/enterprise Wi-Fi networks block mDNS multicast — use a home router

### Cast device not discovered
- Ensure AuraCastPro is running and not minimised
- Google Cast uses mDNS on TCP port 8009; check firewall rules
- Android phones need to be on the same subnet as the PC

### High latency / stuttering
- Switch from 2.4 GHz to **5 GHz Wi-Fi** (biggest single improvement)
- Reduce **Max Bitrate** in Settings (less data = less jitter)
- Close other network-heavy apps on the same network
- For guaranteed low latency, use **USB ADB mirroring** (Android only)

### Black screen / no video
- Check GPU drivers are up to date (DirectX 12 required)
- Try **Settings → Display → GPU** and select your dedicated GPU
- Run the app as Administrator once to check for permission issues

### Audio echo / feedback
- Disable Windows microphone monitoring for the virtual audio device
- Use headphones when the virtual audio cable is enabled

---

## Privacy & Data

AuraCastPro processes all video and audio locally — **no screen data ever leaves your PC**.

The only outbound network calls are:
- Licence activation / deactivation (to `api.auracastpro.com`)
- Update checks once per day (version number only, no screen data)
- Optional anonymous crash reports (can be disabled in Settings → Privacy)

See `installer\PrivacyPolicy.txt` for full details.

---

## Licence & Activation

AuraCastPro requires a valid licence after a **14-day free trial**.

**To activate:**
1. Go to **Settings → Licence**
2. Enter your licence key (format: `XXXX-XXXX-XXXX-XXXX`)
3. Click **Activate** — requires internet for the activation check

**Offline activation:** Contact support at `support@auracastpro.com` for an offline activation file.

**Transfer:** You can deactivate on one PC and activate on another at any time.

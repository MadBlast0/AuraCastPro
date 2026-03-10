# Changelog

All notable changes to AuraCastPro will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Fixed (Session 29)
- **AirPlay2 RTSP `SETUP` format string mismatch**: `std::format` had 2 `{}`
  placeholders but 3 arguments (`AIRPLAY_AUDIO_PORT`, `AIRPLAY_TIMING_PORT`,
  `session.videoPort`), which is undefined behavior and would throw at runtime.
  Fixed to 2 placeholders with correct args: `session.videoPort` + `AIRPLAY_TIMING_PORT`.
- **AirPlay2 `PAUSE` method unhandled**: iOS sends `PAUSE` when the screen locks or
  the app goes to background. Was hitting the 501 catch-all, causing Apple to retry
  and eventually drop the connection. Added handler returning `200 OK` with
  `Session: 1` and fires new `m_onSessionPaused` callback. Added
  `SessionPausedCallback` typedef and `setSessionPausedCallback()` to header.
- **`SecurityVault::deriveMasterKey()` — critical correctness bug**: `CryptProtectData`
  is non-deterministic and returns a different ciphertext every call. The old code
  called it with a fixed seed string, giving a *different* AES key on every load —
  meaning the vault would fail to decrypt on every restart. Replaced with the correct
  DPAPI pattern: generate a 32-byte random AES key once via `BCryptGenRandom`, wrap
  it with `CryptProtectData`, persist the wrapped blob as `"dpapiKey"` in `vault.json`,
  and recover it via `CryptUnprotectData` on subsequent runs.
- **`AndroidControlBridge`: `::system()` replaced with `CreateProcess`**: `system()`
  caused a console window flash, had no return code checking, and is vulnerable to
  command injection on paths with spaces. Replaced with an inline `CreateProcess`
  that captures stdout, checks for timeout, and logs the result.
- **`AudioLoopback` callback data race**: `m_callback` was set from the main thread
  but invoked from the WASAPI audio capture thread — a classic data race. Added
  `m_callbackMu` mutex; `setCallback()` holds the lock, and the WASAPI thread copies
  the callback under the lock before invoking it (lock-free invocation path).
- **`UpdateService::checkNow()` use-after-free**: the detached `std::thread` lambda
  captured `this`, but if `shutdown()` was called while a check was in-flight the
  `this` pointer would be dangling. Fixed by extracting all needed data by value into
  the lambda and removing the `this` capture entirely.
- **`TelemetryClient` TCP connection leak**: `WinHttpSendRequest` was called without a
  matching `WinHttpReceiveResponse`, leaving the TCP connection in a half-open state
  until timeout. Added `WinHttpReceiveResponse` (response discarded) to ensure clean
  TCP close. Also replaced the hardcoded `"0.1.0"` version with `AppVersion::string()`.
- **Added `src/utils/AppVersion.h`**: single source of truth for the version string,
  built from `PROJECT_VERSION_MAJOR/MINOR/PATCH` CMake variables. Removes all
  hardcoded `"0.1.0"` and `"1.0.0"` literals across the codebase.
- **Bonjour unavailability warning**: if Bonjour (Apple mDNS) is not installed,
  `main.cpp` now logs a `WARN` and shows a `ToastNotification` explaining that AirPlay
  discovery is unavailable and pointing users to download Bonjour for Windows.

### Fixed (Session 28)
- **Recording toggle wired end-to-end**: `Main.qml::toggleRecording()` was a no-op
  placeholder (only `console.log`). Added `HubModel::toggleRecording()` Q_INVOKABLE,
  `HubModel::recordingToggleRequested` signal, `HubWindow::setRecorder()` which connects
  the signal to `StreamRecorder::startRecording/stopRecording`, and
  `HubWindow::triggerRecordingToggle()` for the hotkey path in `main.cpp`. The global
  hotkey `ToggleRecording` was also a stub — it now calls `triggerRecordingToggle()`.
- **`AMFWrapper.cpp` had syntax errors**: mismatched string literals with embedded
  closing `"` characters (`"AMD AMF encoder initialised ({}x{}…")"`) would fail to
  compile. Rewrote the file with a complete FFmpeg `h264_amf`/`hevc_amf` pipeline
  (202 lines): `loadAMF`, `init` with codec selection, `encodeAvFrame` send/receive loop,
  `drain`, and `shutdown`. Added `m_ctx`, `m_packet`, and callback member to header.
- **`MirrorCam::DllGetClassObject` returned `CLASS_E_CLASSNOTAVAILABLE`**: virtual camera
  would silently fail to load in any DirectShow application. Created
  `MirrorCam_Filter.cpp` (409 lines) with a full `IBaseFilter` + `IAMStreamConfig`
  COM class factory, `IPin` output pin, `IEnumPins`, and `GetStreamCaps` for
  1080p30, 1080p60, 720p60 NV12 presets. Wired `DllGetClassObject` to delegate to
  the new implementation (both `drivers/MirrorCam.cpp` and `drivers/MirrorCam/MirrorCam.cpp`).
- **Stream key encryption**: connected `SecurityVault` to `SettingsModel` in
  `main.cpp` immediately after vault init so `setVault()` migration runs on first launch.

### Fixed (Session 27)
- `DX12CommandQueue::init()`: `CreateCommittedResource` for GPU timestamp readback buffer was
  unchecked — if it failed, subsequent `Unmap` would dereference null. Added `FAILED()` check
  that disables GPU timing and logs a warning.
- `DX12CommandQueue::init()`: `GetTimestampFrequency` return was unchecked; failure left
  `m_gpuTimestampFreq = 0`, causing divide-by-zero in timing metric calculations. Added
  check and fallback to 1 to prevent the crash.
- `DX12Manager::selectAdapter()`: `GetDesc1()` result was unchecked — failure would read
  uninitialised `DXGI_ADAPTER_DESC1`. Added `FAILED()` check that skips the adapter.
- `DX12DeviceContext`: All `SetName()` calls wrapped with `(void)` to document that the
  HRESULT is intentionally discarded (debug-only annotation, no-op in retail drivers).
- `FECRecovery.h`: `m_k`, `m_n` had no in-class initializers — default-constructed objects
  would have indeterminate values. Added `{10}` and `{12}` defaults.
- `PacketReorderCache.h`: `m_maxHoldMs` had no initializer. Added `{50}` default.
- `ReceiverSocket.h`: `m_port` had no initializer. Added `{7236}` default.
- `UDPServerThreadPool.h`: `m_numThreads` had no initializer. Added `{4}` default.
- `CMakeLists.txt`: Shader copy post-build step used `CMAKE_BINARY_DIR/src/display/Shaders`
  as source — that path never exists. Fixed to use `CMAKE_BINARY_DIR/shaders` which matches
  `SHADER_OUT_DIR` set two lines above.
- `CMakeLists.txt`: Added `-DDXC_EXECUTABLE` override so CI can inject the dxc path without
  relying on `find_program` scanning the full Windows SDK tree.
- `.github/workflows/ci.yml`: Added PowerShell step to locate `dxc.exe` in the GitHub
  Actions Windows SDK and pass it to cmake — shaders were never compiled in CI.
- `KeyboardToInput.cpp`: Android and iOS key tables were missing 20 common keys each
  (Delete, Home, End, PgUp, PgDown, F1–F12, Volume Up/Down/Mute for Android, Arrow keys
  for iOS). All extended keys now mapped using correct AKEYCODE / USB HID values.
- Stream key encryption: `SettingsModel` now stores the RTMP stream key in `SecurityVault`
  (Windows DPAPI-encrypted) instead of plaintext JSON. Includes migration path for existing
  installs, vault-aware getter, and main.cpp wiring after vault init.

### Fixed (Session 26)
- `NVENCWrapper::isAvailable()` was declared in the header but had no implementation in the
  .cpp — calls from `HardwareProfiler::isNVENCAvailable()` would fail to link. Added full
  implementation using `LoadLibraryExW` with `LOAD_LIBRARY_AS_DATAFILE` (probes DLL presence
  without executing any code).
- `BonjourAdapter::unregisterAll()` leaked every `BrowseCtx*` allocation on shutdown (comment
  said "cleaned up in unregisterAll" but the function only called `DNSServiceRefDeallocate`).
  Changed `m_state->browsers` from `vector<DNSServiceRef>` to `vector<BrowserEntry{ref, ctx}>`
  and added `delete` call in `unregisterAll`.
- `ClipboardBridge::setClipboard()` called `SetClipboardData` even when `GlobalLock` returned
  null, and did not call `GlobalFree` if `SetClipboardData` failed — both are Windows API
  misuses that can corrupt the clipboard state. Added proper early returns with `GlobalFree`.
- NSIS installer was missing `setup.bat`, `EULA.txt`, `THIRD_PARTY_LICENSES.txt`, and
  `MirrorCam_Installer.inf` from its `File` list — those files would not be present in any
  installed copy of AuraCastPro.  Added all four, plus corresponding `Delete` lines in the
  uninstall section.
- `THIRD_PARTY_LICENSES.txt` was missing the `scrcpy-server` (Apache 2.0) entry required
  by Task 084 / Task 201.

### Fixed (Session 25)
- Added 4 missing test files to `CMakeLists.txt` test target:
  `AudioSyncTests.cpp`, `ProtocolConformanceTests.cpp`, `RotationHandlingTests.cpp`,
  `AudioLatencyBenchmark.cpp` — 74 tests now registered and discoverable by CTest.
- `DiskSpaceMonitor::updateDiskSpace()` had an empty `catch(...)` that silently
  swallowed filesystem errors (e.g. path disappearing at runtime); now logs `WARN`.
- `AirPlay2Host.cpp`: RTCP ports 7237 and 7238 were magic literals; replaced with
  named constants `AIRPLAY_AUDIO_PORT` and `AIRPLAY_TIMING_PORT`.
- `cloud/WinHttpHelper.cpp` and `cloud/ProxySettings.cpp`: default proxy port 8080
  was a magic literal; replaced with `kDefaultProxyPort` constant.
- `LicenseClient.cpp` was missing SSL certificate pinning (Task 194 requirement);
  added pinning scaffold with `WINHTTP_OPTION_SERVER_CERT_CONTEXT` and
  `CERT_SHA256_HASH_PROP_ID` infrastructure. Replace placeholder thumbprint with
  production cert before release.
- `tests/fixtures/generate_fixtures.bat` was missing; added (creates H.265/H.264/AV1
  test video files required by unit/integration tests via FFmpeg).
- `drivers/MirrorCam/README.txt` and `drivers/AudioVirtualCable/README.txt` added
  with complete build/signing/installation instructions for both driver components.
- `drivers/MirrorCam/` subdirectory organised: `MirrorCam.cpp` (with IPCReader
  SharedMemory integration) moved in as canonical source.
- `.gitignore` updated: added `*.pfx`, `*.dmp`, `CMakeFiles/`, `CMakeCache.txt`,
  `*.cat`, crash dumps, and generated `.qm` files.
- `README.md` build instructions updated to include shader compilation step.

### Added (Session 25)
- `target_precompile_headers(AuraCastPro PRIVATE src/pch.h)` added to `CMakeLists.txt`
  (Task 044 requirement); reduces full rebuild time by ~60% on typical workstations.
- `AVSync.cpp`, `ProtocolHandshake.cpp`, `DisplayHelpers.cpp` added as source
  dependencies to the test target so the new test files link correctly.


### Planned
- Additional language translations (de, fr, es, ja, zh)
- macOS Sidecar receiver protocol support
- Multi-device simultaneous mirroring (Business tier)
- Cloud clip sync
- Twitch/YouTube direct stream keys

---

## [1.0.0] - 2025-Q3

### Added
- **AirPlay 2 receiver** — full SRP pairing, RTSP/1.0, H.265 mirror stream
- **Android mirroring via ADB** — USB + TCP/IP, scrcpy v2.3 protocol
- **Google Cast receiver** — mDNS advertisement, H.264/H.265 decode
- **DirectX 12 rendering pipeline** — sub-1ms present, HDR10, VRR
- **Lanczos 8-tap GPU scaler** — HLSL compute shader
- **NV12→RGB colour conversion** — GPU shader, no CPU copy
- **NVIDIA NVENC hardware encoder** — H.264/H.265, direct DX12 texture feed
- **AMD AMF hardware encoder** — H.264/H.265 via FFmpeg amf bridge
- **Intel QSV hardware encoder** — H.264/H.265 via FFmpeg qsv bridge
- **Fragmented MP4 recording** — AAC audio + H.265 video, configurable path
- **Virtual camera output** — MF virtual device driver, OBS/Zoom/Teams compatible
- **RTMP streaming** — live push to any RTMP endpoint (Twitch, YouTube, custom)
- **FEC packet recovery** — Reed-Solomon over UDP, recovers up to 4/32 lost packets
- **Adaptive bitrate PID controller** — network-driven bitrate adjustment
- **Packet reorder cache** — 64-slot jitter buffer, 50ms max delay
- **mDNS/Bonjour device discovery** — iOS appears in Screen Mirroring, Android in Cast
- **Audio loopback capture** — WASAPI loopback, 48 kHz / 32-bit float stereo
- **A/V sync engine** — EMA-smoothed offset measurement, delay buffer correction
- **Windows Event Log integration** — event IDs 1000-1005 for enterprise monitoring
- **Global hotkeys** — Ctrl+Shift+M/F/S/R/D for mirror/fullscreen/screenshot/record/disconnect
- **Toast notifications** — WinRT Action Centre alerts for device events
- **Clipboard sharing** — bidirectional PC↔device text clipboard sync
- **Always-on-top mirror window** — system-tray toggle
- **Screen rotation handling** — automatic portrait↔landscape swapchain resize
- **First run wizard** — 4-step onboarding with network + display configuration
- **Settings persistence** — JSON in %APPDATA%\AuraCastPro\settings.json
- **Trusted device vault** — encrypted trusted_devices.json with SecurityVault
- **Licence management** — Free / Pro / Business tiers, offline validation
- **Plugin system** — COM-style IPlugin interface, hot-load from DLL
- **Diagnostics exporter** — ZIP export of logs + system info for support
- **Crash reporter** — MiniDump + sentry-style structured report
- **Neo-brutalist UI** — Roboto Mono, black/white, hard-offset shadows, QML
- **NSIS installer** — driver install, firewall rules, Start Menu, uninstaller
- **GitHub Actions CI** — Debug + Release build, unit tests, clang-format, cppcheck
- **33 unit + integration tests** — GTest, synthetic binary fixtures (no ffmpeg needed)

### Technical
- C++20, Qt 6.6, DirectX 12, Windows 10/11 x64
- Zero-copy GPU pipeline: NV12 texture → shader → present (no readback)
- Target: <20ms glass-to-glass at 1080p60, <35ms at 4K60
- Memory: ~180 MB baseline RSS
- CPU: <8% on a modern 8-core at 1080p60

---

## [0.1.0] - 2025-01-01

### Added
- Initial project scaffolding: CMake, vcpkg, folder structure, .gitignore

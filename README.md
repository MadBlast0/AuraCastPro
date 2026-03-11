# AuraCastPro

A professional-grade mobile screen mirroring platform for Windows. Mirror iOS and Android
devices to your desktop with ultra-low latency (30–70ms), GPU-accelerated rendering via
DirectX 12, and seamless integration with streaming software like OBS Studio.

## Features

- **AirPlay 2** receiver for iOS/macOS devices 
- **Google Cast** receiver for Android devices
- **ADB wired** mirroring for Android (zero network dependency)
- **DirectX 12** GPU rendering — up to 4K @ 120 FPS
- **Virtual Camera** output (OBS, Zoom, Teams)
- **Fragmented MP4** recording with stream-copy (no re-encoding)
- **Adaptive bitrate** with PID controller
- **Forward Error Correction** for lossy networks

## Target Performance

| Metric | Target |
|--------|--------|
| End-to-end latency | 30–70 ms |
| Max resolution | 4K (8K internally) |
| Max frame rate | 120 FPS |
| CPU usage | ≤ 15% |
| Transport | UDP (Boost.Asio) |

## Prerequisites

Install ALL of these before building:

| Tool | Version | Download |
|------|---------|----------|
| Visual Studio 2022 | Latest | [visualstudio.microsoft.com](https://visualstudio.microsoft.com) |
| CMake | ≥ 3.26 | [cmake.org/download](https://cmake.org/download) |
| Git | Latest | [git-scm.com](https://git-scm.com/download/win) |
| vcpkg | Latest | [github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg) |
| Qt 6.6+ | 6.6+ | [qt.io/download](https://www.qt.io/download-qt-installer) |
| Windows SDK | 10.0.22621+ | Via VS Installer |
| WDK | Matching SDK | [learn.microsoft.com](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) |
| DXC Shader Compiler | Latest | [github.com/microsoft/DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler/releases) |
| Android Platform Tools | Latest | [developer.android.com](https://developer.android.com/tools/releases/platform-tools) |
| Apple Bonjour SDK | Latest | [developer.apple.com/bonjour](https://developer.apple.com/bonjour/) |
| NSIS | Latest | [nsis.sourceforge.io](https://nsis.sourceforge.io/Download) |
| Python | ≥ 3.11 | [python.org](https://www.python.org/downloads) |

## Required Environment Variables

```
QTDIR        = C:\Qt\6.6.0\msvc2022_64
VCPKG_ROOT   = C:\vcpkg
ANDROID_TOOLS= C:\adb
```

Add to PATH: `C:\Qt\6.6.0\msvc2022_64\bin`

## Build Instructions

```bash
# 1. Install vcpkg dependencies
cd C:\vcpkg
vcpkg install --triplet x64-windows

# 2. Compile HLSL shaders (requires dxc.exe from Windows SDK or DirectXShaderCompiler)
cd assets/shaders && compile_shaders.bat && cd ../..

# 3. Configure (debug)
cmake --preset windows-debug

# 3. Build
cmake --build --preset build-debug

# 4. Configure (release)
cmake --preset windows-release -B build/release_x64

# 5. Build release
cmake --build --preset build-release
```

## Running Tests

```bash
cmake --preset windows-release
ctest --preset run-all-tests
```

## Folder Structure

```
AuraCastPro_Root/
├── src/
│   ├── engine/       # Protocol, network, video decode
│   ├── display/      # DirectX 12 GPU rendering
│   ├── integration/  # Audio, recording, virtual camera
│   ├── streaming/    # RTMP/NVENC output
│   ├── discovery/    # mDNS / Bonjour device discovery
│   ├── input/        # Mouse/keyboard → touch injection
│   ├── manager/      # Qt UI, settings, device management
│   ├── utils/        # Logging, timers, encryption helpers
│   ├── licensing/    # License validation and feature gates
│   └── plugins/      # Extensibility plugin system
├── assets/
│   ├── shaders/      # HLSL source shaders
│   └── textures/     # UI artwork
├── cloud/            # License, update, telemetry clients
├── tests/            # Unit, integration, performance tests
├── drivers/          # Virtual camera and audio drivers
├── scripts/          # Install automation
└── doc/              # Architecture and API documentation
```

## FFmpeg LGPL Compliance

FFmpeg is linked as shared DLLs (dynamic linking), not compiled into the executable.
This complies with the FFmpeg LGPL license. The FFmpeg source code and build scripts
are available at [ffmpeg.org](https://ffmpeg.org). Users may relink against a modified
FFmpeg by replacing the DLLs in the installation directory.

## License

Proprietary — AuraCastPro © 2025. All rights reserved.

# AuraCastPro MirrorCam Virtual Camera
=====================================

## What this is
A Windows DirectShow source filter + Media Foundation video capture source
that makes AuraCastPro appear as a webcam in OBS, Zoom, Teams, etc.

Reads frames from named shared memory ("AuraCastProSharedFrame") written by
VCamBridge.cpp in the main AuraCastPro process.

## Architecture
  AuraCastPro.exe
    └─ VCamBridge → SharedMemory("AuraCastProSharedFrame")
                              │
  MirrorCam.dll ─────────────┘  (reads via OpenFileMapping)
    ├─ DirectShow:  IBaseFilter + IPin + IAMStreamConfig
    └─ MF Source:   IMFMediaSource + IMFMediaStream

## Build (CMake)
MirrorCam.dll IS buildable via CMake (unlike the kernel driver).
It is listed in CMakeLists.txt as a SHARED library target.

## Registration
  regsvr32.exe MirrorCam.dll         (install)
  regsvr32.exe /u MirrorCam.dll      (uninstall)
The installer runs scripts/register_dlls.bat automatically.

## Code signing
  signtool sign /fd SHA256 /a /t http://timestamp.digicert.com MirrorCam.dll

## Files
  MirrorCam.cpp          — DllMain, RegisterServer, CreateInstance, filter impl
  MirrorCam.h            — COM interface declarations
  MirrorCam_Installer.inf — Driver registration manifest
  MirrorCam_Driver.cat   — Digital signature catalog
  README.txt             — This file

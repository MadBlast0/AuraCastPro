# installer/adb_bundle — ADB Platform Tools Bundle
# Task 216

## Purpose
AuraCastPro bundles a minimal set of ADB (Android Debug Bridge) binaries so
that Android mirroring works on the end-user's machine without requiring the
user to install Android Studio or the full Android SDK.

## Contents (populated on your PC before building the installer)
```
adb_bundle/
  adb.exe           — ADB host executable (Platform Tools r34+)
  AdbWinApi.dll     — Required DLL for adb.exe
  AdbWinUsbApi.dll  — Required DLL for USB ADB connections
  README.txt        — This file
```

## How to populate this folder (Task 216, PC-required step)

1. Download the latest Windows Platform Tools from:
   https://developer.android.com/tools/releases/platform-tools

2. Extract the ZIP. Copy ONLY these files into this folder:
   - adb.exe
   - AdbWinApi.dll
   - AdbWinUsbApi.dll

3. Do NOT include adb.exe from Android Studio — use the official Platform Tools.

4. Verify the version:
   > adb.exe version
   Android Debug Bridge version 1.0.41
   Version 34.0.x ...

## Licensing
ADB is part of the Android Open Source Project (AOSP), licensed under the
Apache License 2.0. The full license text is in:
   installer/THIRD_PARTY_LICENSES.txt  (section: "Android Platform Tools")

Redistribution is permitted under Apache 2.0 provided the license notice
is preserved.

## Installer Integration
The NSIS installer script (package_installer.nsis) references this folder:
  File "installer\adb_bundle\adb.exe"
  File "installer\adb_bundle\AdbWinApi.dll"
  File "installer\adb_bundle\AdbWinUsbApi.dll"

These are installed to:
  $INSTDIR\adb\adb.exe

ADBBridge.cpp reads the path from:
  QStandardPaths::writableLocation(AppLocalDataLocation) + "/adb/adb.exe"
  or falls back to: QCoreApplication::applicationDirPath() + "/adb/adb.exe"

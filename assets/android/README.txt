# assets/android/ — Android Mirroring Assets
# Task 084

## scrcpy-server (REQUIRED for Android mirroring)

### What it is
scrcpy-server is an Android APK that runs on the device and streams
H.264 video + input events to AuraCastPro over ADB.

### Download (PC-required step)
1. Go to: https://github.com/Genymobile/scrcpy/releases
2. Download the latest scrcpy Windows ZIP (e.g. scrcpy-win64-v2.x.zip)
3. Extract and copy `scrcpy-server` from the ZIP into THIS directory.
   The file has no extension — it IS the Android APK renamed.
4. Verify: `file scrcpy-server` should show "Zip archive data" (it's an APK)

### Version compatibility
AuraCastPro is tested with scrcpy-server v2.3+.
The server version is checked at runtime in ADBBridge::init().

### Licensing
scrcpy is Apache 2.0 licensed.
The server APK may be bundled and redistributed.
Attribution is included in installer/THIRD_PARTY_LICENSES.txt.

### CMake post-build (already configured in CMakeLists.txt)
```cmake
add_custom_command(TARGET AuraCastPro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/android/scrcpy-server"
        "$<TARGET_FILE_DIR:AuraCastPro>/scrcpy-server"
)
```

### Runtime path
ADBBridge looks for scrcpy-server at:
  QCoreApplication::applicationDirPath() + "/scrcpy-server"

If not found, Android USB mirroring is disabled with a warning in the log.

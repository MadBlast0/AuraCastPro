cmake_minimum_required(VERSION 3.26)
project(AuraCastPro VERSION 0.1.0 LANGUAGES CXX)

# Task 024: Centralised version number system
include("${CMAKE_SOURCE_DIR}/cmake/Version.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Compiler flags ─────────────────────────────────────────────────────────────
if(MSVC)
    # /W4: warning level 4 (high but not all)
    # /WX-: warnings NOT errors in Debug (developer convenience)
    # /MP: parallel compilation
    # /arch:AVX2: enable AVX2 SIMD (Haswell+ CPU required)
    # /permissive-: strict conformance mode
    add_compile_options(/W4 /WX- /MP /arch:AVX2 /permissive-)

    # Task 199: Warnings-as-errors disabled — large codebase has many C4100/C4996 warnings.
    # Re-enable /WX once all warnings are cleaned up.
    # add_compile_options($<$<CONFIG:Release>:/WX>)

    # Task 199: MSVC /analyze — static analysis (PREfast)
    # Enable in Debug builds only (analysis is slow).
    # To run: cmake --build build --config Debug
    # add_compile_options($<$<CONFIG:Debug>:/analyze>)  # uncomment to enable

    add_compile_definitions(
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        AURA_VERSION_STRING="${PROJECT_VERSION}"
    AURA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    AURA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    AURA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        # Task 048: RC file uses these to build VS_VERSIONINFO block
        AURA_VER_MAJOR=${PROJECT_VERSION_MAJOR}
        AURA_VER_MINOR=${PROJECT_VERSION_MINOR}
        AURA_VER_PATCH=${PROJECT_VERSION_PATCH}
        AURA_VER_BUILD=1
    )
endif()

# ── Task 199: clang-tidy integration ──────────────────────────────────────────
# Disabled by default (slow). Enable with: cmake -DENABLE_CLANG_TIDY=ON
option(ENABLE_CLANG_TIDY "Run clang-tidy during build" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-17 clang-tidy-16 clang-tidy
                 DOC "Path to clang-tidy")
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY=ON but clang-tidy not found.")
    endif()
endif()

# ── vcpkg ──────────────────────────────────────────────────────────────────────
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

# ── Find packages ──────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2 Charts Widgets QuickWidgets LinguistTools)

# DirectX helper headers (d3dx12.h) — from directx-headers vcpkg package
# Falls back gracefully if not installed (Windows SDK ships d3dx12.h in some versions)
find_package(directx-headers CONFIG QUIET)
if(directx-headers_FOUND)
    message(STATUS "Found directx-headers — d3dx12.h available via vcpkg")
else()
    message(STATUS "directx-headers not found via vcpkg — relying on Windows SDK")
endif()

# ── Task 170: Auto-compile translations (lrelease) ────────────────────────────
# Generates src/manager/i18n/AuraCastPro_en.qm before the resource compile.
set(TS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/manager/i18n/AuraCastPro_en.ts")
set(QM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/manager/i18n/AuraCastPro_en.qm")
# Always create a placeholder .qm file so resources.qrc can embed it.
# qt6_add_lrelease has API differences across Qt versions; use lrelease directly.
if(NOT EXISTS "${QM_FILE}")
    file(WRITE "${QM_FILE}" "")
endif()
if(Qt6LinguistTools_FOUND)
    find_program(LRELEASE_EXECUTABLE lrelease
        HINTS "${Qt6_DIR}/../../../bin"
        NO_DEFAULT_PATH)
    if(LRELEASE_EXECUTABLE)
        add_custom_command(
            OUTPUT  "${QM_FILE}"
            COMMAND "${LRELEASE_EXECUTABLE}" "${TS_FILE}" -qm "${QM_FILE}"
            DEPENDS "${TS_FILE}"
            COMMENT "Compiling translation: AuraCastPro_en.ts -> .qm"
            VERBATIM)
        message(STATUS "lrelease found — translations will be compiled automatically.")
    else()
        message(STATUS "Qt6LinguistTools found but lrelease not in PATH — using placeholder.")
    endif()
else()
    message(STATUS "Qt6LinguistTools not found — using empty placeholder .qm.")
endif()
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(PkgConfig)

# FFmpeg (via vcpkg or system)
find_path(AVCODEC_INCLUDE_DIR  libavcodec/avcodec.h)
find_library(AVCODEC_LIB  avcodec)
find_library(AVFORMAT_LIB avformat)
find_library(AVUTIL_LIB   avutil)
find_library(SWRESAMPLE_LIB swresample)
find_library(SWSCALE_LIB swscale)

# Task 025/vcpkg: Fail early with a clear message if FFmpeg wasn't found
if(NOT AVCODEC_LIB OR NOT AVFORMAT_LIB OR NOT AVUTIL_LIB OR NOT SWRESAMPLE_LIB OR NOT SWSCALE_LIB)
    message(FATAL_ERROR
        "FFmpeg libraries not found.\n"
        "Run: vcpkg install ffmpeg[avcodec,avformat,avutil,swresample,swscale] --triplet x64-windows\n"
        "Then re-configure with -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake")
endif()
if(NOT AVCODEC_INCLUDE_DIR)
    message(FATAL_ERROR
        "FFmpeg headers not found (libavcodec/avcodec.h).\n"
        "Ensure FFmpeg is installed via vcpkg and VCPKG_ROOT is set correctly.")
endif()

# ── Source files ───────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp

    # Utils
    src/utils/Logger.cpp
    src/utils/PerformanceTimer.cpp
    src/utils/HardwareProfiler.cpp
    src/utils/NetworkTools.cpp
    src/utils/EncryptHelper.cpp
    src/utils/OSVersionHelper.cpp
    src/utils/CrashReporter.cpp
    src/utils/DiagnosticsExporter.cpp
    src/utils/WindowsEventLog.cpp
    src/utils/AppDataInit.cpp
    src/utils/BinaryPlist.cpp

    # Engine
    src/engine/AirPlay2Host.cpp
    src/engine/CastV2Host.cpp
    src/engine/ADBBridge.cpp
    src/engine/ADBServerLifecycle.cpp
    src/engine/ProtocolHandshake.cpp
    src/engine/ReceiverSocket.cpp
    src/engine/TimingServer.cpp
    src/engine/MirroringListener.cpp
    src/engine/PacketReorderCache.cpp
    src/engine/FECRecovery.cpp
    src/engine/NALParser.cpp
    src/engine/H264Demuxer.cpp
    src/engine/H265Demuxer.cpp
    src/engine/AV1Demuxer.cpp
    src/engine/VideoDecoder.cpp
    src/engine/SoftwareVideoDecoder.cpp
    src/engine/FFmpegVideoDecoder.cpp
    src/engine/BitratePID.cpp
    src/engine/NetworkPredictor.cpp
    src/engine/UDPServerThreadPool.cpp
    src/engine/FrameTimingQueue.cpp
    src/engine/PacketValidator.cpp
    src/engine/PortConflict.cpp
    src/engine/ReconnectManager.cpp
    src/engine/USBHotplug.cpp

    # Display
    src/display/MirrorWindow.cpp
    src/display/MirrorWindowWin32.cpp
    src/display/DeviceMirrorWindow.cpp
    src/display/DX12Manager.cpp
    src/display/DX12DeviceContext.cpp
    src/display/DX12CommandQueue.cpp
    src/display/DX12Fence.cpp
    src/display/DisplayHelpers.cpp
    src/display/PipelineStateObjects.cpp
    src/display/RootSignatures.cpp
    src/display/GPUFrameBufferPool.cpp
    src/display/HDRDetection.cpp
    src/display/DeviceLostRecovery.cpp

    # Discovery
    src/discovery/BonjourApi.cpp
    src/discovery/MDNSService.cpp
    src/discovery/BonjourAdapter.cpp
    src/discovery/DeviceAdvertiser.cpp

    # Integration
    src/integration/AudioLoopback.cpp
    src/integration/AudioMixer.cpp
    src/integration/AudioDeviceEnumerator.cpp
    src/integration/AudioResampler.cpp
    src/integration/AVSync.cpp
    src/integration/VCamBridge.cpp
    src/integration/StreamRecorder.cpp
    src/integration/RecordingSystem.cpp
    src/integration/SharedMemoryIPC.cpp
    src/integration/VirtualAudioDriver.cpp
    src/integration/DiskSpaceMonitor.cpp
    src/integration/MP4MuxerFragments/MP4Muxer.cpp

    # Streaming
    src/streaming/HardwareEncoder.cpp
    src/streaming/NVENCWrapper.cpp
    src/streaming/AMFWrapper.cpp
    src/streaming/QSVWrapper.cpp
    src/streaming/RTMPOutput.cpp
    src/streaming/StreamBroadcaster.cpp

    # Input
    src/input/MouseToTouch.cpp
    src/input/KeyboardToInput.cpp
    src/input/AndroidControlBridge.cpp
    src/input/InputExtras.cpp
    src/input/ClipboardBridge.cpp

    # Manager
    src/manager/HubWindow.cpp
    src/manager/HubModel.cpp
    src/manager/SettingsModel.cpp
    src/manager/SecurityVault.cpp
    src/manager/DeviceManager.cpp
    src/manager/DeviceMirrorManager.cpp
    src/manager/NetworkStatsModel.cpp
    src/manager/PerformanceOverlay.cpp
    src/manager/GlobalHotkey.cpp
    src/manager/ToastNotification.cpp
    src/manager/ErrorDialog.cpp
    src/manager/WindowsServices.cpp
    src/manager/FirstRunAndSplash.cpp
    src/manager/ProxySettings.cpp
    src/manager/VideoFrameProvider.cpp
    src/manager/resources.qrc

    # Licensing
    src/licensing/LicenseManager.cpp
    src/licensing/LicenseValidator.cpp
    src/licensing/FeatureGate.cpp

    # Plugins
    src/plugins/PluginManager.cpp
    src/plugins/PluginLoader.cpp

    # Cloud
    cloud/LicenseClient.cpp
    cloud/UpdateService.cpp
    cloud/TelemetryClient.cpp
    cloud/ProxySettings.cpp
    cloud/WinHttpHelper.cpp

    # Windows resources (icon, version info, manifest — Task 047/048)
    src/AuraCastPro.rc
)

# ── Main executable ────────────────────────────────────────────────────────────
qt6_add_executable(AuraCastPro WIN32 ${SOURCES})

qt6_add_qml_module(AuraCastPro
    URI AuraCastPro
    VERSION 1.0
    RESOURCE_PREFIX /
    QML_FILES
        src/manager/qml/Main.qml
        src/manager/qml/Theme.qml
        src/manager/qml/AppHeader.qml
        src/manager/qml/Dashboard.qml
        src/manager/qml/DeviceList.qml
        src/manager/qml/DeviceCard.qml
        src/manager/qml/BitrateGraph.qml
        src/manager/qml/LatencyGraph.qml
        src/manager/qml/StatsPage.qml
        src/manager/qml/SettingsPage.qml
        src/manager/qml/PairingDialog.qml
        src/manager/qml/AboutDialog.qml
        src/manager/qml/DiagnosticsPanel.qml
        src/manager/qml/SplashProgress.qml
        src/manager/qml/FirstRunWizard.qml
        src/manager/qml/StatsOverlay.qml
        src/manager/qml/LicensePage.qml
    SOURCES
        src/manager/qml/qmldir
)

target_include_directories(AuraCastPro PRIVATE
    src
    cloud
    ${CMAKE_BINARY_DIR}/generated    # Task 024: version_info.h
    ${AVCODEC_INCLUDE_DIR}
    # d3dx12.h helper headers from directx-headers vcpkg package (if found)
    $<$<TARGET_EXISTS:Microsoft::DirectX-Headers>:${directx-headers_INCLUDE_DIRS}>
)

# ── Precompiled Header (Task 044) — major build speed improvement ─────────
target_precompile_headers(AuraCastPro PRIVATE src/pch.h)

target_link_libraries(AuraCastPro PRIVATE
    Qt6::Core Qt6::Quick Qt6::QuickControls2 Qt6::Charts Qt6::Widgets Qt6::QuickWidgets
    OpenSSL::SSL OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Boost::system
    ${AVCODEC_LIB} ${AVFORMAT_LIB} ${AVUTIL_LIB} ${SWRESAMPLE_LIB} ${SWSCALE_LIB}
    d3d12 dxgi dxguid d3dcompiler
    mf mfplat mfreadwrite mfuuid
    winhttp
    ws2_32 iphlpapi
    crypt32 bcrypt
    wbemuuid ole32 oleaut32
    dbghelp        # CrashReporter: MiniDumpWriteDump
    psapi          # PortConflict: GetProcessImageFileName
    gdiplus        # splash screen / icon rendering
    strmiids       # DirectShow / COM filter GUIDs (MirrorCam driver)
    uuid           # COM UUID support
    Shcore         # DisplayHelpers: GetDpiForMonitor
    windowsapp     # ToastNotification: WinRT API
)

# Qt Network: for QNetworkProxyFactory in main.cpp
target_link_libraries(AuraCastPro PRIVATE Qt6::Network)

# =============================================================================
# Task 042 — DLL deployment post-build step
# After every build, automatically copy all required DLLs next to the .exe so
# it can run on the developer's machine and for packaging.
# windeployqt handles Qt DLLs + QML imports.
# FFmpeg, OpenSSL, and VC++ runtime DLLs are copied separately.
# =============================================================================
if(WIN32)
    # windeployqt — copies Qt*.dll, QML plugins, platform plugins
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${QTDIR}/bin" "$ENV{QTDIR}/bin"
        DOC "Qt windeployqt tool"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET AuraCastPro POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --qmldir "${CMAKE_SOURCE_DIR}/src/manager/qml"
                --no-translations
                --no-system-d3d-compiler
                --no-compiler-runtime
                "$<TARGET_FILE:AuraCastPro>"
            COMMENT "Running windeployqt to copy Qt DLLs..."
        )
    else()
        message(WARNING
            "windeployqt not found — Qt DLLs will not be auto-deployed.\n"
            "Set QTDIR environment variable (e.g. C:/Qt/6.6.0/msvc2022_64)")
    endif()

    # ── Compile HLSL shaders to .cso with dxc.exe (Windows SDK) ─────────────
    set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
    set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

    # Locate dxc.exe — can be overridden via -DDXC_EXECUTABLE=<path>
    if(DXC_EXECUTABLE)
        set(DXC_EXE "${DXC_EXECUTABLE}")
        message(STATUS "Using provided dxc: ${DXC_EXE}")
    else()
    # Locate dxc.exe from Windows SDK
    set(DXC_HINTS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
    )
    find_program(DXC_EXE NAMES dxc HINTS ${DXC_HINTS})

    if(DXC_EXE)
        message(STATUS "Found dxc: ${DXC_EXE}")
        set(CSO_FILES "")

        # Helper macro to add one shader compile command
        macro(add_shader NAME PROFILE ENTRY)
            set(_HLSL "${SHADER_SRC_DIR}/${NAME}.hlsl")
            set(_CSO  "${SHADER_OUT_DIR}/${NAME}.cso")
            list(APPEND CSO_FILES "${_CSO}")
            add_custom_command(
                OUTPUT  "${_CSO}"
                COMMAND "${DXC_EXE}" -T "${PROFILE}" -E "${ENTRY}" "${_HLSL}" -Fo "${_CSO}"
                DEPENDS "${_HLSL}"
                COMMENT "Compiling HLSL: ${NAME}.hlsl [${PROFILE}, entry=${ENTRY}]"
            )
        endmacro()

        # VS  — entry point is "main"
        add_shader(fullscreen_vs         vs_6_0 main)
        # PS  — entry point is "PSMain"
        add_shader(nv12_to_rgb           ps_6_0 PSMain)
        add_shader(hdr10_tonemap         ps_6_0 PSMain)
        add_shader(lanczos8              ps_6_0 PSMain)
        # CS  — entry point is "CSMain", profile is cs_6_0
        add_shader(chroma_upsample       cs_6_0 CSMain)
        add_shader(temporal_frame_pacing cs_6_0 CSMain)

        add_custom_target(AuraCastPro_Shaders ALL DEPENDS ${CSO_FILES})
        add_dependencies(AuraCastPro AuraCastPro_Shaders)
    else()
        message(WARNING
            "dxc.exe not found — shaders will not be compiled automatically.\n"
            "Run assets/shaders/compile_shaders.bat manually after installing Windows SDK 10.0.22621.")
    endif()  # DXC_EXECUTABLE override
    endif()

    # Copy compiled shader .cso files next to the exe
    # SHADER_OUT_DIR = ${CMAKE_BINARY_DIR}/shaders (set above)
    add_custom_command(TARGET AuraCastPro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:AuraCastPro>/Shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/shaders"
            "$<TARGET_FILE_DIR:AuraCastPro>/Shaders"
        COMMENT "Copying compiled shaders (.cso) to output directory..."
    )

    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/android/platform-tools/adb.exe")
        add_custom_command(TARGET AuraCastPro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:AuraCastPro>/adb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/android/platform-tools/adb.exe"
                "$<TARGET_FILE_DIR:AuraCastPro>/adb/adb.exe"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/android/platform-tools/AdbWinApi.dll"
                "$<TARGET_FILE_DIR:AuraCastPro>/adb/AdbWinApi.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/android/platform-tools/AdbWinUsbApi.dll"
                "$<TARGET_FILE_DIR:AuraCastPro>/adb/AdbWinUsbApi.dll"
            COMMENT "Copying bundled ADB runtime..."
        )
    else()
        message(WARNING "Bundled ADB runtime not found in third_party/android/platform-tools")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/android/scrcpy-server")
        add_custom_command(TARGET AuraCastPro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/android/scrcpy-server"
                "$<TARGET_FILE_DIR:AuraCastPro>/scrcpy-server"
            COMMENT "Copying scrcpy-server to output directory..."
        )
    else()
        message(WARNING "scrcpy-server not found in third_party/android")
    endif()
endif()


# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(AuraCastPro_tests
    tests/unit/BitratePIDTests.cpp
    tests/unit/ParserTests.cpp
    tests/unit/SecurityTests.cpp
    tests/unit/PacketRecoveryTests.cpp
    tests/integration/StreamEndToEnd.cpp
    tests/performance/LatencyMonitor.cpp
    tests/performance/GPUThroughputBenchmark.cpp
    tests/performance/AudioLatencyBenchmark.cpp
    tests/unit/AudioSyncTests.cpp
    tests/unit/ProtocolConformanceTests.cpp
    tests/unit/RotationHandlingTests.cpp

    # Include engine sources needed by tests
    src/engine/BitratePID.cpp
    src/engine/PacketReorderCache.cpp
    src/engine/NALParser.cpp
    src/engine/H265Demuxer.cpp
    src/engine/AV1Demuxer.cpp
    src/engine/FECRecovery.cpp
    src/engine/NetworkPredictor.cpp
    src/engine/FrameTimingQueue.cpp
    src/utils/Logger.cpp
    src/utils/PerformanceTimer.cpp
    src/utils/EncryptHelper.cpp
    src/integration/AVSync.cpp
    src/engine/ProtocolHandshake.cpp
    src/display/DisplayHelpers.cpp
)

target_include_directories(AuraCastPro_tests PRIVATE src)
target_compile_definitions(AuraCastPro_tests PRIVATE AURA_VERSION_STRING="test")

target_link_libraries(AuraCastPro_tests PRIVATE
    GTest::gtest GTest::gtest_main
    OpenSSL::SSL OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    crypt32 bcrypt ws2_32 Shcore
)

include(GoogleTest)
gtest_discover_tests(AuraCastPro_tests)

# ── Install ────────────────────────────────────────────────────────────────────
install(TARGETS AuraCastPro DESTINATION bin)
install(DIRECTORY assets/ DESTINATION bin/assets)
install(DIRECTORY drivers/ DESTINATION bin/drivers)

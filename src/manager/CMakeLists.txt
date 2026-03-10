set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


qt_add_resources(manager "resources"
    PREFIX "/"
    FILES resources.qrc
)

# ── Asset placeholder check ───────────────────────────────────────────────────
# Warn (not error) if generated assets are missing. App will run with
# broken images but will not crash — Qt gracefully handles missing qrc files.
set(ASSET_ROOT "${CMAKE_SOURCE_DIR}/assets")

if(NOT EXISTS "${ASSET_ROOT}/fonts/RobotoMono-Regular.ttf")
    message(WARNING
        "Roboto Mono font not found. UI will use system monospace fallback.\n"
        "   Download: see ${ASSET_ROOT}/fonts/README.txt")
    # Create stub 0-byte placeholder so qrcc does not error out
    file(TOUCH "${ASSET_ROOT}/fonts/RobotoMono-Regular.ttf")
    file(TOUCH "${ASSET_ROOT}/fonts/RobotoMono-Bold.ttf")
    file(TOUCH "${ASSET_ROOT}/fonts/RobotoMono-Medium.ttf")
endif()

if(NOT EXISTS "${ASSET_ROOT}/textures/icons/png/app_icon.png")
    message(WARNING
        "App icon PNG not found. Toolbar icon will be blank.\n"
        "   Generate: see ${ASSET_ROOT}/textures/icons/README.txt")
    file(MAKE_DIRECTORY "${ASSET_ROOT}/textures/icons/png")
    file(TOUCH "${ASSET_ROOT}/textures/icons/png/app_icon.png")
    file(TOUCH "${ASSET_ROOT}/textures/icons/png/ios.png")
    file(TOUCH "${ASSET_ROOT}/textures/icons/png/android.png")
endif()

if(NOT EXISTS "${ASSET_ROOT}/textures/8K_Splashes/splash.png")
    message(WARNING
        "Splash screen PNG not found. Startup splash will be skipped.\n"
        "   Generate: see ${ASSET_ROOT}/textures/8K_Splashes/README.txt")
    file(TOUCH "${ASSET_ROOT}/textures/8K_Splashes/splash.png")
endif()

add_library(manager STATIC
    HubWindow.cpp
    HubModel.cpp
    SettingsModel.cpp
    SecurityVault.cpp
    DeviceManager.cpp
    NetworkStatsModel.cpp
    PerformanceOverlay.cpp
    GlobalHotkey.cpp
    ToastNotification.cpp
    ErrorDialog.cpp
    FirstRunAndSplash.cpp
    ProxySettings.cpp
    WindowsServices.cpp
    # WindowsEventLog is in utils/ which manager links against — no duplicate needed
)

target_include_directories(manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(manager
    PUBLIC
        utils
        Qt6::Core
        Qt6::Quick
        Qt6::Gui
        Qt6::Charts
        Qt6::Widgets
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
)

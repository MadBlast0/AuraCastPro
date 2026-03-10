# ── Bonjour SDK ──────────────────────────────────────────────────────────────
# dnssd.lib and dns_sd.h ship with Apple Bonjour SDK for Windows.
# Download from https://developer.apple.com/bonjour/ and install to:
#   C:\Program Files\Bonjour SDK\
# Or set BONJOUR_SDK_DIR in CMakePresets.json:
#   "BONJOUR_SDK_DIR": "C:/Program Files/Bonjour SDK"
# ─────────────────────────────────────────────────────────────────────────────
if(DEFINED BONJOUR_SDK_DIR)
    target_include_directories(discovery PRIVATE "${BONJOUR_SDK_DIR}/include")
    target_link_directories(discovery PRIVATE "${BONJOUR_SDK_DIR}/lib/x64")
else()
    message(STATUS "BONJOUR_SDK_DIR not set — dnssd.lib must be on the system link path.")
endif()

add_library(discovery STATIC
    MDNSService.cpp
    BonjourAdapter.cpp
    DeviceAdvertiser.cpp
)

target_include_directories(discovery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(discovery
    PUBLIC utils
    PRIVATE dnssd.lib  # Apple Bonjour SDK — must be installed separately
)

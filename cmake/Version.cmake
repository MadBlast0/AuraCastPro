# =============================================================================
# cmake/Version.cmake — Task 024: Centralised version number system
#
# Single source of truth for version numbers.
# Included by the root CMakeLists.txt.
#
# Version scheme: MAJOR.MINOR.PATCH[-STAGE.BUILD]
#   MAJOR — breaking protocol/API change
#   MINOR — new feature, backwards-compatible
#   PATCH — bug fix / maintenance
#   STAGE — alpha | beta | rc | "" (empty = release)
#   BUILD — build number (set by CI, otherwise 0)
#
# Consumers:
#   - AuraCastPro.rc (VS_VERSION_INFO) reads AURA_VERSION_* via CMake configure
#   - main.cpp:         #include "version_info.h"  → AURA_VERSION_STRING
#   - NSIS installer:   ${APPVERSION} = "1.0.0"
#   - UpdateService:    compares against server "version" field
# =============================================================================

set(AURA_VERSION_MAJOR 1)
set(AURA_VERSION_MINOR 0)
set(AURA_VERSION_PATCH 0)
set(AURA_VERSION_STAGE "")       # Set to "alpha", "beta", "rc1" for pre-releases; empty for release
set(AURA_VERSION_BUILD 0)        # Overridden by CI: cmake -DAURA_VERSION_BUILD=$BUILD_NUMBER

# Full semver string: "1.0.0" or "1.0.0-beta.42"
if(AURA_VERSION_STAGE AND NOT AURA_VERSION_STAGE STREQUAL "")
    set(AURA_VERSION_STRING "${AURA_VERSION_MAJOR}.${AURA_VERSION_MINOR}.${AURA_VERSION_PATCH}-${AURA_VERSION_STAGE}.${AURA_VERSION_BUILD}")
else()
    set(AURA_VERSION_STRING "${AURA_VERSION_MAJOR}.${AURA_VERSION_MINOR}.${AURA_VERSION_PATCH}")
endif()

# Windows FILEVERSION quad (must be 4 comma-separated integers)
# Maps to VS_VERSION_INFO FILEVERSION in AuraCastPro.rc
set(AURA_VERSION_QUAD "${AURA_VERSION_MAJOR},${AURA_VERSION_MINOR},${AURA_VERSION_PATCH},${AURA_VERSION_BUILD}")

# Human-readable for display in UI / About dialog
set(AURA_PRODUCT_NAME    "AuraCastPro")
set(AURA_COMPANY_NAME    "AuraCastPro")
set(AURA_COPYRIGHT       "Copyright (C) 2025 AuraCastPro. All rights reserved.")
set(AURA_WEBSITE         "https://auracastpro.com")

# =============================================================================
# Configure a version header so C++ code can read the version at compile time.
# version_info.h is generated into the build directory and added to include path.
# =============================================================================
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/version_info.h.in"
    "${CMAKE_BINARY_DIR}/generated/version_info.h"
    @ONLY
)

# Make the generated header available to all targets
include_directories("${CMAKE_BINARY_DIR}/generated")

message(STATUS "AuraCastPro version: ${AURA_VERSION_STRING} (build ${AURA_VERSION_BUILD})")

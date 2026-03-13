#pragma once
// =============================================================================
// AppVersion.h -- Single source of truth for the application version string.
//
// The version is injected at CMake configure time via:
//   target_compile_definitions(AuraCastPro PRIVATE
//       AURA_VERSION_STRING="1.0.0")
//
// Usage:
//   #include "utils/AppVersion.h"
//   const char* v = aura::AppVersion::string();  // -> "1.0.0"
// =============================================================================

#ifndef AURA_VERSION_STRING
#  define AURA_VERSION_STRING "1.0.0"
#endif
#ifndef AURA_VERSION_MAJOR
#  define AURA_VERSION_MAJOR 1
#endif
#ifndef AURA_VERSION_MINOR
#  define AURA_VERSION_MINOR 0
#endif
#ifndef AURA_VERSION_PATCH
#  define AURA_VERSION_PATCH 0
#endif

namespace aura {

struct AppVersion {
    static constexpr const char* string() noexcept {
        return AURA_VERSION_STRING;
    }
    static constexpr int major() noexcept { return AURA_VERSION_MAJOR; }
    static constexpr int minor() noexcept { return AURA_VERSION_MINOR; }
    static constexpr int patch() noexcept { return AURA_VERSION_PATCH; }
};

} // namespace aura

#pragma once
namespace aura {
class CrashHandler {
public:
    // Call once at startup — registers SEH + signal handlers.
    static void install();
    // Restore default handlers.
    static void uninstall();
};
} // namespace aura

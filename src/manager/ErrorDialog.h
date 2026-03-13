#pragma once
// =============================================================================
// ErrorDialog.h -- Task 177: User-facing error dialog system
//
// Provides a consistent, neo-brutalist styled error presentation layer
// that maps internal error codes and exception messages to user-friendly
// text with actionable advice (retry, open logs, contact support).
//
// Two display modes:
//   1. Qt QML overlay -- shown inside the main window (non-blocking)
//   2. Win32 MessageBoxW -- used for fatal errors before Qt is ready
//
// Usage:
//   ErrorDialog::show(ErrorDialog::Level::Fatal, "DX12 Init Failed",
//       "Your GPU does not support Direct3D 12. "
//       "Please update your GPU drivers.",
//       ErrorDialog::Action::OpenLogs | ErrorDialog::Action::Quit);
// =============================================================================
#include <string>
#include <functional>
#include <cstdint>

namespace aura {

class ErrorDialog {
public:
    enum class Level {
        Info,       // Informational -- blue tint
        Warning,    // Non-fatal -- yellow tint
        Error,      // Recoverable error -- orange tint
        Fatal,      // App must exit or subsystem restart required -- red tint
    };

    enum Action : uint32_t {
        None        = 0,
        Retry       = 1 << 0,
        OpenLogs    = 1 << 1,
        ContactSupport = 1 << 2,
        Quit        = 1 << 3,
        Dismiss     = 1 << 4,  // Default: always available
    };

    struct Options {
        Level       level     {Level::Error};
        std::string title;
        std::string message;
        std::string technical;  // Optional: shown in collapsed "Details" section
        uint32_t    actions   {Action::Dismiss};
        std::function<void()> onRetry;
        std::function<void()> onQuit;
    };

    // Show a blocking native Win32 MessageBoxW (safe before Qt is ready)
    static void showNative(Level level,
                           const std::string& title,
                           const std::string& message);

    // Show in the QML overlay (non-blocking, preferred after app init)
    // Emits the errorOccurred signal on the registered QML bridge.
    static void show(const Options& opts);

    // Convenience overloads
    static void showFatal(const std::string& title,
                          const std::string& message,
                          const std::string& technical = "");

    static void showError(const std::string& title,
                          const std::string& message,
                          const std::string& technical = "");

    static void showWarning(const std::string& title,
                            const std::string& message);

    // Register the QML bridge object (QObject*) that has an
    // "errorOccurred(level, title, message, technical, actions)" signal.
    // Call from HubWindow after QML is loaded.
    static void setQmlBridge(void* qmlRootObject);  // QObject*

    // Map a std::exception to an error dialog automatically.
    // Called from top-level catch blocks.
    static void showFromException(const std::exception& e,
                                  const std::string& context);
};

} // namespace aura

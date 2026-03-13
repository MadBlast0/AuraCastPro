// =============================================================================
// ErrorDialog.cpp -- Task 177: User-facing error dialog system
// =============================================================================
#include "../pch.h"  // PCH
#include "ErrorDialog.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <QMetaObject>
#include <QObject>
#include <QString>

#include <format>
#include <exception>

namespace aura {

static void* g_qmlBridge = nullptr;  // QObject*

// =============================================================================
// Helpers
// =============================================================================
static UINT levelToMBIcon(ErrorDialog::Level level) {
    switch (level) {
    case ErrorDialog::Level::Info:    return MB_ICONINFORMATION;
    case ErrorDialog::Level::Warning: return MB_ICONWARNING;
    case ErrorDialog::Level::Error:   return MB_ICONERROR;
    case ErrorDialog::Level::Fatal:   return MB_ICONERROR;
    }
    return MB_ICONERROR;
}

static std::string levelToString(ErrorDialog::Level level) {
    switch (level) {
    case ErrorDialog::Level::Info:    return "info";
    case ErrorDialog::Level::Warning: return "warning";
    case ErrorDialog::Level::Error:   return "error";
    case ErrorDialog::Level::Fatal:   return "fatal";
    }
    return "error";
}

// =============================================================================
void ErrorDialog::showNative(Level level,
                               const std::string& title,
                               const std::string& message)
{
    AURA_LOG_ERROR("ErrorDialog", "[{}] {}: {}", levelToString(level), title, message);

    const std::wstring wTitle(title.begin(), title.end());
    const std::wstring wMsg(message.begin(), message.end());

    MessageBoxW(nullptr, wMsg.c_str(), wTitle.c_str(),
                MB_OK | levelToMBIcon(level) | MB_TASKMODAL);
}

// =============================================================================
void ErrorDialog::show(const Options& opts) {
    AURA_LOG_ERROR("ErrorDialog",
        "[{}] {}: {} | technical: {}",
        levelToString(opts.level),
        opts.title,
        opts.message,
        opts.technical.empty() ? "(none)" : opts.technical);

    if (!g_qmlBridge) {
        // Qt not ready yet -- fall back to native dialog
        showNative(opts.level, opts.title, opts.message);
        return;
    }

    // Invoke the QML bridge signal:
    //   Q_INVOKABLE void errorOccurred(QString level, QString title,
    //                                   QString message, QString technical,
    //                                   int actions)
    QObject* bridge = static_cast<QObject*>(g_qmlBridge);
    QMetaObject::invokeMethod(bridge, "errorOccurred",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(levelToString(opts.level))),
        Q_ARG(QString, QString::fromStdString(opts.title)),
        Q_ARG(QString, QString::fromStdString(opts.message)),
        Q_ARG(QString, QString::fromStdString(opts.technical)),
        Q_ARG(int,     static_cast<int>(opts.actions)));
}

// =============================================================================
void ErrorDialog::showFatal(const std::string& title,
                             const std::string& message,
                             const std::string& technical)
{
    show({Level::Fatal, title, message, technical,
          Action::OpenLogs | Action::Quit});
}

void ErrorDialog::showError(const std::string& title,
                             const std::string& message,
                             const std::string& technical)
{
    show({Level::Error, title, message, technical,
          Action::Retry | Action::OpenLogs | Action::Dismiss});
}

void ErrorDialog::showWarning(const std::string& title,
                               const std::string& message)
{
    show({Level::Warning, title, message, "",
          Action::Dismiss});
}

// =============================================================================
void ErrorDialog::setQmlBridge(void* qmlRootObject) {
    g_qmlBridge = qmlRootObject;
    AURA_LOG_DEBUG("ErrorDialog", "QML bridge registered.");
}

// =============================================================================
void ErrorDialog::showFromException(const std::exception& e,
                                     const std::string& context)
{
    const std::string what = e.what();
    AURA_LOG_ERROR("ErrorDialog",
        "Exception in {}: {}", context, what);

    // Map common exception types to user-friendly messages
    std::string userTitle = "Unexpected Error";
    std::string userMsg   = "An internal error occurred.";

    if (what.find("DX12") != std::string::npos ||
        what.find("d3d12") != std::string::npos) {
        userTitle = "GPU Error";
        userMsg   = "A Direct3D 12 error occurred. "
                    "Try updating your GPU drivers.";
    } else if (what.find("bind") != std::string::npos ||
               what.find("port") != std::string::npos) {
        userTitle = "Network Error";
        userMsg   = "Could not open network port. "
                    "Another application may be using it.";
    } else if (what.find("OpenSSL") != std::string::npos ||
               what.find("certificate") != std::string::npos) {
        userTitle = "Security Error";
        userMsg   = "A TLS/certificate error occurred. "
                    "Check your system date and time.";
    }

    show({Level::Error, userTitle, userMsg,
          std::format("Context: {}\nDetail: {}", context, what),
          Action::OpenLogs | Action::Dismiss});
}

} // namespace aura

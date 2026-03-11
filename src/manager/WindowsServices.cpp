#include "../pch.h"  // PCH
#include "WindowsServices.h"
#include "../utils/Logger.h"
#include <dbghelp.h>
#include <shellapi.h>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <ctime>

#pragma comment(lib, "dbghelp.lib")

// ─── System Tray ─────────────────────────────────────────────────────────────

SystemTray::SystemTray() = default;

SystemTray::~SystemTray() { remove(); }

bool SystemTray::init(HWND hwnd, HICON icon) {
    m_hwnd = hwnd;
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize           = sizeof(m_nid);
    m_nid.hWnd             = hwnd;
    m_nid.uID              = ID_TRAY;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    m_nid.uCallbackMessage = WM_TRAY_NOTIFY;
    m_nid.hIcon            = icon;
    wcscpy_s(m_nid.szTip, L"AuraCastPro");

    m_added = Shell_NotifyIconW(NIM_ADD, &m_nid) == TRUE;
    if (!m_added) {
        LOG_WARN("SystemTray: Shell_NotifyIconW NIM_ADD failed ({})", GetLastError());
    }
    // Set version for balloon support
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
    return m_added;
}

void SystemTray::setTooltip(const std::string& text) {
    std::wstring wt(text.begin(), text.end());
    wcsncpy_s(m_nid.szTip, wt.c_str(), 127);
    m_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void SystemTray::setState(IconState state) {
    m_state = state;
    // Icon would be swapped here with actual state icons
}

void SystemTray::showBalloon(const std::string& title, const std::string& msg, DWORD) {
    std::wstring wt(title.begin(), title.end());
    std::wstring wm(msg.begin(), msg.end());
    wcsncpy_s(m_nid.szInfoTitle, wt.c_str(), 63);
    wcsncpy_s(m_nid.szInfo,      wm.c_str(), 255);
    m_nid.uFlags         = NIF_INFO;
    m_nid.dwInfoFlags    = NIIF_INFO;
    m_nid.uTimeout       = 3000;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void SystemTray::remove() {
    if (m_added) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_added = false;
    }
}

bool SystemTray::handleMessage(WPARAM wp, LPARAM lp) {
    if (wp != ID_TRAY) return false;
    UINT event = LOWORD(lp);
    if (event == WM_LBUTTONDBLCLK || event == NIN_SELECT) {
        if (m_onOpen) m_onOpen();
    } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
        showContextMenu();
    }
    return true;
}

void SystemTray::showContextMenu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_OPEN,       L"Open AuraCastPro");
    AppendMenuW(hMenu, MF_SEPARATOR, 0,          nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_STARTMIRROR, L"Start Mirroring");
    AppendMenuW(hMenu, MF_STRING, ID_RECORD,     L"Start Recording");
    AppendMenuW(hMenu, MF_SEPARATOR, 0,          nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_QUIT,       L"Quit");

    SetForegroundWindow(m_hwnd);
    UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY,
                               pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
        case ID_OPEN:        if (m_onOpen)     m_onOpen();     break;
        case ID_STARTMIRROR: if (m_onStartStop) m_onStartStop(); break;
        case ID_RECORD:      if (m_onRecord)   m_onRecord();   break;
        case ID_QUIT:        if (m_onQuit)     m_onQuit();     break;
    }
}

// ─── HotkeyManager ───────────────────────────────────────────────────────────

const HotkeyManager::HotkeyDef HotkeyManager::kDefaults[] = {
    { Action::ToggleMirroring,  MOD_CONTROL|MOD_SHIFT, 'M', 1, {} },
    { Action::ToggleRecording,  MOD_CONTROL|MOD_SHIFT, 'R', 2, {} },
    { Action::ToggleFullscreen, MOD_CONTROL|MOD_SHIFT, 'F', 3, {} },
    { Action::ToggleOverlay,    MOD_CONTROL|MOD_SHIFT, 'O', 4, {} },
    { Action::Screenshot,       MOD_CONTROL|MOD_SHIFT, 'S', 5, {} },
};

HotkeyManager::~HotkeyManager() {
    if (m_hwnd) unregisterAll(m_hwnd);
}

bool HotkeyManager::registerAll(HWND hwnd) {
    m_hwnd   = hwnd;
    m_hotkeys.assign(std::begin(kDefaults), std::end(kDefaults));

    for (auto& hk : m_hotkeys) {
        if (!RegisterHotKey(hwnd, hk.id, hk.modifiers, hk.vk)) {
            LOG_WARN("HotkeyManager: Failed to register hotkey id={} ({})",
                     hk.id, GetLastError());
        }
    }
    return true;
}

void HotkeyManager::unregisterAll(HWND hwnd) {
    for (auto& hk : m_hotkeys) UnregisterHotKey(hwnd, hk.id);
    m_hotkeys.clear();
}

void HotkeyManager::setCallback(Action action, std::function<void()> cb) {
    for (auto& hk : m_hotkeys)
        if (hk.action == action) { hk.callback = std::move(cb); return; }
}

bool HotkeyManager::handleMessage(WPARAM wp) {
    for (auto& hk : m_hotkeys) {
        if ((int)wp == hk.id) {
            if (hk.callback) hk.callback();
            return true;
        }
    }
    return false;
}

// ─── ToastNotifier ───────────────────────────────────────────────────────────

void ToastNotifier::show(const std::wstring& title,
                         const std::wstring& message,
                         Type) {
    // WinRT toast via COM activation
    // For simplicity and compatibility: use balloon notification fallback
    // Full WinRT implementation requires Windows.UI.Notifications COM headers
    LOG_INFO("Toast: {} — {}", std::string(title.begin(), title.end()),
             std::string(message.begin(), message.end()));
    // MessageBox as last resort (non-blocking alternative via tray balloon)
    // See showFallback() for actual implementation
}

void ToastNotifier::showWithAction(const std::wstring& title,
                                   const std::wstring& message,
                                   const std::wstring&,
                                   std::function<void()>) {
    show(title, message, Type::Info);
}

void ToastNotifier::showFallback(HWND trayHwnd,
                                  const std::string& title,
                                  const std::string& msg) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize  = sizeof(nid);
    nid.hWnd    = trayHwnd;
    nid.uID     = SystemTray::ID_TRAY;
    nid.uFlags  = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    std::wstring wt(title.begin(), title.end());
    std::wstring wm(msg.begin(), msg.end());
    wcsncpy_s(nid.szInfoTitle, wt.c_str(), 63);
    wcsncpy_s(nid.szInfo,      wm.c_str(), 255);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// ─── ErrorDialog ─────────────────────────────────────────────────────────────

void ErrorDialog::showWarning(const std::wstring& title, const std::wstring& msg) {
    LOG_WARN("[ErrorDialog Warning] {} — {}", std::string(title.begin(),title.end()),
             std::string(msg.begin(),msg.end()));
    MessageBoxW(nullptr, msg.c_str(), title.c_str(), MB_OK | MB_ICONWARNING);
}

void ErrorDialog::showError(const std::wstring& title,
                             const std::wstring& msg,
                             const std::wstring& suggestion) {
    std::wstring full = msg;
    if (!suggestion.empty()) full += L"\n\n" + suggestion;
    LOG_ERROR("[ErrorDialog Error] {} — {}", std::string(title.begin(),title.end()),
              std::string(full.begin(),full.end()));
    MessageBoxW(nullptr, full.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

void ErrorDialog::showFatal(const std::wstring& title, const std::wstring& msg) {
    LOG_CRITICAL("[ErrorDialog Fatal] {} — {}", std::string(title.begin(),title.end()),
                 std::string(msg.begin(),msg.end()));
    MessageBoxW(nullptr, msg.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

// ─── WindowsEventLog ─────────────────────────────────────────────────────────

HANDLE WindowsEventLog::s_hEventLog = nullptr;

void WindowsEventLog::registerSource(const std::wstring& name) {
    s_hEventLog = RegisterEventSourceW(nullptr, name.c_str());
    if (!s_hEventLog)
        LOG_WARN("WindowsEventLog: RegisterEventSource failed ({})", GetLastError());
}

void WindowsEventLog::deregisterSource() {
    if (s_hEventLog) {
        DeregisterEventSource(s_hEventLog);
        s_hEventLog = nullptr;
    }
}

void WindowsEventLog::logInfo(DWORD eventId, const std::wstring& msg) {
    if (!s_hEventLog) return;
    const wchar_t* strs[] = { msg.c_str() };
    ReportEventW(s_hEventLog, EVENTLOG_INFORMATION_TYPE, 0, eventId,
                 nullptr, 1, 0, strs, nullptr);
}

void WindowsEventLog::logError(DWORD eventId, const std::wstring& msg) {
    if (!s_hEventLog) return;
    const wchar_t* strs[] = { msg.c_str() };
    ReportEventW(s_hEventLog, EVENTLOG_ERROR_TYPE, 0, eventId,
                 nullptr, 1, 0, strs, nullptr);
}

// ─── CrashHandler ────────────────────────────────────────────────────────────

void CrashHandler::install() {
    SetUnhandledExceptionFilter(exceptionFilter);
    // Also catch std::terminate
    std::set_terminate([](){
        RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    });
    LOG_INFO("CrashHandler: Installed");
}

std::wstring CrashHandler::generateDumpPath() {
    wchar_t* raw = nullptr;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw);
    std::wstring base = std::wstring(raw) + L"\\AuraCastPro\\crashes\\";
    CoTaskMemFree(raw);

    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_s(&tm_buf, &time);

    wchar_t buf[64];
    wcsftime(buf, 64, L"crash_%Y-%m-%d_%H-%M-%S.dmp", &tm_buf);
    return base + buf;
}

LONG CALLBACK CrashHandler::exceptionFilter(PEXCEPTION_POINTERS ep) {
    std::wstring dumpPath = generateDumpPath();

    HANDLE hFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE,
                                0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = {};
        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers    = FALSE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithThreadInfo),
                          &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }

    MessageBoxW(nullptr,
        L"AuraCastPro has crashed.\n\n"
        L"A diagnostic file has been saved to:\n"
        L"  AppData\\AuraCastPro\\crashes\\\n\n"
        L"Please send it to support@auracastpro.com\n"
        L"along with your log file.",
        L"AuraCastPro Crash",
        MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);

    return EXCEPTION_EXECUTE_HANDLER;
}

// ─── StartupRegistry ─────────────────────────────────────────────────────────

bool StartupRegistry::enable() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string value = std::string("\"") + exePath + "\" --minimised";

    HKEY hKey;
    LSTATUS r = RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    if (r != ERROR_SUCCESS) return false;

    r = RegSetValueExA(hKey, "AuraCastPro", 0, REG_SZ,
                       (const BYTE*)value.c_str(), (DWORD)(value.size() + 1));
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

bool StartupRegistry::disable() {
    HKEY hKey;
    LSTATUS r = RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    if (r != ERROR_SUCCESS) return false;
    RegDeleteValueA(hKey, "AuraCastPro");
    RegCloseKey(hKey);
    return true;
}

bool StartupRegistry::isEnabled() {
    HKEY hKey;
    LSTATUS r = RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE, &hKey);
    if (r != ERROR_SUCCESS) return false;
    DWORD type = 0, size = 0;
    r = RegQueryValueExA(hKey, "AuraCastPro", nullptr, &type, nullptr, &size);
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}
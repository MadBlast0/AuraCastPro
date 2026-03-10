// =============================================================================
// AndroidControlBridge.cpp — ADB-based Android touch/key injection
//
// Uses the scrcpy control protocol over a TCP socket forwarded via ADB.
// Protocol: github.com/Genymobile/scrcpy/blob/master/DEVELOP.md
//
// Message format (binary, big-endian):
//   Byte 0:   message type (0=inject_keycode, 1=inject_text, 2=inject_touch,
//             3=inject_scroll, 4=back_or_screen_on, 5=expand_notification_panel,
//             8=set_clipboard)
//   Bytes 1+: message-specific payload
//
// Touch event payload (type=2, 28 bytes total):
//   action:     1 byte (0=down, 1=up, 2=move)
//   pointerId:  8 bytes (int64)
//   x, y:       4 bytes each (float, normalised 0..1 then * device_resolution)
//   width, height: 4 bytes each (device resolution)
//   pressure:   2 bytes (uint16, 0..0xFFFF)
//   buttons:    4 bytes
//   actionButton: 4 bytes
// =============================================================================
#include "AndroidControlBridge.h"
#include "../utils/Logger.h"
#include "../engine/ADBBridge.h"
#include "MouseToTouch.h"
#include "KeyboardToInput.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include <array>
#include <cstring>
#include <format>
#include <algorithm>

namespace aura {

// scrcpy control message types
enum ScrcpyMsg : uint8_t {
    MSG_INJECT_KEYCODE  = 0,
    MSG_INJECT_TEXT     = 1,
    MSG_INJECT_TOUCH    = 2,
    MSG_INJECT_SCROLL   = 3,
    MSG_BACK_OR_SCREEN  = 4,
};

struct AndroidControlBridge::ScrcpyControl {
    SOCKET controlSocket{INVALID_SOCKET};
};

AndroidControlBridge::AndroidControlBridge()
    : m_ctrl(std::make_unique<ScrcpyControl>()) {}

AndroidControlBridge::~AndroidControlBridge() { shutdown(); }

void AndroidControlBridge::init() {
    AURA_LOG_INFO("AndroidControlBridge",
        "Initialised. scrcpy control protocol over ADB TCP forward. "
        "Touch: MSG_INJECT_TOUCH. Keys: MSG_INJECT_KEYCODE. Text: MSG_INJECT_TEXT.");
}

void AndroidControlBridge::start() {}
void AndroidControlBridge::stop()  { disconnect(); }
void AndroidControlBridge::shutdown() { stop(); }

bool AndroidControlBridge::connect(const std::string& serial,
                                    uint32_t w, uint32_t h) {
    m_deviceSerial = serial;
    m_deviceWidth  = w;
    m_deviceHeight = h;

    AURA_LOG_INFO("AndroidControlBridge",
        "Connecting control socket to device {} ({}×{})...", serial, w, h);

    // ADB forward: adb -s <serial> forward tcp:27184 localabstract:scrcpy
    // Use CreateProcess (not system()) to avoid console flash and get return code
    {
        const std::string fullCmd = std::format(
            "adb -s {} forward tcp:27184 localabstract:scrcpy 2>&1", serial);
        std::string fwdCmd = "cmd /c " + fullCmd;

        SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        HANDLE hRead = nullptr, hWrite = nullptr;
        if (CreatePipe(&hRead, &hWrite, &sa, 0)) {
            STARTUPINFOA si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.hStdOutput = hWrite;
            si.hStdError  = hWrite;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi{};
            if (CreateProcessA(nullptr, fwdCmd.data(), nullptr, nullptr,
                               TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                CloseHandle(hWrite);
                char buf[256]{};
                DWORD r = 0;
                std::string out;
                while (ReadFile(hRead, buf, sizeof(buf)-1, &r, nullptr) && r)
                    out.append(buf, r);
                DWORD waitRes = WaitForSingleObject(pi.hProcess, 5000);
                if (waitRes == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                AURA_LOG_DEBUG("AndroidControlBridge",
                    "adb forward: {:.60s}", out.empty() ? "(ok)" : out);
            } else {
                CloseHandle(hWrite);
            }
            CloseHandle(hRead);
        }
    }

    // Connect to the forwarded port
    m_ctrl->controlSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(27184);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(m_ctrl->controlSocket,
                  reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        AURA_LOG_WARN("AndroidControlBridge",
            "Control socket connect failed: {}. Touch injection unavailable.", WSAGetLastError());
        closesocket(m_ctrl->controlSocket);
        m_ctrl->controlSocket = INVALID_SOCKET;
        return false;
    }

    m_connected.store(true);
    AURA_LOG_INFO("AndroidControlBridge", "Control socket connected.");
    return true;
}

void AndroidControlBridge::disconnect() {
    if (!m_connected.exchange(false)) return;
    if (m_ctrl->controlSocket != INVALID_SOCKET) {
        closesocket(m_ctrl->controlSocket);
        m_ctrl->controlSocket = INVALID_SOCKET;
    }
    AURA_LOG_INFO("AndroidControlBridge", "Control socket closed.");
}

void AndroidControlBridge::sendControlMessage(const uint8_t* msg, size_t len) {
    if (m_ctrl->controlSocket == INVALID_SOCKET) return;
    // TCP send may be partial — loop until all bytes sent or error
    const char* ptr = reinterpret_cast<const char*>(msg);
    size_t remaining = len;
    while (remaining > 0) {
        const int sent = send(m_ctrl->controlSocket,
                              ptr, static_cast<int>(remaining), 0);
        if (sent == SOCKET_ERROR) {
            AURA_LOG_WARN("AndroidControlBridge",
                "send() failed: {} — closing control socket.", WSAGetLastError());
            closesocket(m_ctrl->controlSocket);
            m_ctrl->controlSocket = INVALID_SOCKET;
            m_connected.store(false);
            return;
        }
        ptr       += sent;
        remaining -= static_cast<size_t>(sent);
    }
}

void AndroidControlBridge::sendTouch(const TouchEvent& e) {
    if (!m_connected.load()) return;

    // Build 28-byte scrcpy touch message
    std::array<uint8_t, 28> msg{};
    msg[0] = MSG_INJECT_TOUCH;

    // action: 0=down, 1=up, 2=move
    uint8_t action = 2; // move
    if (e.type == TouchEventType::Down) action = 0;
    else if (e.type == TouchEventType::Up) action = 1;
    msg[1] = action;

    // pointerId (8 bytes, big-endian int64 = 0)
    // already zero from array init

    // x, y as int32 (device pixels)
    const auto writeI32 = [&](int offset, int32_t val) {
        msg[offset+0] = (val >> 24) & 0xFF;
        msg[offset+1] = (val >> 16) & 0xFF;
        msg[offset+2] = (val >>  8) & 0xFF;
        msg[offset+3] = (val      ) & 0xFF;
    };

    writeI32(10, static_cast<int32_t>(e.x * m_deviceWidth));
    writeI32(14, static_cast<int32_t>(e.y * m_deviceHeight));
    writeI32(18, static_cast<int32_t>(m_deviceWidth));
    writeI32(22, static_cast<int32_t>(m_deviceHeight));

    // pressure (2 bytes): 0xFFFF = full contact
    msg[26] = 0xFF; msg[27] = 0xFF;

    sendControlMessage(msg.data(), msg.size());
}

void AndroidControlBridge::sendKey(const KeyEvent& e) {
    if (!m_connected.load()) return;

    // 14-byte key message
    std::array<uint8_t, 14> msg{};
    msg[0] = MSG_INJECT_KEYCODE;
    msg[1] = e.isDown ? 0 : 1; // 0=down, 1=up

    // keyCode as int32 big-endian at offset 2
    const int32_t kc = static_cast<int32_t>(e.deviceKeyCode);
    msg[2] = (kc >> 24) & 0xFF;
    msg[3] = (kc >> 16) & 0xFF;
    msg[4] = (kc >>  8) & 0xFF;
    msg[5] = (kc      ) & 0xFF;

    sendControlMessage(msg.data(), msg.size());
}

void AndroidControlBridge::sendText(const std::string& text) {
    if (!m_connected.load() || text.empty()) return;
    // Text message: type(1) + length(4) + UTF-8 bytes
    const uint32_t len = static_cast<uint32_t>(text.size());
    std::vector<uint8_t> msg(5 + text.size());
    msg[0] = MSG_INJECT_TEXT;
    msg[1] = (len >> 24) & 0xFF;
    msg[2] = (len >> 16) & 0xFF;
    msg[3] = (len >>  8) & 0xFF;
    msg[4] = (len      ) & 0xFF;
    memcpy(msg.data() + 5, text.data(), text.size());
    sendControlMessage(msg.data(), msg.size());
    AURA_LOG_DEBUG("AndroidControlBridge", "Text injected: {}", text);
}

// ─── Task 147: Gesture convenience wrappers ───────────────────────────────────

void AndroidControlBridge::sendTap(int x, int y) {
    if (!m_connected.load()) return;
    // Tap = DOWN + UP at the same point with 30ms interval
    TouchEvent down, up;
    down.type = TouchEventType::Down;  down.x = (float)x; down.y = (float)y;
    up.type   = TouchEventType::Up;    up.x   = (float)x; up.y   = (float)y;
    sendTouch(down);
    sendTouch(up);
    AURA_LOG_DEBUG("AndroidControlBridge", "Tap at ({},{})", x, y);
}

void AndroidControlBridge::sendSwipe(int x0, int y0, int x1, int y1, int durationMs) {
    if (!m_connected.load()) return;
    // Simulate a swipe as DOWN → MOVE(s) → UP
    int steps = std::max(2, durationMs / 16); // ~60Hz steps
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        float fx = x0 + t * (x1 - x0);
        float fy = y0 + t * (y1 - y0);
        TouchEvent e;
        e.type = (i == 0) ? TouchEventType::Down
               : (i == steps) ? TouchEventType::Up
               : TouchEventType::Move;
        e.x = fx; e.y = fy;
        sendTouch(e);
    }
    AURA_LOG_DEBUG("AndroidControlBridge",
        "Swipe ({},{}) → ({},{}) {}ms", x0, y0, x1, y1, durationMs);
}

void AndroidControlBridge::sendKeyEvent(int keycode) {
    if (!m_connected.load()) return;
    KeyEvent e;
    e.keycode = keycode;
    e.action  = 0; // ACTION_DOWN
    sendKey(e);
    e.action  = 1; // ACTION_UP
    sendKey(e);
    AURA_LOG_DEBUG("AndroidControlBridge", "Key event: keycode={}", keycode);
}


// openControlSocket() — extracted helper, called by connect()
// Opens a TCP socket to the scrcpy control port (27183) on the device
bool AndroidControlBridge::openControlSocket() {
    if (!m_ctrl) return false;
    if (m_ctrl->controlSocket != INVALID_SOCKET) return true; // already open

    m_ctrl->controlSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_ctrl->controlSocket == INVALID_SOCKET) {
        AURA_LOG_ERROR("AndroidControlBridge",
            "openControlSocket: socket() failed: {}", WSAGetLastError());
        return false;
    }

    constexpr uint16_t kScrcpyControlPort = 27183;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(kScrcpyControlPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // forwarded via ADB

    if (::connect(m_ctrl->controlSocket,
                  reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        AURA_LOG_WARN("AndroidControlBridge",
            "openControlSocket: connect failed: {} — touch injection unavailable.",
            WSAGetLastError());
        closesocket(m_ctrl->controlSocket);
        m_ctrl->controlSocket = INVALID_SOCKET;
        return false;
    }

    AURA_LOG_INFO("AndroidControlBridge", "Control socket opened on port {}.",
                  kScrcpyControlPort);
    return true;
}

} // namespace aura

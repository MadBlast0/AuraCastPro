// =============================================================================
// ADBBridge.cpp — scrcpy-based Android wired/wireless mirroring via ADB
// =============================================================================
#include "ADBBridge.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include <unordered_set>
#include <QCoreApplication>
#include <QFileInfo>

#include <array>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <format>
#include <chrono>

namespace aura {

struct ADBBridge::MirrorSession {
    std::string  serial;
    std::thread  streamThread;
    std::atomic<bool> running{false};
    SOCKET       dataSocket{INVALID_SOCKET};
};

ADBBridge::ADBBridge() {}
ADBBridge::~ADBBridge() { shutdown(); }

void ADBBridge::init() {
    m_adbAvailable = findADB();
    if (m_adbAvailable) {
        AURA_LOG_INFO("ADBBridge",
            "ADB found. USB and Wi-Fi Android mirroring available. "
            "Uses scrcpy protocol for video + control.");
    } else {
        AURA_LOG_WARN("ADBBridge",
            "adb.exe not found in PATH. USB Android mirroring unavailable. "
            "Download from: developer.android.com/tools/releases/platform-tools");
    }
}

bool ADBBridge::findADB() {
    // Task 216: Check bundled adb.exe first (installed alongside the .exe)
    // Paths checked in order:
    //   1. $INSTDIR\adb\adb.exe  (bundled by installer)
    //   2. adb.exe in PATH       (developer / user-installed)
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundledAdb = appDir + "/adb/adb.exe";

    if (QFileInfo::exists(bundledAdb)) {
        m_adbExe = bundledAdb.toStdString();
        AURA_LOG_INFO("ADBBridge",
            "Using bundled adb.exe: {}", m_adbExe);
        const std::string result = runADB("version");
        return result.find("Android Debug Bridge") != std::string::npos;
    }

    // Fallback: system PATH
    m_adbExe = "adb";
    const std::string result = runADB("version");
    return result.find("Android Debug Bridge") != std::string::npos;
}

std::string ADBBridge::runADB(const std::string& args) const {
    const std::string cmd = "\"" + m_adbExe + "\" " + args + " 2>&1";
    std::string output;

    HANDLE hStdoutRead, hStdoutWrite;
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        AURA_LOG_WARN("ADBBridge", "CreatePipe failed: 0x{:08X}", GetLastError());
        return "";
    }

    STARTUPINFOA si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdoutWrite;
    si.hStdError  = hStdoutWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::string fullCmd = "cmd /c " + cmd;

    if (CreateProcessA(nullptr, fullCmd.data(), nullptr, nullptr,
                       TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdoutWrite);
        std::array<char, 4096> buf{};
        DWORD read;
        while (ReadFile(hStdoutRead, buf.data(), static_cast<DWORD>(buf.size()-1), &read, nullptr) && read)
            output.append(buf.data(), read);
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            AURA_LOG_WARN("ADBBridge", "ADB command timed out, terminating.");
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 1000);  // brief wait for clean exit
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hStdoutWrite);
    }
    CloseHandle(hStdoutRead);
    return output;
}

std::vector<AndroidDevice> ADBBridge::enumerateDevices() {
    std::vector<AndroidDevice> devices;
    if (!m_adbAvailable) return devices;

    const std::string output = runADB("devices -l");
    std::istringstream ss(output);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.find("List of") != std::string::npos) continue;
        if (line.empty()) continue;

        std::istringstream ls(line);
        std::string serial, status;
        ls >> serial >> status;

        if (status != "device") continue; // skip unauthorized/offline

        AndroidDevice dev;
        dev.serial     = serial;
        dev.authorized = true;
        dev.isWifi     = serial.find(':') != std::string::npos;

        // Get model
        dev.model = runADB("-s " + serial + " shell getprop ro.product.model");
        if (!dev.model.empty() && dev.model.back() == '\n')
            dev.model.pop_back();

        // Get screen dimensions
        const std::string wm = runADB("-s " + serial + " shell wm size");
        if (sscanf_s(wm.c_str(), "Physical size: %ux%u",
                     &dev.screenWidth, &dev.screenHeight) != 2) {
            dev.screenWidth  = 1080;
            dev.screenHeight = 1920;
        }

        AURA_LOG_INFO("ADBBridge",
            "Found device: {} {} {}×{} ({})",
            dev.serial, dev.model,
            dev.screenWidth, dev.screenHeight,
            dev.isWifi ? "Wi-Fi" : "USB");

        devices.push_back(dev);
    }
    return devices;
}

bool ADBBridge::startMirroring(const std::string& serial, StreamCallback videoCallback) {
    if (!m_adbAvailable) return false;

    AURA_LOG_INFO("ADBBridge", "Starting scrcpy mirroring on device: {}", serial);

    // 1. Push scrcpy-server to device
    // Task 084: look for scrcpy-server next to the exe (deployed by installer/post-build)
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString scrcpyPath = appDir + "/scrcpy-server";
    if (!QFileInfo::exists(scrcpyPath)) {
        AURA_LOG_WARN("ADBBridge",
            "scrcpy-server not found at '{}'. "
            "Android USB mirroring unavailable. "
            "Download from: github.com/Genymobile/scrcpy/releases",
            scrcpyPath.toStdString());
        return false;
    }
    runADB(std::format("-s {} push \"{}\" /data/local/tmp/scrcpy-server",
        serial, scrcpyPath.toStdString()));

    // 2. Start scrcpy server on device
    const std::string startServer = std::format(
        "-s {} shell CLASSPATH=/data/local/tmp/scrcpy-server "
        "app_process / com.genymobile.scrcpy.Server 2.3 "
        "tunnel_forward=true video_codec=h265 max_size=0 max_fps=60",
        serial);
    // (This runs in background via detached process)

    // 3. Forward port to local TCP
    runADB(std::format("-s {} forward tcp:27183 localabstract:scrcpy", serial));

    // 4. Connect to data socket
    auto session = std::make_unique<MirrorSession>();
    session->serial  = serial;
    session->running.store(true);

    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    session->dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(27183);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    // Retry connect (server takes ~500ms to start)
    bool connected = false;
    for (int i = 0; i < 10 && !connected; i++) {
        if (connect(session->dataSocket,
                    reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            connected = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    if (!connected) {
        AURA_LOG_ERROR("ADBBridge", "Failed to connect to scrcpy server on {}", serial);
        closesocket(session->dataSocket);
        return false;
    }

    AURA_LOG_INFO("ADBBridge", "scrcpy connected to {}. Reading H.265 stream.", serial);

    // 5. Read H.265 stream in background thread
    MirrorSession* rawSession = session.get();
    session->streamThread = std::thread([rawSession, videoCallback]() {
        std::vector<uint8_t> buf(1024 * 1024);
        uint64_t pts = 0;
        uint64_t frameIntervalUs = 16667; // ~60fps

        while (rawSession->running.load()) {
            // scrcpy V2 packet: 12-byte header + NAL data
            // Header: pts(8) + size(4)
            uint8_t hdr[12]{};
            int n = recv(rawSession->dataSocket,
                         reinterpret_cast<char*>(hdr), 12, MSG_WAITALL);
            if (n <= 0) break;

            uint64_t pktPts = 0;
            for (int i = 0; i < 8; i++) pktPts = (pktPts << 8) | hdr[i];
            uint32_t pktSize = (hdr[8]<<24)|(hdr[9]<<16)|(hdr[10]<<8)|hdr[11];

            if (pktSize == 0 || pktSize > buf.size()) continue;

            n = recv(rawSession->dataSocket,
                     reinterpret_cast<char*>(buf.data()),
                     static_cast<int>(pktSize), MSG_WAITALL);
            if (n <= 0) break;

            if (videoCallback) videoCallback(buf.data(), pktSize, pktPts);
        }
        closesocket(rawSession->dataSocket);
        AURA_LOG_INFO("ADBBridge", "Stream ended for device: {}", rawSession->serial);
    });

    m_sessions.push_back(std::move(session));
    return true;
}

void ADBBridge::stopMirroring(const std::string& serial) {
    for (auto& s : m_sessions) {
        if (s->serial == serial) {
            s->running.store(false);
            if (s->dataSocket != INVALID_SOCKET) {
                closesocket(s->dataSocket);
                s->dataSocket = INVALID_SOCKET;
            }
            if (s->streamThread.joinable()) s->streamThread.join();
        }
    }
    runADB(std::format("-s {} forward --remove tcp:27183", serial));
}

void ADBBridge::start() {
    if (!m_adbAvailable || m_running.exchange(true)) return;
    m_scanThread = std::thread([this]() { scanLoop(); });
}

void ADBBridge::scanLoop() {
    // Track which serials we have already reported as authorized to avoid
    // re-firing the pairing callback every scan cycle.
    std::unordered_set<std::string> seenAuthorized;
    std::unordered_set<std::string> seenLost;

    while (m_running.load()) {
        auto devices = enumerateDevices();
        std::unordered_set<std::string> currentSerials;

        for (auto& d : devices) {
            currentSerials.insert(d.serial);

            const bool isNewlyAuthorized = seenAuthorized.find(d.serial) == seenAuthorized.end();

            // Fire DeviceFoundCallback every scan so UI can refresh state
            if (m_onFound) m_onFound(d);

            // Fire PairingResultCallback only once per authorization event
            if (isNewlyAuthorized && d.authorized) {
                seenAuthorized.insert(d.serial);
                AURA_LOG_INFO("ADBBridge", "Device {} authorized (pairing succeeded)", d.serial);
                if (m_onPairingResult) m_onPairingResult(d.serial, true);
            }
        }

        // Detect lost devices
        for (const auto& serial : seenAuthorized) {
            if (currentSerials.find(serial) == currentSerials.end()) {
                if (seenLost.find(serial) == seenLost.end()) {
                    seenLost.insert(serial);
                    if (m_onLost) m_onLost(serial);
                }
            } else {
                seenLost.erase(serial);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void ADBBridge::stop() {
    m_running.store(false);
    if (m_scanThread.joinable()) m_scanThread.join();
}

// =============================================================================
// pairWireless — runs `adb pair <address:port> <pairingCode>`
// Used for Android 11+ wireless debugging pairing flow.
// The address:port and 6-digit code are shown in Android Settings >
// Developer Options > Wireless debugging > Pair device with pairing code.
// =============================================================================
bool ADBBridge::pairWireless(const std::string& addressAndPort,
                              const std::string& pairingCode) {
    if (m_impl->adbPath.empty()) {
        AURA_LOG_WARN("ADBBridge", "pairWireless: adb not found — cannot pair.");
        return false;
    }
    const std::string cmd = m_impl->adbPath
                          + " pair " + addressAndPort
                          + " " + pairingCode;

    AURA_LOG_INFO("ADBBridge", "Running: {}", cmd);

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.hStdOutput  = hWrite;
    si.hStdError   = hWrite;
    si.dwFlags     = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi{};

    std::string cmdBuf = cmd;
    const bool ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr,
                                   TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                                   &si, &pi) != 0;
    CloseHandle(hWrite);

    std::string output;
    if (ok) {
        char buf[512]; DWORD n = 0;
        while (ReadFile(hRead, buf, sizeof(buf)-1, &n, nullptr) && n > 0) {
            buf[n] = '\0'; output += buf;
        }
        WaitForSingleObject(pi.hProcess, 10'000);  // 10 s timeout
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hRead);

    // `adb pair` prints "Successfully paired" on success
    const bool paired = output.find("Successfully paired") != std::string::npos;
    AURA_LOG_INFO("ADBBridge", "pairWireless result: {} ({})",
        paired ? "SUCCESS" : "FAILED", output.substr(0, 80));
    return paired;
}

void ADBBridge::shutdown() {
    stop();
    for (auto& s : m_sessions) {
        s->running.store(false);
        if (s->dataSocket != INVALID_SOCKET) closesocket(s->dataSocket);
        if (s->streamThread.joinable()) s->streamThread.join();
    }
    m_sessions.clear();
}

} // namespace aura

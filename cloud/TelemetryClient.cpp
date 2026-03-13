// =============================================================================
// TelemetryClient.cpp -- Privacy-first telemetry
//
// Disabled by default. User must opt-in via Settings -> Privacy.
// Sends ONLY: app version, OS version, GPU model, session duration/codec/res,
// and crash stack traces. NEVER sends screen content, IP, or user identity.
// =============================================================================
#include "TelemetryClient.h"
#include "WinHttpHelper.h"
#include "../src/utils/Logger.h"
#include <nlohmann/json.hpp>
#include <thread>
#include "../src/utils/AppVersion.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

using json = nlohmann::json;

namespace aura {

struct TelemetryClient::Impl {};

TelemetryClient::TelemetryClient() : m_impl(std::make_unique<Impl>()) {}
TelemetryClient::~TelemetryClient() {}

void TelemetryClient::init() {
    AURA_LOG_INFO("TelemetryClient",
        "Initialised. Telemetry disabled by default. "
        "Enable via Settings -> Privacy -> Anonymous Diagnostics. "
        "Data sent: app version, OS, GPU model, session stats, crash reports. "
        "NEVER sends: screen content, IP address, or personal info.");
}

void TelemetryClient::reportStartup(const std::string& gpu, const std::string& os) {
    if (!m_enabled.load()) return;
    json payload;
    payload["event"]   = "startup";
    payload["gpu"]     = gpu;
    payload["os"]      = os;
    payload["version"] = aura::AppVersion::string();
    sendAsync("/api/telemetry/event", payload.dump());
}

void TelemetryClient::reportSession(double dur, const std::string& codec,
                                     uint32_t w, uint32_t h, double avgBr) {
    if (!m_enabled.load()) return;
    json payload;
    payload["event"]        = "session_end";
    payload["duration_sec"] = dur;
    payload["codec"]        = codec;
    payload["width"]        = w;
    payload["height"]       = h;
    payload["avg_bitrate"]  = avgBr;
    sendAsync("/api/telemetry/event", payload.dump());
}

void TelemetryClient::reportCrash(const std::string& stackTrace) {
    if (!m_enabled.load()) return;
    json payload;
    payload["event"]       = "crash";
    payload["stack_trace"] = stackTrace.substr(0, 4096); // cap at 4KB
    payload["version"]     = aura::AppVersion::string();
    sendAsync("/api/telemetry/crash", payload.dump());
}

void TelemetryClient::sendAsync(const std::string& endpoint,
                                 const std::string& body) {
    // Fire-and-forget HTTPS POST -- Task 195: proxy-aware via WinHttpHelper
    std::thread([endpoint, body]() {
        HINTERNET session = aura::WinHttpHelper::openSession(
            L"AuraCastPro-Telemetry/1.0");
        if (!session) {
            AURA_LOG_WARN("TelemetryClient",
                "WinHttpOpen failed for telemetry POST: {}", GetLastError());
            return;
        }

        HINTERNET connect = WinHttpConnect(session,
            L"auracastpro.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

        const std::wstring wEndpoint(endpoint.begin(), endpoint.end());
        HINTERNET request = connect ? WinHttpOpenRequest(connect,
            L"POST", wEndpoint.c_str(), nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;

        if (request) {
            const std::wstring ct = L"Content-Type: application/json";
            WinHttpAddRequestHeaders(request, ct.c_str(), (DWORD)-1L,
                                     WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    const_cast<char*>(body.c_str()),
                    (DWORD)body.size(), (DWORD)body.size(), 0)) {
                // Receive and discard the response to allow clean TCP close
                WinHttpReceiveResponse(request, nullptr);
            }
            WinHttpCloseHandle(request);
        }
        if (connect) WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);

        AURA_LOG_DEBUG("TelemetryClient", "Telemetry POST sent to {}", endpoint);
    }).detach();
}

void TelemetryClient::shutdown() {}

} // namespace aura

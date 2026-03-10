// =============================================================================
// UpdateService.cpp — HTTPS update check using WinHTTP
//
// Checks https://auracastpro.com/api/version every 24 hours.
// Response format:
// {
//   "version":       "1.2.0",
//   "download_url":  "https://auracastpro.com/download/AuraCastPro_Setup_1.2.0.exe",
//   "release_notes": "Bug fixes and performance improvements.",
//   "is_critical":   false
// }
// =============================================================================
#include "UpdateService.h"
#include "WinHttpHelper.h"
#include "../src/utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <sstream>

using json = nlohmann::json;

namespace aura {

struct UpdateService::Impl {
    std::thread    checkThread;
    std::string    currentVersion;
    UpdateCallback onUpdateAvailable; // persistent callback set by caller
};

UpdateService::UpdateService() : m_impl(std::make_unique<Impl>()) {}
UpdateService::~UpdateService() { shutdown(); }

void UpdateService::init() {
    AURA_LOG_INFO("UpdateService",
        "Initialised. Endpoint: https://auracastpro.com/api/version. "
        "Check interval: 24 hours. Uses WinHTTP (TLS 1.3).");
}

void UpdateService::setUpdateAvailableCallback(UpdateCallback cb) {
    m_impl->onUpdateAvailable = std::move(cb);
}

void UpdateService::startAutoCheck(const std::string& currentVersion) {
    m_impl->currentVersion = currentVersion;
    m_running.store(true);

    m_impl->checkThread = std::thread([this]() {
        while (m_running.load()) {
            checkNow(m_impl->currentVersion, [this](const UpdateInfo& info) {
                if (info.available) {
                    AURA_LOG_INFO("UpdateService",
                        "Update available: v{} — {}",
                        info.latestVersion, info.downloadUrl);
                    // Fire persistent callback (wired to HubWindow in main.cpp)
                    if (m_impl->onUpdateAvailable)
                        m_impl->onUpdateAvailable(info);
                }
            });
            // Wait 24 hours between checks
            for (int i = 0; i < 24 * 60 && m_running.load(); i++)
                std::this_thread::sleep_for(std::chrono::minutes(1));
        }
    });
}

void UpdateService::checkNow(const std::string& currentVersion, UpdateCallback cb) {
    // Perform HTTP GET via WinHTTP on a background thread.
    // Use a shared running flag so the lambda doesn't capture 'this' unsafely.
    auto running = std::shared_ptr<std::atomic<bool>>(
        &m_running, [](std::atomic<bool>*){});  // non-owning alias
    auto compareVersionsCopy = [](const std::string& v1, const std::string& v2) {
    auto parse = [](const std::string& v) {
        int maj=0,min=0,pat=0; sscanf_s(v.c_str(),"%d.%d.%d",&maj,&min,&pat);
        return maj*10000+min*100+pat;
    };
    return parse(v1)-parse(v2);
};
std::thread([currentVersion, cb, running, compareVersionsCopy]() {
        if (!running->load()) return;
        // Task 195: use WinHttpHelper so proxy is honoured
        HINTERNET session = aura::WinHttpHelper::openSession(
            L"AuraCastPro-Updater/1.0");

        if (!session) {
            LOG_WARN("UpdateService: WinHttpOpen failed: {}", GetLastError());
            return;
        }

        HINTERNET connect = WinHttpConnect(session,
            L"auracastpro.com", INTERNET_DEFAULT_HTTPS_PORT, 0);


        HINTERNET request = connect ? WinHttpOpenRequest(connect,
            L"GET", L"/api/version",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE) : nullptr;

        if (!request) {
            if (connect) WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            AURA_LOG_DEBUG("UpdateService", "Update check skipped (no network).");
            return;
        }

        if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr)) {

            std::string body;
            DWORD bytesAvail = 0;
            constexpr size_t kMaxUpdateResp = 32 * 1024; // 32 KB max for version JSON
            while (WinHttpQueryDataAvailable(request, &bytesAvail) && bytesAvail > 0) {
                if (body.size() + bytesAvail > kMaxUpdateResp) break;
                std::string chunk(bytesAvail, '\0');
                DWORD bytesRead = 0;
                WinHttpReadData(request, chunk.data(),
                                static_cast<DWORD>(chunk.size()), &bytesRead);
                chunk.resize(bytesRead);
                body += chunk;
            }

            try {
                const json j = json::parse(body);
                UpdateInfo info;
                info.latestVersion = j.value("version",       "");
                info.downloadUrl   = j.value("download_url",  "");
                info.releaseNotes  = j.value("release_notes", "");
                info.isCritical    = j.value("is_critical",   false);
                info.available     = compareVersionsCopy(info.latestVersion, currentVersion) > 0;
                if (cb) cb(info);
            } catch (...) { /* parse error — ignore */ }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
    }).detach();
}

int UpdateService::compareVersions(const std::string& v1, const std::string& v2) {
    auto parse = [](const std::string& v) {
        int maj=0, min=0, pat=0;
        sscanf_s(v.c_str(), "%d.%d.%d", &maj, &min, &pat);
        return maj * 10000 + min * 100 + pat;
    };
    return parse(v1) - parse(v2);
}

void UpdateService::shutdown() {
    m_running.store(false);
    if (m_impl->checkThread.joinable()) m_impl->checkThread.join();
}

} // namespace aura

#pragma once
// =============================================================================
// UpdateService.h -- Background update checker
// =============================================================================
#include <string>
#include <functional>
#include <atomic>
#include <memory>

namespace aura {

struct UpdateInfo {
    std::string latestVersion;
    std::string downloadUrl;
    std::string releaseNotes;
    bool        isCritical{false};
    bool        available{false};
};

class UpdateService {
public:
    using UpdateCallback = std::function<void(const UpdateInfo&)>;

    UpdateService();
    ~UpdateService();

    void init();
    void shutdown();

    // Starts a background 24-hour check timer
    void startAutoCheck(const std::string& currentVersion);

    // One-shot manual check (async, non-blocking)
    void checkNow(const std::string& currentVersion, UpdateCallback cb);

    // Set persistent callback fired when an update is found by the auto-check thread.
    // Called on the background check thread -- caller must marshal to UI thread if needed.
    void setUpdateAvailableCallback(UpdateCallback cb);

    static int compareVersions(const std::string& v1, const std::string& v2);

private:
    std::atomic<bool> m_running{false};
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace aura

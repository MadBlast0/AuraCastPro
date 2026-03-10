#pragma once
#include <string>

class AppDataInit {
public:
    static void ensureDirectoriesExist();
    static std::wstring getAppDataPath();
    static std::wstring getLogsPath();
    static std::wstring getRecordingsPath();
    static std::wstring getSecurityPath();
    static std::wstring getCrashesPath();
    static std::wstring getScreenshotsPath();

private:
    static void createIfMissing(const std::wstring& path);
};

#include "AppDataInit.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdexcept>

void AppDataInit::ensureDirectoriesExist() {
    wchar_t* appDataPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath);
    if (FAILED(hr) || !appDataPath) {
        throw std::runtime_error("Failed to get AppData path");
    }
    std::wstring base = std::wstring(appDataPath) + L"\\AuraCastPro";
    CoTaskMemFree(appDataPath);

    createIfMissing(base);
    createIfMissing(base + L"\\logs");
    createIfMissing(base + L"\\recordings");
    createIfMissing(base + L"\\security");
    createIfMissing(base + L"\\crashes");
    createIfMissing(base + L"\\screenshots");
}

std::wstring AppDataInit::getAppDataPath() {
    wchar_t* raw = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw);
    if (FAILED(hr) || !raw) {
        return L"C:\\Users\\Public\\AuraCastPro";  // safe fallback
    }
    std::wstring result = std::wstring(raw) + L"\\AuraCastPro";
    CoTaskMemFree(raw);
    return result;
}

std::wstring AppDataInit::getLogsPath()       { return getAppDataPath() + L"\\logs"; }
std::wstring AppDataInit::getRecordingsPath() { return getAppDataPath() + L"\\recordings"; }
std::wstring AppDataInit::getSecurityPath()   { return getAppDataPath() + L"\\security"; }
std::wstring AppDataInit::getCrashesPath()    { return getAppDataPath() + L"\\crashes"; }
std::wstring AppDataInit::getScreenshotsPath(){ return getAppDataPath() + L"\\screenshots"; }

void AppDataInit::createIfMissing(const std::wstring& path) {
    if (!CreateDirectoryW(path.c_str(), nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            throw std::runtime_error("Failed to create directory");
        }
    }
}

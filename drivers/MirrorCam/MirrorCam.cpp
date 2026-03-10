// MirrorCam.cpp — DirectShow virtual camera source filter
// Reads frames from AuraCastPro shared memory (SharedMemoryIPC) and delivers
// them to any application requesting camera input (OBS, Zoom, Teams, etc.)
//
// Build: Standard C++ DLL, no WDK required.
// Install: regsvr32 MirrorCam.dll (as Administrator)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dshow.h>
#include <strmif.h>
#include <uuids.h>
#include <comdef.h>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstring>

// Shared memory consumer (compiled into DLL)
// We duplicate the IPC structs here to avoid dependency on main exe headers.
static const wchar_t* kSharedMemName = L"AuraCastProSharedFrame";
static const wchar_t* kNewFrameEvent = L"AuraCastProNewFrame";
static const wchar_t* kFrameMutex    = L"AuraCastProFrameMutex";

#pragma pack(push, 1)
struct SharedFrameHeader {
    uint32_t width, height, stride, format;
    uint64_t frameIndex;
    uint32_t dataSize;
    uint8_t  reserved[8];
};
#pragma pack(pop)

// {AuraCastPro-VCam-CLSID-0001-0000-000000000001}
DEFINE_GUID(CLSID_MirrorCam,
    0xAC0001, 0x0001, 0x0001,
    0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);

// ─── IPC Reader ───────────────────────────────────────────────────────────────

class IPCReader {
public:
    bool open() {
        m_hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, kSharedMemName);
        if (!m_hMap) return false;
        m_mapped = MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
        m_hEvent = OpenEventW(SYNCHRONIZE, FALSE, kNewFrameEvent);
        m_hMutex = OpenMutexW(SYNCHRONIZE, FALSE, kFrameMutex);
        return m_mapped != nullptr;
    }
    void close() {
        if (m_mapped) { UnmapViewOfFile(m_mapped); m_mapped = nullptr; }
        if (m_hMap)   { CloseHandle(m_hMap);        m_hMap   = nullptr; }
        if (m_hEvent) { CloseHandle(m_hEvent);      m_hEvent = nullptr; }
        if (m_hMutex) { CloseHandle(m_hMutex);      m_hMutex = nullptr; }
    }
    bool waitFrame(DWORD ms = 33) {
        if (!m_hEvent) return false;
        return WaitForSingleObject(m_hEvent, ms) == WAIT_OBJECT_0;
    }
    bool readFrame(std::vector<uint8_t>& out, uint32_t& w, uint32_t& h) {
        if (!m_mapped) return false;
        if (WaitForSingleObject(m_hMutex, 16) != WAIT_OBJECT_0) return false;
        const auto* hdr = static_cast<const SharedFrameHeader*>(m_mapped);
        w = hdr->width; h = hdr->height;
        if (hdr->dataSize > 0 && hdr->dataSize < 3840*2160*4+1) {
            const uint8_t* pixels =
                static_cast<const uint8_t*>(m_mapped) + sizeof(SharedFrameHeader);
            out.assign(pixels, pixels + hdr->dataSize);
        }
        ReleaseMutex(m_hMutex);
        return !out.empty();
    }
private:
    HANDLE m_hMap = nullptr, m_hEvent = nullptr, m_hMutex = nullptr;
    void*  m_mapped = nullptr;
};

// ─── DLL entry point & COM registration ──────────────────────────────────────

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    return TRUE;
}

extern "C" __declspec(dllexport) HRESULT DllRegisterServer() {
    // Register as video capture source
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    // HKLM\SOFTWARE\Classes\CLSID\{...}
    std::wstring clsidStr = L"{AC000100-0001-0001-8000-00AA00389B71}";
    std::wstring key = L"SOFTWARE\\Classes\\CLSID\\" + clsidStr;
    RegSetKeyValueW(HKEY_LOCAL_MACHINE, key.c_str(),
                    nullptr, REG_SZ, L"AuraCastPro Virtual Camera", 52);
    RegSetKeyValueW(HKEY_LOCAL_MACHINE, (key + L"\\InprocServer32").c_str(),
                    nullptr, REG_SZ, path, (DWORD)(wcslen(path)+1)*2);
    RegSetKeyValueW(HKEY_LOCAL_MACHINE, (key + L"\\InprocServer32").c_str(),
                    L"ThreadingModel", REG_SZ, L"Both", 10);

    // Register as Video Capture Source
    std::wstring vcapKey =
        L"SOFTWARE\\Microsoft\\DirectShow\\VideoCaptureSources\\" + clsidStr;
    RegSetKeyValueW(HKEY_LOCAL_MACHINE, vcapKey.c_str(),
                    nullptr, REG_SZ, L"AuraCastPro Virtual Camera", 52);

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT DllUnregisterServer() {
    std::wstring clsidStr = L"{AC000100-0001-0001-8000-00AA00389B71}";
    RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                   (L"SOFTWARE\\Classes\\CLSID\\" + clsidStr).c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                   (L"SOFTWARE\\Microsoft\\DirectShow\\VideoCaptureSources\\" + clsidStr).c_str());
    return S_OK;
}

// Forward declaration — implemented in MirrorCam_Filter.cpp
extern "C" HRESULT MirrorCam_GetClassObject(REFCLSID, REFIID, LPVOID*);

extern "C" __declspec(dllexport) HRESULT DllGetClassObject(
    REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    // Delegate to the full COM class factory in MirrorCam_Filter.cpp
    return MirrorCam_GetClassObject(rclsid, riid, ppv);
}

extern "C" __declspec(dllexport) HRESULT DllCanUnloadNow() {
    return S_OK;
}

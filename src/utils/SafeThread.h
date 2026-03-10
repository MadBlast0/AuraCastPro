#pragma once
#include <functional>
#include <string>
#include <stdexcept>
#include "Logger.h"

// Wraps any callable in a try/catch so thread crashes are logged
// and surfaced to the UI instead of silently calling std::terminate().
//
// Usage:
//   std::thread t([]{ safeThreadRun("NetworkWorker", []{ doWork(); }); });

template<typename Func>
void safeThreadRun(const std::string& threadName, Func&& func) {
    try {
        func();
    } catch (const std::exception& e) {
        LOG_CRITICAL("Thread '{}' threw exception: {}", threadName, e.what());
        // Post error to main thread via Qt if available
        #ifdef QT_CORE_LIB
        QMetaObject::invokeMethod(qApp, [msg = std::string(e.what()), threadName](){
            // ErrorDialog will be shown if it exists
            LOG_ERROR("Background thread error in {}: {}", threadName, msg);
        }, Qt::QueuedConnection);
        #endif
    } catch (...) {
        LOG_CRITICAL("Thread '{}' threw unknown exception", threadName);
    }
}

// RAII thread name setter (useful for debugger / profiling tools)
class ScopedThreadName {
public:
    explicit ScopedThreadName(const std::wstring& name) {
        #pragma pack(push,8)
        struct THREADNAME_INFO {
            DWORD  dwType;     // 0x1000
            LPCSTR szName;
            DWORD  dwThreadID; // -1 = calling thread
            DWORD  dwFlags;
        };
        #pragma pack(pop)
        // Convert to narrow for SetThreadDescription fallback
        std::string narrow(name.begin(), name.end());
        THREADNAME_INFO info = { 0x1000, narrow.c_str(), (DWORD)-1, 0 };
        __try {
            RaiseException(0x406D1388, 0, sizeof(info)/sizeof(ULONG_PTR),
                           reinterpret_cast<ULONG_PTR*>(&info));
        } __except(EXCEPTION_EXECUTE_HANDLER) {}

        // Also use modern Win10+ API
        #if defined(_WIN32)
        typedef HRESULT(WINAPI* PFN_SetThreadDescription)(HANDLE, PCWSTR);
        auto fn = (PFN_SetThreadDescription)GetProcAddress(
            GetModuleHandleW(L"kernelbase.dll"), "SetThreadDescription");
        if (fn) fn(GetCurrentThread(), name.c_str());
        #endif
    }
};

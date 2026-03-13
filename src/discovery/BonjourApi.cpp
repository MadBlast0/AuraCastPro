#include "../pch.h"
#include "BonjourApi.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aura::bonjour {
namespace {

using DNSServiceRegisterFn = DNSServiceErrorType(__stdcall*)(
    DNSServiceRef*,
    DNSServiceFlags,
    uint32_t,
    const char*,
    const char*,
    const char*,
    const char*,
    uint16_t,
    uint16_t,
    const void*,
    void*,
    void*);

using DNSServiceRefDeallocateFn = void(__stdcall*)(DNSServiceRef);

struct BonjourRuntime {
    HMODULE module{nullptr};
    DNSServiceRegisterFn registerFn{nullptr};
    DNSServiceRefDeallocateFn deallocateFn{nullptr};
    const char* loadedPath{nullptr};
    bool attempted{false};
};

BonjourRuntime& runtime() {
    static BonjourRuntime rt;
    if (rt.attempted) {
        return rt;
    }

    rt.attempted = true;
    const char* candidates[] = {
        "dnssd.dll",
        "C:\\Windows\\System32\\dnssd.dll",
        "C:\\Program Files\\Bonjour\\dnssd.dll",
        "C:\\Program Files (x86)\\Bonjour\\dnssd.dll",
    };

    for (const char* candidate : candidates) {
        HMODULE module = LoadLibraryA(candidate);
        if (!module) {
            continue;
        }

        auto registerFn = reinterpret_cast<DNSServiceRegisterFn>(
            GetProcAddress(module, "DNSServiceRegister"));
        auto deallocateFn = reinterpret_cast<DNSServiceRefDeallocateFn>(
            GetProcAddress(module, "DNSServiceRefDeallocate"));

        if (registerFn && deallocateFn) {
            rt.module = module;
            rt.registerFn = registerFn;
            rt.deallocateFn = deallocateFn;
            rt.loadedPath = candidate;
            return rt;
        }

        FreeLibrary(module);
    }

    return rt;
}

} // namespace

bool isAvailable() {
    const auto& rt = runtime();
    return rt.registerFn != nullptr && rt.deallocateFn != nullptr;
}

const char* libraryPath() {
    const auto& rt = runtime();
    return rt.loadedPath ? rt.loadedPath : "";
}

DNSServiceErrorType registerService(
    DNSServiceRef* sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char* name,
    const char* regtype,
    const char* domain,
    const char* host,
    uint16_t port,
    uint16_t txtLen,
    const void* txtRecord) {
    auto& rt = runtime();
    if (!rt.registerFn) {
        return -65537;
    }

    return rt.registerFn(sdRef, flags, interfaceIndex, name, regtype, domain,
        host, port, txtLen, txtRecord, nullptr, nullptr);
}

void deallocate(DNSServiceRef sdRef) {
    auto& rt = runtime();
    if (rt.deallocateFn && sdRef) {
        rt.deallocateFn(sdRef);
    }
}

} // namespace aura::bonjour

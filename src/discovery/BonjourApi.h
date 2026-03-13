#pragma once

#include <cstdint>

namespace aura::bonjour {

using DNSServiceFlags = uint32_t;
using DNSServiceErrorType = int32_t;
using DNSServiceRef = void*;

constexpr DNSServiceErrorType kDNSServiceErr_NoError = 0;
constexpr uint32_t kDNSServiceInterfaceIndexAny = 0;

bool isAvailable();
const char* libraryPath();

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
    const void* txtRecord);

void deallocate(DNSServiceRef sdRef);

} // namespace aura::bonjour

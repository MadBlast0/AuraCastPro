#pragma once
#include <string>
#include <cstdint>

enum class PortStatus { Available, Conflict, CheckFailed };

struct PortConflictInfo {
    PortStatus  status;
    uint16_t    port;
    std::string protocol;      // "UDP" or "TCP"
    std::string processName;   // e.g. "iTunes.exe"
    uint32_t    pid;
};

class PortConflict {
public:
    // Check if a UDP port is free. Returns info with owning process if taken.
    static PortConflictInfo checkUDP(uint16_t port);

    // Check if a TCP port is free.
    static PortConflictInfo checkTCP(uint16_t port);

    // Human-readable message for the UI error dialog
    static std::string formatError(const PortConflictInfo& info);

private:
    static PortConflictInfo findOwner(uint16_t port, bool isTCP);
    static std::string getProcessName(uint32_t pid);
};

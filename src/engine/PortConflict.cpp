#include "../pch.h"  // PCH
#include "PortConflict.h"
#include "../utils/Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <sstream>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ws2_32.lib")

PortConflictInfo PortConflict::checkUDP(uint16_t port) {
    // Try binding a test socket first
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock != INVALID_SOCKET) {
        sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);
        int result = bind(sock, (sockaddr*)&addr, sizeof(addr));
        closesocket(sock);
        WSACleanup();
        if (result == 0) {
            return { PortStatus::Available, port, "UDP", "", 0 };
        }
        if (WSAGetLastError() == WSAEADDRINUSE) {
            return findOwner(port, false);
        }
    }
    WSACleanup();
    return { PortStatus::CheckFailed, port, "UDP", "", 0 };
}

PortConflictInfo PortConflict::checkTCP(uint16_t port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock != INVALID_SOCKET) {
        sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);
        int result = bind(sock, (sockaddr*)&addr, sizeof(addr));
        closesocket(sock);
        WSACleanup();
        if (result == 0) {
            return { PortStatus::Available, port, "TCP", "", 0 };
        }
        if (WSAGetLastError() == WSAEADDRINUSE) {
            return findOwner(port, true);
        }
    }
    WSACleanup();
    return { PortStatus::CheckFailed, port, "TCP", "", 0 };
}

PortConflictInfo PortConflict::findOwner(uint16_t port, bool isTCP) {
    PortConflictInfo info;
    info.port     = port;
    info.protocol = isTCP ? "TCP" : "UDP";
    info.status   = PortStatus::Conflict;

    if (isTCP) {
        ULONG size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_ALL, 0);
        std::vector<BYTE> buf(size);
        if (GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET,
                                TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buf.data());
            for (DWORD i = 0; i < table->dwNumEntries; i++) {
                if (ntohs((WORD)table->table[i].dwLocalPort) == port) {
                    info.pid         = table->table[i].dwOwningPid;
                    info.processName = getProcessName(info.pid);
                    return info;
                }
            }
        }
    } else {
        ULONG size = 0;
        GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET,
                            UDP_TABLE_OWNER_PID, 0);
        std::vector<BYTE> buf(size);
        if (GetExtendedUdpTable(buf.data(), &size, FALSE, AF_INET,
                                UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
            auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buf.data());
            for (DWORD i = 0; i < table->dwNumEntries; i++) {
                if (ntohs((WORD)table->table[i].dwLocalPort) == port) {
                    info.pid         = table->table[i].dwOwningPid;
                    info.processName = getProcessName(info.pid);
                    return info;
                }
            }
        }
    }
    // Could not find owner but port is taken
    info.processName = "unknown process";
    return info;
}

std::string PortConflict::getProcessName(uint32_t pid) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return "unknown (access denied)";
    char buf[MAX_PATH] = {};
    GetModuleBaseNameA(hProc, nullptr, buf, MAX_PATH);
    CloseHandle(hProc);
    return std::string(buf);
}

std::string PortConflict::formatError(const PortConflictInfo& info) {
    std::ostringstream ss;
    if (info.status == PortStatus::Conflict) {
        ss << info.protocol << " port " << info.port
           << " is already in use by " << info.processName
           << " (PID " << info.pid << ").\n\n"
           << "Please close " << info.processName
           << " or change the port in Settings -> Network.";
    } else if (info.status == PortStatus::CheckFailed) {
        ss << "Could not check " << info.protocol << " port " << info.port
           << ". Port may be unavailable. Check Windows Firewall settings.";
    }
    return ss.str();
}

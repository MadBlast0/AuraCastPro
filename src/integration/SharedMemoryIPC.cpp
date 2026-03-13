#include "../pch.h"  // PCH
#include "SharedMemoryIPC.h"
#include "../utils/Logger.h"
#include <cstring>

// ─── Producer ────────────────────────────────────────────────────────────────

bool SharedMemProducer::open() {
    m_hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                    PAGE_READWRITE, 0, (DWORD)kSharedMemSize,
                                    kSharedMemName);
    if (!m_hMapFile) {
        LOG_ERROR("SharedMemProducer: CreateFileMapping failed ({})", GetLastError());
        return false;
    }
    m_mapped = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!m_mapped) {
        LOG_ERROR("SharedMemProducer: MapViewOfFile failed ({})", GetLastError());
        return false;
    }
    m_hEvent = CreateEventW(nullptr, FALSE, FALSE, kNewFrameEvent);
    m_hMutex = CreateMutexW(nullptr, FALSE, kFrameMutex);

    std::memset(m_mapped, 0, kSharedMemSize);
    LOG_INFO("SharedMemProducer: Shared memory opened ({} bytes)", kSharedMemSize);
    return true;
}

void SharedMemProducer::close() {
    if (m_mapped)    { UnmapViewOfFile(m_mapped); m_mapped    = nullptr; }
    if (m_hMapFile)  { CloseHandle(m_hMapFile);   m_hMapFile  = nullptr; }
    if (m_hEvent)    { CloseHandle(m_hEvent);      m_hEvent    = nullptr; }
    if (m_hMutex)    { CloseHandle(m_hMutex);      m_hMutex    = nullptr; }
}

bool SharedMemProducer::writeFrame(const uint8_t* bgra, uint32_t w, uint32_t h) {
    if (!m_mapped || !bgra) return false;

    DWORD wait = WaitForSingleObject(m_hMutex, 16);
    if (wait != WAIT_OBJECT_0) return false;

    auto* hdr = reinterpret_cast<SharedFrameHeader*>(m_mapped);
    hdr->width     = w;
    hdr->height    = h;
    hdr->stride    = w * 4;
    hdr->format    = 0; // BGRA32
    hdr->dataSize  = w * h * 4;
    hdr->frameIndex++;

    uint8_t* pixels = reinterpret_cast<uint8_t*>(m_mapped) + sizeof(SharedFrameHeader);
    if (hdr->dataSize <= kMaxFrameBytes)
        std::memcpy(pixels, bgra, hdr->dataSize);

    ReleaseMutex(m_hMutex);
    SetEvent(m_hEvent);
    return true;
}

// ─── Consumer ────────────────────────────────────────────────────────────────

bool SharedMemConsumer::open() {
    m_hMapFile = OpenFileMappingW(FILE_MAP_READ, FALSE, kSharedMemName);
    if (!m_hMapFile) {
        // Shared memory not yet created by producer -- that's OK, try later
        return false;
    }
    m_mapped = MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 0);
    if (!m_mapped) {
        CloseHandle(m_hMapFile); m_hMapFile = nullptr;
        return false;
    }
    m_hEvent = OpenEventW(SYNCHRONIZE, FALSE, kNewFrameEvent);
    m_hMutex = OpenMutexW(SYNCHRONIZE, FALSE, kFrameMutex);
    return true;
}

void SharedMemConsumer::close() {
    if (m_mapped)   { UnmapViewOfFile(m_mapped); m_mapped   = nullptr; }
    if (m_hMapFile) { CloseHandle(m_hMapFile);   m_hMapFile = nullptr; }
    if (m_hEvent)   { CloseHandle(m_hEvent);     m_hEvent   = nullptr; }
    if (m_hMutex)   { CloseHandle(m_hMutex);     m_hMutex   = nullptr; }
}

bool SharedMemConsumer::waitNextFrame(uint32_t timeoutMs) {
    if (!m_hEvent) return false;
    return WaitForSingleObject(m_hEvent, timeoutMs) == WAIT_OBJECT_0;
}

bool SharedMemConsumer::readFrame(uint8_t* outBgra, uint32_t& outW,
                                   uint32_t& outH, uint64_t& outIdx) {
    if (!m_mapped) return false;

    DWORD wait = WaitForSingleObject(m_hMutex, 16);
    if (wait != WAIT_OBJECT_0) return false;

    const auto* hdr = reinterpret_cast<const SharedFrameHeader*>(m_mapped);
    outW   = hdr->width;
    outH   = hdr->height;
    outIdx = hdr->frameIndex;

    if (outBgra && hdr->dataSize > 0 && hdr->dataSize <= kMaxFrameBytes) {
        const uint8_t* pixels = reinterpret_cast<const uint8_t*>(m_mapped)
                                 + sizeof(SharedFrameHeader);
        std::memcpy(outBgra, pixels, hdr->dataSize);
    }

    ReleaseMutex(m_hMutex);
    return outIdx > 0;
}

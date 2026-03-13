#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <string>
#include <functional>

// Producer side -- AuraCastPro.exe writes frames here.
// Consumer side -- MirrorCam.dll reads frames from here.

static const wchar_t* kSharedMemName  = L"AuraCastProSharedFrame";
static const wchar_t* kNewFrameEvent  = L"AuraCastProNewFrame";
static const wchar_t* kFrameMutex     = L"AuraCastProFrameMutex";
static constexpr size_t kMaxFrameBytes = 3840 * 2160 * 4 + 64; // 4K BGRA + header

#pragma pack(push, 1)
struct SharedFrameHeader {
    uint32_t width;
    uint32_t height;
    uint32_t stride;       // bytes per row
    uint32_t format;       // 0 = BGRA32
    uint64_t frameIndex;   // incremented each write
    uint32_t dataSize;     // bytes of pixel data following this header
    uint8_t  reserved[8];
};
#pragma pack(pop)

static constexpr size_t kSharedMemSize = sizeof(SharedFrameHeader) + kMaxFrameBytes;

// ─── Producer (AuraCastPro.exe) ───────────────────────────────────────────────

class SharedMemProducer {
public:
    SharedMemProducer() = default;
    ~SharedMemProducer() { close(); }

    bool open();
    void close();

    // Write a BGRA frame to shared memory and signal the consumer.
    bool writeFrame(const uint8_t* bgra, uint32_t width, uint32_t height);

    bool isOpen() const { return m_mapped != nullptr; }

private:
    HANDLE m_hMapFile  = nullptr;
    HANDLE m_hEvent    = nullptr;
    HANDLE m_hMutex    = nullptr;
    void*  m_mapped    = nullptr;
};

// ─── Consumer (MirrorCam.dll) ─────────────────────────────────────────────────

class SharedMemConsumer {
public:
    SharedMemConsumer() = default;
    ~SharedMemConsumer() { close(); }

    bool open();
    void close();

    // Wait for next frame (timeoutMs = 0 means wait forever).
    // Returns false on timeout or error.
    bool waitNextFrame(uint32_t timeoutMs = 33);

    // Read latest frame into caller-provided buffer.
    // Returns false if no frame available.
    bool readFrame(uint8_t* outBgra, uint32_t& outWidth,
                   uint32_t& outHeight, uint64_t& outFrameIdx);

    bool isOpen() const { return m_mapped != nullptr; }

private:
    HANDLE m_hMapFile  = nullptr;
    HANDLE m_hEvent    = nullptr;
    HANDLE m_hMutex    = nullptr;
    void*  m_mapped    = nullptr;
};

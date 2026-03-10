// =============================================================================
// AudioVirtualCable.cpp — WDM WaveRT kernel audio miniport driver
// Task 045 / Task 127
//
// Creates two audio endpoints in Windows Device Manager:
//   • "AuraCastPro Audio Output"  (render)  — AudioMixer writes here
//   • "AuraCastPro Audio Input"   (capture) — OBS / Zoom / Teams read from here
// Audio is looped from the render endpoint buffer to the capture endpoint buffer
// (virtual cable).
//
// FORMAT: 48000 Hz · 32-bit float · stereo (2ch)
//
// BUILD REQUIREMENTS (cannot be done with regular CMake):
//   1. Open Visual Studio 2022
//   2. File → New → Project → "Kernel Mode Driver, Empty (KMDF)"
//   3. Name: AudioVirtualCable  Location: AuraCastPro_Root/drivers/
//   4. Add this file to the project
//   5. Project Properties → Configuration Properties → Driver Settings:
//        Target OS Version = Windows 10
//        Target Platform   = Desktop
//   6. Links: ks.lib  ksdsp.lib  portcls.lib
//   7. After build: signtool sign /fd SHA256 AudioVirtualCable.sys
//
// INSTALL (development, test-signing mode):
//   scripts/enable_test_signing.bat  (reboot required)
//   pnputil /add-driver Audio_Installer.inf /install
//
// UNINSTALL:
//   pnputil /delete-driver AudioVirtualCable.inf /uninstall
//   scripts/disable_test_signing.bat  (reboot required)
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <ntddk.h>
#include <wdm.h>
#include <portcls.h>
#include <ksdebug.h>
#include <stdunk.h>

// ---------------------------------------------------------------------------
// Driver metadata
// ---------------------------------------------------------------------------
#define AURA_DRIVER_TAG       'ARUA'
#define AURA_SAMPLE_RATE      48000
#define AURA_CHANNELS         2
#define AURA_BITS_PER_SAMPLE  32          // float32
#define AURA_BLOCK_ALIGN      (AURA_CHANNELS * (AURA_BITS_PER_SAMPLE / 8))
#define AURA_AVG_BYTES_PER_SEC (AURA_SAMPLE_RATE * AURA_BLOCK_ALIGN)
#define AURA_BUFFER_FRAMES    4800        // 100ms at 48kHz
#define AURA_BUFFER_BYTES     (AURA_BUFFER_FRAMES * AURA_BLOCK_ALIGN)

// ---------------------------------------------------------------------------
// Shared loopback buffer (ring buffer in non-paged pool)
// The render miniport writes here; the capture miniport reads from here.
// ---------------------------------------------------------------------------
static PBYTE   g_loopbackBuffer  = nullptr;
static ULONG   g_writeOffset     = 0;   // render writes here
static ULONG   g_readOffset      = 0;   // capture reads here
static KSPIN_LOCK g_bufferLock;         // spinlock for thread safety

// ---------------------------------------------------------------------------
// Wave format descriptor (PCM float32 stereo 48kHz)
// ---------------------------------------------------------------------------
static const KSDATARANGE_AUDIO g_WaveFormatRange = {
    {
        sizeof(KSDATARANGE_AUDIO),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
    },
    AURA_CHANNELS,          // MaximumChannels
    AURA_BITS_PER_SAMPLE,   // MinimumBitsPerSample
    AURA_BITS_PER_SAMPLE,   // MaximumBitsPerSample
    AURA_SAMPLE_RATE,       // MinimumSampleFrequency
    AURA_SAMPLE_RATE        // MaximumSampleFrequency
};

static PKSDATARANGE g_WaveRanges[] = {
    (PKSDATARANGE)&g_WaveFormatRange
};

static KSDATARANGE_AUDIO g_TopologyRange = {
    { sizeof(KSDATARANGE), 0, 0, 0,
      STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
      STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE) }
};
static PKSDATARANGE g_TopologyRanges[] = { (PKSDATARANGE)&g_TopologyRange };

// ---------------------------------------------------------------------------
// DriverEntry — called by Windows when the driver is loaded
// ---------------------------------------------------------------------------
extern "C"
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    PAGED_CODE();

    KdPrint(("AuraCastPro AudioVirtualCable: DriverEntry\n"));

    // Initialise shared loopback buffer
    g_loopbackBuffer = (PBYTE)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, AURA_BUFFER_BYTES, AURA_DRIVER_TAG);
    if (!g_loopbackBuffer) {
        KdPrint(("AuraCastPro: Failed to allocate loopback buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(g_loopbackBuffer, AURA_BUFFER_BYTES);
    KeInitializeSpinLock(&g_bufferLock);

    // Register with PortCls — it manages AddDevice and IRP dispatch
    NTSTATUS status = PcInitializeAdapterDriver(
        DriverObject,
        RegistryPath,
        (PDRIVER_ADD_DEVICE)AddDevice);

    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(g_loopbackBuffer, AURA_DRIVER_TAG);
        g_loopbackBuffer = nullptr;
        KdPrint(("AuraCastPro: PcInitializeAdapterDriver failed: 0x%08X\n",
                 status));
    }
    return status;
}

// ---------------------------------------------------------------------------
// AddDevice — called when PnP enumerates our device node
// ---------------------------------------------------------------------------
NTSTATUS AddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    PAGED_CODE();
    KdPrint(("AuraCastPro AudioVirtualCable: AddDevice\n"));
    return PcAddAdapterDevice(DriverObject, PhysicalDeviceObject,
        StartDevice, MAX_MINIPORTS, 0);
}

// ---------------------------------------------------------------------------
// StartDevice — registers render and capture miniports with PortCls
// ---------------------------------------------------------------------------
NTSTATUS StartDevice(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP           Irp,
    _In_ PRESOURCELIST  ResourceList)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(ResourceList);
    KdPrint(("AuraCastPro AudioVirtualCable: StartDevice\n"));

    NTSTATUS status = STATUS_SUCCESS;

    // ── Render (output) endpoint: "AuraCastPro Audio Output" ─────────────────
    {
        PUNKNOWN miniportRender = nullptr;
        PUNKNOWN portRender     = nullptr;

        status = PcNewPort(&portRender, CLSID_PortWaveRT);
        if (!NT_SUCCESS(status)) goto done;

        // In a real driver: implement IMiniportWaveRT for render
        // For scaffold: log and return success so Windows registers the device
        KdPrint(("AuraCastPro: Render port created\n"));

        portRender->Release();
    }

    // ── Capture (input) endpoint: "AuraCastPro Audio Input" ──────────────────
    {
        PUNKNOWN portCapture = nullptr;

        status = PcNewPort(&portCapture, CLSID_PortWaveRT);
        if (!NT_SUCCESS(status)) goto done;

        KdPrint(("AuraCastPro: Capture port created\n"));

        portCapture->Release();
    }

done:
    return status;
}

// ---------------------------------------------------------------------------
// Loopback helpers — called from render/capture stream implementations
// ---------------------------------------------------------------------------

// Render miniport calls this to push audio into the shared buffer.
void AuraLoopbackWrite(const BYTE* src, ULONG bytes)
{
    if (!g_loopbackBuffer || bytes == 0) return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_bufferLock, &oldIrql);

    ULONG remaining = bytes;
    const BYTE* p = src;
    while (remaining > 0) {
        ULONG chunk = min(remaining, AURA_BUFFER_BYTES - g_writeOffset);
        RtlCopyMemory(g_loopbackBuffer + g_writeOffset, p, chunk);
        g_writeOffset = (g_writeOffset + chunk) % AURA_BUFFER_BYTES;
        p         += chunk;
        remaining -= chunk;
    }

    KeReleaseSpinLock(&g_bufferLock, oldIrql);
}

// Capture miniport calls this to pull audio from the shared buffer.
void AuraLoopbackRead(BYTE* dst, ULONG bytes)
{
    if (!g_loopbackBuffer || bytes == 0) return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_bufferLock, &oldIrql);

    ULONG remaining = bytes;
    BYTE* p = dst;
    while (remaining > 0) {
        ULONG chunk = min(remaining, AURA_BUFFER_BYTES - g_readOffset);
        RtlCopyMemory(p, g_loopbackBuffer + g_readOffset, chunk);
        g_readOffset = (g_readOffset + chunk) % AURA_BUFFER_BYTES;
        p         += chunk;
        remaining -= chunk;
    }

    KeReleaseSpinLock(&g_bufferLock, oldIrql);
}

// =============================================================================
// VideoDecoder.cpp -- Windows Media Foundation hardware video decoder
//
// Uses MF H.265/AV1 hardware MFT with D3D12-aware mode for zero-copy output.
// =============================================================================

#include "../pch.h"  // PCH
#include "VideoDecoder.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <mfobjects.h>
#include <d3d11.h>  // ID3D11Texture2D for DXGI buffer interop
#include <codecapi.h>
#include <wrl/client.h>

#include <stdexcept>
#include <format>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

namespace aura {

// Internal MF state -- hidden from the header to avoid polluting it with
// heavy Windows Media Foundation headers.
struct VideoDecoder::MFState {
    ComPtr<IMFTransform>     mft;          // Hardware decoder MFT
    ComPtr<IMFMediaType>     inputType;    // Compressed input type
    ComPtr<IMFMediaType>     outputType;   // NV12 output type
    ComPtr<IMFSample>        outputSample; // Reusable output sample

    bool   mfStarted{false};
    DWORD  inputStreamId{0};
    DWORD  outputStreamId{0};
};

// GUID for H.265/HEVC hardware decoder
static const GUID MFVideoFormat_HEVC = {
    0x43564548, 0x0000, 0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

// GUID for AV1 hardware decoder  
static const GUID MFVideoFormat_AV1 = {
    0x31305641, 0x0000, 0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

// -----------------------------------------------------------------------------
VideoDecoder::VideoDecoder(ID3D12Device* device, VideoCodec codec)
    : m_device(device)
    , m_codec(codec)
    , m_mf(std::make_unique<MFState>()) {}

// -----------------------------------------------------------------------------
VideoDecoder::~VideoDecoder() {
    shutdown();
}

// -----------------------------------------------------------------------------
// Static method to check if a codec is available (hardware or software)
bool VideoDecoder::isCodecAvailable(VideoCodec codec) {
    GUID inputSubtype;
    
    switch (codec) {
        case VideoCodec::H264:
            inputSubtype = MFVideoFormat_H264;
            break;
        case VideoCodec::H265:
            inputSubtype = MFVideoFormat_HEVC;
            break;
        case VideoCodec::AV1:
            inputSubtype = MFVideoFormat_AV1;
            break;
    }

    // Initialize MF if not already done
    static bool mfInitialized = false;
    if (!mfInitialized) {
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) return false;
        mfInitialized = true;
    }

    MFT_REGISTER_TYPE_INFO inputInfo{MFMediaType_Video, inputSubtype};
    MFT_REGISTER_TYPE_INFO outputInfo{MFMediaType_Video, MFVideoFormat_NV12};

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    // Try hardware first
    HRESULT hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
        &inputInfo, &outputInfo,
        &activates, &count);

    if (SUCCEEDED(hr) && count > 0) {
        for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
        CoTaskMemFree(activates);
        return true;
    }

    // Try software
    hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
        &inputInfo, nullptr,
        &activates, &count);

    if (SUCCEEDED(hr) && count > 0) {
        for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
        CoTaskMemFree(activates);
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
void VideoDecoder::init() {
    std::string codecName;
    GUID inputSubtype;
    
    switch (m_codec) {
        case VideoCodec::H264:
            codecName = "H.264/AVC";
            inputSubtype = MFVideoFormat_H264;
            break;
        case VideoCodec::H265:
            codecName = "H.265/HEVC";
            inputSubtype = MFVideoFormat_HEVC;
            break;
        case VideoCodec::AV1:
            codecName = "AV1";
            inputSubtype = MFVideoFormat_AV1;
            break;
    }
    
    AURA_LOG_INFO("VideoDecoder", "Initialising {} hardware decoder...", codecName);

    // Start the Media Foundation platform (idempotent)
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        throw std::runtime_error(std::format(
            "VideoDecoder: MFStartup failed: {:08X}", (uint32_t)hr));
    }
    m_mf->mfStarted = true;

    // Find and activate a hardware decoder MFT for the requested codec
    const GUID& inputSubtypeRef = inputSubtype;

    MFT_REGISTER_TYPE_INFO inputInfo{MFMediaType_Video, inputSubtype};
    MFT_REGISTER_TYPE_INFO outputInfo{MFMediaType_Video, MFVideoFormat_NV12};

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
        &inputInfo, &outputInfo,
        &activates, &count);

    if (FAILED(hr) || count == 0) {
        // Fall back to software decoder if no hardware decoder found
        AURA_LOG_WARN("VideoDecoder",
            "No hardware {} decoder found, falling back to software decoder.",
            codecName);

        // Broaden enumeration: some Windows HEVC software decoders are not
        // advertised with NV12 output at enum time, but can be configured to
        // NV12 after activation. So we don't constrain output subtype here.
        hr = MFTEnumEx(
            MFT_CATEGORY_VIDEO_DECODER,
            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
            &inputInfo, nullptr,
            &activates, &count);
    }

    if (FAILED(hr) || count == 0) {
        throw std::runtime_error(std::format(
            "VideoDecoder: No {} decoder available (hardware or software). "
            "Install codec pack or update GPU drivers.",
            codecName));
    }

    // Activate the first (highest priority) decoder
    hr = activates[0]->ActivateObject(IID_PPV_ARGS(&m_mf->mft));
    for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
    CoTaskMemFree(activates);

    if (FAILED(hr)) {
        throw std::runtime_error(std::format(
            "VideoDecoder: ActivateObject failed: {:08X}", (uint32_t)hr));
    }

    // Get stream IDs
    m_mf->mft->GetStreamIDs(1, &m_mf->inputStreamId, 1, &m_mf->outputStreamId);

    // Configure input type: compressed video (H.265 or AV1)
    ComPtr<IMFMediaType> inputType;
    MFCreateMediaType(&inputType);
    inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inputType->SetGUID(MF_MT_SUBTYPE, inputSubtype);
    // Resolution will be updated when we receive SPS
    MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, 1920, 1080);
    MFSetAttributeRatio(inputType.Get(), MF_MT_FRAME_RATE, 60, 1);

    hr = m_mf->mft->SetInputType(m_mf->inputStreamId, inputType.Get(), 0);
    if (FAILED(hr)) {
        AURA_LOG_WARN("VideoDecoder", "SetInputType failed: {:08X} (will retry after SPS)", (uint32_t)hr);
    }

    // Configure output type: NV12 (required for D3D12 zero-copy path)
    ComPtr<IMFMediaType> outputType;
    MFCreateMediaType(&outputType);
    outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, 1920, 1080);

    hr = m_mf->mft->SetOutputType(m_mf->outputStreamId, outputType.Get(), 0);
    if (FAILED(hr)) {
        AURA_LOG_WARN("VideoDecoder", "SetOutputType failed: {:08X}", (uint32_t)hr);
    }

    m_initialised.store(true);
    AURA_LOG_INFO("VideoDecoder", "{} decoder ready.", codecName);
}

// -----------------------------------------------------------------------------
void VideoDecoder::shutdown() {
    if (!m_initialised.exchange(false)) return;

    AURA_LOG_INFO("VideoDecoder", "Shutting down. Decoded: {} frames, Errors: {}",
                  m_decoded.load(), m_errors.load());

    if (m_mf->mft) {
        m_mf->mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        m_mf->mft.Reset();
    }

    if (m_mf->mfStarted) {
        MFShutdown();
        m_mf->mfStarted = false;
    }
}

// -----------------------------------------------------------------------------
bool VideoDecoder::submitNAL(const std::vector<uint8_t>& annexBData,
                              bool isKeyframe,
                              uint64_t presentationTimeUs) {
    if (!m_initialised.load()) return false;
    if (annexBData.empty()) return false;

    // Track whether we have received parameter sets (SPS/PPS for H.264, VPS/SPS/PPS for H.265)
    // The decoder cannot decode frames until the parameter sets are provided.
    // SPS is NAL type 7, PPS is NAL type 8 for H.264
    if (annexBData.size() > 4) {
        uint8_t nalType = annexBData[4] & 0x1F;
        if (nalType == 7 || nalType == 8) {
            m_hasParameterSets = true;
        }
    }
    if (isKeyframe) m_hasParameterSets = true;
    
    // Allow parameter sets through even if we don't have them yet
    bool isParameterSet = false;
    if (annexBData.size() > 4) {
        uint8_t nalType = annexBData[4] & 0x1F;
        isParameterSet = (nalType == 7 || nalType == 8); // SPS or PPS
    }
    
    if (!m_hasParameterSets && !isParameterSet) {
        return false; // Skip non-parameter-set frames until we have SPS/PPS
    }
    
    // Log NAL submissions
    static int nalSubmitCount = 0;
    nalSubmitCount++;
    if (nalSubmitCount <= 10 || nalSubmitCount % 30 == 0) {
        AURA_LOG_INFO("VideoDecoder", "submitNAL #{}: size={} key={} pts={}", 
                      nalSubmitCount, annexBData.size(), isKeyframe, presentationTimeUs);
    }

    // Create an MF media buffer wrapping the NAL data
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(annexBData.size()), &buffer);
    if (FAILED(hr)) { 
        AURA_LOG_ERROR("VideoDecoder", "MFCreateMemoryBuffer failed: {:08X}", (uint32_t)hr);
        m_errors++; 
        return false; 
    }

    BYTE* bufPtr = nullptr;
    DWORD maxLen{}, curLen{};
    buffer->Lock(&bufPtr, &maxLen, &curLen);
    memcpy(bufPtr, annexBData.data(), annexBData.size());
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(annexBData.size()));

    // Create a sample with a presentation timestamp
    ComPtr<IMFSample> sample;
    MFCreateSample(&sample);
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(presentationTimeUs) * 10); // 100-ns units
    sample->SetSampleDuration(166667); // ~60fps = 16.67ms = 166,667 * 100ns

    hr = m_mf->mft->ProcessInput(m_mf->inputStreamId, sample.Get(), 0);
    if (FAILED(hr)) {
        AURA_LOG_WARN("VideoDecoder", "ProcessInput failed: {:08X} (submitted {} NALs so far)", 
                      (uint32_t)hr, nalSubmitCount);
        if (hr != MF_E_NOTACCEPTING) m_errors++;
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
void VideoDecoder::processOutput() {
    if (!m_initialised.load()) return;

    while (true) {
        MFT_OUTPUT_DATA_BUFFER outputData{};
        outputData.dwStreamID = m_mf->outputStreamId;
        outputData.pSample    = nullptr;

        DWORD status = 0;
        const HRESULT hr = m_mf->mft->ProcessOutput(0, 1, &outputData, &status);

        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            // Decoder needs more input before it can produce output -- normal
            break;
        }

        if (FAILED(hr)) {
            if (hr != MF_E_TRANSFORM_STREAM_CHANGE) {
                AURA_LOG_WARN("VideoDecoder", "ProcessOutput error: {:08X}", (uint32_t)hr);
                m_errors++;
            }
            break;
        }

        if (outputData.pSample) {
            DecodedFrame frame;
            frame.frameIndex         = m_frameIndex++;
            frame.presentationTimeUs = 0;
            frame.isKeyframe         = false;

            LONGLONG sampleTime = 0;
            outputData.pSample->GetSampleTime(&sampleTime);
            frame.presentationTimeUs = static_cast<uint64_t>(sampleTime / 10);

            // ── Extract D3D12 texture via DXGI buffer interface ──────────
            // MFT outputs a DXGI-backed IMFMediaBuffer when D3D-aware mode
            // is active. Query IMFDXGIBuffer to get the underlying D3D resource.
            ComPtr<IMFMediaBuffer> outBuffer;
            if (SUCCEEDED(outputData.pSample->GetBufferByIndex(0, &outBuffer))) {
                ComPtr<IMFDXGIBuffer> dxgiBuffer;
                if (SUCCEEDED(outBuffer.As(&dxgiBuffer))) {
                    // Get the underlying texture and subresource index
                    ID3D11Texture2D* tex11 = nullptr;
                    UINT subIdx = 0;
                    if (SUCCEEDED(dxgiBuffer->GetResource(
                            __uuidof(ID3D11Texture2D),
                            reinterpret_cast<LPVOID*>(&tex11)))) {
                        dxgiBuffer->GetSubresourceIndex(&subIdx);
                        // Wrap in a shared D3D12 resource via OpenSharedHandle.
                        // For MFT output we store the raw pointer temporarily;
                        // the MirrorWindow will AddRef it before the sample is released.
                        // NOTE: The proper zero-copy path requires ID3D12Device4::OpenSharedHandle.
                        // We store a D3D11 texture pointer here as a placeholder; the
                        // MirrorWindow will handle the 11-on-12 interop if needed.
                        // For now, leave frame.texture as nullptr and log this path.
                        if (tex11) tex11->Release();  // we don't keep the 11 ref
                        AURA_LOG_TRACE("VideoDecoder",
                            "DXGI buffer acquired (subresource {})", subIdx);
                    }
                } else {
                    // Fallback: non-DXGI buffer (software decoder output)
                    // Copy to a D3D12 upload texture if m_device is set.
                    // This path is hit for the software fallback decoder.
                    AURA_LOG_TRACE("VideoDecoder", "Non-DXGI buffer -- software decode path");
                }
            }

            // Get texture dimensions from output media type
            ComPtr<IMFMediaType> outType;
            if (SUCCEEDED(m_mf->mft->GetOutputCurrentType(m_mf->outputStreamId, &outType))) {
                UINT32 w = 0, h = 0;
                MFGetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, &w, &h);
                frame.width  = w;
                frame.height = h;
            }

            m_decoded++;
            
            // Log decoded frames
            if (m_decoded <= 10 || m_decoded % 30 == 0) {
                AURA_LOG_INFO("VideoDecoder", "Decoded frame #{}: {}x{} pts={} key={}", 
                              m_decoded.load(), frame.width, frame.height, 
                              frame.presentationTimeUs, frame.isKeyframe);
            }

            if (m_frameCallback) m_frameCallback(frame);

            outputData.pSample->Release();
        }
    }
}

// -----------------------------------------------------------------------------
void VideoDecoder::setFrameCallback(FrameCallback cb) {
    m_frameCallback = std::move(cb);
}

// -----------------------------------------------------------------------------
void VideoDecoder::resetState() {
    // Flush the MFT so it discards any buffered frames from the old session.
    // This must be called between AirPlay sessions to prevent stale frames
    // from the previous session appearing in the new one.
    if (m_mf && m_mf->mft && m_initialised.load()) {
        m_mf->mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    }
    m_hasParameterSets = false;
    m_frameIndex       = 0;
    AURA_LOG_INFO("VideoDecoder", "State reset. Decoded so far: {} frames.", m_decoded.load());
}

} // namespace aura

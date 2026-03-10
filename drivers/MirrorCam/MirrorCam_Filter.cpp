// =============================================================================
// MirrorCam_Filter.cpp — DirectShow Virtual Camera Filter (IBaseFilter + KsProxy)
//
// Architecture:
//   MirrorCamFilter  (IBaseFilter, IAMStreamConfig, ISpecifyPropertyPages)
//   └── MirrorCamPin (IPin, IAMStreamControl, IKsPropertySet)
//       └── Reads frames from SharedMemoryIPC ← DX12Manager capture path
//
// Registration: regsvr32 MirrorCam.dll  (calls DllRegisterServer → COM + KS)
// Unregister:   regsvr32 /u MirrorCam.dll
//
// Build: Requires Windows SDK + DirectShow BaseClasses (strmbase.lib).
//        See README.txt for build instructions.
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <ks.h>
#include <ksproxy.h>
#include <dvdmedia.h>
#include <uuids.h>
#include <combaseapi.h>
#include <strmif.h>

#include <string>
#include <atomic>
#include <cstring>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "uuid.lib")

// {4A6D9DA2-B8A1-4C52-B7B4-3E4C5A6F7890}
DEFINE_GUID(CLSID_MirrorCamFilter,
    0x4a6d9da2, 0xb8a1, 0x4c52, 0xb7, 0xb4, 0x3e, 0x4c, 0x5a, 0x6f, 0x78, 0x90);

static constexpr WCHAR kFilterName[]    = L"AuraCastPro Virtual Camera";
static constexpr WCHAR kPinName[]       = L"Capture";
static constexpr int   kDefaultWidth    = 1920;
static constexpr int   kDefaultHeight   = 1080;
static constexpr int   kDefaultFPS      = 30;
static constexpr DWORD kFrameIntervalNs = (DWORD)(10'000'000LL / kDefaultFPS); // 100ns units

// Forward declarations
class MirrorCamPin;
class MirrorCamFilter;

// =============================================================================
// MirrorCamPin — IPin implementation for the video output pin
// =============================================================================

class MirrorCamPin : public IPin, public IAsyncReader {
public:
    explicit MirrorCamPin(MirrorCamFilter* filter);
    virtual ~MirrorCamPin() = default;

    // IUnknown
    STDMETHOD_(ULONG, AddRef)()  override { return InterlockedIncrement(&m_refCount); }
    STDMETHOD_(ULONG, Release)() override {
        LONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) delete this;
        return ref;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;

    // IPin
    STDMETHOD(Connect)(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(ReceiveConnection)(IPin* pConnector, const AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(Disconnect)()                          override;
    STDMETHOD(ConnectedTo)(IPin** pPin)              override;
    STDMETHOD(ConnectionMediaType)(AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(QueryPinInfo)(PIN_INFO* pInfo)         override;
    STDMETHOD(QueryDirection)(PIN_DIRECTION* pPinDir) override;
    STDMETHOD(QueryId)(LPWSTR* Id)                   override;
    STDMETHOD(QueryAccept)(const AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(EnumMediaTypes)(IEnumMediaTypes** ppEnum) override;
    STDMETHOD(QueryInternalConnections)(IPin** apPin, ULONG* nPin) override;
    STDMETHOD(EndOfStream)()                         override;
    STDMETHOD(BeginFlush)()                          override;
    STDMETHOD(EndFlush)()                            override;
    STDMETHOD(NewSegment)(REFERENCE_TIME, REFERENCE_TIME, double) override;

    // IAsyncReader (minimal — some capture graphs require this)
    STDMETHOD(RequestAllocator)(IMemAllocator*, ALLOCATOR_PROPERTIES*, IMemAllocator**) override;
    STDMETHOD(Request)(IMediaSample*, DWORD_PTR)                           override;
    STDMETHOD(WaitForNext)(DWORD, IMediaSample**, DWORD_PTR*)              override;
    STDMETHOD(SyncReadAligned)(IMediaSample*)                              override;
    STDMETHOD(SyncRead)(LONGLONG, LONG, BYTE*)                             override;
    STDMETHOD(Length)(LONGLONG*, LONGLONG*)                                override;
    STDMETHOD(BeginFlush)(void)                                            override;
    STDMETHOD(EndFlush)(void)                                              override;

    void FillVideoInfo(VIDEOINFOHEADER* vih) const;

private:
    volatile LONG   m_refCount{1};
    MirrorCamFilter* m_filter;
    IPin*            m_connectedTo{nullptr};
    AM_MEDIA_TYPE    m_connectedMT{};
};

// =============================================================================
// MirrorCamFilter — IBaseFilter implementation
// =============================================================================

class MirrorCamFilter : public IBaseFilter, public IAMStreamConfig {
public:
    MirrorCamFilter();
    virtual ~MirrorCamFilter();

    // IUnknown
    STDMETHOD_(ULONG, AddRef)()  override { return InterlockedIncrement(&m_refCount); }
    STDMETHOD_(ULONG, Release)() override {
        LONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) delete this;
        return ref;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;

    // IPersist
    STDMETHOD(GetClassID)(CLSID* pClassID) override;

    // IMediaFilter
    STDMETHOD(Stop)()                                       override;
    STDMETHOD(Pause)()                                      override;
    STDMETHOD(Run)(REFERENCE_TIME tStart)                   override;
    STDMETHOD(GetState)(DWORD dwMilliSecsTimeout, FILTER_STATE* State) override;
    STDMETHOD(SetSyncSource)(IReferenceClock* pClock)       override;
    STDMETHOD(GetSyncSource)(IReferenceClock** pClock)      override;

    // IBaseFilter
    STDMETHOD(EnumPins)(IEnumPins** ppEnum)                 override;
    STDMETHOD(FindPin)(LPCWSTR Id, IPin** ppPin)            override;
    STDMETHOD(QueryFilterInfo)(FILTER_INFO* pInfo)          override;
    STDMETHOD(JoinFilterGraph)(IFilterGraph* pGraph, LPCWSTR pName) override;
    STDMETHOD(QueryVendorInfo)(LPWSTR* pVendorInfo)         override;

    // IAMStreamConfig
    STDMETHOD(SetFormat)(AM_MEDIA_TYPE* pmt)                override;
    STDMETHOD(GetFormat)(AM_MEDIA_TYPE** ppmt)              override;
    STDMETHOD(GetNumberOfCapabilities)(int* piCount, int* piSize) override;
    STDMETHOD(GetStreamCaps)(int iIndex, AM_MEDIA_TYPE** ppmt, BYTE* pSCC) override;

    // Internal helpers
    void GetVideoMediaType(AM_MEDIA_TYPE* pmt, int w, int h, int fps) const;
    MirrorCamPin*  GetPin() const { return m_pin; }

private:
    volatile LONG   m_refCount{1};
    MirrorCamPin*   m_pin{nullptr};
    IFilterGraph*   m_graph{nullptr};
    IReferenceClock* m_clock{nullptr};
    FILTER_STATE    m_state{State_Stopped};

    int m_width{kDefaultWidth};
    int m_height{kDefaultHeight};
    int m_fps{kDefaultFPS};
    WCHAR m_name[128]{};
};

// =============================================================================
// MirrorCamClassFactory — creates MirrorCamFilter instances
// =============================================================================

class MirrorCamClassFactory : public IClassFactory {
public:
    STDMETHOD_(ULONG, AddRef)()  override { return 2; }
    STDMETHOD_(ULONG, Release)() override { return 1; }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD(CreateInstance)(IUnknown* pOuter, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (pOuter) return CLASS_E_NOAGGREGATION;
        MirrorCamFilter* filter = new (std::nothrow) MirrorCamFilter();
        if (!filter) return E_OUTOFMEMORY;
        HRESULT hr = filter->QueryInterface(riid, ppv);
        filter->Release();
        return hr;
    }
    STDMETHOD(LockServer)(BOOL /*fLock*/) override { return S_OK; }
};

static MirrorCamClassFactory g_factory;

// =============================================================================
// DLL entry points (referenced from MirrorCam.cpp)
// =============================================================================

extern "C" __declspec(dllexport) HRESULT MirrorCam_GetClassObject(
    REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (rclsid != CLSID_MirrorCamFilter) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, ppv);
}

// =============================================================================
// MirrorCamFilter — implementation
// =============================================================================

MirrorCamFilter::MirrorCamFilter() {
    m_pin = new (std::nothrow) MirrorCamPin(this);
    wcscpy_s(m_name, kFilterName);
}

MirrorCamFilter::~MirrorCamFilter() {
    if (m_clock) { m_clock->Release(); m_clock = nullptr; }
    if (m_pin)   { m_pin->Release();   m_pin   = nullptr; }
}

STDMETHODIMP MirrorCamFilter::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IBaseFilter || riid == IID_IMediaFilter) {
        *ppv = static_cast<IBaseFilter*>(this); AddRef(); return S_OK;
    }
    if (riid == IID_IAMStreamConfig) {
        *ppv = static_cast<IAMStreamConfig*>(this); AddRef(); return S_OK;
    }
    *ppv = nullptr; return E_NOINTERFACE;
}

STDMETHODIMP MirrorCamFilter::GetClassID(CLSID* p) {
    if (!p) return E_POINTER; *p = CLSID_MirrorCamFilter; return S_OK;
}
STDMETHODIMP MirrorCamFilter::Stop()  { m_state = State_Stopped; return S_OK; }
STDMETHODIMP MirrorCamFilter::Pause() { m_state = State_Paused;  return S_OK; }
STDMETHODIMP MirrorCamFilter::Run(REFERENCE_TIME) { m_state = State_Running; return S_OK; }
STDMETHODIMP MirrorCamFilter::GetState(DWORD, FILTER_STATE* s) { *s = m_state; return S_OK; }
STDMETHODIMP MirrorCamFilter::SetSyncSource(IReferenceClock* c) {
    if (m_clock) m_clock->Release();
    m_clock = c; if (m_clock) m_clock->AddRef(); return S_OK;
}
STDMETHODIMP MirrorCamFilter::GetSyncSource(IReferenceClock** c) {
    if (!c) return E_POINTER; *c = m_clock; if (*c) (*c)->AddRef(); return S_OK;
}
STDMETHODIMP MirrorCamFilter::FindPin(LPCWSTR id, IPin** ppPin) {
    if (!ppPin) return E_POINTER;
    if (wcscmp(id, kPinName) == 0 && m_pin) {
        *ppPin = m_pin; m_pin->AddRef(); return S_OK;
    }
    *ppPin = nullptr; return VFW_E_NOT_FOUND;
}
STDMETHODIMP MirrorCamFilter::QueryFilterInfo(FILTER_INFO* p) {
    if (!p) return E_POINTER;
    wcscpy_s(p->achName, m_name); p->pGraph = m_graph;
    if (m_graph) m_graph->AddRef(); return S_OK;
}
STDMETHODIMP MirrorCamFilter::JoinFilterGraph(IFilterGraph* g, LPCWSTR name) {
    m_graph = g;
    if (name) wcscpy_s(m_name, name);
    return S_OK;
}
STDMETHODIMP MirrorCamFilter::QueryVendorInfo(LPWSTR* p) { if (!p) return E_POINTER; *p = nullptr; return E_NOTIMPL; }

void MirrorCamFilter::GetVideoMediaType(AM_MEDIA_TYPE* pmt, int w, int h, int fps) const {
    ZeroMemory(pmt, sizeof(AM_MEDIA_TYPE));
    pmt->majortype  = MEDIATYPE_Video;
    pmt->subtype    = MEDIASUBTYPE_NV12;
    pmt->formattype = FORMAT_VideoInfo;
    pmt->bFixedSizeSamples = TRUE;
    pmt->bTemporalCompression = FALSE;

    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
    if (!vih) return;
    ZeroMemory(vih, sizeof(VIDEOINFOHEADER));
    vih->AvgTimePerFrame = 10'000'000LL / fps;
    vih->dwBitRate       = (DWORD)((int64_t)w * h * fps * 12 / 8);  // NV12 = 12 bpp
    vih->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth       = w;
    vih->bmiHeader.biHeight      = h;
    vih->bmiHeader.biPlanes      = 1;
    vih->bmiHeader.biBitCount    = 12;
    vih->bmiHeader.biCompression = MAKEFOURCC('N','V','1','2');
    vih->bmiHeader.biSizeImage   = (DWORD)(w * h * 3 / 2);

    pmt->cbFormat    = sizeof(VIDEOINFOHEADER);
    pmt->pbFormat    = reinterpret_cast<BYTE*>(vih);
    pmt->lSampleSize = vih->bmiHeader.biSizeImage;
}

STDMETHODIMP MirrorCamFilter::SetFormat(AM_MEDIA_TYPE* pmt) {
    if (!pmt) return E_POINTER;
    if (pmt->formattype == FORMAT_VideoInfo && pmt->pbFormat) {
        auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
        m_width  = vih->bmiHeader.biWidth;
        m_height = vih->bmiHeader.biHeight;
        if (vih->AvgTimePerFrame > 0)
            m_fps = (int)(10'000'000LL / vih->AvgTimePerFrame);
    }
    return S_OK;
}
STDMETHODIMP MirrorCamFilter::GetFormat(AM_MEDIA_TYPE** ppmt) {
    if (!ppmt) return E_POINTER;
    *ppmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (!*ppmt) return E_OUTOFMEMORY;
    GetVideoMediaType(*ppmt, m_width, m_height, m_fps);
    return S_OK;
}
STDMETHODIMP MirrorCamFilter::GetNumberOfCapabilities(int* piCount, int* piSize) {
    if (!piCount || !piSize) return E_POINTER;
    *piCount = 3;   // 1080p30, 1080p60, 720p60
    *piSize  = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}
STDMETHODIMP MirrorCamFilter::GetStreamCaps(int idx, AM_MEDIA_TYPE** ppmt, BYTE* pSCC) {
    if (!ppmt || !pSCC) return E_POINTER;
    struct { int w, h, fps; } caps[] = {{1920,1080,30},{1920,1080,60},{1280,720,60}};
    if (idx < 0 || idx >= 3) return E_INVALIDARG;
    *ppmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (!*ppmt) return E_OUTOFMEMORY;
    GetVideoMediaType(*ppmt, caps[idx].w, caps[idx].h, caps[idx].fps);

    VIDEO_STREAM_CONFIG_CAPS* vscc = reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(pSCC);
    ZeroMemory(vscc, sizeof(*vscc));
    vscc->guid           = FORMAT_VideoInfo;
    vscc->VideoStandard  = 0;
    vscc->MinFrameInterval = 10'000'000LL / caps[idx].fps;
    vscc->MaxFrameInterval = 10'000'000LL / 15;  // minimum 15fps
    vscc->MinOutputSize  = vscc->MaxOutputSize = {caps[idx].w, caps[idx].h};
    return S_OK;
}

// IEnumPins minimal implementation
class SinglePinEnum : public IEnumPins {
    IPin* m_pin; int m_pos{0}; volatile LONG m_ref{1};
public:
    explicit SinglePinEnum(IPin* p) : m_pin(p) { m_pin->AddRef(); }
    ~SinglePinEnum() { m_pin->Release(); }
    STDMETHOD_(ULONG, AddRef)()  override { return InterlockedIncrement(&m_ref); }
    STDMETHOD_(ULONG, Release)() override {
        LONG r = InterlockedDecrement(&m_ref); if (r==0) delete this; return r;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid==IID_IUnknown||riid==IID_IEnumPins) { *ppv=this; AddRef(); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    STDMETHOD(Next)(ULONG n, IPin** ppPins, ULONG* fetched) override {
        ULONG done=0;
        while (done<n && m_pos<1) { ppPins[done++]=m_pin; m_pin->AddRef(); m_pos++; }
        if (fetched) *fetched=done;
        return done==n ? S_OK : S_FALSE;
    }
    STDMETHOD(Skip)(ULONG n) override { m_pos+=n; return m_pos<=1 ? S_OK : S_FALSE; }
    STDMETHOD(Reset)()       override { m_pos=0; return S_OK; }
    STDMETHOD(Clone)(IEnumPins** pp) override { if(!pp) return E_POINTER; *pp=new SinglePinEnum(m_pin); return S_OK; }
};

STDMETHODIMP MirrorCamFilter::EnumPins(IEnumPins** pp) {
    if (!pp) return E_POINTER;
    *pp = new (std::nothrow) SinglePinEnum(m_pin);
    return *pp ? S_OK : E_OUTOFMEMORY;
}

// =============================================================================
// MirrorCamPin — minimal stubs
// =============================================================================
MirrorCamPin::MirrorCamPin(MirrorCamFilter* f) : m_filter(f) {}

STDMETHODIMP MirrorCamPin::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid==IID_IUnknown||riid==IID_IPin) { *ppv=static_cast<IPin*>(this); AddRef(); return S_OK; }
    *ppv=nullptr; return E_NOINTERFACE;
}
STDMETHODIMP MirrorCamPin::Connect(IPin* p, const AM_MEDIA_TYPE* pmt) {
    m_connectedTo = p; return pmt ? QueryAccept(pmt) : S_OK;
}
STDMETHODIMP MirrorCamPin::ReceiveConnection(IPin* p, const AM_MEDIA_TYPE* pmt) {
    m_connectedTo = p; return S_OK;
}
STDMETHODIMP MirrorCamPin::Disconnect()               { m_connectedTo=nullptr; return S_OK; }
STDMETHODIMP MirrorCamPin::ConnectedTo(IPin** pp)     { if(!pp) return E_POINTER; *pp=m_connectedTo; if(*pp) (*pp)->AddRef(); return m_connectedTo?S_OK:VFW_E_NOT_CONNECTED; }
STDMETHODIMP MirrorCamPin::ConnectionMediaType(AM_MEDIA_TYPE* p) { if(!p) return E_POINTER; *p=m_connectedMT; return S_OK; }
STDMETHODIMP MirrorCamPin::QueryPinInfo(PIN_INFO* p) {
    if(!p) return E_POINTER;
    p->pFilter = m_filter; if(p->pFilter) p->pFilter->AddRef();
    p->dir = PINDIR_OUTPUT;
    wcscpy_s(p->achName, kPinName);
    return S_OK;
}
STDMETHODIMP MirrorCamPin::QueryDirection(PIN_DIRECTION* p) { if(!p) return E_POINTER; *p=PINDIR_OUTPUT; return S_OK; }
STDMETHODIMP MirrorCamPin::QueryId(LPWSTR* p)         { if(!p) return E_POINTER; *p=nullptr; return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::QueryAccept(const AM_MEDIA_TYPE* pmt) {
    if (!pmt) return E_POINTER;
    return (pmt->majortype==MEDIATYPE_Video && pmt->subtype==MEDIASUBTYPE_NV12) ? S_OK : S_FALSE;
}
STDMETHODIMP MirrorCamPin::EnumMediaTypes(IEnumMediaTypes** pp) { if(!pp) return E_POINTER; *pp=nullptr; return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::QueryInternalConnections(IPin**, ULONG* n) { if(n) *n=0; return S_FALSE; }
STDMETHODIMP MirrorCamPin::EndOfStream()  { return S_OK; }
STDMETHODIMP MirrorCamPin::BeginFlush()   { return S_OK; }
STDMETHODIMP MirrorCamPin::EndFlush()     { return S_OK; }
STDMETHODIMP MirrorCamPin::NewSegment(REFERENCE_TIME, REFERENCE_TIME, double) { return S_OK; }
// IAsyncReader stubs
STDMETHODIMP MirrorCamPin::RequestAllocator(IMemAllocator*, ALLOCATOR_PROPERTIES*, IMemAllocator** pp) { if(pp)*pp=nullptr; return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::Request(IMediaSample*, DWORD_PTR)         { return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::WaitForNext(DWORD, IMediaSample**, DWORD_PTR*) { return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::SyncReadAligned(IMediaSample*)            { return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::SyncRead(LONGLONG, LONG, BYTE*)           { return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::Length(LONGLONG* t, LONGLONG* a)          { if(t)*t=0; if(a)*a=0; return E_NOTIMPL; }
STDMETHODIMP MirrorCamPin::BeginFlush(void)                          { return S_OK; }
STDMETHODIMP MirrorCamPin::EndFlush(void)                            { return S_OK; }

// dx12HW.cpp — CHW implementation for the DX12 + D3D11On12 backend (R5).
//
// Architecture: a native DX12 device and command queue own the swapchain.
// D3D11On12CreateDevice wraps the DX12 device to expose a DX11 interface
// (HW.pDevice / HW.pContext) so all existing R4 render code can run unchanged
// on top of the DX12 command stream.  Native DX12 features (mesh shaders, RT,
// etc.) can be added incrementally through HW.pDevice12 / HW.m_pCommandQueue12.
//
// Most helper functions (selectResolution, fill_vid_mode_list, …) are identical
// to dx10HW.cpp because they only touch DXGI, which is the same across DX10/11/12.

#include <algorithm>
#include <d3dx9.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d11on12.h>
#include <D3DX10core.h>

#include <defines.h>
#include <HW.h>
#include <xrCore.h>

#include "XR_IOConsole.h"
#include "xrAPI.h"
#include "xrRender_console.h"

#include "StateManager/dx10SamplerStateCache.h"
#include "StateManager/dx10StateCache.h"
#include "StateManager/dx10StateManager.h"

#ifndef _EDITOR
void fill_vid_mode_list(CHW* _hw);
void free_vid_mode_list();
void fill_render_mode_list();
void free_render_mode_list();
#else
void fill_vid_mode_list(CHW* _hw) {}
void free_vid_mode_list() {}
void fill_render_mode_list() {}
void free_render_mode_list() {}
#endif

CHW HW;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CHW::CHW()
    : m_pAdapter(nullptr)
    , pDevice(nullptr)
    , m_move_window(true)
    , pAnnotation(nullptr)
    // DX12 members
    , pDevice12(nullptr)
    , m_pCommandQueue12(nullptr)
    , pDevice11on12(nullptr)
    , m_pRtvHeap12(nullptr)
    , m_pOfflineRT12(nullptr)
    , m_pFence12(nullptr)
    , m_nFenceValue12(0)
    , m_hFenceEvent12(nullptr)
    , m_nFrameIndex12(0)
    , m_nRtvDescriptorSize12(0)
{
    for (UINT i = 0; i < k_FrameCount; ++i)
    {
        m_pRenderTargets12[i]      = nullptr;
        m_pWrappedBackBuffers12[i] = nullptr;
    }

    Device.seqAppActivate.Add(this);
    Device.seqAppDeactivate.Add(this);
}

CHW::~CHW()
{
    Device.seqAppActivate.Remove(this);
    Device.seqAppDeactivate.Remove(this);
}

// ---------------------------------------------------------------------------
// CreateD3D / DestroyD3D — DXGI factory + adapter selection
// ---------------------------------------------------------------------------

void CHW::CreateD3D()
{
    // IDXGIFactory4 required for DX12 swapchain creation.
    R_CHK(CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pFactory)));

    m_pAdapter    = nullptr;
    m_bUsePerfhud = false;

    // Check VRR / tearing support.
    {
        IDXGIFactory5* factory5 = nullptr;
        if (SUCCEEDED(m_pFactory->QueryInterface(&factory5)) && factory5) {
            BOOL supports_vrr = FALSE;
            HRESULT hr = factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &supports_vrr, sizeof(supports_vrr));
            m_SupportsVRR = SUCCEEDED(hr) && supports_vrr;
            factory5->Release();
        } else {
            m_SupportsVRR = false;
        }
    }

#ifndef MASTER_GOLD
    // Look for NVPerfHUD adapter.
    {
        IDXGIFactory4* f4 = nullptr;
        m_pFactory->QueryInterface(&f4);
        if (f4) {
            UINT i = 0;
            while (f4->EnumAdapters1(i, &m_pAdapter) != DXGI_ERROR_NOT_FOUND) {
                DXGI_ADAPTER_DESC desc;
                m_pAdapter->GetDesc(&desc);
                if (!wcscmp(desc.Description, L"NVIDIA PerfHUD")) {
                    m_bUsePerfhud = true;
                    break;
                }
                m_pAdapter->Release();
                m_pAdapter = nullptr;
                ++i;
            }
            f4->Release();
        }
    }
#endif

    if (!m_pAdapter)
        m_pFactory->EnumAdapters1(0, &m_pAdapter);
}

void CHW::DestroyD3D()
{
    _SHOW_REF("refCount:m_pAdapter", m_pAdapter);
    _RELEASE(m_pAdapter);
    _SHOW_REF("refCount:m_pFactory", m_pFactory);
    _RELEASE(m_pFactory);
}

// ---------------------------------------------------------------------------
// CreateDevice — DX12 device + 11on12 wrapper + swapchain
// ---------------------------------------------------------------------------

extern u32 g_screenmode;

void CHW::CreateDevice(HWND hwnd, bool move_window)
{
    m_hWnd       = hwnd;
    m_move_window = move_window;
    CreateD3D();

    BOOL bWindowed = (g_screenmode != 2);

    // --- 1. Create the DX12 device -------------------------------------------

    if (strstr(Core.Params, "--dxgi-dbg")) {
        // Enable DX12 debug layer (must happen before device creation).
        ID3D12Debug* debugController = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            debugController->Release();
            Msg("* DX12: debug layer enabled");
        }
    }

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
                                           D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D12CreateDevice(
        m_pAdapter,
        D3D_FEATURE_LEVEL_11_0, // minimum; actual level reported in FeatureLevel
        IID_PPV_ARGS(&pDevice12));

    if (FAILED(hr)) {
        Msg("Failed to create DX12 device (hr=0x%08x). "
            "Please ensure your GPU and driver support DirectX 12.", hr);
        FlushLog();
        MessageBox(nullptr,
            "Failed to create DirectX 12 device.\n"
            "Please update your GPU driver or use the DX11 (AnomalyDX11.exe) executable.",
            "Error!", MB_OK | MB_ICONERROR);
        TerminateProcess(GetCurrentProcess(), 0);
    }

    // Query the highest feature level the device actually supports.
    {
        FeatureLevel = D3D_FEATURE_LEVEL_11_0;
        for (D3D_FEATURE_LEVEL lvl : featureLevels) {
            D3D12_FEATURE_DATA_FEATURE_LEVELS fd{};
            fd.NumFeatureLevels        = 1;
            fd.pFeatureLevelsRequested = &lvl;
            if (SUCCEEDED(pDevice12->CheckFeatureSupport(
                    D3D12_FEATURE_FEATURE_LEVELS, &fd, sizeof(fd)))) {
                FeatureLevel = fd.MaxSupportedFeatureLevel;
                break;
            }
        }
    }

    DXGI_ADAPTER_DESC adapterDesc;
    m_pAdapter->GetDesc(&adapterDesc);
    Msg("* DX12 GPU [vendor:%X]-[device:%X]: %S (FeatureLevel %X)",
        adapterDesc.VendorId, adapterDesc.DeviceId,
        adapterDesc.Description, (u32)FeatureLevel);
    Msg("*     Texture memory: %u M",
        (u32)(adapterDesc.DedicatedVideoMemory / (1024 * 1024)));

    Caps.id_vendor = adapterDesc.VendorId;
    Caps.id_device = adapterDesc.DeviceId;

    // Legacy caps stubs (R4 render code reads these; DX12 always has the capability).
    Caps.fTarget = D3DFMT_X8R8G8B8;
    Caps.fDepth  = selectDepthStencil(Caps.fTarget);

    // --- 2. Create command queue ---------------------------------------------

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    R_CHK(pDevice12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue12)));

    // --- 3. Create swapchain (DX12 requires command queue, not device) -------

    DXGI_SWAP_CHAIN_DESC1& sd = m_ChainDesc;
    ZeroMemory(&sd, sizeof(sd));
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC& sd_full = m_ChainDescFullscreen;
    ZeroMemory(&sd_full, sizeof(sd_full));

    selectResolution(sd.Width, sd.Height, bWindowed);
    sd_full.Windowed   = bWindowed;
    sd.AlphaMode       = DXGI_ALPHA_MODE_IGNORE;
    sd.Format          = ps_r4_hdr10_on ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferCount     = k_FrameCount;
    sd.SampleDesc.Count   = 1;
    sd.SampleDesc.Quality = 0;
    sd.SwapEffect      = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.BufferUsage     = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.Flags           = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    if (m_SupportsVRR)
        sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    if (bWindowed) {
        sd_full.RefreshRate.Numerator   = 0;
        sd_full.RefreshRate.Denominator = 0;
    } else {
        sd_full.RefreshRate = selectRefresh(sd.Width, sd.Height, sd.Format);
    }

    IDXGISwapChain1* swapChain1 = nullptr;
    IDXGIFactory4* f4 = nullptr;
    R_CHK(m_pFactory->QueryInterface(&f4));
    R_CHK(f4->CreateSwapChainForHwnd(
        m_pCommandQueue12, m_hWnd,
        &sd, &sd_full,
        nullptr, &swapChain1));
    f4->Release();

    R_CHK(swapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain)));
    swapChain1->Release();

    // Setup colour space (HDR10 or SDR).
    {
        IDXGISwapChain3* sc3 = nullptr;
        R_CHK(m_pSwapChain->QueryInterface(&sc3));
        DXGI_COLOR_SPACE_TYPE cs = ps_r4_hdr10_on
            ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
            : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        if (ps_r4_hdr10_on) {
            UINT supported = 0;
            sc3->CheckColorSpaceSupport(cs, &supported);
            if (!(supported & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
                Log("HDR10 colour space unsupported; falling back to SDR.");
                cs = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            }
        }
        R_CHK(sc3->SetColorSpace1(cs));
        sc3->Release();
    }

    m_nFrameIndex12 = static_cast<IDXGISwapChain3*>(m_pSwapChain)->GetCurrentBackBufferIndex();

    Msg("* DX12: swapchain %ux%u fmt=0x%X (%s) buffers=%u %s%s startIndex=%u",
        sd.Width, sd.Height, (u32)sd.Format,
        ps_r4_hdr10_on ? "HDR10" : "SDR",
        sd.BufferCount,
        bWindowed ? "windowed" : "fullscreen",
        m_SupportsVRR ? " +tearing" : "",
        m_nFrameIndex12);

    // --- 4. Create DX11On12 device (wraps pDevice12 + pCommandQueue12) -------

    UINT d3d11Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (strstr(Core.Params, "--dxgi-dbg"))
        d3d11Flags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL d3d11FeatureLevel = D3D_FEATURE_LEVEL_11_0;
    IUnknown* commandQueues[] = { m_pCommandQueue12 };

    ID3D11Device*        d3d11Device  = nullptr;
    ID3D11DeviceContext* d3d11Context = nullptr;

    R_CHK(D3D11On12CreateDevice(
        pDevice12,
        d3d11Flags,
        &d3d11FeatureLevel, 1,
        commandQueues, 1,
        0,
        &d3d11Device,
        &d3d11Context,
        nullptr));

    R_CHK(d3d11Device->QueryInterface(IID_PPV_ARGS(&pDevice)));
    R_CHK(d3d11Context->QueryInterface(IID_PPV_ARGS(&pContext)));
    R_CHK(d3d11Device->QueryInterface(IID_PPV_ARGS(&pDevice11on12)));
    _RELEASE(d3d11Device);
    _RELEASE(d3d11Context);

    R_CHK(pContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
        (void**)&pAnnotation));

    // --- 5. Create DX12 RTV descriptor heap + per-frame render targets -------

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = k_FrameCount;
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    R_CHK(pDevice12->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_pRtvHeap12)));

    m_nRtvDescriptorSize12 = pDevice12->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // --- 6. Fence for CPU/GPU sync -------------------------------------------

    R_CHK(pDevice12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence12)));
    m_nFenceValue12 = 1;

    m_hFenceEvent12 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    R_ASSERT2(m_hFenceEvent12, "Failed to create fence event for DX12 sync.");

    // --- 7. Create 11on12 render target views (UpdateViews) ------------------

    Reset(hwnd);

    fill_vid_mode_list(this);
}

// ---------------------------------------------------------------------------
// DestroyDevice
// ---------------------------------------------------------------------------

void CHW::DestroyDevice()
{
    WaitForGpu();

    StateManager.Reset();
    RSManager.ClearStateArray();
    DSSManager.ClearStateArray();
    BSManager.ClearStateArray();
    SSManager.ClearStateArray();

    _SHOW_REF("refCount:pBaseZB", pBaseZB);
    _RELEASE(pBaseZB);
    _SHOW_REF("refCount:pBaseRT", pBaseRT);
    _RELEASE(pBaseRT);
    _RELEASE(m_pOfflineRT12);

    // Release every reference to the swapchain buffers (11on12 wrappers + DX12 resources)
    // before the swapchain and the 11on12 device themselves.
    for (UINT i = 0; i < k_FrameCount; ++i) {
        _RELEASE(m_pWrappedBackBuffers12[i]);
        _RELEASE(m_pRenderTargets12[i]);
    }

    if (!m_ChainDescFullscreen.Windowed)
        m_pSwapChain->SetFullscreenState(FALSE, nullptr);

    _SHOW_REF("refCount:m_pSwapChain", m_pSwapChain);
    _RELEASE(m_pSwapChain);

    _RELEASE(pAnnotation);
    _RELEASE(pContext);
    _RELEASE(pDevice);
    _RELEASE(pDevice11on12);

    if (m_hFenceEvent12) { CloseHandle(m_hFenceEvent12); m_hFenceEvent12 = nullptr; }
    _RELEASE(m_pFence12);
    _RELEASE(m_pRtvHeap12);
    _RELEASE(m_pCommandQueue12);
    _RELEASE(pDevice12);

    free_vid_mode_list();
}

// ---------------------------------------------------------------------------
// UpdateViews — (re)create 11on12 render target views from swapchain buffers
// ---------------------------------------------------------------------------

void CHW::UpdateViews()
{
    _RELEASE(pBaseRT);
    _RELEASE(pBaseZB);
    _RELEASE(m_pOfflineRT12);
    for (UINT i = 0; i < k_FrameCount; ++i) {
        _RELEASE(m_pWrappedBackBuffers12[i]);
        _RELEASE(m_pRenderTargets12[i]);
    }

    m_nFrameIndex12 = static_cast<IDXGISwapChain3*>(m_pSwapChain)->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_pRtvHeap12->GetCPUDescriptorHandleForHeapStart());

    // Wrap every swapchain back buffer once and keep the wrappers alive. Present12 acquires
    // the current one each frame (InState COPY_DEST: it is only ever a CopyResource target),
    // and releases it back to PRESENT for the swapchain.
    D3D11_RESOURCE_FLAGS d3d11Flags{ D3D11_BIND_RENDER_TARGET };
    for (UINT i = 0; i < k_FrameCount; ++i) {
        R_CHK(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets12[i])));

        // DX12 RTV in the heap — unused by the 11on12 copy path, reserved for a future
        // native-DX12 render pass that would draw straight into the back buffer.
        pDevice12->CreateRenderTargetView(m_pRenderTargets12[i], nullptr, rtvHandle);
        rtvHandle.ptr += m_nRtvDescriptorSize12;

        R_CHK(pDevice11on12->CreateWrappedResource(
            m_pRenderTargets12[i],
            &d3d11Flags,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PRESENT,
            IID_PPV_ARGS(&m_pWrappedBackBuffers12[i])));
    }

    // Offline colour buffer — R4 renders here every frame; Present12 copies it into the
    // acquired back buffer. Format matches the swapchain so CopyResource is a straight blit.
    {
        D3D11_TEXTURE2D_DESC rtDesc{};
        rtDesc.Width              = m_ChainDesc.Width;
        rtDesc.Height             = m_ChainDesc.Height;
        rtDesc.MipLevels          = 1;
        rtDesc.ArraySize          = 1;
        rtDesc.Format             = m_ChainDesc.Format;
        rtDesc.SampleDesc.Count   = 1;
        rtDesc.SampleDesc.Quality = 0;
        rtDesc.Usage              = D3D11_USAGE_DEFAULT;
        rtDesc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        R_CHK(pDevice->CreateTexture2D(&rtDesc, nullptr, &m_pOfflineRT12));
        R_CHK(pDevice->CreateRenderTargetView(m_pOfflineRT12, nullptr, &pBaseRT));

        Msg("* DX12: UpdateViews offlineRT %ux%u fmt=0x%X, backbuffers=%u currentIndex=%u",
            rtDesc.Width, rtDesc.Height, (u32)rtDesc.Format, k_FrameCount, m_nFrameIndex12);
    }

    // Depth/stencil.
    {
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width              = m_ChainDesc.Width;
        depthDesc.Height             = m_ChainDesc.Height;
        depthDesc.MipLevels          = 1;
        depthDesc.ArraySize          = 1;
        depthDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count   = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage              = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;

        ID3D11Texture2D* depthTex = nullptr;
        R_CHK(pDevice->CreateTexture2D(&depthDesc, nullptr, &depthTex));
        R_CHK(pDevice->CreateDepthStencilView(depthTex, nullptr, &pBaseZB));
        _RELEASE(depthTex);
    }
}

// ---------------------------------------------------------------------------
// Reset — resize swap-chain buffers and recreate views
// ---------------------------------------------------------------------------

void CHW::Reset(HWND hwnd)
{
    WaitForGpu();

    _RELEASE(pBaseZB);
    _RELEASE(pBaseRT);
    _RELEASE(m_pOfflineRT12);
    // All references to the swapchain buffers (DX12 resources + 11on12 wrappers) must be
    // released before ResizeBuffers, or the resize fails.
    for (UINT i = 0; i < k_FrameCount; ++i) {
        _RELEASE(m_pWrappedBackBuffers12[i]);
        _RELEASE(m_pRenderTargets12[i]);
    }

    BOOL bWindowed = (g_screenmode != 2);
    selectResolution(m_ChainDesc.Width, m_ChainDesc.Height, bWindowed);
    m_ChainDescFullscreen.Windowed = bWindowed;

    DXGI_SWAP_CHAIN_FLAG flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    if (m_SupportsVRR) flags = (DXGI_SWAP_CHAIN_FLAG)(flags | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);

    R_CHK(m_pSwapChain->ResizeBuffers(
        k_FrameCount,
        m_ChainDesc.Width, m_ChainDesc.Height,
        m_ChainDesc.Format,
        flags));

    m_nFrameIndex12 = static_cast<IDXGISwapChain3*>(m_pSwapChain)->GetCurrentBackBufferIndex();

    updateWindowProps(hwnd);
    UpdateViews();
}

// ---------------------------------------------------------------------------
// WaitForGpu — CPU blocks until the GPU has finished all queued work
// ---------------------------------------------------------------------------

void CHW::WaitForGpu()
{
    if (!pDevice12 || !m_pCommandQueue12 || !m_pFence12 || !m_hFenceEvent12)
        return;

    const UINT64 fence = m_nFenceValue12++;
    R_CHK(m_pCommandQueue12->Signal(m_pFence12, fence));

    if (m_pFence12->GetCompletedValue() < fence) {
        R_CHK(m_pFence12->SetEventOnCompletion(fence, m_hFenceEvent12));
        WaitForSingleObjectEx(m_hFenceEvent12, INFINITE, FALSE);
    }
}

// ---------------------------------------------------------------------------
// Present12 — D3D11On12 present: copy offline RT into the acquired back buffer
// ---------------------------------------------------------------------------

void CHW::Present12(UINT present_interval, UINT present_flags)
{
    // Flip-model: the writable back buffer changes after every Present, so re-query it.
    m_nFrameIndex12 = static_cast<IDXGISwapChain3*>(m_pSwapChain)->GetCurrentBackBufferIndex();
    ID3D11Resource* backBuffer = m_pWrappedBackBuffers12[m_nFrameIndex12];

    static bool s_firstPresentLogged = false;
    if (!s_firstPresentLogged) {
        Msg("* DX12: first Present12 OK (index=%u interval=%u flags=0x%X) — present path live",
            m_nFrameIndex12, present_interval, present_flags);
        s_firstPresentLogged = true;
    }

    // Unbind render targets so the offline RT can serve as a CopyResource source without a
    // read/write hazard against the still-bound pBaseRT.
    pContext->OMSetRenderTargets(0, nullptr, nullptr);

    // 11on12 handshake: PRESENT -> COPY_DEST, blit the frame, COPY_DEST -> PRESENT.
    pDevice11on12->AcquireWrappedResources(&backBuffer, 1);
    pContext->CopyResource(backBuffer, m_pOfflineRT12);
    pDevice11on12->ReleaseWrappedResources(&backBuffer, 1);

    // Submit the recorded 11on12 commands to the DX12 queue, then present.
    pContext->Flush();
    m_pSwapChain->Present(present_interval, present_flags);
}

// ---------------------------------------------------------------------------
// Display mode helpers — identical to dx10HW.cpp (DXGI is API-agnostic)
// ---------------------------------------------------------------------------

void CHW::selectResolution(u32& dwWidth, u32& dwHeight, BOOL bWindowed)
{
    fill_render_mode_list();
    if (bWindowed) {
        dwWidth  = psCurrentVidMode[0];
        dwHeight = psCurrentVidMode[1];
    } else {
        string64 buff;
        xr_sprintf(buff, sizeof(buff), "%dx%d", psCurrentVidMode[0], psCurrentVidMode[1]);
        if (xr_strcmp(buff, "0x0") == 0) {
            RECT rc;
            GetClientRect(GetDesktopWindow(), &rc);
            dwWidth  = rc.right;
            dwHeight = rc.bottom;
        } else {
            dwWidth  = psCurrentVidMode[0];
            dwHeight = psCurrentVidMode[1];
        }
    }
}

D3DFORMAT CHW::selectDepthStencil(D3DFORMAT /*fTarget*/)
{
    return D3DFMT_D24S8; // fixed for DX12 path; actual DXGI format chosen in UpdateViews
}

u32 CHW::selectPresentInterval()
{
    return 0;
}

u32 CHW::selectGPU()
{
    return 0;
}

DXGI_RATIONAL CHW::selectRefresh(u32 dwWidth, u32 dwHeight, DXGI_FORMAT fmt)
{
    DXGI_RATIONAL result{ 0, 0 };
    if (!m_pAdapter) return result;

    IDXGIOutput* pOutput = nullptr;
    if (FAILED(m_pAdapter->EnumOutputs(0, &pOutput)) || !pOutput)
        return result;

    UINT numModes = 0;
    if (FAILED(pOutput->GetDisplayModeList(fmt, 0, &numModes, nullptr)) || numModes == 0) {
        _RELEASE(pOutput);
        return result;
    }

    xr_vector<DXGI_MODE_DESC> modes(numModes);
    pOutput->GetDisplayModeList(fmt, 0, &numModes, modes.data());
    _RELEASE(pOutput);

    int maxRefresh = 0;
    for (auto& m : modes) {
        if (m.Width == dwWidth && m.Height == dwHeight) {
            int r = (int)(m.RefreshRate.Numerator / std::max(m.RefreshRate.Denominator, 1u));
            if (r > maxRefresh || (r == maxRefresh && m.RefreshRate.Denominator == 1)) {
                maxRefresh = r;
                result     = m.RefreshRate;
            }
        }
    }
    return result;
}

BOOL CHW::support(D3DFORMAT /*fmt*/, DWORD /*type*/, DWORD /*usage*/)
{
    return TRUE; // DX12 supports all relevant formats; callers use CheckFormatSupport separately
}

void CHW::updateWindowProps(HWND hw)
{
    BOOL bWindowed = m_ChainDescFullscreen.Windowed;
    SetWindowLong(hw, GWL_STYLE,
        bWindowed
            ? (WS_VISIBLE | WS_BORDER | WS_DLGFRAME | WS_SYSMENU | WS_MINIMIZEBOX)
            : (WS_VISIBLE | WS_POPUP));
    SetWindowPos(hw, HWND_NOTOPMOST, 0, 0,
        m_ChainDesc.Width, m_ChainDesc.Height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CHW::OnAppActivate()   {}
void CHW::OnAppDeactivate() {}

// ---------------------------------------------------------------------------
// fill_vid_mode_list / free_vid_mode_list  (DXGI enumeration, same as DX11)
// ---------------------------------------------------------------------------

#ifndef _EDITOR

struct _vid_mode
{
    u32 w, h;
};

static xr_vector<_vid_mode>* vid_modes   = nullptr;
static xr_vector<xr_token>*  vid_tokens  = nullptr;

void fill_vid_mode_list(CHW* _hw)
{
    if (vid_modes) { free_vid_mode_list(); }

    vid_modes  = new xr_vector<_vid_mode>();
    vid_tokens = new xr_vector<xr_token>();

    IDXGIOutput* pOutput = nullptr;
    if (!_hw->m_pAdapter || FAILED(_hw->m_pAdapter->EnumOutputs(0, &pOutput)) || !pOutput)
        return;

    UINT numModes = 0;
    DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
    pOutput->GetDisplayModeList(fmt, 0, &numModes, nullptr);
    if (!numModes) { _RELEASE(pOutput); return; }

    xr_vector<DXGI_MODE_DESC> modes(numModes);
    pOutput->GetDisplayModeList(fmt, 0, &numModes, modes.data());
    _RELEASE(pOutput);

    for (auto& m : modes) {
        bool found = false;
        for (auto& v : *vid_modes)
            if (v.w == m.Width && v.h == m.Height) { found = true; break; }
        if (!found)
            vid_modes->push_back({ m.Width, m.Height });
    }

    xr_token zero_tok{ nullptr, 0 };
    for (auto& v : *vid_modes) {
        string64 buf;
        xr_sprintf(buf, sizeof(buf), "%dx%d", v.w, v.h);
        xr_token tok;
        tok.name  = xr_strdup(buf);
        tok.id    = vid_tokens->size();
        vid_tokens->push_back(tok);
    }
    vid_tokens->push_back(zero_tok);

    vid_mode_token = vid_tokens->data();
}

void free_vid_mode_list()
{
    if (vid_tokens) {
        for (auto& t : *vid_tokens) xr_free(t.name);
        delete vid_tokens; vid_tokens = nullptr;
    }
    delete vid_modes; vid_modes = nullptr;
    vid_mode_token = nullptr;
}

void fill_render_mode_list() {}
void free_render_mode_list() {}

#endif // _EDITOR

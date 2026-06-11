// vkHW.cpp — CHW Vulkan backend implementation.
//
// Phase 1: Vulkan owns the swapchain and all presentation.
//   D3D11 device is created WITHOUT a swapchain so it can render to offline
//   render targets while Vulkan presents. Each phase progressively replaces
//   D3D11 draw calls with Vulkan equivalents.
//
// The HWND is given exclusively to Vulkan (vkCreateWin32SurfaceKHR).
// D3D11 renders to pBaseRT which is an offline texture, not swapchain-backed.

#include <algorithm>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <defines.h>
#include <HW.h>
#include <xrCore.h>

#include "XR_IOConsole.h"
#include "xrAPI.h"
#include "xrRender_console.h"

#include "StateManager/dx10SamplerStateCache.h"
#include "StateManager/dx10StateCache.h"
#include "StateManager/dx10StateManager.h"

#include "vkAllocator.h"
#include "vkFrame.h"
#include "vkSwapchain.h"

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
    : m_pFactory(nullptr)
    , m_pAdapter(nullptr)
    , pDevice(nullptr)
    , pContext(nullptr)
    , m_pSwapChain(nullptr)   // always nullptr in VK path
    , pBaseRT(nullptr)
    , pBaseZB(nullptr)
    , pAnnotation(nullptr)
    , m_bUsePerfhud(false)
    , m_SupportsVRR(false)
    , m_move_window(true)
    , maxRefreshRate(0)
    , m_vkInstance(VK_NULL_HANDLE)
    , m_vkPhysDevice(VK_NULL_HANDLE)
    , m_vkDevice(VK_NULL_HANDLE)
    , m_vkGraphicsQueue(VK_NULL_HANDLE)
    , m_vkSurface(VK_NULL_HANDLE)
    , m_vkSwapchain(VK_NULL_HANDLE)
    , m_vkGraphicsFamily(0)
    , m_vkFrames(nullptr)
    , m_vkSwapchainRes(nullptr)
    , m_vkCurrentFrame(0)
    , m_vkOfflineTex(nullptr)
    , m_vkOfflineMutex(nullptr)
    , m_vkOfflineHandle(nullptr)
    , m_vkInteropImage(VK_NULL_HANDLE)
    , m_vkInteropMemory(VK_NULL_HANDLE)
    , m_vkInteropReady(false)
{
    Device.seqAppActivate.Add(this);
    Device.seqAppDeactivate.Add(this);
}

CHW::~CHW()
{
    Device.seqAppActivate.Remove(this);
    Device.seqAppDeactivate.Remove(this);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool vk_find_graphics_queue(VkPhysicalDevice phys, VkSurfaceKHR surface, u32& outFamily)
{
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
    xr_vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, props.data());

    for (u32 i = 0; i < count; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &presentSupport);
        if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
            outFamily = i;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// CreateD3D / DestroyD3D  (DXGI adapter enumeration only — no swapchain)
// ---------------------------------------------------------------------------

void CHW::CreateD3D()
{
    R_CHK(CreateDXGIFactory1(IID_PPV_ARGS(&m_pFactory)));

    m_pAdapter    = nullptr;
    m_bUsePerfhud = false;
    m_SupportsVRR = false;

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
// Vulkan instance + surface + device helpers
// ---------------------------------------------------------------------------

extern u32 g_screenmode;

static void vk_create_instance(VkInstance& inst)
{
    const char* layers[]     = { "VK_LAYER_KHRONOS_validation" };
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "STALKER Anomaly R5VK";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 5, 3);
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = (u32)std::size(extensions);
    ci.ppEnabledExtensionNames = extensions;

    bool dbg = strstr(Core.Params, "--vk-dbg") != nullptr;
    if (dbg) {
        ci.enabledLayerCount   = 1;
        ci.ppEnabledLayerNames = layers;
        Msg("* VK: validation layers enabled");
    }

    R_CHK2(vkCreateInstance(&ci, nullptr, &inst), "vkCreateInstance");
}

static VkPhysicalDevice vk_select_adapter(VkInstance inst)
{
    u32 count = 0;
    vkEnumeratePhysicalDevices(inst, &count, nullptr);
    VERIFY2(count > 0, "No Vulkan-capable GPU found");

    xr_vector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(inst, &count, devs.data());

    VkPhysicalDevice chosen = devs[0];
    for (auto& d : devs) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d, &p);
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = d;
            break;
        }
    }
    return chosen;
}

static void vk_create_device(VkPhysicalDevice phys, u32 queueFamily,
                              VkDevice& device, VkQueue& queue)
{
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = queueFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &priority;

    const char* devExts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        // D3D11<->VK present interop (VK_KHR_external_memory is core in 1.1; these two are
        // Windows-specific and must be enabled explicitly).
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME,
    };

    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(phys, &features);

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = (u32)std::size(devExts);
    dci.ppEnabledExtensionNames = devExts;
    dci.pEnabledFeatures        = &features;

    R_CHK2(vkCreateDevice(phys, &dci, nullptr, &device), "vkCreateDevice");
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
}

// ---------------------------------------------------------------------------
// CreateDevice
// ---------------------------------------------------------------------------

void CHW::CreateDevice(HWND hwnd, bool move_window)
{
    m_hWnd        = hwnd;
    m_move_window = move_window;
    CreateD3D();

    BOOL bWindowed = (g_screenmode != 2);

    // -----------------------------------------------------------------------
    // 1. Vulkan: instance → Win32 surface → physical device → logical device
    // -----------------------------------------------------------------------

    vk_create_instance(m_vkInstance);

    VkWin32SurfaceCreateInfoKHR wsci{};
    wsci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    wsci.hinstance = GetModuleHandle(nullptr);
    wsci.hwnd      = hwnd;
    R_CHK2(vkCreateWin32SurfaceKHR(m_vkInstance, &wsci, nullptr, &m_vkSurface),
           "vkCreateWin32SurfaceKHR");

    m_vkPhysDevice = vk_select_adapter(m_vkInstance);

    {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(m_vkPhysDevice, &p);
        Msg("* VK GPU [vendor:%X]-[device:%X]: %s (API %u.%u.%u)",
            p.vendorID, p.deviceID, p.deviceName,
            VK_VERSION_MAJOR(p.apiVersion),
            VK_VERSION_MINOR(p.apiVersion),
            VK_VERSION_PATCH(p.apiVersion));
        Caps.id_vendor = p.vendorID;
        Caps.id_device = p.deviceID;
    }

    if (!vk_find_graphics_queue(m_vkPhysDevice, m_vkSurface, m_vkGraphicsFamily))
        R_ASSERT2(false, "No Vulkan graphics+present queue found");
    vk_create_device(m_vkPhysDevice, m_vkGraphicsFamily, m_vkDevice, m_vkGraphicsQueue);

    // -----------------------------------------------------------------------
    // 2. VMA — must be initialised before any allocation (including swapchain depth)
    // -----------------------------------------------------------------------

    vkAllocator_Create(m_vkInstance, m_vkPhysDevice, m_vkDevice);

    // -----------------------------------------------------------------------
    // 3. Vulkan swapchain + per-frame resources
    // -----------------------------------------------------------------------

    selectResolution(m_ChainDesc.Width, m_ChainDesc.Height, bWindowed);

    m_vkSwapchainRes = new VkSwapchainResources();
    m_vkSwapchainRes->Create(m_vkPhysDevice, m_vkDevice, m_vkSurface,
                              m_ChainDesc.Width, m_ChainDesc.Height, m_vkGraphicsFamily);
    m_vkSwapchain = m_vkSwapchainRes->swapchain;

    m_vkFrames = new VkFrameResources[VK_FRAMES_IN_FLIGHT];
    for (int i = 0; i < VK_FRAMES_IN_FLIGHT; i++)
        m_vkFrames[i].Create(m_vkDevice, m_vkGraphicsFamily);

    Msg("* VK: swapchain %ux%u (%u images), %d frames in flight",
        m_ChainDesc.Width, m_ChainDesc.Height,
        (u32)m_vkSwapchainRes->images.size(), VK_FRAMES_IN_FLIGHT);

    // -----------------------------------------------------------------------
    // 4. D3D11 device — no swapchain, renders to offline pBaseRT
    // -----------------------------------------------------------------------

    UINT d3d11Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (strstr(Core.Params, "--vk-dbg"))
        d3d11Flags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL actualLevel{};

    ID3D11Device*        d3d11Device  = nullptr;
    ID3D11DeviceContext* d3d11Context = nullptr;
    R_CHK(D3D11CreateDevice(
        m_pAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        d3d11Flags,
        featureLevels, (UINT)std::size(featureLevels),
        D3D11_SDK_VERSION,
        &d3d11Device,
        &actualLevel,
        &d3d11Context));

    R_CHK(d3d11Device->QueryInterface(IID_PPV_ARGS(&pDevice)));
    R_CHK(d3d11Context->QueryInterface(IID_PPV_ARGS(&pContext)));
    FeatureLevel = actualLevel;
    _RELEASE(d3d11Device);
    _RELEASE(d3d11Context);

    R_CHK(pContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
        (void**)&pAnnotation));

    Msg("* VK: D3D11 device (feature level 0x%X) for offline render pipeline", (u32)FeatureLevel);

    // Legacy caps stubs
    Caps.fTarget = D3DFMT_X8R8G8B8;
    Caps.fDepth  = selectDepthStencil(Caps.fTarget);

    // -----------------------------------------------------------------------
    // 5. Create offline D3D11 RTs + finalize vid mode
    // -----------------------------------------------------------------------

    UpdateViews();
    fill_vid_mode_list(this);
}

// ---------------------------------------------------------------------------
// DestroyDevice
// ---------------------------------------------------------------------------

void CHW::DestroyDevice()
{
    StateManager.Reset();
    RSManager.ClearStateArray();
    DSSManager.ClearStateArray();
    BSManager.ClearStateArray();
    SSManager.ClearStateArray();

    _SHOW_REF("refCount:pBaseZB", pBaseZB);
    _RELEASE(pBaseZB);
    _SHOW_REF("refCount:pBaseRT", pBaseRT);
    _RELEASE(pBaseRT);

    // Tear down the D3D11<->VK interop (keyed mutex + imported VK image) before the devices.
    VKDestroyInterop();
    _RELEASE(m_vkOfflineTex);

    _RELEASE(pAnnotation);
    _RELEASE(pContext);
    _RELEASE(pDevice);

    // Vulkan teardown (ordered: frames → swapchain → VMA → device → surface → instance)
    if (m_vkDevice != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_vkDevice);

    if (m_vkFrames) {
        for (int i = 0; i < VK_FRAMES_IN_FLIGHT; i++)
            m_vkFrames[i].Destroy(m_vkDevice);
        delete[] m_vkFrames;
        m_vkFrames = nullptr;
    }

    if (m_vkSwapchainRes) {
        m_vkSwapchainRes->Destroy(m_vkDevice);
        delete m_vkSwapchainRes;
        m_vkSwapchainRes = nullptr;
    }
    m_vkSwapchain = VK_NULL_HANDLE;

    vkAllocator_Destroy();

    if (m_vkDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(m_vkDevice, nullptr);
        m_vkDevice = VK_NULL_HANDLE;
    }
    if (m_vkSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_vkInstance, m_vkSurface, nullptr);
        m_vkSurface = VK_NULL_HANDLE;
    }
    if (m_vkInstance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_vkInstance, nullptr);
        m_vkInstance = VK_NULL_HANDLE;
    }

    free_vid_mode_list();
    DestroyD3D();
}

// ---------------------------------------------------------------------------
// UpdateViews — creates offline D3D11 RTs (not swapchain-backed in VK path)
// ---------------------------------------------------------------------------

void CHW::UpdateViews()
{
    VKDestroyInterop();
    _RELEASE(pBaseRT);
    _RELEASE(pBaseZB);
    _RELEASE(m_vkOfflineTex);

    // Offline colour RT — R4 renders here. Created SHARED (NT handle + keyed mutex) so Vulkan
    // can import it as an external-memory image and blit it into the swapchain each frame.
    D3D11_TEXTURE2D_DESC rtDesc{};
    rtDesc.Width              = m_ChainDesc.Width;
    rtDesc.Height             = m_ChainDesc.Height;
    rtDesc.MipLevels          = 1;
    rtDesc.ArraySize          = 1;
    rtDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtDesc.SampleDesc.Count   = 1;
    rtDesc.SampleDesc.Quality = 0;
    rtDesc.Usage              = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    rtDesc.MiscFlags          = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    R_CHK(pDevice->CreateTexture2D(&rtDesc, nullptr, &m_vkOfflineTex));
    R_CHK(pDevice->CreateRenderTargetView(m_vkOfflineTex, nullptr, &pBaseRT));

    VKSetupInterop(); // import into Vulkan; on failure VKPresent falls back to the teal clear

    // Depth/stencil
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

// ---------------------------------------------------------------------------
// Reset — recreate swapchain + offline RTs on resize
// ---------------------------------------------------------------------------

void CHW::Reset(HWND hwnd)
{
    _RELEASE(pBaseZB);
    _RELEASE(pBaseRT);

    BOOL bWindowed = (g_screenmode != 2);
    selectResolution(m_ChainDesc.Width, m_ChainDesc.Height, bWindowed);
    m_ChainDescFullscreen.Windowed = bWindowed;

    if (m_vkSwapchainRes) {
        m_vkSwapchainRes->Recreate(m_vkPhysDevice, m_vkDevice, m_vkSurface,
                                    m_ChainDesc.Width, m_ChainDesc.Height, m_vkGraphicsFamily);
        m_vkSwapchain = m_vkSwapchainRes->swapchain;
    }

    updateWindowProps(hwnd);
    UpdateViews();
}

// ---------------------------------------------------------------------------
// VKSetupInterop — import the shared D3D11 offline RT into Vulkan as an
// external-memory image. On any failure it logs the step and leaves
// m_vkInteropReady=false so VKPresent falls back to the (non-crashing) teal clear.
// ---------------------------------------------------------------------------

void CHW::VKSetupInterop()
{
    m_vkInteropReady = false;
    if (!m_vkOfflineTex || m_vkDevice == VK_NULL_HANDLE)
        return;

    // 1. Keyed mutex (cross-API sync) on the shared texture.
    if (FAILED(m_vkOfflineTex->QueryInterface(IID_PPV_ARGS(&m_vkOfflineMutex)))) {
        Msg("! VK interop: offline RT has no IDXGIKeyedMutex"); return;
    }

    // 2. Export an NT shared handle.
    IDXGIResource1* dxgiRes = nullptr;
    if (FAILED(m_vkOfflineTex->QueryInterface(IID_PPV_ARGS(&dxgiRes)))) {
        Msg("! VK interop: offline RT has no IDXGIResource1"); return;
    }
    HRESULT hr = dxgiRes->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &m_vkOfflineHandle);
    _RELEASE(dxgiRes);
    if (FAILED(hr) || !m_vkOfflineHandle) {
        Msg("! VK interop: CreateSharedHandle failed (0x%08x)", hr); return;
    }

    // 3. Create a VkImage declared as external-memory, matching the D3D11 texture.
    VkExternalMemoryImageCreateInfo extImg{ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
    extImg.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;

    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.pNext         = &extImg;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent        = { m_ChainDesc.Width, m_ChainDesc.Height, 1 };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(m_vkDevice, &ici, nullptr, &m_vkInteropImage) != VK_SUCCESS) {
        Msg("! VK interop: vkCreateImage failed"); return;
    }

    // 4. Resolve the memory type compatible with both the image and the imported handle.
    auto pfnGetMemWin32Props = (PFN_vkGetMemoryWin32HandlePropertiesKHR)
        vkGetDeviceProcAddr(m_vkDevice, "vkGetMemoryWin32HandlePropertiesKHR");
    if (!pfnGetMemWin32Props) {
        Msg("! VK interop: vkGetMemoryWin32HandlePropertiesKHR not available"); return;
    }
    VkMemoryWin32HandlePropertiesKHR handleProps{ VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR };
    if (pfnGetMemWin32Props(m_vkDevice, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
                            m_vkOfflineHandle, &handleProps) != VK_SUCCESS) {
        Msg("! VK interop: GetMemoryWin32HandleProperties failed"); return;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(m_vkDevice, m_vkInteropImage, &memReq);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_vkPhysDevice, &memProps);
    const u32 typeBits = memReq.memoryTypeBits & handleProps.memoryTypeBits;
    int memTypeIndex = -1;
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) // prefer device-local
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIndex = (int)i; break;
        }
    if (memTypeIndex < 0)
        for (u32 i = 0; i < memProps.memoryTypeCount; ++i)
            if (typeBits & (1u << i)) { memTypeIndex = (int)i; break; }
    if (memTypeIndex < 0) {
        Msg("! VK interop: no compatible memory type (bits=0x%X)", typeBits); return;
    }

    // 5. Import the handle as dedicated memory and bind it to the image.
    VkMemoryDedicatedAllocateInfo dedInfo{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
    dedInfo.image = m_vkInteropImage;

    VkImportMemoryWin32HandleInfoKHR importInfo{ VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
    importInfo.pNext      = &dedInfo;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
    importInfo.handle     = m_vkOfflineHandle;

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.pNext           = &importInfo;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = (u32)memTypeIndex;
    if (vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &m_vkInteropMemory) != VK_SUCCESS) {
        Msg("! VK interop: vkAllocateMemory (import) failed"); return;
    }
    if (vkBindImageMemory(m_vkDevice, m_vkInteropImage, m_vkInteropMemory, 0) != VK_SUCCESS) {
        Msg("! VK interop: vkBindImageMemory failed"); return;
    }

    // Hand the texture to D3D11 (key 0) so the R4 pipeline can render the first frame.
    // Finite timeout so a stuck mutex degrades to the teal fallback instead of hanging.
    if (m_vkOfflineMutex->AcquireSync(0, 5000) != S_OK) {
        Msg("! VK interop: initial keyed-mutex AcquireSync(0) failed/timed out"); return;
    }

    m_vkInteropReady = true;
    Msg("* VK interop: D3D11 offline RT imported into Vulkan (%ux%u) — native render + VK present live",
        m_ChainDesc.Width, m_ChainDesc.Height);
}

void CHW::VKDestroyInterop()
{
    if (m_vkDevice != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_vkDevice);

    if (m_vkOfflineMutex) { m_vkOfflineMutex->Release(); m_vkOfflineMutex = nullptr; }
    if (m_vkInteropImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_vkDevice, m_vkInteropImage, nullptr); m_vkInteropImage = VK_NULL_HANDLE;
    }
    if (m_vkInteropMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_vkDevice, m_vkInteropMemory, nullptr); m_vkInteropMemory = VK_NULL_HANDLE;
    }
    if (m_vkOfflineHandle) { CloseHandle(m_vkOfflineHandle); m_vkOfflineHandle = nullptr; }
    m_vkInteropReady = false;
}

// ---------------------------------------------------------------------------
// VKPresent — blit the D3D11-rendered offline RT into the swapchain (or, if
// interop is unavailable, clear to teal as a visible fallback) and present.
// ---------------------------------------------------------------------------

void CHW::VKPresent()
{
    if (!m_vkFrames || !m_vkSwapchainRes) return;

    u32 frameIdx = m_vkCurrentFrame;
    VkFrameResources& frame = m_vkFrames[frameIdx];

    // Wait for this slot's previous submission
    vkWaitForFences(m_vkDevice, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);

    // Acquire next swapchain image
    u32 imageIndex = 0;
    VkResult acqResult = vkAcquireNextImageKHR(
        m_vkDevice, m_vkSwapchainRes->swapchain, UINT64_MAX,
        frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);

    if (acqResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_vkSwapchainRes->Recreate(m_vkPhysDevice, m_vkDevice, m_vkSurface,
                                    m_ChainDesc.Width, m_ChainDesc.Height, m_vkGraphicsFamily);
        m_vkSwapchain = m_vkSwapchainRes->swapchain;
        return;
    }

    vkResetFences(m_vkDevice, 1, &frame.inFlight);

    // Hand the shared offline RT from D3D11 to Vulkan for this frame (keyed mutex key 1).
    if (m_vkInteropReady) {
        m_vkOfflineMutex->ReleaseSync(1); // D3D11 done rendering this frame -> key 1
        pContext->Flush();                // submit the R4 commands + the release to the GPU
    }

    frame.Begin();

    // swapchain image: UNDEFINED -> TRANSFER_DST
    VkImageMemoryBarrier toTransfer{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    toTransfer.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image               = m_vkSwapchainRes->images[imageIndex];
    toTransfer.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    toTransfer.srcAccessMask       = 0;
    toTransfer.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(frame.cmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    if (m_vkInteropReady) {
        // imported D3D11 image: -> TRANSFER_SRC, then blit it into the swapchain image.
        VkImageMemoryBarrier toSrc{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        toSrc.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        toSrc.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSrc.image               = m_vkInteropImage;
        toSrc.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        toSrc.srcAccessMask       = 0;
        toSrc.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(frame.cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toSrc);

        VkImageBlit region{};
        region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.srcOffsets[0]  = { 0, 0, 0 };
        region.srcOffsets[1]  = { (s32)m_ChainDesc.Width, (s32)m_ChainDesc.Height, 1 };
        region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.dstOffsets[0]  = { 0, 0, 0 };
        region.dstOffsets[1]  = { (s32)m_vkSwapchainRes->extent.width,
                                  (s32)m_vkSwapchainRes->extent.height, 1 };
        vkCmdBlitImage(frame.cmdBuffer,
            m_vkInteropImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_vkSwapchainRes->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region, VK_FILTER_LINEAR);
    } else {
        // Fallback (interop unavailable): dark teal so the window is never undefined.
        VkClearColorValue clearColor = { 0.0f, 0.2f, 0.25f, 1.0f };
        VkImageSubresourceRange colorRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(frame.cmdBuffer, m_vkSwapchainRes->images[imageIndex],
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &colorRange);
    }

    VkImageMemoryBarrier toPresent = toTransfer;
    toPresent.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toPresent.dstAccessMask = 0;
    vkCmdPipelineBarrier(frame.cmdBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toPresent);

    frame.End();

    // Submit (chain the keyed-mutex acquire(key1)/release(key0) when interop is active).
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &frame.imageAvailable;
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &frame.cmdBuffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &frame.renderFinished;

    uint64_t kmAcquireKey = 1, kmReleaseKey = 0;
    uint32_t kmTimeout    = INFINITE;
    VkWin32KeyedMutexAcquireReleaseInfoKHR km{ VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR };
    if (m_vkInteropReady) {
        km.acquireCount     = 1;
        km.pAcquireSyncs    = &m_vkInteropMemory;
        km.pAcquireKeys     = &kmAcquireKey;
        km.pAcquireTimeouts = &kmTimeout;
        km.releaseCount     = 1;
        km.pReleaseSyncs    = &m_vkInteropMemory;
        km.pReleaseKeys     = &kmReleaseKey;
        si.pNext            = &km;
    }
    vkQueueSubmit(m_vkGraphicsQueue, 1, &si, frame.inFlight);

    // Present
    VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &frame.renderFinished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &m_vkSwapchainRes->swapchain;
    pi.pImageIndices      = &imageIndex;
    VkResult presentResult = vkQueuePresentKHR(m_vkGraphicsQueue, &pi);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        m_vkSwapchainRes->Recreate(m_vkPhysDevice, m_vkDevice, m_vkSurface,
                                    m_ChainDesc.Width, m_ChainDesc.Height, m_vkGraphicsFamily);
        m_vkSwapchain = m_vkSwapchainRes->swapchain;
    }

    // Reclaim the shared texture for D3D11 (key 0) so the next frame's R4 pipeline can render
    // into it. Vulkan releases key 0 after the blit submitted above completes.
    if (m_vkInteropReady)
        m_vkOfflineMutex->AcquireSync(0, INFINITE);

    m_vkCurrentFrame = (m_vkCurrentFrame + 1) % VK_FRAMES_IN_FLIGHT;
}

// ---------------------------------------------------------------------------
// Display mode helpers
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
    return D3DFMT_D24S8;
}

u32 CHW::selectPresentInterval() { return 0; }
u32 CHW::selectGPU()             { return 0; }

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
    return TRUE;
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
// fill_vid_mode_list — DXGI enumeration
// ---------------------------------------------------------------------------

#ifndef _EDITOR

struct _vid_mode { u32 w, h; };
static xr_vector<_vid_mode>* vid_modes  = nullptr;
static xr_vector<xr_token>*  vid_tokens = nullptr;

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
        if (!found) vid_modes->push_back({ m.Width, m.Height });
    }

    xr_token zero_tok{ nullptr, 0 };
    for (auto& v : *vid_modes) {
        string64 buf;
        xr_sprintf(buf, sizeof(buf), "%dx%d", v.w, v.h);
        xr_token tok;
        tok.name = xr_strdup(buf);
        tok.id   = (int)vid_tokens->size();
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

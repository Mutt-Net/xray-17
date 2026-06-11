// HW.h: interface for the CHW class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_HW_H__0E25CF4A_FFEC_11D3_B4E3_4854E82A090D__INCLUDED_)
#define AFX_HW_H__0E25CF4A_FFEC_11D3_B4E3_4854E82A090D__INCLUDED_
#pragma once

#include <device.h>

#if defined(USE_DX11)
#include <d3d11_4.h>
#include <dxgi1_4.h>
#endif
#if defined(USE_DX12)
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d11on12.h>
#endif
#if defined(USE_VK)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
// Forward declarations for Vulkan frame/swapchain helpers (defined in vkFrame.h / vkSwapchain.h).
// Must be global-scope so CHW member pointers resolve to the same types.
struct VkFrameResources;
struct VkSwapchainResources;
#endif

#include "hwcaps.h"

#include "../../build_config_defines.h"

#ifndef _MAYA_EXPORT
#include "stats_manager.h"
#endif

class CHW
#if defined(USE_DX10) || defined(USE_DX11)
	:	public pureAppActivate,
		public pureAppDeactivate
#endif	//	USE_DX10
{
	//	Functions section
public:
	int maxRefreshRate; //ECO_RENDER add
	CHW();
	~CHW();

	void CreateD3D();
	void DestroyD3D();
	void CreateDevice(HWND hw, bool move_window);

	void DestroyDevice();

	void Reset(HWND hw);

	void selectResolution(u32& dwWidth, u32& dwHeight, BOOL bWindowed);
	D3DFORMAT selectDepthStencil(D3DFORMAT);
	u32 selectPresentInterval();
	u32 selectGPU();
	u32 selectRefresh(u32 dwWidth, u32 dwHeight, D3DFORMAT fmt);
	void updateWindowProps(HWND hw);
	BOOL support(D3DFORMAT fmt, DWORD type, DWORD usage);

#ifdef DEBUG
#if defined(USE_DX10) || defined(USE_DX11)
	void	Validate(void)	{};
#else	//	USE_DX10
	void	Validate(void)	{	VERIFY(pDevice); VERIFY(pD3D); };
#endif	//	USE_DX10
#else
	void Validate(void)
	{
	};
#endif

	//	Variables section
#if defined(USE_DX11)	//	USE_DX10
public:
    IDXGIFactory2*          m_pFactory; //  DXGI factory
	IDXGIAdapter1*			m_pAdapter;	//	pD3D equivalent
	ID3D11Device1*			pDevice;	//	combine with DX9 pDevice via typedef
	ID3D11DeviceContext1*   pContext;	//	combine with DX9 pDevice via typedef
	IDXGISwapChain1*        m_pSwapChain;
	ID3D11RenderTargetView*	pBaseRT;	//	combine with DX9 pBaseRT via typedef
	ID3D11DepthStencilView*	pBaseZB;
	ID3DUserDefinedAnnotation* pAnnotation;

	CHWCaps					Caps;

	D3D_DRIVER_TYPE					m_DriverType;	//	DevT equivalent
	DXGI_SWAP_CHAIN_DESC1			m_ChainDesc;	//	DevPP equivalent
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_ChainDescFullscreen;
    HWND                            m_hWnd;
	bool							m_bUsePerfhud;
	D3D_FEATURE_LEVEL				FeatureLevel;
	bool 							m_SupportsVRR; // whether we can use DXGI_PRESENT_ALLOW_TEARING etc.

#if defined(USE_DX12)
	// DX12 backend: the fields above (pDevice, pContext, pBaseRT, etc.) are populated via
	// D3D11On12CreateDevice so all existing render code can run unchanged on top of DX12.
	// These fields hold the native DX12 objects.
	ID3D12Device*               pDevice12;
	ID3D12CommandQueue*         m_pCommandQueue12;
	ID3D11On12Device*           pDevice11on12;
	// Per-buffer DX12 resources (double-buffered; index via m_nFrameIndex12).
	static constexpr UINT       k_FrameCount = 2;
	ID3D12Resource*             m_pRenderTargets12[k_FrameCount];
	UINT                        m_nRtvDescriptorSize12;
	ID3D12DescriptorHeap*       m_pRtvHeap12;
	// D3D11On12 wrappers of the swapchain back buffers. Kept alive for the lifetime of the
	// swapchain so each frame can Acquire/Release them (the required 11on12 handshake).
	ID3D11Resource*             m_pWrappedBackBuffers12[k_FrameCount];
	// Offline colour buffer the R4 pipeline renders into (pBaseRT is an RTV over this).
	// At present time it is copied into the acquired swapchain back buffer. This keeps the
	// wrapped-resource handshake fully contained in Present12 — independent of the flip-model
	// buffer index and safe across SVP frames that skip presentation.
	ID3D11Texture2D*            m_pOfflineRT12;
	// GPU/CPU sync
	ID3D12Fence*                m_pFence12;
	UINT64                      m_nFenceValue12;
	HANDLE                      m_hFenceEvent12;
	UINT                        m_nFrameIndex12;
	// Correct D3D11On12 present: acquire current back buffer, copy the offline RT into it,
	// release it back to PRESENT state, flush the 11on12 command stream, then Present.
	void Present12(UINT present_interval, UINT present_flags);
#endif // USE_DX12
#if defined(USE_VK)
	// Vulkan core handles
	VkInstance              m_vkInstance;
	VkPhysicalDevice        m_vkPhysDevice;
	VkDevice                m_vkDevice;
	VkQueue                 m_vkGraphicsQueue;
	VkSurfaceKHR            m_vkSurface;
	VkSwapchainKHR          m_vkSwapchain;  // mirrors m_vkSwapchainRes->swapchain
	u32                     m_vkGraphicsFamily;
	// Phase 1: per-frame sync + swapchain image management
	VkFrameResources*       m_vkFrames;      // heap array [VK_FRAMES_IN_FLIGHT]
	VkSwapchainResources*   m_vkSwapchainRes;
	u32                     m_vkCurrentFrame;

	// D3D11 -> Vulkan present interop: the R4 pipeline renders to a shared D3D11 texture
	// (m_vkOfflineTex, viewed by pBaseRT); Vulkan imports it as an external-memory VkImage
	// and blits it into the swapchain each frame. A keyed mutex synchronises the two APIs.
	ID3D11Texture2D*        m_vkOfflineTex;     // shared offline colour RT (D3D11 side)
	IDXGIKeyedMutex*        m_vkOfflineMutex;   // keyed mutex on the shared texture
	HANDLE                  m_vkOfflineHandle;  // exported NT shared handle
	VkImage                 m_vkInteropImage;   // VK image bound to the imported D3D11 memory
	VkDeviceMemory          m_vkInteropMemory;  // imported external memory
	bool                    m_vkInteropReady;   // true once interop is fully set up
	void VKSetupInterop();   // import the shared D3D11 RT into Vulkan (called from UpdateViews)
	void VKDestroyInterop(); // release the imported VK image/memory/handle

	void VKPresent();
#endif // USE_VK
#elif defined(USE_DX10)
public:
	IDXGIAdapter*			m_pAdapter;	//	pD3D equivalent
	ID3D10Device1*       	pDevice1;	//	combine with DX9 pDevice via typedef
	ID3D10Device*        	pDevice;	//	combine with DX9 pDevice via typedef
	ID3D10Device1*       	pContext1;	//	combine with DX9 pDevice via typedef
	ID3D10Device*        	pContext;	//	combine with DX9 pDevice via typedef
	IDXGISwapChain*         m_pSwapChain;
	ID3D10RenderTargetView*	pBaseRT;	//	combine with DX9 pBaseRT via typedef
	ID3D10DepthStencilView*	pBaseZB;

	CHWCaps					Caps;

	D3D10_DRIVER_TYPE		m_DriverType;	//	DevT equivalent
	DXGI_SWAP_CHAIN_DESC	m_ChainDesc;	//	DevPP equivalent
	bool					m_bUsePerfhud;
	D3D_FEATURE_LEVEL		FeatureLevel;
#else
private:
	HINSTANCE hD3D;

public:

	IDirect3D9* pD3D; // D3D
	IDirect3DDevice9* pDevice; // render device

	IDirect3DSurface9* pBaseRT;
	IDirect3DSurface9* pBaseZB;

	CHWCaps Caps;

	UINT DevAdapter;
	D3DDEVTYPE DevT;
	D3DPRESENT_PARAMETERS DevPP;
#endif	//	USE_DX10

#ifndef _MAYA_EXPORT
	stats_manager stats_manager;
#endif
#if defined(USE_DX10) || defined(USE_DX11) || defined(USE_DX12)
	void			UpdateViews();
	DXGI_RATIONAL	selectRefresh(u32 dwWidth, u32 dwHeight, DXGI_FORMAT fmt);

	virtual	void	OnAppActivate();
	virtual void	OnAppDeactivate();
#if defined(USE_DX12)
	void			WaitForGpu(); // flush the DX12 command queue and advance the fence
#endif
#endif	//	USE_DX10

private:
	bool m_move_window;
};

extern ECORE_API CHW HW;

#endif // !defined(AFX_HW_H__0E25CF4A_FFEC_11D3_B4E3_4854E82A090D__INCLUDED_)

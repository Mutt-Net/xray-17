// xrRender_R5VK.cpp — entry point for the Vulkan R5 renderer.
#include "../xrRender/dxRenderFactory.h"
#include "../xrRender/dxUIRender.h"
#include "../xrRender/dxDebugRender.h"

BOOL DllMainXrRenderR5Vk(HANDLE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		::Render        = &RImplementation;
		::RenderFactory = &RenderFactoryImpl;
		::DU            = &DUImpl;
		UIRender        = &UIRenderImpl;
		DRender         = &DebugRenderImpl;
		xrRender_initconsole();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C" {
bool SupportsVulkanRendering();
// LuaJIT's custom GC32 allocator (xr_alloc.c). XR_INIT reserves its 256MB heap in the
// low 2GB; it is idempotent and self-initialising.
void          XR_INIT();
extern size_t g_xr_heap_reserved;
}

bool SupportsVulkanRendering()
{
	// Reserve LuaJIT's low-2GB heap BEFORE creating the first VkInstance. The Vulkan
	// loader/ICD otherwise claims that low address space first, leaving LuaJIT unable to
	// reserve its GC32 heap -> crash on boot in lj_alloc_create. Doing it here (the VK
	// support probe, before any vk call) lets LuaJIT win the low memory; the ICD then uses
	// whatever is left. VK build only — DX11/DX12 never call this.
	XR_INIT();
	Msg("* R5VK: LuaJIT low-2GB heap reserved before Vulkan probe (%Iu MB)",
	    g_xr_heap_reserved / (1024 * 1024));

	return xrRender_test_hw() ? true : false;
}

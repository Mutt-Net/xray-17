// xrRender_R5.cpp — entry point for the R5 (DX12/D3D11On12) renderer.
#include "../xrRender/dxRenderFactory.h"
#include "../xrRender/dxUIRender.h"
#include "../xrRender/dxDebugRender.h"

BOOL DllMainXrRenderR5(HANDLE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
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
bool SupportsDX12Rendering();
}

bool SupportsDX12Rendering()
{
	return xrRender_test_hw() ? true : false;
}

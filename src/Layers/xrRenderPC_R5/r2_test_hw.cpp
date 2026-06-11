// r2_test_hw.cpp — DX12 hardware capability probe for R5.
// Tests for D3D_FEATURE_LEVEL_11_0 on a DX12 device (minimum for D3D11On12).
#include <d3d12.h>
#include <dxgi1_4.h>

BOOL xrRender_test_hw()
{
	HMODULE hD3D12 = LoadLibraryA("d3d12.dll");
	if (!hD3D12) {
		Msg("* R5: d3d12.dll not found — DX12 unavailable");
		return FALSE;
	}

	typedef HRESULT(WINAPI* PFN_D3D12CreateDevice)(
		IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);

	auto pD3D12CreateDevice = reinterpret_cast<PFN_D3D12CreateDevice>(
		GetProcAddress(hD3D12, "D3D12CreateDevice"));

	if (!pD3D12CreateDevice) {
		FreeLibrary(hD3D12);
		Msg("* R5: D3D12CreateDevice not found in d3d12.dll");
		return FALSE;
	}

	// Probe: attempt device creation at feature level 11.0 (minimum for 11on12).
	ID3D12Device* pDevice = nullptr;
	HRESULT hr = pD3D12CreateDevice(
		nullptr,                    // use default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&pDevice));

	if (pDevice) pDevice->Release();
	FreeLibrary(hD3D12);

	if (FAILED(hr)) {
		Msg("* R5: DX12 device creation failed (hr=0x%08x) — R5 unavailable", hr);
		return FALSE;
	}

	Msg("* R5: DX12 hardware probe passed");
	return TRUE;
}

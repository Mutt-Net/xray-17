#include "xrCPU_Pipe.h"
#include "ttapi.h"

BOOL DllMainIgnore2(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
}

// ---------------------------------------------------------------------------
// Scalar baseline (x86 C++)
// ---------------------------------------------------------------------------

extern xrSkin1W xrSkin1W_x86;
extern xrSkin2W xrSkin2W_x86;
extern xrSkin3W xrSkin3W_x86;
extern xrSkin4W xrSkin4W_x86;

// ---------------------------------------------------------------------------
// SSE2 (C++ intrinsics, x64-compatible)
// ---------------------------------------------------------------------------

extern xrSkin1W xrSkin1W_SSE2;
extern xrSkin2W xrSkin2W_SSE2;
extern xrSkin3W xrSkin3W_SSE2;
extern xrSkin4W xrSkin4W_SSE2;

// ---------------------------------------------------------------------------
// AVX2 + FMA3 (compiled with /arch:AVX2 per-file)
// ---------------------------------------------------------------------------

extern xrSkin1W xrSkin1W_AVX2;
extern xrSkin2W xrSkin2W_AVX2;
extern xrSkin3W xrSkin3W_AVX2;
extern xrSkin4W xrSkin4W_AVX2;

// ---------------------------------------------------------------------------
// Multithreaded override
// ---------------------------------------------------------------------------

extern xrSkin4W xrSkin4W_thread;
xrSkin4W* skin4W_func = NULL;

// ---------------------------------------------------------------------------
// PLC
// ---------------------------------------------------------------------------

extern xrPLC_calc3 PLC_calc3_x86;
extern xrPLC_calc3 PLC_calc3_SSE;

// ---------------------------------------------------------------------------
// Dispatch — tiered selection: AVX2 > SSE2 > scalar
// ---------------------------------------------------------------------------

extern "C" {
void __cdecl xrBind_PSGP(xrDispatchTable* T, _processor_info* ID)
{
    // Scalar baseline — always valid
    T->skin1W    = xrSkin1W_x86;
    T->skin2W    = xrSkin2W_x86;
    T->skin3W    = xrSkin3W_x86;
    T->skin4W    = xrSkin4W_x86;
    skin4W_func  = xrSkin4W_x86;
    T->PLC_calc3 = PLC_calc3_x86;

    // SSE2 — guaranteed on all x64 CPUs
    if (ID->feature & _CPU_FEATURE_SSE2)
    {
        T->skin1W    = xrSkin1W_SSE2;
        T->skin2W    = xrSkin2W_SSE2;
        T->skin3W    = xrSkin3W_SSE2;
        T->skin4W    = xrSkin4W_SSE2;
        skin4W_func  = xrSkin4W_SSE2;
        T->PLC_calc3 = PLC_calc3_SSE;
    }

    // AVX2 + FMA3 — Haswell (2013) / Zen (2017) and newer
    if (ID->feature & _CPU_FEATURE_AVX2)
    {
        T->skin1W    = xrSkin1W_AVX2;
        T->skin2W    = xrSkin2W_AVX2;
        T->skin3W    = xrSkin3W_AVX2;
        T->skin4W    = xrSkin4W_AVX2;
        skin4W_func  = xrSkin4W_AVX2;
    }

    // Threading override for 4W (parallel bone computation)
    ttapi_Init(ID);
    if (ttapi_GetWorkersCount() > 1)
        T->skin4W = xrSkin4W_thread;
}
};

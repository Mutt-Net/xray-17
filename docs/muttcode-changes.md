# MuttCode Changes — Patch Notes

This document records all changes applied to the `cmake` branch that are not part of
the upstream community fork. Each entry describes the symptom, root cause, exact fix,
and the commit where it landed. The goal is to make every change portable — another
fork maintainer should be able to read an entry and apply the fix independently.

All changes target `src/Layers/xrRenderDX10/`, `src/Layers/xrRenderPC_R3/`, or
`src/Layers/xrRenderPC_R4/` unless stated otherwise.

---

## Sprint s1 — Render Bug Fixes

### S1-1: DXGI Windowed Refresh Rate

**Commit:** `b31cfba0`
**Files:** `src/Layers/xrRenderDX10/dx10HW.cpp`

**Symptom:** D3D validation layer warnings on displays not running at exactly 60 Hz.
No visible rendering artefact, but the incorrect value is logged by the debug runtime.

**Root cause:** Both the initial swapchain creation path and the device-reset path
hardcoded `RefreshRate = {60, 1}` in windowed mode. The DXGI documentation specifies
that the refresh rate field is ignored for windowed swapchains, and that `{0, 0}` is
the correct neutral value.

**Fix:** Replace both occurrences (creation and reset, DX10 and DX11 sub-branches)
with `{Numerator: 0, Denominator: 0}`. The `// TODO: fix this, shouldn't just default
to 60hz` comments were removed.

**Two sites to update:**
```cpp
// Site 1 — initial swapchain creation (windowed branch)
// Site 2 — device reset path (windowed branch)
// In both: set Numerator = 0, Denominator = 0 under both USE_DX11 and USE_DX10
```

---

### S1-2: DoAsyncScreenshot COM Reference Leak + Null Dereference

**Commit:** `f67ebd2c`
**Files:**
- `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp`
- `src/Layers/xrRenderPC_R3/r3_rendertarget_phase_combine.cpp`

**Symptom:** One COM reference to the swapchain backbuffer leaked on every screenshot
capture. On a long session with many screenshots, this accumulates. On a failed
`GetBuffer` call (e.g., device lost), the code would proceed with a null `pBuffer`
and crash inside `CopyResource`.

**Root cause:** `GetBuffer` on a DXGI swapchain increments the COM reference count on
the returned texture pointer. The code assigned the returned pointer but never called
`Release()`. Additionally, the `HRESULT` from `GetBuffer` was stored in `hr` but the
result was never checked before using `pBuffer`.

**Fix — applies to both R3 and R4:**
1. Check `hr` with `VERIFY(SUCCEEDED(hr))` immediately after `GetBuffer`.
2. Call `pBuffer->Release()` after `CopyResource`.
3. Remove the stale `// TODO: fox that later` comment.
4. Remove dead commented-out code blocks in the function body.

**R4-specific additional fix (commit `f67ebd2c`):**
```cpp
// Before (R4 only — wrong GUID on DX11 device):
hr = HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (LPVOID*)&pBuffer);

// After:
hr = HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3DTexture2D), (LPVOID*)&pBuffer);
```
Under `USE_DX11`, `ID3DTexture2D` resolves to `ID3D11Texture2D`. Passing the DX10
GUID to a DX11 swapchain returns `E_NOINTERFACE`, making the null-deref certain.
R3 uses `ID3D10Texture2D` directly, which is correct for a D3D10 device — do not
change R3.

---

### S1-3: set_AlphaRef Crash → Intentional No-Op

**Commit:** `e65c07ee`
**Files:** `src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h`

**Symptom:** Any code path that calls `CBackend::set_AlphaRef` crashes with a
`VERIFY(!"Not implemented.")` assertion.

**Root cause:** DX10+ removed the `D3DRS_ALPHAREF` hardware register. Alpha testing
in DX10/11 is performed in the shader via `clip()`. The DX10 port left the function
body as a crash guard rather than a proper no-op. The only caller in this codebase
is `dxUIRender::SetAlphaRef`, which has no active call sites in the DX10/11 game
paths. A separate, working alpha-ref path exists via `dx10StateManager::SetAlphaRef`
(shader constant binding) — it is unaffected by this change.

**Fix:**
```cpp
IC void CBackend::set_AlphaRef(u32 _value)
{
    // DX10+: no hardware alpha-ref state. Alpha test is performed in shader
    // via clip(). Alpha ref for shader-driven state (dx10StateManager) is
    // handled separately via BindAlphaRef / set_c. This path (via
    // dxUIRender::SetAlphaRef) has no active callers in DX10/11 paths.
    (void)_value;
}
```
Remove the `VERIFY` crash, the `// TODO: DX10: Implement rasterizer state update`
comment, and the commented-out DX9 `//if (alpha_ref != _value) { ... }` block.

---

### S1-4: Sun Shaft Gate on Sun Colour Luminance

**Commit:** `0669138b`
**Files:**
- `src/Layers/xrRenderPC_R3/r3_rendertarget.cpp`
- `src/Layers/xrRenderPC_R4/r4_rendertarget.cpp`

**Symptom:** Sun shaft rendering executes even when the sun colour is fully black,
wasting GPU time on a pass that will produce no visible output.

**Root cause:** `need_to_render_sunshafts()` only tested `m_fSunShaftsIntensity`
against a small threshold. The effective shaft contribution is
`intensity × sun_color`. A black sun (`sun_color = {0,0,0}`) passes the intensity
check and triggers the full shaft rendering pipeline regardless.

**Fix — identical in R3 and R4:**
```cpp
// Existing check:
if (fValue < 0.0001) return false;

// Add immediately after:
if (E.sun_color.square_magnitude() < 0.0001f) return false;
```
`CEnvDescriptor::sun_color` is a `Fvector3`. `square_magnitude()` is an existing
method — no new headers required.

---

### S1-5: MSAA Volumetric Accumulator Depth Binding

**Commit:** `4b2f6cb8` (part of the knowledge-comment + accumulator fix commit)
**Files:**
- `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_accumulator.cpp`
- `src/Layers/xrRenderPC_R3/r3_rendertarget_phase_accumulator.cpp`

**Symptom:** When running with MSAA enabled and the D3D debug layer active, the
following error is emitted on every frame during volumetric light accumulation:

```
ID3D11DeviceContext::OMSetRenderTargets: The RenderTargetView at slot 0 is not
compatible with the DepthStencilView. [...] The RenderTargetView has (mc:4,mq:...)
while the DepthStencilView has (mc:1,mq:0).
```

**Root cause:** `phase_vol_accumulator()` bound `HW.pBaseZB` (single-sample depth,
`mc:1`) as the depth-stencil view while the RT (`rt_Generic_2`) is a multisampled
rendertarget. D3D requires that the RT and DSV have matching sample counts.

Volumetric accumulation uses additive blending — it does not write to depth and does
not perform depth clip. A depth-stencil view is not needed. The fix is to pass `NULL`,
which is already what the SSfx branch of R4 does.

**Fix — R4:**
```cpp
// Before (inside the non-SSfx else branch):
if (!RImplementation.o.dx10_msaa)
    u_setrt(rt_Generic_2, NULL, NULL, HW.pBaseZB);
else
    u_setrt(rt_Generic_2, NULL, NULL, RImplementation.Target->rt_MSAADepth->pZRT);

// After (both cases collapsed):
u_setrt(rt_Generic_2, NULL, NULL, NULL);
```

**Fix — R3:** Identical pattern. R3 has no SSfx guard, so both the first-frame
(`m_bHasActiveVolumetric = true`) and subsequent branches are updated.

---

### S1-6: Investigation TODO → Knowledge Comment Pass

**Commit:** `4b2f6cb8`
**Files:** `dx10HW.cpp`, `dx10R_Backend_Runtime.h`, `r3.cpp`, `r4.cpp`,
`r3/r4 light_vis.cpp`, `r4_rendertarget_phase_combine.cpp`

**No behaviour change.** All remaining `// TODO: DX10:` markers that described
known limitations without a clear fix path were converted to explanatory comments.
The distinction: `// TODO` implies action is needed now; a knowledge comment
documents the constraint for future contributors.

Examples:
- Triangle fan unsupported → comment explains DX10 limitation and what an audit would require
- Input signature sharing → comment describes valid optimisation, flags it as non-blocking
- HBAO per-subsample → comment states shader changes are required; forced off for now
- DWM vsync edge case → `TODO:` prefix removed; limitation is documented

---

## Sprint s2 — DX10 Device Layer Correctness

### S2-1: RT Format Validation Before CreateTexture2D

**Commit:** `3ee2a20f`
**Files:**
- `src/Layers/xrRenderDX10/DXCommonTypes.h`
- `src/Layers/xrRenderDX10/dx10SH_RT.cpp`

**Symptom:** Creating a render target with a format unsupported by the device would
produce a hard D3D error inside `CreateTexture2D` with no graceful recovery. The
`CRT::create` function would not return early.

**Root cause:** The format validation block in `CRT::create` was entirely commented
out with a `// TODO: DX10: implement format support check` marker. The DX9 equivalent
called `CheckDeviceFormat`; the DX10 equivalent (`CheckFormatSupport`) was stubbed but
never wired up.

**Fix — Step 1: Add `D3D_FORMAT_SUPPORT_*` aliases to `DXCommonTypes.h`**

In both the DX11 section (~line 186) and the DX10 section (~line 442), after
`D3D_RESOURCE_MISC_GDI_COMPATIBLE`, add:
```cpp
#define D3D_FORMAT_SUPPORT_TEXTURE2D      D3D1x_FORMAT_SUPPORT_TEXTURE2D
#define D3D_FORMAT_SUPPORT_RENDER_TARGET  D3D1x_FORMAT_SUPPORT_RENDER_TARGET
#define D3D_FORMAT_SUPPORT_DEPTH_STENCIL  D3D1x_FORMAT_SUPPORT_DEPTH_STENCIL
#define D3D_FORMAT_SUPPORT_TEXTURECUBE    D3D1x_FORMAT_SUPPORT_TEXTURECUBE
```
(Replace `D3D1x_` with `D3D11_` in the DX11 section and `D3D10_` in the DX10 section.)

**Fix — Step 2: Replace commented-out block in `CRT::create`**
```cpp
// Replace the commented-out validation block with:
UINT FormatSupport = 0;
if (FAILED(HW.pDevice->CheckFormatSupport(dx10FMT, &FormatSupport))) return;
if (!(FormatSupport & D3D_FORMAT_SUPPORT_TEXTURE2D)) return;
if (!(FormatSupport & (bUseAsDepth
        ? D3D_FORMAT_SUPPORT_DEPTH_STENCIL
        : D3D_FORMAT_SUPPORT_RENDER_TARGET))) return;
```
Also remove the dead `//HRESULT _hr;` line (line 39 of `dx10SH_RT.cpp`).

---

### S2-2: CRTC DX10/11 Port — Cube Render Targets

**Commit:** `02d1738e`
**Files:**
- `src/Layers/xrRender/SH_RT.h`
- `src/Layers/xrRenderDX10/dx10SH_RT.cpp`
- `src/Layers/xrRenderDX10/dx10ResourceManager_Resources.cpp`

**Symptom:** Any code path that creates a cube render target (`ref_rtc`, `_CreateRTC`)
produces null surfaces silently. Effects that rely on cube RTs (environment reflections,
point light shadow maps using cube maps) produce black output or crash later when
dereferencing the null surface.

**Root cause:** The entire `CRTC` class in `SH_RT.h`, the implementation in
`dx10SH_RT.cpp`, and `_CreateRTC`/`_DeleteRTC` in `dx10ResourceManager_Resources.cpp`
were all inside `/* DX10 cut */` comment blocks. They were never ported from DX9 to DX10.

**Fix — SH_RT.h:** Uncomment the `CRTC` class; update types for DX10/11:
```cpp
// DX9:
IDirect3DCubeTexture9*  pSurface;
IDirect3DSurface9*      pRT[6];
IC BOOL valid() { return !pTexture; }   // BUG: inverted predicate

// DX10/11:
ID3DTexture2D*          pSurface;
ID3DRenderTargetView*   pRT[6];
IC BOOL valid() { return !!pTexture; }  // Fixed: double-negation for bool conversion
```

**Fix — dx10SH_RT.cpp:** Replace the `/* DX10 cut */` block with a DX10/11 implementation:

```cpp
void CRTC::create(LPCSTR Name, u32 size, D3DFORMAT f)
{
    if (pSurface) return;
    R_ASSERT(HW.pDevice && Name && Name[0] && size && btwIsPow2(size));
    _order = CPU::GetCLK();
    dwSize = size;
    fmt    = f;

    if (size > D3D10_REQ_TEXTURECUBE_DIMENSION) return;

    DXGI_FORMAT dx10FMT = dx10TextureUtils::ConvertTextureFormat(f);

    // Validate format supports cube RT usage.
    UINT FormatSupport = 0;
    if (FAILED(HW.pDevice->CheckFormatSupport(dx10FMT, &FormatSupport))) return;
    if (!(FormatSupport & D3D_FORMAT_SUPPORT_TEXTURECUBE))   return;
    if (!(FormatSupport & D3D_FORMAT_SUPPORT_RENDER_TARGET)) return;

    DEV->Evict();

    D3D_TEXTURE2D_DESC desc = {};
    desc.Width            = size;
    desc.Height           = size;
    desc.MipLevels        = 1;
    desc.ArraySize        = 6;
    desc.Format           = dx10FMT;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D_USAGE_DEFAULT;
    desc.BindFlags        = D3D_BIND_SHADER_RESOURCE | D3D_BIND_RENDER_TARGET;
    desc.MiscFlags        = D3D_RESOURCE_MISC_TEXTURECUBE;

    CHK_DX(HW.pDevice->CreateTexture2D(&desc, NULL, &pSurface));
    HW.stats_manager.increment_stats_rtarget(pSurface);

    for (u32 face = 0; face < 6; ++face)
    {
        D3D_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format                         = dx10FMT;
        rtvDesc.ViewDimension                  = D3D_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice        = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = face;
        rtvDesc.Texture2DArray.ArraySize       = 1;
        CHK_DX(HW.pDevice->CreateRenderTargetView(pSurface, &rtvDesc, &pRT[face]));
    }

    Msg("* created RTc(%s), 6(%d)", Name, size);
    pTexture = DEV->_CreateTexture(Name);
    pTexture->surface_set(pSurface);
}

void CRTC::destroy()
{
    if (!pSurface) return;
    if (pTexture._get())
    {
        pTexture->surface_set(0);
        pTexture.destroy();
        pTexture = nullptr;
    }
    for (u32 face = 0; face < 6; ++face)
        _RELEASE(pRT[face]);
    HW.stats_manager.decrement_stats_rtarget(pSurface);
    _RELEASE(pSurface);
}
```

**Fix — dx10ResourceManager_Resources.cpp:** Uncomment `_CreateRTC` and `_DeleteRTC`.
The logic is unchanged from DX9; only the comment wrapper is removed.

---

### S2-3: A2R10G10B10 / A2B10G10R10 Channel-Order Documentation

**Commit:** `74f6a88c`
**Files:** `src/Layers/xrRenderDX10/dx10TextureUtils.cpp`

**Symptom:** The format conversion table entry for `D3DFMT_A2R10G10B10` was marked
`// TODO: gross hack` with no explanation. `D3DFMT_A2B10G10R10` had a commented-out
mapping (also unexplained).

**Root cause:** `D3DFMT_A2R10G10B10` and `DXGI_FORMAT_R10G10B10A2_UNORM` differ in
channel order in the 32-bit word:
- `D3DFMT_A2R10G10B10`: bits 31–30 = A, 29–20 = R, 19–10 = G, 9–0 = B
- `DXGI_FORMAT_R10G10B10A2_UNORM`: bits 31–22 = R, 21–12 = G, 11–2 = B, 1–0 = A

The mapping is **correct** for DX10/11-generated data (which always uses RGBA channel
ordering), but would produce swapped colours for DX9-authored textures. Since no DX9
textures in this format exist on the R3/R4 path, the mapping is functionally sound —
it just needed documenting. `D3DFMT_A2B10G10R10` does have matching channel order to
`DXGI_FORMAT_R10G10B10A2_UNORM` and its commented-out entry was correct and restored.

**Fix:**
```cpp
// Replace the single TODO line with:
// D3DFMT_A2R10G10B10 and DXGI_FORMAT_R10G10B10A2_UNORM differ in channel order
// (A2R10G10B10: R[29:20] B[9:0] vs R10G10B10A2: R[31:22] B[11:2]).
// This mapping is correct for DX10/11-generated data (RGBA ordering in the pipeline)
// but would be wrong for D3D9-authored textures. No such textures exist in this path.
{D3DFMT_A2R10G10B10, DXGI_FORMAT_R10G10B10A2_UNORM},
// D3DFMT_A2B10G10R10 has matching channel order to DXGI_FORMAT_R10G10B10A2_UNORM.
{D3DFMT_A2B10G10R10, DXGI_FORMAT_R10G10B10A2_UNORM},
```

---

### S2-4: DX10 HDR10 Backbuffer Format and Colour Space

**Commit:** `d6b5eb8b`
**Files:** `src/Layers/xrRenderDX10/dx10HW.cpp`

**Symptom:** Setting `ps_r4_hdr10_on 1` had no effect on DX10. The DX10 path always
used `DXGI_FORMAT_R8G8B8A8_UNORM` for the backbuffer regardless of the HDR10 cvar,
and no colour space was set for the swapchain. HDR10 output was silently broken for the
DX10 renderer.

**Root cause:** The DX11 path already had the correct conditional format selection and
`IDXGISwapChain3` colour space setup. The DX10 path had a `// TODO: DX10: implement
dynamic format selection` comment and a hardcoded `DXGI_FORMAT_R8G8B8A8_UNORM`. The
colour space block was inside a `#if defined(USE_DX11)` guard and never ran for DX10.

**Fix — Step 1: Format selection**
```cpp
// Before (DX10 branch):
sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

// After:
sd.BufferDesc.Format = ps_r4_hdr10_on
    ? DXGI_FORMAT_R10G10B10A2_UNORM
    : DXGI_FORMAT_R8G8B8A8_UNORM;
```

**Fix — Step 2: Colour space block (DX10 path, after device creation)**

Add an `IDXGISwapChain3` colour space setup block to the DX10 device creation
path, mirroring the DX11 block. Key implementation notes:
- `IDXGISwapChain3` is a DXGI 1.4 interface — available on modern Windows 10 drivers
  regardless of whether the device is DX10 or DX11. Query it via `QueryInterface`.
- If `IDXGISwapChain3` is unavailable (DXGI < 1.4), log a note and continue — the
  format will be `R10G10B10A2_UNORM` but colour space defaults to SDR; HDR tone mapping
  will not engage.
- If the requested colour space (`DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020` for HDR10)
  is not supported by the display, fall back to SDR colour space and log accordingly.
- Always release the `IDXGISwapChain3` pointer after use.

---

## Follow-On Fixes (same session, addressing compilation issues from above)

| Commit | Fix |
|--------|-----|
| `a5ee052c` | Uncomment CRTC declarations in `dx10ResourceManager.h` and reset loops in `dx10ResourceManager_Resources.cpp` — required for S2-2 to link |
| `5b51435a` | Guard CRTC declarations inside `#if defined(USE_DX10) || defined(USE_DX11)` — prevents R1/R2 compilation breakage |
| `791eb3ee` | Guard the DX10 colour space block against failed device creation — `m_pSwapChain` may be null if creation fails earlier |
| `a0ef3fad` | Include `dxgi1_4.h` explicitly for DX10 builds — needed for `IDXGISwapChain3` declaration |
| `ed736fdc` | Disable IPO / strip LTCG for RelWithDebInfo — link-time optimisation was causing unreproducible failures in RelWithDebInfo builds |
| `77a476bc` | Add `static` to explicit template specialisations — fixes MSVC/Clang warning about symbol visibility |

---

## Sprint s3 — DX10/11 Port Cleanup + Regression-Harness Scaffold

Track A start (finish-the-port correctness) plus the Track B render-regression scaffold.
Full rationale: `docs/superpowers/specs/2026-06-03-sprint-s3-design.md`.

### S3-1: Remove dead cube-RT (`CRTC`) blocks from the DX9 base (RND-01)

**Files:** `src/Layers/xrRender/ResourceManager_Resources.cpp`, `src/Layers/xrRender/SH_RT.cpp`

**Symptom / framing correction:** the backlog claimed the "shared resource manager" cube-RT
(`CRTC`) path was "half-wired" and needed completing. It is not. `CRTC`, `m_rtargets_c`, and
`_CreateRTC`/`_DeleteRTC` are `#if defined(USE_DX10) || defined(USE_DX11)`-guarded in `SH_RT.h`
and `ResourceManager.h`. The two `/* DX10 cut */` blocks live in translation units compiled
**only into R2/DX9**, where those symbols do not exist — uncommenting them would *break* the DX9
build, not complete anything. The real implementation already exists and compiles for DX10/11 in
`xrRenderDX10/dx10SH_RT.cpp` + `dx10ResourceManager_Resources.cpp` (landed in sprint s2). The
whole `CRTC` apparatus is in fact vestigial — no caller anywhere constructs a `ref_rtc`.

**Fix:** delete the misleading dead blocks; leave a one-line pointer to the DX10/11 home. No
behavioural change (the code was never compiled).

### S3-2: Restore cube-RT census in the shared resource dump (RND-02)

**File:** `src/Layers/xrRender/ResourceManager_Reset.cpp`

**Symptom:** `CResourceManager::Dump()` (the resource census logged on reset/shutdown) omitted the
`rtargetsc` map — the line was `/* DX10 cut */`. `Dump()` is compiled into all four backends.

**Fix:** restore the `Msg("* RM_Dump: rtargetsc ...")` + `mdump(m_rtargets_c)` lines under
`#if defined(USE_DX10) || defined(USE_DX11)`, matching how `reset()` already references
`m_rtargets_c` in the same file. DX9 build is unaffected (guarded out).

### S3-3: Resolve the `set_xform` stub (RND-03)

**File:** `src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h`

**Symptom:** `CBackend::set_xform(u32, const Fmatrix&)` carried a commented-out
`//VERIFY(!"Implement CBackend::set_xform")`, reading as unfinished work.

**Root cause / disposition:** confirmed **dead on the runtime**. DX10+ has no fixed-function
transform stack (the DX9 form mapped to `SetTransform(D3DTS_*)`); world/view/projection reach
shaders via `R_xforms` constant buffers. The only caller, `set_Matrices`, is `#ifdef _EDITOR`
and never built into the shipping exes.

**Fix:** replace the commented `VERIFY` with a knowledge comment recording the deliberate no-op
and the evidence. Keeps `stat.xforms++`. (No compiled behaviour change.)

### S3-4: Render-regression harness — scaffold only (TEST-01 / TEST-02, OPEN)

**Files:** `tools/render_regression/` (`compare.py`, `cameras.example.ltx`, `README.md`),
`.github/workflows/render-regression.yml`, design spec under `docs/superpowers/specs/`.

A golden-image perceptual-diff safety net. The dependency-free comparator is delivered and
self-tested (`compare.py --selftest`); the CI workflow is gated (`workflow_dispatch`) so it
cannot fail the dormant pipeline. **Not completed:** golden capture needs a GPU runner
(BUILD-01/02) and a deterministic level fixture, neither available. TEST-01/02 remain open.

### S3-5: `GetGpuNum` hardcoded minimum forces 2-GPU mode on all DX10/11 systems (playtest find)

**Files:** `src/Layers/xrRender/HWCaps.cpp`

**Symptom:** On systems with an NVIDIA dGPU + AMD iGPU (e.g. Ryzen APU + RTX discrete), Anomaly/
GAMMA DX11 launches at a fraction of the configured resolution with the mouse cursor confined to the
top-left quarter of the window. Fullscreen mode freezes. Windowed mode renders into a tiny area.
Disabling the iGPU in Device Manager has no effect.

**Root cause:** `GetGpuNum()` (line 89 of `HWCaps.cpp`) unconditionally clamps the GPU count to a
minimum of 2:

```cpp
res = _max(res, 2);  // BUG: forces 2-GPU/SLI path on every DX10/11 system
```

This means DX10/DX11 always enters the multi-GPU rendering path regardless of what NVAPI or
DXGI actually reports. On single-GPU systems this is harmless when only an NVIDIA adapter is
present, but with a mixed NVIDIA+AMD setup the SLI swap-chain path misbehaves: DXGI enumerates
the AMD adapter even after it is OS-disabled (Status: Error, still returned by `IDXGIFactory::
EnumAdapters`), and the engine creates a swap chain sized against the wrong adapter's output,
producing the quarter-screen cursor clip region.

The iGPU's DXGI presence is the trigger, but the root cause is the hardcoded minimum — a single-
GPU system should never enter multi-GPU rendering.

**Fix:**
```cpp
// Before:
res = _max(res, 2);

// After:
res = _max(res, 1u);
```

**Workaround for live Anomaly/GAMMA builds (engine not recompilable):**
Set `rs_screenmode borderless` in `appdata/user.ltx`. Borderless mode uses a different swap-chain
present path that is less sensitive to the multi-GPU confusion. Fullscreen exclusive is broken
until the engine is rebuilt with the fix.

### S3-6: `CGammaControl::Update` hard crash in windowed/borderless mode (playtest find)

**Files:** `src/Layers/xrRender/xr_effgamma.cpp`

**Symptom:** Launching in borderless windowed mode causes a hard freeze on the taupe loading
screen requiring a reboot. Crash trace: `CGammaControl::Update` → `dxRenderDeviceRender::
OnDeviceCreate` → unhandled exception handler.

**Root cause:** `GetContainingOutput` on a DXGI swap chain returns `DXGI_ERROR_UNSUPPORTED`
when the window is not in exclusive fullscreen — the swap chain does not own a specific output
in windowed/borderless mode. The call was wrapped in `CHK_DX`, which asserts on any failure
HRESULT. The resulting unhandled exception fired during device creation, producing a GPU hang
that Windows could not recover from without a reboot.

**Fix:** Replace `CHK_DX(...)` with a guarded check and early return:
```cpp
IDXGIOutput* pOutput = nullptr;
if (FAILED(HW.m_pSwapChain->GetContainingOutput(&pOutput)) || !pOutput)
    return; // Windowed/borderless — no exclusive output, skip gamma ramp
```

---

## Render TODO Triage Reference

`docs/render-todo-triage.md` contains a full classified inventory of all render TODO/FIXME
items that existed at the start of sprint s1 (generated 2026-03-06). Each group records
whether it was resolved, cleaned up, or left as a documented knowledge comment.

Groups A–I summary:
- **A** (half-pixel offset): 14 sites fixed — commit `05b96961`
- **B** (rain): stale TODOs only — removed
- **C** (NV DBT / smap): dead code removed — commit `6fc6b71d`
- **D** (inverse culling): activated — commit `9f8093b4`
- **E** (HDR texture format): `t_ss_async` format matched to swapchain — commit `527180c6`
- **F** (stencil two-sided): dead code removed — commit `2b9e760d`
- **G** (blenders): all already implemented; stale TODOs removed — commit `3a92cc8a`
- **H** (device layer): cleanup done (sprint s1); four items resolved in sprint s2 (S2-1 through S2-4)
- **I** (performance / misc): all converted to knowledge comments
